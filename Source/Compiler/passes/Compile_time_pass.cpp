module;

#include <cassert>
#include <compare>

module h.compiler.compile_time_pass;

import llvm;
import std;
import std.compat;

import h.compiler.types;
import h.core;
import h.core.declarations;
import h.core.expressions;
import h.core.formatter;
import h.core.types;

namespace h::compiler
{    
    static h::Statement create_block_statement(
        std::pmr::vector<h::Statement> statements,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<Statement> block_statements{output_allocator};
        block_statements.assign(statements.begin(), statements.end());

        return h::Statement
        {
            .expressions = {
                h::Expression
                {
                    .data = h::Block_expression
                    {
                        .statements = std::move(statements)
                    }
                },
            },
        };
    }

    static h::Statement create_constant_expression_statement(
        Type_reference type,
        std::pmr::string data
    )
    {
        return h::Statement
        {
            .expressions = {
                h::Expression
                {
                    .data = h::Constant_expression
                    {
                        .type = std::move(type),
                        .data = std::move(data)
                    }
                },
            },
        };
    }

    static h::Statement create_constant_bool_expression_statement(bool const value)
    {
        return h::Statement
        {
            .expressions = {
                h::Expression
                {
                    .data = h::Constant_expression
                    {
                        .type = create_bool_type_reference(),
                        .data = value ? "true" : "false"
                    }
                },
            },
        };
    }

    static h::Statement create_type_expression_statement(Type_reference type)
    {
        return h::Statement
        {
            .expressions = {
                h::Expression
                {
                    .data = h::Type_expression
                    {
                        .type = std::move(type)
                    }
                },
            },
        };
    }

    static void add_import_usage_for_module(
        h::Module& core_module,
        std::string_view const module_name,
        std::string_view const usage,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        auto const location = std::find_if(
            core_module.dependencies.alias_imports.begin(),
            core_module.dependencies.alias_imports.end(),
            [&](Import_module_with_alias const& alias_import) -> bool { return alias_import.module_name == module_name; }
        );
        if (location == core_module.dependencies.alias_imports.end())
            return;

        auto const usage_location = std::find(location->usages.begin(), location->usages.end(), usage);
        if (usage_location != location->usages.end())
            return;

        location->usages.push_back(std::pmr::string{usage, output_allocator});
    }

    static Compile_time_value_and_type create_value_and_type(
        h::Statement statement
    )
    {
        return Compile_time_value_and_type
        {
            .statement = std::move(statement),
            .type = std::nullopt,
        };
    }

    static std::optional<bool> get_bool_from_value(
        Compile_time_value_and_type const& value
    )
    {
        h::Statement const& statement = value.statement;
        if (statement.expressions.empty())
            return std::nullopt;

        h::Expression const& expression = statement.expressions[0];
        if (std::holds_alternative<h::Constant_expression>(expression.data))
        {
            h::Constant_expression const& constant_expression = std::get<h::Constant_expression>(expression.data);
            if (h::is_bool(constant_expression.type) || h::is_c_bool(constant_expression.type))
                return constant_expression.data == "true" || constant_expression.data == "1";
        }

        return std::nullopt;
    }

    struct Compile_time_integer_value
    {
        bool is_signed;
        std::int64_t signed_value;
        std::uint64_t unsigned_value;
    };

    static std::optional<Compile_time_integer_value> get_integer_from_value(
        Compile_time_value_and_type const& value
    )
    {
        h::Statement const& statement = value.statement;
        if (statement.expressions.empty())
            return std::nullopt;

        h::Expression const& expression = statement.expressions[0];
        if (!std::holds_alternative<h::Constant_expression>(expression.data))
            return std::nullopt;

        h::Constant_expression const constant_expression = std::get<h::Constant_expression>(expression.data);
        if (!h::is_integer(constant_expression.type) && !h::is_byte(constant_expression.type))
            return std::nullopt;

        if (h::is_unsigned_integer(constant_expression.type) || h::is_byte(constant_expression.type))
        {
            std::uint64_t unsigned_value = 0;
            auto [pointer, error_code] = std::from_chars(constant_expression.data.data(), constant_expression.data.data() + constant_expression.data.size(), unsigned_value);
            if (error_code != std::errc() || pointer != constant_expression.data.data() + constant_expression.data.size())
                return std::nullopt;

            return Compile_time_integer_value{.is_signed = false, .signed_value = 0, .unsigned_value = unsigned_value};
        }

        if (h::is_signed_integer(constant_expression.type))
        {
            std::int64_t signed_value = 0;
            auto [pointer, error_code] = std::from_chars(constant_expression.data.data(), constant_expression.data.data() + constant_expression.data.size(), signed_value);
            if (error_code != std::errc() || pointer != constant_expression.data.data() + constant_expression.data.size())
                return std::nullopt;

            return Compile_time_integer_value{.is_signed = true, .signed_value = signed_value, .unsigned_value = 0};
        }

        return std::nullopt;
    }

    static std::uint64_t get_required_reflection_index_argument(
        h::Statement const& statement,
        h::Reflection_expression const& expression,
        std::string_view const function_name,
        Compile_time_parameters const& parameters
    )
    {
        if (expression.arguments.size() != 1)
            throw std::runtime_error{ std::format("{}() requires exactly one argument!", function_name) };

        h::Expression_index const index_expression_index = expression.arguments[0];
        if (index_expression_index.expression_index >= statement.expressions.size())
            throw std::runtime_error{ std::format("{}() has an invalid argument index!", function_name) };

        h::Expression const& index_expression = statement.expressions[index_expression_index.expression_index];
        std::optional<Compile_time_value_and_type> const index_value = evaluate_compile_time_expression(statement, index_expression, parameters);
        if (!index_value.has_value())
            throw std::runtime_error{ std::format("{}() argument must be a compile-time integer constant!", function_name) };

        std::optional<Compile_time_integer_value> const integer_value = get_integer_from_value(index_value.value());
        if (!integer_value.has_value())
            throw std::runtime_error{ std::format("{}() argument must be an integer constant!", function_name) };

        if (integer_value->is_signed)
        {
            if (integer_value->signed_value < 0)
                throw std::runtime_error{ std::format("{}() argument must be >= 0!", function_name) };

            return static_cast<std::uint64_t>(integer_value->signed_value);
        }

        return integer_value->unsigned_value;
    }

    static std::pmr::string get_type_kind_member_name(
        Type_reference const& type_reference,
        Compile_time_parameters const& parameters
    )
    {
        std::optional<Type_reference> const underlying_type = get_underlying_type(parameters.declaration_database, type_reference);
        Type_reference const& resolved_type = underlying_type.value_or(type_reference);

        if (std::holds_alternative<h::Array_slice_type>(resolved_type.data))
            return std::pmr::string{"Array_slice", parameters.output_allocator};

        if (std::holds_alternative<h::Builtin_type_reference>(resolved_type.data))
            return std::pmr::string{"Builtin", parameters.output_allocator};

        if (std::holds_alternative<h::Constant_array_type>(resolved_type.data))
            return std::pmr::string{"Constant_array", parameters.output_allocator};

        if (std::holds_alternative<h::Custom_type_reference>(resolved_type.data))
        {
            std::optional<h::Declaration> const declaration = find_underlying_declaration(parameters.declaration_database, resolved_type);
            if (!declaration.has_value())
                return std::pmr::string{"Custom", parameters.output_allocator};

            if (std::holds_alternative<h::Struct_declaration const*>(declaration->data))
                return std::pmr::string{"Struct", parameters.output_allocator};
            if (std::holds_alternative<h::Union_declaration const*>(declaration->data))
                return std::pmr::string{"Union", parameters.output_allocator};
            if (std::holds_alternative<h::Enum_declaration const*>(declaration->data))
                return std::pmr::string{"Enum", parameters.output_allocator};

            return std::pmr::string{"Custom", parameters.output_allocator};
        }

        if (std::holds_alternative<h::Fundamental_type>(resolved_type.data))
        {
            h::Fundamental_type const fundamental_type = std::get<h::Fundamental_type>(resolved_type.data);
            switch (fundamental_type)
            {
                case h::Fundamental_type::Bool:
                case h::Fundamental_type::C_bool:
                    return std::pmr::string{"Bool", parameters.output_allocator};
                case h::Fundamental_type::Float16:
                case h::Fundamental_type::Float32:
                case h::Fundamental_type::Float64:
                case h::Fundamental_type::C_longdouble:
                    return std::pmr::string{"Float", parameters.output_allocator};
                case h::Fundamental_type::Byte:
                case h::Fundamental_type::C_uchar:
                case h::Fundamental_type::C_ushort:
                case h::Fundamental_type::C_uint:
                case h::Fundamental_type::C_ulong:
                case h::Fundamental_type::C_ulonglong:
                    return std::pmr::string{"Uint", parameters.output_allocator};
                case h::Fundamental_type::C_char:
                case h::Fundamental_type::C_schar:
                case h::Fundamental_type::C_short:
                case h::Fundamental_type::C_int:
                case h::Fundamental_type::C_long:
                case h::Fundamental_type::C_longlong:
                    return std::pmr::string{"Int", parameters.output_allocator};
                case h::Fundamental_type::String:
                case h::Fundamental_type::Any_type:
                    return std::pmr::string{"Builtin", parameters.output_allocator};
                default:
                    return std::pmr::string{"Builtin", parameters.output_allocator};
            }
        }

        if (std::holds_alternative<h::Function_pointer_type>(resolved_type.data))
            return std::pmr::string{"Function_pointer", parameters.output_allocator};

        if (std::holds_alternative<h::Integer_type>(resolved_type.data))
        {
            h::Integer_type const& integer_type = std::get<h::Integer_type>(resolved_type.data);
            return std::pmr::string{integer_type.is_signed ? "Int" : "Uint", parameters.output_allocator};
        }

        if (std::holds_alternative<h::Null_pointer_type>(resolved_type.data))
            return std::pmr::string{"Null_pointer", parameters.output_allocator};

        if (std::holds_alternative<h::Pointer_type>(resolved_type.data))
            return std::pmr::string{"Pointer", parameters.output_allocator};

        if (std::holds_alternative<h::Type_instance>(resolved_type.data))
            return std::pmr::string{"Custom", parameters.output_allocator};

        return std::pmr::string{"Custom", parameters.output_allocator};
    }

    static void replace_variable_with_constant_in_statement(
        h::Statement& statement,
        std::string_view const variable_name,
        h::Type_reference const& constant_type,
        std::pmr::string const& constant_data
    );

    static void replace_variable_with_constant_in_expression(
        h::Expression& expression,
        std::string_view const variable_name,
        h::Type_reference const& constant_type,
        std::pmr::string const& constant_data
    );

    static void replace_variable_with_constant_in_statement(
        h::Statement& statement,
        std::string_view const variable_name,
        h::Type_reference const& constant_type,
        std::pmr::string const& constant_data
    )
    {
        for (h::Expression& expression : statement.expressions)
            replace_variable_with_constant_in_expression(expression, variable_name, constant_type, constant_data);
    }

    static void replace_variable_with_constant_in_expression(
        h::Expression& expression,
        std::string_view const variable_name,
        h::Type_reference const& constant_type,
        std::pmr::string const& constant_data
    )
    {
        if (std::holds_alternative<h::Variable_expression>(expression.data))
        {
            h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(expression.data);
            if (variable_expression.name == variable_name)
            {
                expression = h::Expression
                {
                    .data = h::Constant_expression
                    {
                        .type = constant_type,
                        .data = constant_data
                    }
                };
                return;
            }
        }

        if (std::holds_alternative<h::Block_expression>(expression.data))
        {
            h::Block_expression& data = std::get<h::Block_expression>(expression.data);
            for (h::Statement& statement : data.statements)
                replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
        }
        else if (std::holds_alternative<h::Constant_array_expression>(expression.data))
        {
            h::Constant_array_expression& data = std::get<h::Constant_array_expression>(expression.data);
            for (h::Statement& statement : data.array_data)
                replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
        }
        else if (std::holds_alternative<h::For_loop_expression>(expression.data))
        {
            h::For_loop_expression& data = std::get<h::For_loop_expression>(expression.data);
            replace_variable_with_constant_in_statement(data.range_end, variable_name, constant_type, constant_data);
            for (h::Statement& statement : data.then_statements)
                replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
        }
        else if (std::holds_alternative<h::If_expression>(expression.data))
        {
            h::If_expression& data = std::get<h::If_expression>(expression.data);
            for (h::Condition_statement_pair& pair : data.series)
            {
                if (pair.condition.has_value())
                    replace_variable_with_constant_in_statement(*pair.condition, variable_name, constant_type, constant_data);

                for (h::Statement& statement : pair.then_statements)
                    replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
            }
        }
        else if (std::holds_alternative<h::Switch_expression>(expression.data))
        {
            h::Switch_expression& data = std::get<h::Switch_expression>(expression.data);
            for (h::Switch_case_expression_pair& pair : data.cases)
                for (h::Statement& statement : pair.statements)
                    replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
        }
        else if (std::holds_alternative<h::Ternary_condition_expression>(expression.data))
        {
            h::Ternary_condition_expression& data = std::get<h::Ternary_condition_expression>(expression.data);
            replace_variable_with_constant_in_statement(data.then_statement, variable_name, constant_type, constant_data);
            replace_variable_with_constant_in_statement(data.else_statement, variable_name, constant_type, constant_data);
        }
        else if (std::holds_alternative<h::While_loop_expression>(expression.data))
        {
            h::While_loop_expression& data = std::get<h::While_loop_expression>(expression.data);
            replace_variable_with_constant_in_statement(data.condition, variable_name, constant_type, constant_data);
            for (h::Statement& statement : data.then_statements)
                replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
        }
    }

    static std::optional<Compile_time_value_and_type> evaluate_compile_time_for_loop_expression(
        h::Statement const& statement,
        h::For_loop_expression const& expression,
        Compile_time_parameters const& parameters
    )
    {
        if (expression.range_begin.expression_index >= statement.expressions.size())
            return std::nullopt;

        switch (expression.range_comparison_operation)
        {
            case h::Binary_operation::Less_than:
            case h::Binary_operation::Less_than_or_equal_to:
            case h::Binary_operation::Greater_than:
            case h::Binary_operation::Greater_than_or_equal_to:
                break;
            default:
                return std::nullopt;
        }

        h::Expression const& range_begin_expression = statement.expressions[expression.range_begin.expression_index];
        std::optional<Compile_time_value_and_type> const range_begin_value = evaluate_compile_time_expression(statement, range_begin_expression, parameters);
        if (!range_begin_value.has_value())
            return std::nullopt;

        std::optional<Compile_time_integer_value> const range_begin_integer = get_integer_from_value(range_begin_value.value());
        if (!range_begin_integer.has_value())
            return std::nullopt;

        std::optional<Compile_time_value_and_type> const range_end_value = evaluate_compile_time_statement(expression.range_end, parameters);
        if (!range_end_value.has_value())
            return std::nullopt;

        std::optional<Compile_time_integer_value> const range_end_integer = get_integer_from_value(range_end_value.value());
        if (!range_end_integer.has_value())
            return std::nullopt;

        auto const get_step_integer = [&]() -> std::optional<Compile_time_integer_value>
        {
            if (expression.step_by.has_value())
            {
                if (expression.step_by->expression_index >= statement.expressions.size())
                    return std::nullopt;

                h::Expression const& step_expression = statement.expressions[expression.step_by->expression_index];
                std::optional<Compile_time_value_and_type> const step_value = evaluate_compile_time_expression(statement, step_expression, parameters);
                if (!step_value.has_value())
                    return std::nullopt;

                return get_integer_from_value(step_value.value());
            }

            return std::nullopt;
        };
        std::optional<Compile_time_integer_value> const step_integer = get_step_integer();

        // Limits to avoid runaway unrolling
        constexpr std::size_t maximum_unroll_iterations = 1024;
        std::pmr::vector<h::Statement> iteration_blocks{parameters.output_allocator};
        iteration_blocks.reserve(16);

        auto const create_iteration_block = [&](auto const loop_index_value) -> void
        {
            std::pmr::vector<h::Statement> body{parameters.output_allocator};
            body.assign(expression.then_statements.begin(), expression.then_statements.end());

            std::pmr::string const integer_string = std::pmr::string{std::to_string(loop_index_value)};
            h::Type_reference const index_type = range_begin_value->type.value_or(create_integer_type_type_reference(64, true));

            for (h::Statement& statement : body)
                replace_variable_with_constant_in_statement(statement, expression.variable_name, index_type, integer_string);

            iteration_blocks.push_back(create_block_statement(std::move(body), parameters.output_allocator));
        };

        auto compare = [&](auto const value, auto const range_end) -> bool
        {
            switch (expression.range_comparison_operation)
            {
                case h::Binary_operation::Less_than:
                    return value < range_end;
                case h::Binary_operation::Less_than_or_equal_to:
                    return value <= range_end;
                case h::Binary_operation::Greater_than:
                    return value > range_end;
                case h::Binary_operation::Greater_than_or_equal_to:
                    return value >= range_end;
                default:
                    return false;
            }

            return false;
        };

        if (range_begin_integer->is_signed)
        {
            std::int64_t const range_begin = range_begin_integer->signed_value;
            std::int64_t const range_end = range_end_integer->signed_value;
            std::int64_t const step_value = step_integer.has_value() ? step_integer->signed_value : std::int64_t{1};
            std::int64_t current = range_begin;

            while (compare(current, range_end))
            {
                if (iteration_blocks.size() >= maximum_unroll_iterations)
                    return std::nullopt;

                create_iteration_block(current);
                current += step_value;
            }
        }
        else
        {
            std::uint64_t const range_begin = range_begin_integer->unsigned_value;
            std::uint64_t const range_end = range_end_integer->unsigned_value;
            std::uint64_t const step_value = step_integer.has_value() ? step_integer->unsigned_value : std::uint64_t{1};
            std::uint64_t current = range_begin;

            while (compare(current, range_end))
            {
                if (iteration_blocks.size() >= maximum_unroll_iterations)
                    return std::nullopt;

                create_iteration_block(current);
                current += step_value;
            }
        }

        return create_value_and_type(create_block_statement(std::move(iteration_blocks), parameters.output_allocator));
    }

    static std::optional<Compile_time_value_and_type> evaluate_compile_time_reflection_expression(
        h::Statement const& statement,
        h::Reflection_expression const& expression,
        Compile_time_parameters const& parameters
    )
    {
        if (expression.name == "alignment_of")
        {
            if (expression.type_arguments.size() != 1)
                throw std::runtime_error{ "alignment_of() requires exactly one type argument!" };

            Type_reference const& type_reference = expression.type_arguments[0];
            llvm::Type* const llvm_type = type_reference_to_llvm_type_on_demand(
                parameters.llvm_context,
                parameters.llvm_data_layout,
                parameters.core_module,
                type_reference,
                parameters.declaration_database,
                parameters.clang_context
            );
            llvm::Align const alignment = parameters.llvm_data_layout.getABITypeAlign(llvm_type);
            std::uint64_t const alignment_in_bytes = alignment.value();

            std::pmr::string data = std::pmr::string{std::to_string(alignment_in_bytes)};
            return create_value_and_type(create_constant_expression_statement(
                create_integer_type_type_reference(64, false),
                std::move(data)
            ));
        }
        else if (expression.name == "size_of")
        {
            if (expression.type_arguments.size() != 1)
                throw std::runtime_error{ "size_of() requires exactly one type argument!" };

            Type_reference const& type_reference = expression.type_arguments[0];
            llvm::Type* const llvm_type = type_reference_to_llvm_type_on_demand(
                parameters.llvm_context,
                parameters.llvm_data_layout,
                parameters.core_module,
                type_reference,
                parameters.declaration_database,
                parameters.clang_context
            );
            std::uint64_t const size_in_bytes = parameters.llvm_data_layout.getTypeAllocSize(llvm_type);

            std::pmr::string data = std::pmr::string{std::to_string(size_in_bytes)};
            return create_value_and_type(create_constant_expression_statement(
                create_integer_type_type_reference(64, false),
                std::move(data)
            ));
        }
        else if (expression.name == "type_name")
        {
            if (expression.type_arguments.size() != 1)
                throw std::runtime_error{ "type_name() requires exactly one type argument!" };

            if (!expression.arguments.empty())
                throw std::runtime_error{ "type_name() does not take runtime arguments!" };

            std::pmr::string const type_name = format_type_reference(
                parameters.core_module,
                expression.type_arguments[0],
                parameters.output_allocator,
                parameters.temporaries_allocator
            );

            return create_value_and_type(create_constant_expression_statement(
                create_c_string_type_reference(false),
                type_name
            ));
        }
        else if (expression.name == "member_count")
        {
            if (expression.type_arguments.size() != 1)
                throw std::runtime_error{ "member_count() requires exactly one type argument!" };

            if (!expression.arguments.empty())
                throw std::runtime_error{ "member_count() does not take runtime arguments!" };

            Type_reference const& type_reference = expression.type_arguments[0];
            std::optional<h::Declaration> const declaration = find_underlying_declaration(parameters.declaration_database, type_reference);
            if (!declaration.has_value())
                throw std::runtime_error{ "member_count() could not resolve declaration for type argument!" };

            std::uint64_t member_count = 0;
            if (std::holds_alternative<h::Struct_declaration const*>(declaration->data))
            {
                h::Struct_declaration const& struct_declaration = *std::get<h::Struct_declaration const*>(declaration->data);
                member_count = struct_declaration.member_types.size();
            }
            else if (std::holds_alternative<h::Union_declaration const*>(declaration->data))
            {
                h::Union_declaration const& union_declaration = *std::get<h::Union_declaration const*>(declaration->data);
                member_count = union_declaration.member_types.size();
            }
            else
            {
                throw std::runtime_error{ "member_count() requires a struct or union type argument!" };
            }

            return create_value_and_type(create_constant_expression_statement(
                create_integer_type_type_reference(64, false),
                std::pmr::string{std::to_string(member_count)}
            ));
        }
        else if (expression.name == "member_type")
        {
            if (expression.type_arguments.size() != 1)
                throw std::runtime_error{ "member_type() requires exactly one type argument!" };

            std::uint64_t const member_index = get_required_reflection_index_argument(statement, expression, "member_type", parameters);

            Type_reference const& type_reference = expression.type_arguments[0];
            std::optional<h::Declaration> const declaration = find_underlying_declaration(parameters.declaration_database, type_reference);
            if (!declaration.has_value())
                throw std::runtime_error{ "member_type() could not resolve declaration for type argument!" };

            std::optional<Type_reference> output_type = std::nullopt;
            if (std::holds_alternative<h::Struct_declaration const*>(declaration->data))
            {
                h::Struct_declaration const& struct_declaration = *std::get<h::Struct_declaration const*>(declaration->data);
                if (member_index >= struct_declaration.member_types.size())
                    throw std::runtime_error{ "member_type() index is out of bounds!" };

                output_type = struct_declaration.member_types[member_index];
            }
            else if (std::holds_alternative<h::Union_declaration const*>(declaration->data))
            {
                h::Union_declaration const& union_declaration = *std::get<h::Union_declaration const*>(declaration->data);
                if (member_index >= union_declaration.member_types.size())
                    throw std::runtime_error{ "member_type() index is out of bounds!" };

                output_type = union_declaration.member_types[member_index];
            }
            else
            {
                throw std::runtime_error{ "member_type() requires a struct or union type argument!" };
            }

            add_import_usage_for_module(parameters.core_module, "iris.builtin", "Type_kind", parameters.output_allocator);

            return create_value_and_type(create_type_expression_statement(output_type.value()));
        }
        else if (expression.name == "member_offset")
        {
            if (expression.type_arguments.size() != 1)
                throw std::runtime_error{ "member_offset() requires exactly one type argument!" };

            std::uint64_t const member_index = get_required_reflection_index_argument(statement, expression, "member_offset", parameters);

            Type_reference const& type_reference = expression.type_arguments[0];
            std::optional<h::Declaration> const declaration = find_underlying_declaration(parameters.declaration_database, type_reference);
            if (!declaration.has_value())
                throw std::runtime_error{ "member_offset() could not resolve declaration for type argument!" };

            std::uint64_t offset_in_bits = 0;
            if (std::holds_alternative<h::Struct_declaration const*>(declaration->data))
            {
                h::Struct_declaration const& struct_declaration = *std::get<h::Struct_declaration const*>(declaration->data);
                if (member_index >= struct_declaration.member_types.size())
                    throw std::runtime_error{ "member_offset() index is out of bounds!" };

                llvm::Type* const llvm_type = type_reference_to_llvm_type_on_demand(
                    parameters.llvm_context,
                    parameters.llvm_data_layout,
                    parameters.core_module,
                    type_reference,
                    parameters.declaration_database,
                    parameters.clang_context
                );
                if (!llvm::StructType::classof(llvm_type))
                    throw std::runtime_error{ "member_offset() expected a StructType in LLVM!" };

                llvm::StructType* const llvm_struct_type = llvm::cast<llvm::StructType>(llvm_type);
                llvm::StructLayout const* const llvm_struct_layout = parameters.llvm_data_layout.getStructLayout(llvm_struct_type);
                offset_in_bits = 8 * llvm_struct_layout->getElementOffset(member_index);
            }
            else if (std::holds_alternative<h::Union_declaration const*>(declaration->data))
            {
                h::Union_declaration const& union_declaration = *std::get<h::Union_declaration const*>(declaration->data);
                if (member_index >= union_declaration.member_types.size())
                    throw std::runtime_error{ "member_offset() index is out of bounds!" };

                offset_in_bits = 0;
            }
            else
            {
                throw std::runtime_error{ "member_offset() requires a struct or union type argument!" };
            }

            return create_value_and_type(create_constant_expression_statement(
                create_integer_type_type_reference(64, false),
                std::pmr::string{std::to_string(offset_in_bits)}
            ));
        }
        else if (expression.name == "member_name")
        {
            if (expression.type_arguments.size() != 1)
                throw std::runtime_error{ "member_name() requires exactly one type argument!" };

            std::uint64_t const member_index = get_required_reflection_index_argument(statement, expression, "member_name", parameters);

            Type_reference const& type_reference = expression.type_arguments[0];
            std::optional<h::Declaration> const declaration = find_underlying_declaration(parameters.declaration_database, type_reference);
            if (!declaration.has_value())
                throw std::runtime_error{ "member_name() could not resolve declaration for type argument!" };

            std::pmr::string member_name{parameters.output_allocator};
            if (std::holds_alternative<h::Struct_declaration const*>(declaration->data))
            {
                h::Struct_declaration const& struct_declaration = *std::get<h::Struct_declaration const*>(declaration->data);
                if (member_index >= struct_declaration.member_names.size())
                    throw std::runtime_error{ "member_name() index is out of bounds!" };

                member_name = struct_declaration.member_names[member_index];
            }
            else if (std::holds_alternative<h::Union_declaration const*>(declaration->data))
            {
                h::Union_declaration const& union_declaration = *std::get<h::Union_declaration const*>(declaration->data);
                if (member_index >= union_declaration.member_names.size())
                    throw std::runtime_error{ "member_name() index is out of bounds!" };

                member_name = union_declaration.member_names[member_index];
            }
            else
            {
                throw std::runtime_error{ "member_name() requires a struct or union type argument!" };
            }

            return create_value_and_type(create_constant_expression_statement(
                create_c_string_type_reference(false),
                std::move(member_name)
            ));
        }
        else if (expression.name == "get_type_kind")
        {
            if (expression.type_arguments.size() != 1)
                throw std::runtime_error{ "get_type_kind() requires exactly one type argument!" };

            if (!expression.arguments.empty())
                throw std::runtime_error{ "get_type_kind() does not take runtime arguments!" };

            std::pmr::string const member_name = get_type_kind_member_name(expression.type_arguments[0], parameters);
            h::Statement enum_statement =
            {
                .expressions = create_enum_value_expressions("Type_kind", member_name)
            };

            return create_value_and_type(std::move(enum_statement));
        }
        else
        {
            throw std::runtime_error{ std::format("Reflection expression '{}' not implemented!", expression.name) };
        }
    }

    static std::optional<Compile_time_value_and_type> evaluate_compile_time_unary_expression(
        h::Statement const& statement,
        h::Unary_expression const& expression,
        Compile_time_parameters const& parameters
    )
    {
        switch (expression.operation)
        {
            case h::Unary_operation::Bitwise_not:
            case h::Unary_operation::Minus:
            case h::Unary_operation::Pre_increment:
            case h::Unary_operation::Post_increment:
            case h::Unary_operation::Pre_decrement:
            case h::Unary_operation::Post_decrement:
            case h::Unary_operation::Indirection:
            case h::Unary_operation::Address_of:
                return std::nullopt;
            default:
                break;
        }

        h::Expression const& right_side_expression = statement.expressions[expression.expression.expression_index];
        std::optional<Compile_time_value_and_type> const right_side_value = evaluate_compile_time_expression(statement, right_side_expression, parameters);
        if (!right_side_value.has_value())
            return std::nullopt;

        if (expression.operation == h::Unary_operation::Not)
        {
            std::optional<bool> const value = get_bool_from_value(right_side_value.value());
            if (!value.has_value())
                return std::nullopt;

            bool const not_value = !value.value();
            return create_value_and_type(create_constant_bool_expression_statement(not_value));
        }

        return std::nullopt;
    }

    static std::optional<Compile_time_value_and_type> evaluate_compile_time_binary_expression(
        h::Statement const& statement,
        h::Binary_expression const& expression,
        Compile_time_parameters const& parameters
    )
    {
        if (!is_comparison_binary_operation(expression.operation) && !is_equality_binary_operation(expression.operation))
            throw std::runtime_error{ std::format("Unsupported compile_time binary operation '{}'", static_cast<int>(expression.operation)) };

        if (expression.left_hand_side.expression_index >= statement.expressions.size())
            throw std::runtime_error{ "Invalid left operand index in compile_time binary expression" };

        if (expression.right_hand_side.expression_index >= statement.expressions.size())
            throw std::runtime_error{ "Invalid right operand index in compile_time binary expression" };

        h::Expression const& left_expression = statement.expressions[expression.left_hand_side.expression_index];
        h::Expression const& right_expression = statement.expressions[expression.right_hand_side.expression_index];

        std::optional<Compile_time_value_and_type> const left_value = evaluate_compile_time_expression(statement, left_expression, parameters);
        if (!left_value.has_value())
            throw std::runtime_error{ "Could not evaluate left operand in compile_time binary expression" };

        std::optional<Compile_time_value_and_type> const right_value = evaluate_compile_time_expression(statement, right_expression, parameters);
        if (!right_value.has_value())
            throw std::runtime_error{ "Could not evaluate right operand in compile_time binary expression" };

        std::optional<Compile_time_integer_value> const left_integer = get_integer_from_value(left_value.value());
        if (!left_integer.has_value())
            throw std::runtime_error{ "Left operand of compile_time binary expression is not an integer constant" };

        std::optional<Compile_time_integer_value> const right_integer = get_integer_from_value(right_value.value());
        if (!right_integer.has_value())
            throw std::runtime_error{ "Right operand of compile_time binary expression is not an integer constant" };

        if (left_integer->is_signed != right_integer->is_signed)
            throw std::runtime_error{ "Signed/unsigned comparison is not supported in compile_time binary expression" };

        auto const compare = [&](auto const left_side, auto const right_side) -> bool
        {
            switch (expression.operation)
            {
                case h::Binary_operation::Equal:
                    return left_side == right_side;
                case h::Binary_operation::Not_equal:
                    return left_side != right_side;
                case h::Binary_operation::Less_than:
                    return left_side < right_side;
                case h::Binary_operation::Less_than_or_equal_to:
                    return left_side <= right_side;
                case h::Binary_operation::Greater_than:
                    return left_side > right_side;
                case h::Binary_operation::Greater_than_or_equal_to:
                    return left_side >= right_side;
                default:
                    throw std::runtime_error{ "Unsupported compile_time comparison operation" };
            }

            return false;
        };

        bool const result = left_integer->is_signed
            ? compare(left_integer->signed_value, right_integer->signed_value)
            : compare(left_integer->unsigned_value, right_integer->unsigned_value);

        return create_value_and_type(create_constant_bool_expression_statement(result));
    }

    static std::optional<Compile_time_value_and_type> evaluate_compile_time_variable_expression(
        h::Statement const& statement,
        h::Variable_expression const& expression,
        Compile_time_parameters const& parameters
    )
    {
        // Search for global variables:
        {
            std::optional<h::Global_variable_declaration const*> const declaration = h::find_global_variable_declaration(parameters.core_module, expression.name);
            if (declaration.has_value())
            {
                h::Global_variable_declaration const& global_variable_declaration = *declaration.value();
                return Compile_time_value_and_type
                {
                    .statement = global_variable_declaration.initial_value,
                    .type = global_variable_declaration.type
                };
            }
        }

        return std::nullopt;
    }

    std::optional<Compile_time_value_and_type> evaluate_compile_time_expression(
        h::Statement const& statement,
        h::Expression const& expression,
        Compile_time_parameters const& parameters
    )
    {
        if (std::holds_alternative<h::Compile_time_expression>(expression.data))
        {
            h::Compile_time_expression const& compile_time_expression = std::get<h::Compile_time_expression>(expression.data);
            if (compile_time_expression.expression.expression_index >= statement.expressions.size())
                return std::nullopt;
                
            h::Expression const& right_side_expression = statement.expressions[compile_time_expression.expression.expression_index];
            return evaluate_compile_time_expression(statement, right_side_expression, parameters);
        }
        else if (std::holds_alternative<h::Constant_expression>(expression.data))
        {
            h::Constant_expression const& constant_expression = std::get<h::Constant_expression>(expression.data);
            return create_value_and_type({.expressions = {h::Expression{.data = constant_expression}}});
        }
        else if (std::holds_alternative<h::If_expression>(expression.data))
        {
            h::If_expression const& if_expression = std::get<h::If_expression>(expression.data);

            for (std::size_t index = 0; index < if_expression.series.size(); ++index)
            {
                h::Condition_statement_pair const& serie = if_expression.series[index];

                if (!serie.condition.has_value())
                    return create_value_and_type(create_block_statement(serie.then_statements, parameters.output_allocator));

                h::Statement const& condition_statement = serie.condition.value();
                std::optional<Compile_time_value_and_type> const condition_value = evaluate_compile_time_statement(condition_statement, parameters);
                if (!condition_value.has_value())
                    return std::nullopt;

                std::optional<bool> const condition = get_bool_from_value(condition_value.value());
                if (!condition.has_value())
                    return std::nullopt;
                
                if (condition.value())
                    return create_value_and_type(create_block_statement(serie.then_statements, parameters.output_allocator));
            }

            return std::nullopt;
        }
        else if (std::holds_alternative<h::For_loop_expression>(expression.data))
        {
            h::For_loop_expression const& for_loop_expression = std::get<h::For_loop_expression>(expression.data);
            return evaluate_compile_time_for_loop_expression(statement, for_loop_expression, parameters);
        }
        else if (std::holds_alternative<h::Reflection_expression>(expression.data))
        {
            h::Reflection_expression const& reflection_expression = std::get<h::Reflection_expression>(expression.data);
            return evaluate_compile_time_reflection_expression(statement, reflection_expression, parameters);
        }
        else if (std::holds_alternative<h::Unary_expression>(expression.data))
        {
            h::Unary_expression const& unary_expression = std::get<h::Unary_expression>(expression.data);
            return evaluate_compile_time_unary_expression(statement, unary_expression, parameters);
        }
        else if (std::holds_alternative<h::Binary_expression>(expression.data))
        {
            h::Binary_expression const& binary_expression = std::get<h::Binary_expression>(expression.data);
            return evaluate_compile_time_binary_expression(statement, binary_expression, parameters);
        }
        else if (std::holds_alternative<h::Variable_expression>(expression.data))
        {
            h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(expression.data);
            return evaluate_compile_time_variable_expression(statement, variable_expression, parameters);
        }

        return std::nullopt;
    }

    std::optional<Compile_time_value_and_type> evaluate_compile_time_statement(
        h::Statement const& statement,
        Compile_time_parameters const& parameters
    )
    {
        if (statement.expressions.empty())
            return std::nullopt;

        h::Expression const& expression = statement.expressions[0];
        return evaluate_compile_time_expression(
            statement,
            expression,
            parameters
        );
    }

    static void visit_and_replace_compile_time_expressions(
        h::Statement& statement,
        h::Expression const& expression,
        Compile_time_parameters const& parameters
    )
    {
        if (std::holds_alternative<h::Compile_time_expression>(expression.data))
        {
            h::Compile_time_expression const& compile_time_expression = std::get<h::Compile_time_expression>(expression.data);
            if (compile_time_expression.expression.expression_index >= statement.expressions.size())
                return;
                
            h::Expression const& right_side_expression = statement.expressions[compile_time_expression.expression.expression_index];
            
            std::optional<Compile_time_value_and_type> new_value = evaluate_compile_time_expression(
                statement,
                right_side_expression,
                parameters
            );
            if (new_value.has_value())
            {
                replace_expression(statement, expression, new_value->statement, parameters.temporaries_allocator);
            }
        }
        else if (std::holds_alternative<h::Reflection_expression>(expression.data))
        {
            h::Reflection_expression const& reflection_expression = std::get<h::Reflection_expression>(expression.data);
            
            std::optional<Compile_time_value_and_type> new_value = evaluate_compile_time_reflection_expression(
                statement,
                reflection_expression,
                parameters
            );
            if (new_value.has_value())
            {
                replace_expression(statement, expression, new_value->statement, parameters.temporaries_allocator);
            }
        }
    }

    void run_compile_time_pass_on_module(
        h::Module& core_module,
        Compile_time_parameters const& parameters
    )
    {
        for (h::Function_definition& function_definition : core_module.definitions.function_definitions)
        {
            std::optional<Function_declaration const*> const function_declaration = find_function_declaration(core_module, function_definition.name);
            if (!function_declaration.has_value())
                continue;

            run_compile_time_pass_on_function(
                *function_declaration.value(),
                function_definition,
                parameters
            );
        }
    }

    void run_compile_time_pass_on_function(
        h::Function_declaration const& function_declaration,
        h::Function_definition& function_definition,
        Compile_time_parameters const& parameters
    )
    {
        auto const visit_and_replace = [&](h::Expression const& expression, h::Statement const& statement) -> bool
        {
            h::Statement& mutable_statement = const_cast<h::Statement&>(statement);
            visit_and_replace_compile_time_expressions(mutable_statement, expression, parameters);
            return false;
        };

        visit_expressions(function_definition.statements, visit_and_replace);
    }
}
