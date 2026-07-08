module;

#include <cassert>
#include <compare>

module iris.compiler.compile_time_pass;

import llvm;
import std;
import std.compat;

import iris.compiler.analysis;
import iris.compiler.types;
import iris.core;
import iris.core.declarations;
import iris.core.expressions;
import iris.core.formatter;
import iris.core.types;

namespace iris::compiler
{    
    constexpr std::string_view g_check_function_name = "check";
    constexpr std::string_view g_json_module_name = "iris.json";
    constexpr std::string_view g_json_alias = "iris_json";
    constexpr std::string_view g_json_print_difference_function_name = "print_json_difference";

    struct Compile_time_local_variable
    {
        std::string name;
        iris::Statement statement;
        std::optional<iris::Type_reference> type;
    };

    std::optional<Compile_time_value_and_type> evaluate_compile_time_expression(
        std::string_view const module_name,
        iris::Statement const& statement,
        iris::Expression const& expression,
        Compile_time_parameters const& parameters,
        std::vector<Compile_time_local_variable> const& compile_time_local_variables
    );

    std::optional<Compile_time_value_and_type> evaluate_compile_time_statement(
        std::string_view const module_name,
        iris::Statement const& statement,
        Compile_time_parameters const& parameters,
        std::vector<Compile_time_local_variable> const& compile_time_local_variables
    );

    static iris::Statement create_block_statement(
        std::pmr::vector<iris::Statement> statements,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<Statement> block_statements{output_allocator};
        block_statements.assign(statements.begin(), statements.end());

        return iris::Statement
        {
            .expressions = {
                iris::Expression
                {
                    .data = iris::Block_expression
                    {
                        .statements = std::move(statements)
                    }
                },
            },
        };
    }

    static iris::Statement create_constant_expression_statement(
        Type_reference type,
        std::pmr::string data
    )
    {
        return iris::Statement
        {
            .expressions = {
                iris::Expression
                {
                    .data = iris::Constant_expression
                    {
                        .type = std::move(type),
                        .data = std::move(data)
                    }
                },
            },
        };
    }

    static iris::Statement create_constant_bool_expression_statement(bool const value)
    {
        return iris::Statement
        {
            .expressions = {
                iris::Expression
                {
                    .data = iris::Constant_expression
                    {
                        .type = create_bool_type_reference(),
                        .data = value ? "true" : "false"
                    }
                },
            },
        };
    }

    static iris::Statement create_type_expression_statement(Type_reference type)
    {
        return iris::Statement
        {
            .expressions = {
                iris::Expression
                {
                    .data = iris::Type_expression
                    {
                        .type = std::move(type)
                    }
                },
            },
        };
    }

    static void add_import_usage_for_module(
        iris::Module_dependencies& dependencies,
        std::string_view const module_name,
        std::string_view const usage
    )
    {
        auto const location = std::find_if(
            dependencies.alias_imports.begin(),
            dependencies.alias_imports.end(),
            [&](Import_module_with_alias const& alias_import) -> bool { return alias_import.module_name == module_name; }
        );
        if (location == dependencies.alias_imports.end())
            return;

        auto const usage_location = std::find(location->usages.begin(), location->usages.end(), usage);
        if (usage_location != location->usages.end())
            return;

        location->usages.push_back(std::pmr::string{usage, location->usages.get_allocator()});
    }

    static Import_module_with_alias const& ensure_import_module_with_alias(
        iris::Module_dependencies& dependencies,
        std::string_view const module_name,
        std::string_view const alias,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        Import_module_with_alias const* const existing_import = find_import_module_with_module_name(dependencies, module_name);
        if (existing_import != nullptr)
            return *existing_import;

        Import_module_with_alias new_import
        {
            .module_name = std::pmr::string{module_name, output_allocator},
            .alias = std::pmr::string{alias, output_allocator},
            .usages = std::pmr::vector<std::pmr::string>{output_allocator},
            .source_range = std::nullopt,
        };

        dependencies.alias_imports.push_back(std::move(new_import));
        return dependencies.alias_imports.back();
    }

    static bool is_addressable_expression(
        std::string_view const module_name,
        iris::Declaration_database const& declaration_database,
        iris::Statement const& statement,
        iris::Expression const& expression
    )
    {
        if (std::holds_alternative<iris::Variable_expression>(expression.data))
            return true;
        if (std::holds_alternative<iris::Access_expression>(expression.data))
        {
            iris::Access_expression const& access = std::get<iris::Access_expression>(expression.data);
            iris::Expression const& left = statement.expressions[access.expression.expression_index];

            if (std::holds_alternative<iris::Variable_expression>(left.data))
            {
                iris::Variable_expression const& base = std::get<iris::Variable_expression>(left.data);
                std::optional<iris::Declaration> const declaration = iris::find_underlying_declaration(declaration_database, module_name, base.name);
                if (declaration.has_value() && std::holds_alternative<iris::Enum_declaration const*>(declaration->data))
                    return false;
            }

            return is_addressable_expression(module_name, declaration_database, statement, left);
        }
        if (std::holds_alternative<iris::Access_array_expression>(expression.data))
        {
            iris::Access_array_expression const& access = std::get<iris::Access_array_expression>(expression.data);
            iris::Expression const& left = statement.expressions[access.expression.expression_index];
            return is_addressable_expression(module_name, declaration_database, statement, left);
        }
        if (std::holds_alternative<iris::Dereference_and_access_expression>(expression.data))
        {
            iris::Dereference_and_access_expression const& access = std::get<iris::Dereference_and_access_expression>(expression.data);
            iris::Expression const& left = statement.expressions[access.expression.expression_index];
            return is_addressable_expression(module_name, declaration_database, statement, left);
        }
        return false;
    }

    // A single-expression statement containing Variable_expression{name}.
    static iris::Statement create_variable_expression_statement(
        std::string_view const name,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        return iris::Statement
        {
            .expressions = {
                iris::Expression
                {
                    .data = iris::Variable_expression
                    {
                        .name = std::pmr::string{name, output_allocator}
                    }
                }
            }
        };
    }

    // Creates "var <name> = <expr>" where <expr> is copied from source_statement at expression_index.
    static iris::Statement create_variable_declaration_statement(
        std::string_view const name,
        iris::Statement const& source_statement,
        iris::Expression_index const expression_index,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        iris::Statement output = {};

        std::size_t const decl_index = output.expressions.size();
        output.expressions.push_back(iris::Expression{.data = iris::Variable_declaration_expression{}});

        iris::Expression_index const copied_rhs = copy_expressions_to_new_statement(output, source_statement, expression_index);

        std::get<iris::Variable_declaration_expression>(output.expressions[decl_index].data) =
        {
            .name = std::pmr::string{name, output_allocator},
            .is_mutable = false,
            .right_hand_side = copied_rhs,
        };

        return output;
    }

    // Represents a check operand that has been made addressable.
    // When spill_declaration is non-nullopt, it must be prepended to the block as a
    // variable declaration before the generated if and check statements.
    struct Normalized_operand
    {
        iris::Statement expression;                         // single-expression statement (index 0 is the addressable lvalue)
        std::optional<iris::Statement> spill_declaration;  // set when the original operand was not addressable
    };

    static Normalized_operand normalize_check_operand(
        std::string_view const module_name,
        iris::Declaration_database const& declaration_database,
        std::string_view const temp_name,
        iris::Statement const& source_statement,
        iris::Expression_index const expression_index,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        iris::Expression const& expression = source_statement.expressions[expression_index.expression_index];

        if (is_addressable_expression(module_name, declaration_database, source_statement, expression))
        {
            iris::Statement normalized;
            copy_expressions_to_new_statement(normalized, source_statement, expression_index);
            return Normalized_operand{.expression = std::move(normalized), .spill_declaration = std::nullopt};
        }

        iris::Statement spill = create_variable_declaration_statement(temp_name, source_statement, expression_index, output_allocator);
        iris::Statement normalized = create_variable_expression_statement(temp_name, output_allocator);
        return Normalized_operand{.expression = std::move(normalized), .spill_declaration = std::move(spill)};
    }

    // Builds "<left> <op> <right>" where left and right are single-expression statements.
    static iris::Statement create_binary_expression_statement(
        iris::Statement const& left_source,
        iris::Statement const& right_source,
        iris::Binary_operation const operation
    )
    {
        iris::Statement output = {};

        std::size_t const binary_index = output.expressions.size();
        output.expressions.push_back(iris::Expression{.data = iris::Binary_expression{}});

        iris::Expression_index const copied_left = copy_expressions_to_new_statement(output, left_source, {.expression_index = 0});
        iris::Expression_index const copied_right = copy_expressions_to_new_statement(output, right_source, {.expression_index = 0});

        std::get<iris::Binary_expression>(output.expressions[binary_index].data) =
        {
            .left_hand_side = copied_left,
            .right_hand_side = copied_right,
            .operation = operation,
        };

        return output;
    }

    // Builds "<op><value>" where value_source is a single-expression statement.
    static iris::Statement create_unary_expression_statement(
        iris::Statement const& value_source,
        iris::Unary_operation const operation
    )
    {
        iris::Statement output = {};

        std::size_t const unary_index = output.expressions.size();
        output.expressions.push_back(iris::Expression{.data = iris::Unary_expression{}});

        iris::Expression_index const copied_value = copy_expressions_to_new_statement(output, value_source, {.expression_index = 0});

        std::get<iris::Unary_expression>(output.expressions[unary_index].data) =
        {
            .expression = copied_value,
            .operation = operation,
        };

        return output;
    }

    // Builds: iris_json.print_json_difference::<T>(&left, &right)
    // left_source and right_source are single-expression statements whose expression at
    // index 0 must be an addressable lvalue (Variable_expression or access chain).
    static iris::Statement create_print_json_difference_statement(
        std::string_view const import_alias,
        iris::Type_reference const& argument_type,
        iris::Statement const& left_source,
        iris::Statement const& right_source,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        iris::Statement output = {};

        // Push Call_expression and Instance_call_expression with explicitly initialized
        // PMR vector members using output_allocator to avoid default-resource crashes in
        // MSVC debug mode. Indices are captured before any further push_back calls.
        std::size_t const call_expression_index = output.expressions.size();
        output.expressions.push_back(iris::Expression{
            .data = iris::Call_expression{
                .expression = {},
                .arguments = std::pmr::vector<iris::Expression_index>{output_allocator},
            }
        });

        std::size_t const instance_call_expression_index = output.expressions.size();
        output.expressions.push_back(iris::Expression{
            .data = iris::Instance_call_expression{
                .left_hand_side = {},
                .arguments = std::pmr::vector<iris::Statement>{output_allocator},
            }
        });

        iris::Expression_index const copied_left = copy_expressions_to_new_statement(output, left_source, {.expression_index = 0});
        iris::Expression_index const copied_right = copy_expressions_to_new_statement(output, right_source, {.expression_index = 0});

        std::size_t const left_address_of_index = output.expressions.size();
        output.expressions.push_back(iris::Expression{ .data = iris::Unary_expression{} });

        std::size_t const right_address_of_index = output.expressions.size();
        output.expressions.push_back(iris::Expression{ .data = iris::Unary_expression{} });

        std::size_t const alias_expression_index = output.expressions.size();
        output.expressions.push_back(iris::Expression{ .data = iris::Variable_expression{} });

        std::size_t const access_expression_index = output.expressions.size();
        output.expressions.push_back(iris::Expression{ .data = iris::Access_expression{} });

        // All push_backs done; access expressions by index (pointers from push_back may be
        // invalid after any reallocation above).
        std::get<iris::Unary_expression>(output.expressions[left_address_of_index].data) = {
            .expression = copied_left,
            .operation = iris::Unary_operation::Address_of,
        };
        std::get<iris::Unary_expression>(output.expressions[right_address_of_index].data) = {
            .expression = copied_right,
            .operation = iris::Unary_operation::Address_of,
        };

        std::get<iris::Variable_expression>(output.expressions[alias_expression_index].data).name =
            std::pmr::string{import_alias, output_allocator};

        iris::Access_expression& access_expr = std::get<iris::Access_expression>(output.expressions[access_expression_index].data);
        access_expr.expression = {.expression_index = alias_expression_index};
        access_expr.member_name = std::pmr::string{g_json_print_difference_function_name, output_allocator};

        iris::Instance_call_expression& instance_call_expr = std::get<iris::Instance_call_expression>(output.expressions[instance_call_expression_index].data);
        instance_call_expr.left_hand_side = {.expression_index = access_expression_index};
        instance_call_expr.arguments.push_back(create_type_expression_statement(argument_type));

        iris::Call_expression& call_expr = std::get<iris::Call_expression>(output.expressions[call_expression_index].data);
        call_expr.expression = {.expression_index = instance_call_expression_index};
        call_expr.arguments.push_back({.expression_index = left_address_of_index});
        call_expr.arguments.push_back({.expression_index = right_address_of_index});

        return output;
    }

    // Builds: check(value) where value_source is a single-expression statement.
    static iris::Statement create_check_statement(
        iris::Statement const& value_source,
        std::optional<iris::Source_range> const& source_range,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        iris::Statement output = {};

        std::size_t const call_expression_index = output.expressions.size();
        output.expressions.push_back(iris::Expression{
            .data = iris::Call_expression{
                .expression = {},
                .arguments = std::pmr::vector<iris::Expression_index>{output_allocator},
            },
            .source_range = source_range
        });

        std::size_t const callee_index = output.expressions.size();
        output.expressions.push_back(iris::Expression{
            .data = iris::Variable_expression{
                .name = std::pmr::string{g_check_function_name, output_allocator}
            }
        });

        iris::Expression_index const copied_value = copy_expressions_to_new_statement(output, value_source, {.expression_index = 0});

        iris::Call_expression& call_expr = std::get<iris::Call_expression>(output.expressions[call_expression_index].data);
        call_expr.expression = {.expression_index = callee_index};
        call_expr.arguments.push_back(copied_value);

        return output;
    }

    static iris::Statement create_if_statement(
        iris::Statement condition,
        iris::Statement then_statement,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        iris::Statement output = {};

        Expression_reference<iris::If_expression> if_expression = create_expression_inside_statement<iris::If_expression>(output.expressions);
        if_expression.value->series = std::pmr::vector<iris::Condition_statement_pair>{output_allocator};

        iris::Condition_statement_pair pair
        {
            .condition = std::move(condition),
            .then_statements = std::pmr::vector<iris::Statement>{output_allocator},
            .block_source_range = std::nullopt,
        };
        pair.then_statements.push_back(std::move(then_statement));

        if_expression.value->series.push_back(std::move(pair));
        return output;
    }

    static bool is_check_equality_call(
        std::string_view const module_name,
        iris::Declaration_database const& declaration_datbase,
        iris::Statement const& statement,
        iris::Call_expression const*& call_expression,
        iris::Binary_expression const*& binary_expression
    )
    {
        if (statement.expressions.empty())
            return false;

        iris::Expression const& root_expression = statement.expressions[0];
        if (!root_expression.source_range.has_value())
            return false;

        if (!std::holds_alternative<iris::Call_expression>(root_expression.data))
            return false;

        iris::Call_expression const& current_call_expression = std::get<iris::Call_expression>(root_expression.data);
        if (current_call_expression.expression.expression_index >= statement.expressions.size())
            return false;

        iris::Expression const& callee_expression = statement.expressions[current_call_expression.expression.expression_index];
        if (!std::holds_alternative<iris::Variable_expression>(callee_expression.data))
            return false;

        iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(callee_expression.data);
        if (variable_expression.name != g_check_function_name)
            return false;

        if (iris::find_declaration(declaration_datbase, module_name, g_check_function_name).has_value())
            return false;

        if (current_call_expression.arguments.size() != 1)
            return false;

        iris::Expression_index const argument_expression_index = current_call_expression.arguments[0];
        if (argument_expression_index.expression_index >= statement.expressions.size())
            return false;

        iris::Expression const& argument_expression = statement.expressions[argument_expression_index.expression_index];
        if (!std::holds_alternative<iris::Binary_expression>(argument_expression.data))
            return false;

        iris::Binary_expression const& current_binary_expression = std::get<iris::Binary_expression>(argument_expression.data);
        if (current_binary_expression.operation != iris::Binary_operation::Equal)
            return false;

        call_expression = &current_call_expression;
        binary_expression = &current_binary_expression;
        return true;
    }

    static void rewrite_check_equality_statement(
        std::string_view const module_name,
        iris::Function_declaration const& function_declaration,
        iris::Statement& statement,
        Scope const& scope,
        Compile_time_parameters const& parameters
    )
    {
        iris::Call_expression const* call_expression = nullptr;
        iris::Binary_expression const* binary_expression = nullptr;
        if (!is_check_equality_call(module_name, parameters.declaration_database, statement, call_expression, binary_expression))
            return;

        if (binary_expression->left_hand_side.expression_index >= statement.expressions.size())
            return;

        iris::Expression const& left_hand_side_expression = statement.expressions[binary_expression->left_hand_side.expression_index];
        std::optional<iris::Type_reference> const left_hand_side_type = get_expression_type(
            module_name,
            &function_declaration,
            scope,
            statement,
            left_hand_side_expression,
            std::nullopt,
            parameters.declaration_database
        );
        if (!left_hand_side_type.has_value())
            return;

        Import_module_with_alias const& json_import = ensure_import_module_with_alias(
            parameters.dependencies,
            g_json_module_name,
            g_json_alias,
            parameters.output_allocator
        );
        add_import_usage_for_module(
            parameters.dependencies,
            g_json_module_name,
            g_json_print_difference_function_name
        );

        // Normalize operands: non-addressable expressions (e.g. binary temporaries) are
        // spilled to local variables so that address-of can safely be applied to them.
        Normalized_operand left_operand = normalize_check_operand(
            module_name,
            parameters.declaration_database,
            "__lhs",
            statement,
            binary_expression->left_hand_side,
            parameters.output_allocator
        );
        Normalized_operand right_operand = normalize_check_operand(
            module_name,
            parameters.declaration_database,
            "__rhs",
            statement,
            binary_expression->right_hand_side,
            parameters.output_allocator
        );

        iris::Statement condition_value_statement = create_binary_expression_statement(
            left_operand.expression,
            right_operand.expression,
            iris::Binary_operation::Equal
        );
        iris::Statement condition_declaration_statement = create_variable_declaration_statement(
            "__condition",
            condition_value_statement,
            {.expression_index = 0},
            parameters.output_allocator
        );
        iris::Statement condition_expression_statement = create_variable_expression_statement(
            "__condition",
            parameters.output_allocator
        );

        iris::Statement print_difference_statement = create_print_json_difference_statement(
            json_import.alias,
            left_hand_side_type.value(),
            left_operand.expression,
            right_operand.expression,
            parameters.output_allocator
        );
        iris::Statement condition_statement = create_unary_expression_statement(
            condition_expression_statement,
            iris::Unary_operation::Not
        );
        iris::Statement if_statement = create_if_statement(
            std::move(condition_statement),
            std::move(print_difference_statement),
            parameters.output_allocator
        );

        iris::Statement check_statement = create_check_statement(
            condition_expression_statement,
            statement.expressions[0].source_range,
            parameters.output_allocator
        );

        std::pmr::vector<iris::Statement> block_statements{parameters.output_allocator};
        if (left_operand.spill_declaration.has_value())
            block_statements.push_back(std::move(*left_operand.spill_declaration));
        if (right_operand.spill_declaration.has_value())
            block_statements.push_back(std::move(*right_operand.spill_declaration));
        block_statements.push_back(std::move(condition_declaration_statement));
        block_statements.push_back(std::move(check_statement));
        block_statements.push_back(std::move(if_statement));

        statement = create_block_statement(std::move(block_statements), parameters.output_allocator);
    }

    static Compile_time_value_and_type create_value_and_type(
        iris::Statement statement
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
        iris::Statement const& statement = value.statement;
        if (statement.expressions.empty())
            return std::nullopt;

        iris::Expression const& expression = statement.expressions[0];
        if (std::holds_alternative<iris::Constant_expression>(expression.data))
        {
            iris::Constant_expression const& constant_expression = std::get<iris::Constant_expression>(expression.data);
            if (iris::is_bool(constant_expression.type) || iris::is_c_bool(constant_expression.type))
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
        iris::Statement const& statement = value.statement;
        if (statement.expressions.empty())
            return std::nullopt;

        iris::Expression const& expression = statement.expressions[0];
        if (!std::holds_alternative<iris::Constant_expression>(expression.data))
            return std::nullopt;

        iris::Constant_expression const constant_expression = std::get<iris::Constant_expression>(expression.data);
        if (!iris::is_integer(constant_expression.type) && !iris::is_byte(constant_expression.type))
            return std::nullopt;

        if (iris::is_unsigned_integer(constant_expression.type) || iris::is_byte(constant_expression.type))
        {
            std::uint64_t unsigned_value = 0;
            auto [pointer, error_code] = std::from_chars(constant_expression.data.data(), constant_expression.data.data() + constant_expression.data.size(), unsigned_value);
            if (error_code != std::errc() || pointer != constant_expression.data.data() + constant_expression.data.size())
                return std::nullopt;

            return Compile_time_integer_value{.is_signed = false, .signed_value = 0, .unsigned_value = unsigned_value};
        }

        if (iris::is_signed_integer(constant_expression.type))
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
        std::string_view const module_name,
        iris::Statement const& statement,
        iris::Reflection_expression const& expression,
        std::string_view const function_name,
        Compile_time_parameters const& parameters
    )
    {
        if (expression.arguments.size() != 1)
            throw std::runtime_error{ std::format("{}() requires exactly one argument!", function_name) };

        iris::Expression_index const index_expression_index = expression.arguments[0];
        if (index_expression_index.expression_index >= statement.expressions.size())
            throw std::runtime_error{ std::format("{}() has an invalid argument index!", function_name) };

        iris::Expression const& index_expression = statement.expressions[index_expression_index.expression_index];
        std::optional<Compile_time_value_and_type> const index_value = evaluate_compile_time_expression(module_name, statement, index_expression, parameters);
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

        if (std::holds_alternative<iris::Array_slice_type>(resolved_type.data))
            return std::pmr::string{"Array_slice", parameters.output_allocator};

        if (std::holds_alternative<iris::Builtin_type_reference>(resolved_type.data))
            return std::pmr::string{"Builtin", parameters.output_allocator};

        if (std::holds_alternative<iris::Constant_array_type>(resolved_type.data))
            return std::pmr::string{"Constant_array", parameters.output_allocator};

        if (std::holds_alternative<iris::Custom_type_reference>(resolved_type.data))
        {
            std::optional<iris::Declaration> const declaration = find_underlying_declaration(parameters.declaration_database, resolved_type);
            if (!declaration.has_value())
                return std::pmr::string{"Custom", parameters.output_allocator};

            if (std::holds_alternative<iris::Struct_declaration const*>(declaration->data))
                return std::pmr::string{"Struct", parameters.output_allocator};
            if (std::holds_alternative<iris::Union_declaration const*>(declaration->data))
                return std::pmr::string{"Union", parameters.output_allocator};
            if (std::holds_alternative<iris::Enum_declaration const*>(declaration->data))
                return std::pmr::string{"Enum", parameters.output_allocator};

            return std::pmr::string{"Custom", parameters.output_allocator};
        }

        if (std::holds_alternative<iris::Decimal_type>(resolved_type.data))
            return std::pmr::string{"Decimal", parameters.output_allocator};

        if (std::holds_alternative<iris::Fundamental_type>(resolved_type.data))
        {
            iris::Fundamental_type const fundamental_type = std::get<iris::Fundamental_type>(resolved_type.data);
            switch (fundamental_type)
            {
                case iris::Fundamental_type::Bool:
                case iris::Fundamental_type::C_bool:
                    return std::pmr::string{"Bool", parameters.output_allocator};
                case iris::Fundamental_type::Float16:
                case iris::Fundamental_type::Float32:
                case iris::Fundamental_type::Float64:
                case iris::Fundamental_type::C_longdouble:
                    return std::pmr::string{"Float", parameters.output_allocator};
                case iris::Fundamental_type::Byte:
                case iris::Fundamental_type::C_uchar:
                case iris::Fundamental_type::C_ushort:
                case iris::Fundamental_type::C_uint:
                case iris::Fundamental_type::C_ulong:
                case iris::Fundamental_type::C_ulonglong:
                    return std::pmr::string{"Uint", parameters.output_allocator};
                case iris::Fundamental_type::C_char:
                case iris::Fundamental_type::C_schar:
                case iris::Fundamental_type::C_short:
                case iris::Fundamental_type::C_int:
                case iris::Fundamental_type::C_long:
                case iris::Fundamental_type::C_longlong:
                    return std::pmr::string{"Int", parameters.output_allocator};
                case iris::Fundamental_type::String:
                case iris::Fundamental_type::Any_type:
                    return std::pmr::string{"Builtin", parameters.output_allocator};
                default:
                    return std::pmr::string{"Builtin", parameters.output_allocator};
            }
        }

        if (std::holds_alternative<iris::Function_pointer_type>(resolved_type.data))
            return std::pmr::string{"Function_pointer", parameters.output_allocator};

        if (std::holds_alternative<iris::Integer_type>(resolved_type.data))
        {
            iris::Integer_type const& integer_type = std::get<iris::Integer_type>(resolved_type.data);
            return std::pmr::string{integer_type.is_signed ? "Int" : "Uint", parameters.output_allocator};
        }

        if (std::holds_alternative<iris::Null_pointer_type>(resolved_type.data))
            return std::pmr::string{"Null_pointer", parameters.output_allocator};

        if (std::holds_alternative<iris::Pointer_type>(resolved_type.data))
            return std::pmr::string{"Pointer", parameters.output_allocator};

        if (std::holds_alternative<iris::Type_instance>(resolved_type.data))
            return std::pmr::string{"Custom", parameters.output_allocator};

        return std::pmr::string{"Custom", parameters.output_allocator};
    }

    static void replace_variable_with_constant_in_statement(
        iris::Statement& statement,
        std::string_view const variable_name,
        iris::Type_reference const& constant_type,
        std::pmr::string const& constant_data
    );

    static void replace_variable_with_constant_in_expression(
        iris::Expression& expression,
        std::string_view const variable_name,
        iris::Type_reference const& constant_type,
        std::pmr::string const& constant_data
    );

    static void replace_variable_with_constant_in_statement(
        iris::Statement& statement,
        std::string_view const variable_name,
        iris::Type_reference const& constant_type,
        std::pmr::string const& constant_data
    )
    {
        for (iris::Expression& expression : statement.expressions)
            replace_variable_with_constant_in_expression(expression, variable_name, constant_type, constant_data);
    }

    static void replace_variable_with_constant_in_expression(
        iris::Expression& expression,
        std::string_view const variable_name,
        iris::Type_reference const& constant_type,
        std::pmr::string const& constant_data
    )
    {
        if (std::holds_alternative<iris::Variable_expression>(expression.data))
        {
            iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(expression.data);
            if (variable_expression.name == variable_name)
            {
                expression = iris::Expression
                {
                    .data = iris::Constant_expression
                    {
                        .type = constant_type,
                        .data = constant_data
                    }
                };
                return;
            }
        }

        if (std::holds_alternative<iris::Block_expression>(expression.data))
        {
            iris::Block_expression& data = std::get<iris::Block_expression>(expression.data);
            for (iris::Statement& statement : data.statements)
                replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
        }
        else if (std::holds_alternative<iris::Constant_array_expression>(expression.data))
        {
            iris::Constant_array_expression& data = std::get<iris::Constant_array_expression>(expression.data);
            for (iris::Statement& statement : data.array_data)
                replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
        }
        else if (std::holds_alternative<iris::For_loop_expression>(expression.data))
        {
            iris::For_loop_expression& data = std::get<iris::For_loop_expression>(expression.data);
            replace_variable_with_constant_in_statement(data.range_end, variable_name, constant_type, constant_data);
            for (iris::Statement& statement : data.then_statements)
                replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
        }
        else if (std::holds_alternative<iris::If_expression>(expression.data))
        {
            iris::If_expression& data = std::get<iris::If_expression>(expression.data);
            for (iris::Condition_statement_pair& pair : data.series)
            {
                if (pair.condition.has_value())
                    replace_variable_with_constant_in_statement(*pair.condition, variable_name, constant_type, constant_data);

                for (iris::Statement& statement : pair.then_statements)
                    replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
            }
        }
        else if (std::holds_alternative<iris::Switch_expression>(expression.data))
        {
            iris::Switch_expression& data = std::get<iris::Switch_expression>(expression.data);
            for (iris::Switch_case_expression_pair& pair : data.cases)
                for (iris::Statement& statement : pair.statements)
                    replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
        }
        else if (std::holds_alternative<iris::Ternary_condition_expression>(expression.data))
        {
            iris::Ternary_condition_expression& data = std::get<iris::Ternary_condition_expression>(expression.data);
            replace_variable_with_constant_in_statement(data.then_statement, variable_name, constant_type, constant_data);
            replace_variable_with_constant_in_statement(data.else_statement, variable_name, constant_type, constant_data);
        }
        else if (std::holds_alternative<iris::While_loop_expression>(expression.data))
        {
            iris::While_loop_expression& data = std::get<iris::While_loop_expression>(expression.data);
            replace_variable_with_constant_in_statement(data.condition, variable_name, constant_type, constant_data);
            for (iris::Statement& statement : data.then_statements)
                replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
        }
    }

    static std::optional<Compile_time_value_and_type> evaluate_compile_time_for_loop_expression(
        std::string_view const module_name,
        iris::Statement const& statement,
        iris::For_loop_expression const& expression,
        Compile_time_parameters const& parameters,
        std::vector<Compile_time_local_variable> const& compile_time_local_variables
    )
    {
        if (expression.range_begin.expression_index >= statement.expressions.size())
            return std::nullopt;

        switch (expression.range_comparison_operation)
        {
            case iris::Binary_operation::Less_than:
            case iris::Binary_operation::Less_than_or_equal_to:
            case iris::Binary_operation::Greater_than:
            case iris::Binary_operation::Greater_than_or_equal_to:
                break;
            default:
                return std::nullopt;
        }

        iris::Expression const& range_begin_expression = statement.expressions[expression.range_begin.expression_index];
        std::optional<Compile_time_value_and_type> const range_begin_value = evaluate_compile_time_expression(module_name, statement, range_begin_expression, parameters, compile_time_local_variables);
        if (!range_begin_value.has_value())
            return std::nullopt;

        std::optional<Compile_time_integer_value> const range_begin_integer = get_integer_from_value(range_begin_value.value());
        if (!range_begin_integer.has_value())
            return std::nullopt;

        std::optional<Compile_time_value_and_type> const range_end_value = evaluate_compile_time_statement(module_name, expression.range_end, parameters, compile_time_local_variables);
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

                iris::Expression const& step_expression = statement.expressions[expression.step_by->expression_index];
                std::optional<Compile_time_value_and_type> const step_value = evaluate_compile_time_expression(module_name, statement, step_expression, parameters, compile_time_local_variables);
                if (!step_value.has_value())
                    return std::nullopt;

                return get_integer_from_value(step_value.value());
            }

            return std::nullopt;
        };
        std::optional<Compile_time_integer_value> const step_integer = get_step_integer();

        // Limits to avoid runaway unrolling
        constexpr std::size_t maximum_unroll_iterations = 1024;
        std::pmr::vector<iris::Statement> iteration_blocks{parameters.output_allocator};
        iteration_blocks.reserve(16);

        auto const create_iteration_block = [&](auto const loop_index_value) -> void
        {
            std::pmr::vector<iris::Statement> body{parameters.output_allocator};
            body.assign(expression.then_statements.begin(), expression.then_statements.end());

            std::pmr::string const integer_string = std::pmr::string{std::to_string(loop_index_value)};
            iris::Type_reference const index_type = range_begin_value->type.value_or(create_integer_type_type_reference(64, true));

            for (iris::Statement& statement : body)
                replace_variable_with_constant_in_statement(statement, expression.variable_name, index_type, integer_string);

            iteration_blocks.push_back(create_block_statement(std::move(body), parameters.output_allocator));
        };

        auto compare = [&](auto const value, auto const range_end) -> bool
        {
            switch (expression.range_comparison_operation)
            {
                case iris::Binary_operation::Less_than:
                    return value < range_end;
                case iris::Binary_operation::Less_than_or_equal_to:
                    return value <= range_end;
                case iris::Binary_operation::Greater_than:
                    return value > range_end;
                case iris::Binary_operation::Greater_than_or_equal_to:
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
        std::string_view const module_name,
        iris::Statement const& statement,
        iris::Reflection_expression const& expression,
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
                parameters.dependencies,
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
            std::optional<iris::Declaration> const declaration = find_underlying_declaration(parameters.declaration_database, type_reference);
            if (!declaration.has_value())
                throw std::runtime_error{ "member_count() could not resolve declaration for type argument!" };

            std::uint64_t member_count = 0;
            if (std::holds_alternative<iris::Struct_declaration const*>(declaration->data))
            {
                iris::Struct_declaration const& struct_declaration = *std::get<iris::Struct_declaration const*>(declaration->data);
                member_count = struct_declaration.member_types.size();
            }
            else if (std::holds_alternative<iris::Union_declaration const*>(declaration->data))
            {
                iris::Union_declaration const& union_declaration = *std::get<iris::Union_declaration const*>(declaration->data);
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

            std::uint64_t const member_index = get_required_reflection_index_argument(module_name, statement, expression, "member_type", parameters);

            Type_reference const& type_reference = expression.type_arguments[0];
            std::optional<iris::Declaration> const declaration = find_underlying_declaration(parameters.declaration_database, type_reference);
            if (!declaration.has_value())
                throw std::runtime_error{ "member_type() could not resolve declaration for type argument!" };

            std::optional<Type_reference> output_type = std::nullopt;
            if (std::holds_alternative<iris::Struct_declaration const*>(declaration->data))
            {
                iris::Struct_declaration const& struct_declaration = *std::get<iris::Struct_declaration const*>(declaration->data);
                if (member_index >= struct_declaration.member_types.size())
                    throw std::runtime_error{ "member_type() index is out of bounds!" };

                output_type = struct_declaration.member_types[member_index];
            }
            else if (std::holds_alternative<iris::Union_declaration const*>(declaration->data))
            {
                iris::Union_declaration const& union_declaration = *std::get<iris::Union_declaration const*>(declaration->data);
                if (member_index >= union_declaration.member_types.size())
                    throw std::runtime_error{ "member_type() index is out of bounds!" };

                output_type = union_declaration.member_types[member_index];
            }
            else
            {
                throw std::runtime_error{ "member_type() requires a struct or union type argument!" };
            }

            add_import_usage_for_module(parameters.dependencies, "iris.builtin", "Type_kind");

            return create_value_and_type(create_type_expression_statement(output_type.value()));
        }
        else if (expression.name == "member_offset")
        {
            if (expression.type_arguments.size() != 1)
                throw std::runtime_error{ "member_offset() requires exactly one type argument!" };

            std::uint64_t const member_index = get_required_reflection_index_argument(module_name, statement, expression, "member_offset", parameters);

            Type_reference const& type_reference = expression.type_arguments[0];
            std::optional<iris::Declaration> const declaration = find_underlying_declaration(parameters.declaration_database, type_reference);
            if (!declaration.has_value())
                throw std::runtime_error{ "member_offset() could not resolve declaration for type argument!" };

            std::uint64_t offset_in_bits = 0;
            if (std::holds_alternative<iris::Struct_declaration const*>(declaration->data))
            {
                iris::Struct_declaration const& struct_declaration = *std::get<iris::Struct_declaration const*>(declaration->data);
                if (member_index >= struct_declaration.member_types.size())
                    throw std::runtime_error{ "member_offset() index is out of bounds!" };

                llvm::Type* const llvm_type = type_reference_to_llvm_type_on_demand(
                    parameters.llvm_context,
                    parameters.llvm_data_layout,
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
            else if (std::holds_alternative<iris::Union_declaration const*>(declaration->data))
            {
                iris::Union_declaration const& union_declaration = *std::get<iris::Union_declaration const*>(declaration->data);
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

            std::uint64_t const member_index = get_required_reflection_index_argument(module_name, statement, expression, "member_name", parameters);

            Type_reference const& type_reference = expression.type_arguments[0];
            std::optional<iris::Declaration> const declaration = find_underlying_declaration(parameters.declaration_database, type_reference);
            if (!declaration.has_value())
                throw std::runtime_error{ "member_name() could not resolve declaration for type argument!" };

            std::pmr::string member_name{parameters.output_allocator};
            if (std::holds_alternative<iris::Struct_declaration const*>(declaration->data))
            {
                iris::Struct_declaration const& struct_declaration = *std::get<iris::Struct_declaration const*>(declaration->data);
                if (member_index >= struct_declaration.member_names.size())
                    throw std::runtime_error{ "member_name() index is out of bounds!" };

                member_name = struct_declaration.member_names[member_index];
            }
            else if (std::holds_alternative<iris::Union_declaration const*>(declaration->data))
            {
                iris::Union_declaration const& union_declaration = *std::get<iris::Union_declaration const*>(declaration->data);
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
            iris::Statement enum_statement =
            {
                .expressions = create_enum_value_expressions("Type_kind", member_name)
            };

            return create_value_and_type(std::move(enum_statement));
        }
        else if (expression.name == "decimal_scale")
        {
            if (expression.type_arguments.size() != 1)
                throw std::runtime_error{ "decimal_scale() requires exactly one type argument!" };

            if (!expression.arguments.empty())
                throw std::runtime_error{ "decimal_scale() does not take runtime arguments!" };

            Type_reference const& type_reference = expression.type_arguments[0];
            std::optional<Type_reference> const underlying_type = get_underlying_type(parameters.declaration_database, type_reference);
            Type_reference const& resolved_type = underlying_type.value_or(type_reference);

            if (!std::holds_alternative<iris::Decimal_type>(resolved_type.data))
                throw std::runtime_error{ "decimal_scale() requires a Decimal type argument!" };

            iris::Decimal_type const& decimal_type = std::get<iris::Decimal_type>(resolved_type.data);

            return create_value_and_type(create_constant_expression_statement(
                create_integer_type_type_reference(32, false),
                std::pmr::string{std::to_string(decimal_type.scale), parameters.output_allocator}
            ));
        }
        else if (expression.name == "type_of")
        {
            if (!expression.type_arguments.empty())
                throw std::runtime_error{ "type_of() does not take type arguments!" };

            if (expression.arguments.size() != 1)
                throw std::runtime_error{ "type_of() requires exactly one argument!" };

            iris::Expression_index const argument_expression_index = expression.arguments[0];
            if (argument_expression_index.expression_index >= statement.expressions.size())
                throw std::runtime_error{ "type_of() argument index is out of bounds!" };

            Scope scope = {};
            std::optional<iris::Type_reference> const type = get_expression_type(
                module_name,
                nullptr,
                scope,
                statement,
                statement.expressions[argument_expression_index.expression_index],
                std::nullopt,
                parameters.declaration_database
            );
            if (!type.has_value())
                throw std::runtime_error{ "type_of() could not resolve expression type!" };

            return create_value_and_type(create_type_expression_statement(type.value()));
        }
        else
        {
            throw std::runtime_error{ std::format("Reflection expression '{}' not implemented!", expression.name) };
        }
    }

    static std::optional<Compile_time_value_and_type> evaluate_compile_time_unary_expression(
        std::string_view const module_name,
        iris::Statement const& statement,
        iris::Unary_expression const& expression,
        Compile_time_parameters const& parameters,
        std::vector<Compile_time_local_variable> const& compile_time_local_variables
    )
    {
        switch (expression.operation)
        {
            case iris::Unary_operation::Bitwise_not:
            case iris::Unary_operation::Minus:
            case iris::Unary_operation::Pre_increment:
            case iris::Unary_operation::Post_increment:
            case iris::Unary_operation::Pre_decrement:
            case iris::Unary_operation::Post_decrement:
            case iris::Unary_operation::Indirection:
            case iris::Unary_operation::Address_of:
                return std::nullopt;
            default:
                break;
        }

        iris::Expression const& right_side_expression = statement.expressions[expression.expression.expression_index];
        std::optional<Compile_time_value_and_type> const right_side_value = evaluate_compile_time_expression(module_name, statement, right_side_expression, parameters, compile_time_local_variables);
        if (!right_side_value.has_value())
            return std::nullopt;

        if (expression.operation == iris::Unary_operation::Not)
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
        std::string_view const module_name,
        iris::Statement const& statement,
        iris::Binary_expression const& expression,
        Compile_time_parameters const& parameters,
        std::vector<Compile_time_local_variable> const& compile_time_local_variables
    )
    {
        if (!is_comparison_binary_operation(expression.operation) && !is_equality_binary_operation(expression.operation) && !is_logical_binary_operation(expression.operation))
            throw std::runtime_error{ std::format("Unsupported compile_time binary operation '{}'", static_cast<int>(expression.operation)) };

        if (expression.left_hand_side.expression_index >= statement.expressions.size())
            throw std::runtime_error{ "Invalid left operand index in compile_time binary expression" };

        if (expression.right_hand_side.expression_index >= statement.expressions.size())
            throw std::runtime_error{ "Invalid right operand index in compile_time binary expression" };

        iris::Expression const& left_expression = statement.expressions[expression.left_hand_side.expression_index];
        iris::Expression const& right_expression = statement.expressions[expression.right_hand_side.expression_index];

        std::optional<Compile_time_value_and_type> const left_value = evaluate_compile_time_expression(module_name, statement, left_expression, parameters, compile_time_local_variables);
        if (!left_value.has_value())
            throw std::runtime_error{ "Could not evaluate left operand in compile_time binary expression" };

        std::optional<Compile_time_value_and_type> const right_value = evaluate_compile_time_expression(module_name, statement, right_expression, parameters, compile_time_local_variables);
        if (!right_value.has_value())
            throw std::runtime_error{ "Could not evaluate right operand in compile_time binary expression" };

        if (is_logical_binary_operation(expression.operation))
        {
            std::optional<bool> const left_bool = get_bool_from_value(left_value.value());
            if (!left_bool.has_value())
                return std::nullopt;

            std::optional<bool> const right_bool = get_bool_from_value(right_value.value());
            if (!right_bool.has_value())
                return std::nullopt;

            bool const result = expression.operation == iris::Binary_operation::Logical_and
                ? left_bool.value() && right_bool.value()
                : left_bool.value() || right_bool.value();

            return create_value_and_type(create_constant_bool_expression_statement(result));
        }

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
                case iris::Binary_operation::Equal:
                    return left_side == right_side;
                case iris::Binary_operation::Not_equal:
                    return left_side != right_side;
                case iris::Binary_operation::Less_than:
                    return left_side < right_side;
                case iris::Binary_operation::Less_than_or_equal_to:
                    return left_side <= right_side;
                case iris::Binary_operation::Greater_than:
                    return left_side > right_side;
                case iris::Binary_operation::Greater_than_or_equal_to:
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

    static std::optional<iris::Constant_expression> try_get_constant_expression(
        Compile_time_value_and_type const& value
    )
    {
        if (value.statement.expressions.size() != 1)
            return std::nullopt;

        iris::Expression const& expression = value.statement.expressions[0];
        if (!std::holds_alternative<iris::Constant_expression>(expression.data))
            return std::nullopt;

        return std::get<iris::Constant_expression>(expression.data);
    }

    static Compile_time_local_variable const* find_compile_time_local_variable(
        std::vector<Compile_time_local_variable> const& local_variables,
        std::string_view const name
    )
    {
        for (auto iterator = local_variables.rbegin(); iterator != local_variables.rend(); ++iterator)
        {
            if (iterator->name == name)
                return &(*iterator);
        }

        return nullptr;
    }

    static std::optional<Compile_time_value_and_type> evaluate_propagated_compile_time_local_variable(
        std::string_view const module_name,
        iris::Variable_expression const& expression,
        Compile_time_parameters const& parameters,
        std::vector<Compile_time_local_variable> const& compile_time_local_variables
    )
    {
        Compile_time_local_variable const* local_variable = find_compile_time_local_variable(compile_time_local_variables, expression.name);
        if (local_variable == nullptr)
            return std::nullopt;

        std::optional<Compile_time_value_and_type> const evaluated_local_statement = evaluate_compile_time_statement(
            module_name,
            local_variable->statement,
            parameters,
            compile_time_local_variables
        );
        if (evaluated_local_statement.has_value())
            return evaluated_local_statement;

        return Compile_time_value_and_type
        {
            .statement = local_variable->statement,
            .type = local_variable->type
        };
    }

    static std::optional<Compile_time_value_and_type> evaluate_compile_time_variable_expression(
        std::string_view const module_name,
        iris::Statement const& statement,
        iris::Variable_expression const& expression,
        Compile_time_parameters const& parameters,
        std::vector<Compile_time_local_variable> const& compile_time_local_variables
    )
    {
        // Search for local compile_time variables that were propagated earlier.
        {
            std::optional<Compile_time_value_and_type> const local_value = evaluate_propagated_compile_time_local_variable(
                module_name,
                expression,
                parameters,
                compile_time_local_variables
            );
            if (local_value.has_value())
                return local_value;
        }

        // Search for global variables:
        {
            std::optional<iris::Declaration> const declaration = iris::find_underlying_declaration(parameters.declaration_database, module_name, expression.name);
            if (declaration.has_value() && std::holds_alternative<iris::Global_variable_declaration const*>(declaration->data))
            {
                iris::Global_variable_declaration const& global_variable_declaration = *std::get<iris::Global_variable_declaration const*>(declaration->data);
                return Compile_time_value_and_type
                {
                    .statement = global_variable_declaration.initial_value,
                    .type = global_variable_declaration.type
                };
            }
        }

        return std::nullopt;
    }

    std::optional<Compile_time_value_and_type> evaluate_compile_time_access_enum_value(
        iris::Declaration_database const& declaration_database,
        std::string_view const module_name,
        std::string_view const enum_name,
        std::string_view const enum_value_name
    )
    {
        std::optional<iris::Declaration> const declaration = iris::find_underlying_declaration(declaration_database, module_name, enum_name);
        if (declaration.has_value())
        {
            if (std::holds_alternative<iris::Enum_declaration const*>(declaration->data))
            {
                iris::Enum_declaration const& enum_declaration = *std::get<iris::Enum_declaration const*>(declaration->data);
                auto const enum_value_location = std::find_if(
                    enum_declaration.values.begin(),
                    enum_declaration.values.end(),
                    [&](Enum_value const& enum_value) -> bool { return enum_value.name == enum_value_name; }
                );

                if (enum_value_location == enum_declaration.values.end())
                    return std::nullopt;

                std::size_t const index = std::distance(enum_declaration.values.begin(), enum_value_location);

                if (enum_declaration.values[index].value.has_value())
                {
                    return Compile_time_value_and_type
                    {
                        .statement = enum_declaration.values[index].value.value(),
                        .type = iris::create_custom_type_reference(module_name, enum_name)
                    };
                }
                else
                {
                    auto const create_inferred_value = [&](std::int64_t const value) -> Compile_time_value_and_type
                    {
                        return Compile_time_value_and_type
                        {
                            .statement = create_constant_expression_statement(
                                create_integer_type_type_reference(32, true),
                                std::pmr::string{std::to_string(value)}
                            ),
                            .type = iris::create_custom_type_reference(module_name, enum_name)
                        };
                    };

                    if (index == 0)
                        return create_inferred_value(0);

                    std::optional<std::size_t> previous_explicit_index = std::nullopt;
                    for (std::size_t previous_index = index; previous_index > 0; --previous_index)
                    {
                        if (enum_declaration.values[previous_index - 1].value.has_value())
                        {
                            previous_explicit_index = previous_index - 1;
                            break;
                        }
                    }

                    if (!previous_explicit_index.has_value())
                        return create_inferred_value(static_cast<std::int64_t>(index));

                    Compile_time_value_and_type const previous_explicit_value =
                    {
                        .statement = enum_declaration.values[previous_explicit_index.value()].value.value(),
                        .type = std::nullopt
                    };

                    std::optional<Compile_time_integer_value> const previous_integer_value = get_integer_from_value(previous_explicit_value);
                    if (!previous_integer_value.has_value())
                        return std::nullopt;

                    std::int64_t const delta = static_cast<std::int64_t>(index - previous_explicit_index.value());
                    if (previous_integer_value->is_signed)
                        return create_inferred_value(previous_integer_value->signed_value + delta);

                    return create_inferred_value(static_cast<std::int64_t>(previous_integer_value->unsigned_value) + delta);
                }
            }
        }

        return std::nullopt;
    }

    std::optional<Compile_time_value_and_type> evaluate_compile_time_expression(
        std::string_view const module_name,
        iris::Statement const& statement,
        iris::Expression const& expression,
        Compile_time_parameters const& parameters,
        std::vector<Compile_time_local_variable> const& compile_time_local_variables
    )
    {
        if (std::holds_alternative<iris::Access_expression>(expression.data))
        {
            iris::Access_expression const& access_expression = std::get<iris::Access_expression>(expression.data);
            if (access_expression.expression.expression_index >= statement.expressions.size())
                return std::nullopt;
                
            iris::Expression const& left_side_expression = statement.expressions[access_expression.expression.expression_index];

            if (std::holds_alternative<iris::Variable_expression>(left_side_expression.data))
            {
                iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(left_side_expression.data);
                
                if (variable_expression.name == "Type_kind")
                    return evaluate_compile_time_access_enum_value(parameters.declaration_database, "iris.builtin", "Type_kind", access_expression.member_name);

                return evaluate_compile_time_access_enum_value(parameters.declaration_database, module_name, variable_expression.name, access_expression.member_name);
            }

            return std::nullopt;
        }
        else if (std::holds_alternative<iris::Compile_time_expression>(expression.data))
        {
            iris::Compile_time_expression const& compile_time_expression = std::get<iris::Compile_time_expression>(expression.data);
            if (compile_time_expression.expression.expression_index >= statement.expressions.size())
                return std::nullopt;
                
            iris::Expression const& right_side_expression = statement.expressions[compile_time_expression.expression.expression_index];
            return evaluate_compile_time_expression(module_name, statement, right_side_expression, parameters, compile_time_local_variables);
        }
        else if (std::holds_alternative<iris::Constant_expression>(expression.data))
        {
            iris::Constant_expression const& constant_expression = std::get<iris::Constant_expression>(expression.data);
            return create_value_and_type({.expressions = {iris::Expression{.data = constant_expression}}});
        }
        else if (std::holds_alternative<iris::If_expression>(expression.data))
        {
            iris::If_expression const& if_expression = std::get<iris::If_expression>(expression.data);

            for (std::size_t index = 0; index < if_expression.series.size(); ++index)
            {
                iris::Condition_statement_pair const& serie = if_expression.series[index];

                if (!serie.condition.has_value())
                    return create_value_and_type(create_block_statement(serie.then_statements, parameters.output_allocator));

                iris::Statement const& condition_statement = serie.condition.value();
                std::optional<Compile_time_value_and_type> const condition_value = evaluate_compile_time_statement(module_name, condition_statement, parameters, compile_time_local_variables);
                if (!condition_value.has_value())
                    return std::nullopt;

                std::optional<bool> const condition = get_bool_from_value(condition_value.value());
                if (!condition.has_value())
                    return std::nullopt;
                
                if (condition.value())
                    return create_value_and_type(create_block_statement(serie.then_statements, parameters.output_allocator));
            }

            return create_value_and_type(create_block_statement({}, parameters.output_allocator));
        }
        else if (std::holds_alternative<iris::For_loop_expression>(expression.data))
        {
            iris::For_loop_expression const& for_loop_expression = std::get<iris::For_loop_expression>(expression.data);
            return evaluate_compile_time_for_loop_expression(module_name, statement, for_loop_expression, parameters, compile_time_local_variables);
        }
        else if (std::holds_alternative<iris::Parenthesis_expression>(expression.data))
        {
            iris::Parenthesis_expression const& parenthesis_expression = std::get<iris::Parenthesis_expression>(expression.data);
            iris::Expression const& inner_expression = statement.expressions[parenthesis_expression.expression.expression_index];
            return evaluate_compile_time_expression(module_name, statement, inner_expression, parameters, compile_time_local_variables);
        }
        else if (std::holds_alternative<iris::Reflection_expression>(expression.data))
        {
            iris::Reflection_expression const& reflection_expression = std::get<iris::Reflection_expression>(expression.data);
            return evaluate_compile_time_reflection_expression(module_name, statement, reflection_expression, parameters);
        }
        else if (std::holds_alternative<iris::Unary_expression>(expression.data))
        {
            iris::Unary_expression const& unary_expression = std::get<iris::Unary_expression>(expression.data);
            return evaluate_compile_time_unary_expression(module_name, statement, unary_expression, parameters, compile_time_local_variables);
        }
        else if (std::holds_alternative<iris::Binary_expression>(expression.data))
        {
            iris::Binary_expression const& binary_expression = std::get<iris::Binary_expression>(expression.data);
            return evaluate_compile_time_binary_expression(module_name, statement, binary_expression, parameters, compile_time_local_variables);
        }
        else if (std::holds_alternative<iris::Variable_expression>(expression.data))
        {
            iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(expression.data);
            return evaluate_compile_time_variable_expression(module_name, statement, variable_expression, parameters, compile_time_local_variables);
        }

        return std::nullopt;
    }

    std::optional<Compile_time_value_and_type> evaluate_compile_time_statement(
        std::string_view const module_name,
        iris::Statement const& statement,
        Compile_time_parameters const& parameters,
        std::vector<Compile_time_local_variable> const& compile_time_local_variables
    )
    {
        if (statement.expressions.empty())
            return std::nullopt;

        iris::Expression const& expression = statement.expressions[0];
        return evaluate_compile_time_expression(
            module_name,
            statement,
            expression,
            parameters,
            compile_time_local_variables
        );
    }

    std::optional<Compile_time_value_and_type> evaluate_compile_time_expression(
        std::string_view const module_name,
        iris::Statement const& statement,
        iris::Expression const& expression,
        Compile_time_parameters const& parameters
    )
    {
        std::vector<Compile_time_local_variable> local_variables = {};
        return evaluate_compile_time_expression(module_name, statement, expression, parameters, local_variables);
    }

    std::optional<Compile_time_value_and_type> evaluate_compile_time_statement(
        std::string_view const module_name,
        iris::Statement const& statement,
        Compile_time_parameters const& parameters
    )
    {
        std::vector<Compile_time_local_variable> local_variables = {};
        return evaluate_compile_time_statement(module_name, statement, parameters, local_variables);
    }

    static void visit_and_replace_compile_time_expressions(
        std::string_view const module_name,
        iris::Statement& statement,
        iris::Expression const& expression,
        Compile_time_parameters const& parameters,
        std::vector<Compile_time_local_variable>& compile_time_local_variables
    )
    {
        if (std::holds_alternative<iris::Variable_expression>(expression.data))
        {
            iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(expression.data);
            std::optional<Compile_time_value_and_type> const new_value = evaluate_propagated_compile_time_local_variable(
                module_name,
                variable_expression,
                parameters,
                compile_time_local_variables
            );
            if (new_value.has_value())
            {
                replace_expression(statement, expression, new_value->statement, parameters.temporaries_allocator);
            }
        }
        else if (std::holds_alternative<iris::Instance_call_expression>(expression.data))
        {
            iris::Instance_call_expression const& instance_call_expression = std::get<iris::Instance_call_expression>(expression.data);

            for (iris::Statement const& argument_statement : instance_call_expression.arguments)
            {
                iris::Statement& mutable_argument_statement = const_cast<iris::Statement&>(argument_statement);
                auto const visit_and_replace = [&](iris::Expression const& nested_expression, iris::Statement const& nested_statement) -> bool
                {
                    iris::Statement& mutable_nested_statement = const_cast<iris::Statement&>(nested_statement);
                    visit_and_replace_compile_time_expressions(
                        module_name,
                        mutable_nested_statement,
                        nested_expression,
                        parameters,
                        compile_time_local_variables
                    );
                    return false;
                };

                visit_expressions(mutable_argument_statement, visit_and_replace);
            }
        }
        else if (std::holds_alternative<iris::Compile_time_expression>(expression.data))
        {
            iris::Compile_time_expression const& compile_time_expression = std::get<iris::Compile_time_expression>(expression.data);
            if (compile_time_expression.expression.expression_index >= statement.expressions.size())
                return;

            iris::Expression const& right_side_expression = statement.expressions[compile_time_expression.expression.expression_index];

            if (std::holds_alternative<iris::Variable_declaration_expression>(right_side_expression.data))
            {
                iris::Variable_declaration_expression const& declaration = std::get<iris::Variable_declaration_expression>(right_side_expression.data);
                if (declaration.right_hand_side.expression_index >= statement.expressions.size())
                    return;

                iris::Expression const& right_hand_side_expression = statement.expressions[declaration.right_hand_side.expression_index];
                std::optional<Compile_time_value_and_type> const right_hand_side_value = evaluate_compile_time_expression(
                    module_name,
                    statement,
                    right_hand_side_expression,
                    parameters,
                    compile_time_local_variables
                );
                if (!right_hand_side_value.has_value())
                    return;

                compile_time_local_variables.push_back(
                    Compile_time_local_variable
                    {
                        .name = std::string{declaration.name},
                        .statement = right_hand_side_value->statement,
                        .type = right_hand_side_value->type
                    }
                );

                // compile_time variable declarations are propagated and do not become runtime locals.
                statement.expressions.clear();
                return;
            }

            if (std::holds_alternative<iris::Variable_declaration_with_type_expression>(right_side_expression.data))
            {
                iris::Variable_declaration_with_type_expression const& declaration = std::get<iris::Variable_declaration_with_type_expression>(right_side_expression.data);
                if (declaration.right_hand_side.expression_index >= statement.expressions.size())
                    return;

                iris::Expression const& right_hand_side_expression = statement.expressions[declaration.right_hand_side.expression_index];
                std::optional<Compile_time_value_and_type> const right_hand_side_value = evaluate_compile_time_expression(
                    module_name,
                    statement,
                    right_hand_side_expression,
                    parameters,
                    compile_time_local_variables
                );
                if (!right_hand_side_value.has_value())
                    return;

                compile_time_local_variables.push_back(
                    Compile_time_local_variable
                    {
                        .name = std::string{declaration.name},
                        .statement = right_hand_side_value->statement,
                        .type = right_hand_side_value->type
                    }
                );

                // compile_time variable declarations are propagated and do not become runtime locals.
                statement.expressions.clear();
                return;
            }

            std::optional<Compile_time_value_and_type> new_value = evaluate_compile_time_expression(
                module_name,
                statement,
                right_side_expression,
                parameters,
                compile_time_local_variables
            );
            if (new_value.has_value())
            {
                replace_expression(statement, expression, new_value->statement, parameters.temporaries_allocator);
            }
        }
        else if (std::holds_alternative<iris::Reflection_expression>(expression.data))
        {
            iris::Reflection_expression const& reflection_expression = std::get<iris::Reflection_expression>(expression.data);

            std::optional<Compile_time_value_and_type> new_value = evaluate_compile_time_reflection_expression(
                module_name,
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
        iris::Module& core_module,
        Compile_time_parameters const& parameters
    )
    {
        for (iris::Function_definition& function_definition : core_module.definitions.function_definitions)
        {
            std::optional<Function_declaration const*> const function_declaration = find_function_declaration(core_module, function_definition.name);
            if (!function_declaration.has_value())
                continue;

            if (function_declaration.value()->is_test && !parameters.is_test_mode)
                continue;

            run_compile_time_pass_on_function(
                core_module.name,
                *function_declaration.value(),
                function_definition,
                parameters
            );
        }
    }

    void run_compile_time_pass_on_function(
        std::string_view const module_name,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition& function_definition,
        Compile_time_parameters const& parameters
    )
    {
        Scope scope{};
        add_parameters_to_scope(
            scope,
            function_declaration.input_parameter_names,
            function_declaration.type.input_parameter_types,
            function_declaration.input_parameter_source_positions
        );

        std::vector<Compile_time_local_variable> compile_time_local_variables = {};

        auto const callback = [&](iris::Statement const& statement, Scope const& scope) -> void
        {
            iris::Statement& mutable_statement = const_cast<iris::Statement&>(statement);
            auto const visit_and_replace = [&](iris::Expression const& expression, iris::Statement const& statement) -> bool
            {
                iris::Statement& mutable_nested_statement = const_cast<iris::Statement&>(statement);
                visit_and_replace_compile_time_expressions(
                    module_name,
                    mutable_nested_statement,
                    expression,
                    parameters,
                    compile_time_local_variables
                );
                return false;
            };

            visit_expressions(mutable_statement, visit_and_replace);
            rewrite_check_equality_statement(module_name, function_declaration, mutable_statement, scope, parameters);
        };

        visit_statements_using_scope(
            module_name,
            &function_declaration,
            scope,
            function_definition.statements,
            parameters.declaration_database,
            callback
        );
    }
}
