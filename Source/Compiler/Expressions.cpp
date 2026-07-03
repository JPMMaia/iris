module;

#include <assert.h>

module iris.compiler.expressions;

import std;
import llvm;

import iris.core;
import iris.core.declarations;
import iris.core.execution_engine;
import iris.core.types;
import iris.compiler.analysis;
import iris.compiler.clang_data;
import iris.compiler.clang_code_generation;
import iris.compiler.common;
import iris.compiler.debug_info;
import iris.compiler.instructions;
import iris.compiler.test_framework;
import iris.compiler.types;

namespace iris::compiler
{
    template <typename T1, typename T2>
    inline size_t constexpr offset_of(T1 T2::*member) {
        constexpr T2 object {};
        return size_t(&(object.*member)) - size_t(&object);
    }

    static std::string format_error(std::string_view const message, std::optional<Source_position> const& source_position)
    {
        if (!source_position.has_value())
            return std::format("{} (location unknown)", message);
        return std::format("{} (at line {}, column {})", message, source_position->line, source_position->column);
    }

    Expression_parameters set_core_module(Expression_parameters const& parameters, iris::Module const& core_module)
    {
        if (&parameters.core_module == &core_module)
            return parameters;

        return Expression_parameters
        {
            .llvm_context = parameters.llvm_context,
            .llvm_data_layout = parameters.llvm_data_layout,
            .llvm_builder = parameters.llvm_builder,
            .llvm_parent_function = parameters.llvm_parent_function,
            .llvm_module = parameters.llvm_module,
            .clang_module_data = parameters.clang_module_data,
            .core_module = core_module,
            .core_module_dependencies = parameters.core_module_dependencies,
            .declaration_database = parameters.declaration_database,
            .type_database = parameters.type_database,
            .enum_value_constants = parameters.enum_value_constants,
            .blocks = parameters.blocks,
            .defer_expressions_per_block = parameters.defer_expressions_per_block,
            .function_declaration = parameters.function_declaration,
            .function_arguments = parameters.function_arguments,
            .local_variables = parameters.local_variables,
            .expression_type = parameters.expression_type,
            .debug_info = parameters.debug_info,
            .contract_options = parameters.contract_options,
            .enable_bounds_checks = parameters.enable_bounds_checks,
            .source_position = parameters.source_position,
            .temporaries_allocator = parameters.temporaries_allocator,
        };
    }

    std::optional<Module const*> get_module(std::pmr::unordered_map<std::pmr::string, Module const*> const& core_module_dependencies, std::string_view const name)
    {
        auto const location = core_module_dependencies.find(name.data());
        if (location == core_module_dependencies.end())
            return std::nullopt;

        return location->second;
    }

    std::optional<std::string_view> get_module_name_from_alias(Module const& module, std::string_view const alias_name)
    {
        auto const location = std::find_if(module.dependencies.alias_imports.begin(), module.dependencies.alias_imports.end(), [alias_name](Import_module_with_alias const& import) { return import.alias == alias_name; });
        if (location == module.dependencies.alias_imports.end())
            return std::nullopt;

        return location->module_name;
    }

    llvm::Constant* fold_constant(llvm::Value* value, llvm::DataLayout const& llvm_data_layout, std::optional<Source_position> const& source_position);

    std::optional<Value_and_type> get_global_variable_value_and_type(
        iris::Module const& global_variable_module,
        iris::Global_variable_declaration const& global_variable_declaration,
        Expression_parameters const& parameters
    )
    {
        if (global_variable_declaration.global_type == Global_variable_type::Macro)
        {
            Expression_parameters new_parameters = set_core_module(parameters, global_variable_module);
            new_parameters.expression_type = global_variable_declaration.type;

            Value_and_type const value = create_statement_value(
                global_variable_declaration.initial_value,
                new_parameters
            );

            if (parameters.llvm_parent_function == nullptr)
            {
                llvm::Constant* const constant = fold_constant(value.value, parameters.llvm_data_layout, parameters.source_position);

                return Value_and_type
                {
                    .name = global_variable_declaration.name,
                    .value = constant,
                    .type = value.type
                };
            }

            llvm::Value* folded_or_runtime_value = value.value;
            if (llvm::BinaryOperator::classof(value.value) || llvm::Constant::classof(value.value))
                folded_or_runtime_value = fold_constant(value.value, parameters.llvm_data_layout, parameters.source_position);

            return Value_and_type
            {
                .name = global_variable_declaration.name,
                .value = folded_or_runtime_value,
                .type = value.type
            };
        }
        else
        {
            std::string const mangled_name = mangle_name(global_variable_module, global_variable_declaration.name, global_variable_declaration.unique_name);
            llvm::GlobalValue* const llvm_global_value = parameters.llvm_module.getNamedValue(mangled_name);
            if (llvm_global_value == nullptr) {
                return std::nullopt;
            }

            iris::compiler::Scope scope{};
            std::optional<iris::Type_reference> type = get_expression_type(
                global_variable_module.name,
                parameters.function_declaration.has_value() ? parameters.function_declaration.value() : nullptr,
                scope,
                global_variable_declaration.initial_value,
                global_variable_declaration.type,
                parameters.declaration_database
            );

            return Value_and_type
            {
                .name = global_variable_declaration.name,
                .value = llvm_global_value,
                .type = std::move(type)
            };
        }
    }

    static std::pmr::vector<std::pmr::vector<Statement>> create_defer_block(
        std::span<std::pmr::vector<Statement>> const current_block
    )
    {
        std::pmr::vector<std::pmr::vector<Statement>> new_block;
        new_block.resize(current_block.size() + 1);

        for (std::size_t index = 0; index < current_block.size(); ++index)
            new_block[index] = current_block[index];

        return new_block;
    }

    using Blocks_to_pop_count = std::size_t;

    static std::pair<Block_info const*, Blocks_to_pop_count> find_target_block(
        std::span<Block_info const> const blocks,
        std::size_t const break_count,
        std::span<Block_type const> const target_block_types,
        std::optional<Source_position> const& source_position
    )
    {
        std::uint64_t target_break_count = break_count <= 1 ? 1 : break_count;
        std::uint64_t found_break_blocks = 0;

        for (std::size_t index = 0; index < blocks.size(); ++index)
        {
            std::size_t const block_index = blocks.size() - index - 1;
            Block_info const& block_info = blocks[block_index];

            auto const location = std::find(target_block_types.begin(), target_block_types.end(), block_info.block_type);
            if (location != target_block_types.end())
            {
                found_break_blocks += 1;

                if (found_break_blocks == target_break_count)
                {
                    return std::make_pair(&block_info, index + 1);
                }
            }
        }

        throw std::runtime_error{ format_error("Could not find block to break!", source_position) };
    }

    static void create_local_variable_debug_description(
        Debug_info& debug_info,
        Expression_parameters const& parameters,
        std::string_view const name,
        llvm::Value* const alloca,
        Type_reference const& type_reference
    )
    {
        Source_position const source_position = parameters.source_position.value_or(Source_position{});

        llvm::DIType* const llvm_argument_debug_type = type_reference_to_llvm_debug_type(
            *debug_info.llvm_builder,
            *get_debug_scope(debug_info),
            parameters.llvm_data_layout,
            parameters.core_module,
            type_reference,
            debug_info.type_database
        );

        llvm::DIScope* const debug_scope = get_debug_scope(debug_info);

        llvm::DILocalVariable* debug_parameter_variable = debug_info.llvm_builder->createAutoVariable(
            debug_scope,
            name.data(),
            debug_scope->getFile(),
            source_position.line,
            llvm_argument_debug_type
        );

        llvm::DILocation* const debug_location = llvm::DILocation::get(
            parameters.llvm_context,
            source_position.line,
            source_position.column,
            debug_scope
        );

        debug_info.llvm_builder->insertDeclare(
            alloca,
            debug_parameter_variable,
            debug_info.llvm_builder->createExpression(),
            debug_location,
            parameters.llvm_builder.GetInsertBlock()
        );
    }

    bool can_store(std::optional<Type_reference> const& type)
    {
        if (type.has_value() && std::holds_alternative<Constant_array_type>(type->data))
        {
            Constant_array_type const& constant_array_type = std::get<Constant_array_type>(type->data);
            return constant_array_type.size > 0;
        }

        return true;
    }

    bool ends_with_terminator_statement(std::span<Statement const> const statements)
    {
        if (statements.empty())
            return false;

        Statement const& last_statement = statements.back();

        if (last_statement.expressions.empty())
            return false;

        Expression const& first_expression = last_statement.expressions[0];
        return std::holds_alternative<Break_expression>(first_expression.data) || std::holds_alternative<Continue_expression>(first_expression.data) || std::holds_alternative<Return_expression>(first_expression.data);
    }

    static llvm::Value* create_soa_array_view_adjusted_index(
        Value_and_type const& soa_value,
        llvm::Value* const index_value,
        Expression_parameters const& parameters
    );

    std::optional<Value_and_type> search_in_function_scope(
        std::string_view const variable_name,
        std::span<Value_and_type const> const function_arguments,
        std::span<Value_and_type const> const local_variables
    )
    {
        auto const is_variable = [variable_name](Value_and_type const& element) -> bool
        {
            return element.name == variable_name;
        };

        // Search in local variables:
        {
            auto const location = std::find_if(local_variables.rbegin(), local_variables.rend(), is_variable);
            if (location != local_variables.rend())
                return *location;
        }

        // Search in function arguments:
        {
            auto const location = std::find_if(function_arguments.begin(), function_arguments.end(), is_variable);
            if (location != function_arguments.end())
                return *location;
        }

        return {};
    }

    static Scope create_type_analysis_scope(Expression_parameters const& parameters)
    {
        Scope scope{};

        if (parameters.function_declaration.has_value() && parameters.function_declaration.value() != nullptr)
        {
            Function_declaration const& function_declaration = *parameters.function_declaration.value();
            add_parameters_to_scope(
                scope,
                function_declaration.input_parameter_names,
                function_declaration.type.input_parameter_types,
                function_declaration.input_parameter_source_positions
            );
        }

        for (Value_and_type const& local_variable : parameters.local_variables)
        {
            if (local_variable.name.empty() || !local_variable.type.has_value())
                continue;

            scope.variables.push_back(
                create_variable(
                    local_variable.name,
                    local_variable.type.value(),
                    true,
                    false,
                    std::optional<Source_position>{std::nullopt}
                )
            );
        }

        return scope;
    }

    llvm::Constant* fold_constant(
        llvm::Value* const value,
        llvm::DataLayout const& llvm_data_layout,
        std::optional<Source_position> const& source_position
    )
    {
        if (llvm::BinaryOperator::classof(value))
        {
            llvm::BinaryOperator* const binary_operator = static_cast<llvm::BinaryOperator*>(value);

            llvm::Constant* const left_hand_side = fold_constant(binary_operator->getOperand(0), llvm_data_layout, source_position);
            llvm::Constant* const right_hand_side = fold_constant(binary_operator->getOperand(1), llvm_data_layout, source_position);

            llvm::Constant* const folded_constant = llvm::ConstantFoldBinaryOpOperands(
                binary_operator->getOpcode(),
                left_hand_side,
                right_hand_side,
                llvm_data_layout
            );

            if (folded_constant == nullptr)
                throw std::runtime_error{ format_error("Could not unfold binary operation constant!", source_position) };

            return folded_constant;
        }
        else if (llvm::Constant::classof(value))
        {
            return static_cast<llvm::Constant*>(value);
        }
        else
        {
            throw std::runtime_error{ format_error("Could not unfold constant!", source_position) };
        }
    }

    llvm::Constant* fold_statement_constant(
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        Value_and_type const statement_value = create_statement_value(
            statement,
            parameters
        );

        if (statement_value.value == nullptr)
            throw std::runtime_error{ format_error("Could not fold constant!", parameters.source_position) };

        return fold_constant(statement_value.value, parameters.llvm_data_layout, parameters.source_position);
    }

    static std::pmr::string replace_string_literal_special_values(std::string_view const value)
    {
        std::pmr::string output = std::pmr::string{value};

        size_t index = 0;
        while (true)
        {
            index = output.find("\\n", index);
            if (index == std::pmr::string::npos)
                break;

            output.replace(index, 2, "\n");

            index += 2;
        }

        return output;
    }

    static llvm::Value* create_c_string_constant(
        llvm::LLVMContext& llvm_context,
        llvm::Module& llvm_module,
        std::string_view const string_data,
        std::optional<std::string_view> const global_name = std::nullopt
    )
    {
        if (global_name.has_value())
        {
            llvm::GlobalValue* const llvm_global_value = llvm_module.getNamedValue(global_name->data());
            if (llvm_global_value != nullptr)
                return llvm_global_value;
        }

        std::pmr::string const final_string = replace_string_literal_special_values(string_data);

        std::uint64_t const null_terminator_size = 1;
        std::uint64_t const array_size = final_string.size() + null_terminator_size;
        llvm::ArrayType* const array_type = llvm::ArrayType::get(llvm::IntegerType::get(llvm_context, 8), array_size);

        bool const is_constant = true;
        std::string const global_variable_name = global_name.has_value() ? std::string{global_name.value()} : std::format("global_{}", llvm_module.global_size());
        llvm::GlobalVariable* const global_variable = new llvm::GlobalVariable(
            llvm_module,
            array_type,
            is_constant,
            llvm::GlobalValue::InternalLinkage,
            llvm::ConstantDataArray::getString(llvm_context, final_string.c_str()),
            global_variable_name
        );

        llvm::Value* const instruction = global_variable;
        return instruction;
    }

    Value_and_type access_enum_value(
        std::string_view const module_name,
        Enum_declaration const& declaration,
        std::string_view const enum_value_name,
        Enum_value_constants const& enum_value_constants
    )
    {
        auto const is_enum_value = [enum_value_name](Enum_value const& value) -> bool { return value.name == enum_value_name; };

        auto const enum_value_location = std::find_if(declaration.values.begin(), declaration.values.end(), is_enum_value);
        if (enum_value_location == declaration.values.end())
            throw std::runtime_error{ format_error(std::format("Unknown enum value '{}.{}' referenced.", declaration.name, enum_value_name), std::nullopt) };

        auto const enum_value_index = std::distance(declaration.values.begin(), enum_value_location);

        std::pmr::string const key = std::pmr::string{std::format("{}.{}", module_name, declaration.name)};
        Enum_constants const& constants = enum_value_constants.map.at(key);
        llvm::Constant* const constant = constants[enum_value_index];

        return Value_and_type
        {
            .name = "",
            .value = constant,
            .type = create_custom_type_reference(module_name, declaration.name)
        };
    }

    std::optional<Module const*> get_module_from_access_expression(
        Access_expression const& expression,
        Value_and_type const& left_hand_side,
        Statement const& statement,
        Module const& core_module,
        std::pmr::unordered_map<std::pmr::string, Module const*> const& core_module_dependencies
    )
    {
        if (left_hand_side.value == nullptr)
        {
            Expression const& left_hand_side_expression = statement.expressions[expression.expression.expression_index];

            if (std::holds_alternative<Variable_expression>(left_hand_side_expression.data))
            {
                Variable_expression const& variable_expression = std::get<Variable_expression>(left_hand_side_expression.data);

                std::string_view const module_alias_name = variable_expression.name;
                std::optional<std::string_view> const external_module_name = get_module_name_from_alias(core_module, module_alias_name);

                if (external_module_name.has_value())
                {
                    return get_module(core_module_dependencies, external_module_name.value()).value();
                }
            }
        }

        return std::nullopt;
    }

    std::optional<Custom_type_reference> get_custom_type_reference_from_access_expression(
        Access_expression const& expression,
        Value_and_type const& left_hand_side,
        Statement const& statement,
        std::string_view const current_module_name
    )
    {
        if (left_hand_side.value == nullptr)
        {
            Expression const& left_hand_side_expression = statement.expressions[expression.expression.expression_index];

            if (std::holds_alternative<Access_expression>(left_hand_side_expression.data))
            {
                if (left_hand_side.type.has_value() && std::holds_alternative<Custom_type_reference>(left_hand_side.type.value().data))
                {
                    return std::get<Custom_type_reference>(left_hand_side.type.value().data);
                }
            }
            else if (std::holds_alternative<Variable_expression>(left_hand_side_expression.data))
            {
                Variable_expression const& variable_expression = std::get<Variable_expression>(left_hand_side_expression.data);

                if (variable_expression.name == "Type_kind")
                {
                    return Custom_type_reference
                    {
                        .module_reference =
                        {
                            .name = "iris.builtin",
                        },
                        .name = variable_expression.name
                    };
                }

                return Custom_type_reference
                {
                    .module_reference =
                    {
                        .name = std::pmr::string{ current_module_name },
                    },
                    .name = variable_expression.name
                };
            }
        }
        else if (left_hand_side.value != nullptr)
        {
            if (left_hand_side.type.has_value() && std::holds_alternative<Custom_type_reference>(left_hand_side.type.value().data))
            {
                Custom_type_reference const& type_reference = std::get<Custom_type_reference>(left_hand_side.type.value().data);
                return type_reference;
            }
        }

        return std::nullopt;
    }

    Value_and_type create_access_struct_member(
        Value_and_type const& left_hand_side,
        std::string_view const access_member_name,
        std::string_view const module_name,
        Struct_declaration const& struct_declaration,
        Expression_parameters const& parameters
    )
    {
        if (left_hand_side.value == nullptr)
            throw std::runtime_error{ format_error("create_access_struct_member(): left_hand_side.value == nullptr", parameters.source_position) };

        Value_and_type value = generate_load_struct_member_instructions(
            parameters.clang_module_data,
            parameters.llvm_context,
            parameters.llvm_builder,
            parameters.llvm_data_layout,
            left_hand_side.value,
            access_member_name,
            module_name,
            struct_declaration,
            parameters.type_database
        );

        return value;
    }

    Value_and_type create_access_union_member(
        Value_and_type const& left_hand_side,
        std::string_view const access_member_name,
        std::string_view const module_name,
        Union_declaration const& union_declaration,
        Expression_parameters const& parameters
    )
    {
        if (left_hand_side.value == nullptr)
            throw std::runtime_error{ format_error("create_access_union_member(): left_hand_side.value == nullptr", parameters.source_position) };

        llvm::Type* const union_llvm_type = type_reference_to_llvm_type(
            parameters.llvm_context,
            parameters.llvm_data_layout,
            create_custom_type_reference(module_name, union_declaration.name),
            parameters.type_database
        );

        auto const member_location = std::find(union_declaration.member_names.begin(), union_declaration.member_names.end(), access_member_name);
        if (member_location == union_declaration.member_names.end())
            throw std::runtime_error{ format_error(std::format("'{}' does not exist in union type '{}'.", access_member_name, union_declaration.name), parameters.source_position) };

        unsigned const member_index = static_cast<unsigned>(std::distance(union_declaration.member_names.begin(), member_location));

        Type_reference const& member_type = union_declaration.member_types[member_index];

        llvm::Type* const llvm_member_type = type_reference_to_llvm_type(
            parameters.llvm_context,
            parameters.llvm_data_layout,
            member_type,
            parameters.type_database
        );

        std::array<llvm::Value*, 2> const indices
        {
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(parameters.llvm_context), 0),
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(parameters.llvm_context), 0),
        };
        llvm::Value* const get_element_pointer_instruction = parameters.llvm_builder.CreateGEP(union_llvm_type, left_hand_side.value, indices, "", true);

        llvm::Value* const bitcast_instruction = parameters.llvm_builder.CreateBitCast(get_element_pointer_instruction, llvm_member_type->getPointerTo());

        return
        {
            .name = "",
            .value = bitcast_instruction,
            .type = std::move(member_type)
        };
    }

    struct Soa_array_type_info
    {
        Soa_array_type const* soa_array_type;
        Type_reference const* element_type;
        Struct_declaration const* struct_declaration;
        std::string_view module_name;
        Soa_layout layout;
    };

    struct Soa_array_view_type_info
    {
        Soa_array_view_type const* soa_array_view_type;
        Type_reference const* element_type;
        Struct_declaration const* struct_declaration;
        std::string_view module_name;
        Soa_layout layout;
    };

    static Soa_array_type_info get_soa_array_type_info(
        Type_reference const& type_reference,
        Expression_parameters const& parameters
    );

    static Soa_array_view_type_info get_soa_array_view_type_info(
        Type_reference const& type_reference,
        Expression_parameters const& parameters
    );

    static std::pair<llvm::AllocaInst*, llvm::Value*> create_soa_array_storage(
        Type_reference const& soa_type_reference,
        Soa_layout const& layout,
        Expression_parameters const& parameters,
        std::string_view const name
    );

    static llvm::Value* create_soa_member_element_pointer(
        llvm::Value* const data_pointer,
        Soa_member_layout const& member_layout,
        llvm::Type* const member_llvm_type,
        llvm::Value* const index_value,
        Expression_parameters const& parameters
    );

    static llvm::Value* create_soa_member_element_pointer(
        llvm::Value* const data_pointer,
        std::span<Soa_member_layout const> const member_layouts,
        std::size_t const member_index,
        llvm::Value* const length_value,
        llvm::Type* const member_llvm_type,
        llvm::Value* const index_value,
        Expression_parameters const& parameters
    );

    static llvm::Value* load_soa_data_pointer(
        Value_and_type const& soa_value,
        Expression_parameters const& parameters
    );

    static llvm::Value* load_soa_array_view_data_pointer(
        Value_and_type const& soa_value,
        Expression_parameters const& parameters
    );

    static llvm::Value* load_soa_array_view_field(
        Value_and_type const& soa_value,
        std::uint32_t field_index,
        llvm::Type* field_type,
        Expression_parameters const& parameters
    );

    static llvm::Value* load_soa_array_view_length(
        Value_and_type const& soa_value,
        Expression_parameters const& parameters
    );

    static void store_soa_member_value(
        llvm::Value* const data_pointer,
        Soa_array_type_info const& soa_info,
        std::size_t const member_index,
        llvm::Value* const index_value,
        Value_and_type const& member_value,
        Expression_parameters const& parameters
    );

    static void initialize_default_soa_array_storage(
        llvm::Value* const data_pointer,
        Soa_array_type_info const& soa_info,
        Expression_parameters const& parameters
    );

    static void initialize_soa_array_storage_from_values(
        llvm::Value* const data_pointer,
        std::span<Value_and_type const> const element_values,
        Soa_array_type_info const& soa_info,
        Expression_parameters const& parameters
    );

    Value_and_type create_access_expression_value(
        Access_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        Module const& core_module = parameters.core_module;
        std::pmr::unordered_map<std::pmr::string, Module const*> const& core_module_dependencies = parameters.core_module_dependencies;
        llvm::Module& llvm_module = parameters.llvm_module;
        Declaration_database const& declaration_database = parameters.declaration_database;
        Enum_value_constants const& enum_value_constants = parameters.enum_value_constants;

        Value_and_type const left_hand_side = create_expression_value(expression.expression.expression_index, statement, parameters);

        // Check if left hand side corresponds to a module name:
        {
            std::optional<Module const*> const external_module_optional = get_module_from_access_expression(expression, left_hand_side, statement, core_module, core_module_dependencies);
            if (external_module_optional.has_value())
            {
                Module const& external_module = *external_module_optional.value();

                std::string_view const declaration_name = expression.member_name;
                std::optional<Declaration> const declaration_optional = find_declaration(declaration_database, external_module.name, declaration_name);
                if (!declaration_optional.has_value())
                    throw std::runtime_error{ format_error(std::format("Could not find declaration '{}.{}' referenced.", external_module.name, declaration_name), parameters.source_position) };

                Declaration const& declaration = declaration_optional.value();

                if (std::holds_alternative<Alias_type_declaration const*>(declaration.data))
                {
                    Alias_type_declaration const& alias_type_declaration = *std::get<Alias_type_declaration const*>(declaration.data);
                    Type_reference type = create_custom_type_reference(external_module.name, alias_type_declaration.name);

                    return Value_and_type
                    {
                        .name = expression.member_name,
                        .value = nullptr,
                        .type = std::move(type)
                    };
                }
                else if (std::holds_alternative<Enum_declaration const*>(declaration.data))
                {
                    Enum_declaration const& enum_declaration = *std::get<Enum_declaration const*>(declaration.data);
                    Type_reference type = create_custom_type_reference(external_module.name, enum_declaration.name);

                    return Value_and_type
                    {
                        .name = expression.member_name,
                        .value = nullptr,
                        .type = std::move(type)
                    };
                }
                else if (std::holds_alternative<Global_variable_declaration const*>(declaration.data))
                {
                    Global_variable_declaration const& global_variable_declaration = *std::get<Global_variable_declaration const*>(declaration.data);
                    
                    std::optional<Value_and_type> value_and_type = get_global_variable_value_and_type(
                        external_module,
                        global_variable_declaration,
                        parameters
                    );
                    if (!value_and_type.has_value())
                        throw std::runtime_error{ format_error(std::format("Internal error while trying to find global variable '{}.{}'", external_module.name, declaration_name), parameters.source_position) };

                    return *value_and_type;
                }
                else if (std::holds_alternative<Function_declaration const*>(declaration.data))
                {
                    Function_declaration const& function_declaration = *std::get<Function_declaration const*>(declaration.data);
                    Type_reference function_type = create_function_type_type_reference(function_declaration.type, function_declaration.input_parameter_names, function_declaration.output_parameter_names);

                    llvm::Function* const llvm_function = get_llvm_function(
                        external_module,
                        llvm_module,
                        expression.member_name
                    );
                    if (!llvm_function)
                        throw std::runtime_error{ format_error(std::format("Unknown function '{}.{}' referenced.", external_module.name, expression.member_name), parameters.source_position) };

                    return Value_and_type
                    {
                        .name = std::pmr::string{ llvm_function->getName().str() },
                        .value = llvm_function,
                        .type = std::move(function_type)
                    };
                }
                else if (std::holds_alternative<Function_constructor const*>(declaration.data))
                {
                    Function_constructor const& function_constructor = *std::get<Function_constructor const*>(declaration.data);
                    Type_reference type = create_custom_type_reference(external_module.name, function_constructor.name);

                    return Value_and_type
                    {
                        .name = expression.member_name,
                        .value = nullptr,
                        .type = std::move(type)
                    };
                }
            }
        }

        if (left_hand_side.type.has_value() && iris::is_array_slice_type_reference(left_hand_side.type.value()))
        {
            iris::Array_slice_type const& array_slice_type = std::get<iris::Array_slice_type>(left_hand_side.type->data);

            iris::Struct_declaration const struct_declaration = create_array_slice_type_struct_declaration(array_slice_type.element_type);

            return create_access_struct_member(
                left_hand_side,
                expression.member_name,
                "iris.builtin",
                struct_declaration,
                parameters
            );
        }

        if (left_hand_side.type.has_value() && iris::is_soa_array_type_reference(left_hand_side.type.value()))
        {
            iris::Soa_array_type const& soa_array_type = std::get<iris::Soa_array_type>(left_hand_side.type->data);

            if (expression.member_name == "length")
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm::ConstantInt::get(llvm::Type::getInt64Ty(parameters.llvm_context), soa_array_type.size),
                    .type = create_integer_type_type_reference(64, false)
                };
            }
            else if (expression.member_name == "data")
            {
                return Value_and_type
                {
                    .name = "",
                    .value = load_soa_data_pointer(left_hand_side, parameters),
                    .type = create_pointer_type_type_reference({}, true)
                };
            }
        }

        if (left_hand_side.type.has_value() && iris::is_soa_array_view_type_reference(left_hand_side.type.value()))
        {
            if (expression.member_name == "start_index")
            {
                return Value_and_type
                {
                    .name = "",
                    .value = load_soa_array_view_field(left_hand_side, 0, llvm::Type::getInt64Ty(parameters.llvm_context), parameters),
                    .type = create_integer_type_type_reference(64, false)
                };
            }
            else if (expression.member_name == "end_index")
            {
                return Value_and_type
                {
                    .name = "",
                    .value = load_soa_array_view_field(left_hand_side, 1, llvm::Type::getInt64Ty(parameters.llvm_context), parameters),
                    .type = create_integer_type_type_reference(64, false)
                };
            }
            else if (expression.member_name == "length")
            {
                return Value_and_type
                {
                    .name = "",
                    .value = load_soa_array_view_length(left_hand_side, parameters),
                    .type = create_integer_type_type_reference(64, false)
                };
            }
            else if (expression.member_name == "data")
            {
                return Value_and_type
                {
                    .name = "",
                    .value = load_soa_array_view_data_pointer(left_hand_side, parameters),
                    .type = create_pointer_type_type_reference({}, true)
                };
            }
        }

        {
            std::optional<Custom_type_reference> custom_type_reference = get_custom_type_reference_from_access_expression(expression, left_hand_side, statement, core_module.name);

            if (custom_type_reference.has_value())
            {
                std::string_view const module_name = custom_type_reference.value().module_reference.name;
                std::string_view const declaration_name = custom_type_reference.value().name;

                std::optional<Declaration> const declaration = find_declaration(declaration_database, module_name, declaration_name);
                if (declaration.has_value())
                {
                    Declaration const& declaration_value = declaration.value();
                    if (std::holds_alternative<Alias_type_declaration const*>(declaration_value.data))
                    {
                        Alias_type_declaration const* data = std::get<Alias_type_declaration const*>(declaration_value.data);

                        std::optional<Declaration> const underlying_declaration = get_underlying_declaration(declaration_database, *data);
                        if (underlying_declaration.has_value())
                        {
                            if (std::holds_alternative<Enum_declaration const*>(underlying_declaration.value().data))
                            {
                                Enum_declaration const& enum_declaration = *std::get<Enum_declaration const*>(underlying_declaration.value().data);

                                return access_enum_value(
                                    module_name,
                                    enum_declaration,
                                    expression.member_name,
                                    enum_value_constants
                                );
                            }
                            else if (std::holds_alternative<Struct_declaration const*>(underlying_declaration.value().data))
                            {
                                if (left_hand_side.value != nullptr)
                                {
                                    Struct_declaration const& struct_declaration = *std::get<Struct_declaration const*>(underlying_declaration.value().data);
                                    return create_access_struct_member(
                                        left_hand_side,
                                        expression.member_name,
                                        underlying_declaration->module_name,
                                        struct_declaration,
                                        parameters
                                    );
                                }
                            }
                        }
                    }
                    else if (std::holds_alternative<Enum_declaration const*>(declaration_value.data))
                    {
                        Enum_declaration const& enum_declaration = *std::get<Enum_declaration const*>(declaration_value.data);

                        return access_enum_value(
                            module_name,
                            enum_declaration,
                            expression.member_name,
                            enum_value_constants
                        );
                    }
                    else if (std::holds_alternative<Struct_declaration const*>(declaration_value.data))
                    {
                        if (left_hand_side.value != nullptr)
                        {
                            Struct_declaration const& struct_declaration = *std::get<Struct_declaration const*>(declaration_value.data);
                            return create_access_struct_member(
                                left_hand_side,
                                expression.member_name,
                                module_name,
                                struct_declaration,
                                parameters
                            );
                        }
                    }
                    else if (std::holds_alternative<Union_declaration const*>(declaration_value.data))
                    {
                        if (left_hand_side.value != nullptr)
                        {
                            Union_declaration const& union_declaration = *std::get<Union_declaration const*>(declaration_value.data);
                            return create_access_union_member(
                                left_hand_side,
                                expression.member_name,
                                module_name,
                                union_declaration,
                                parameters
                            );
                        }
                    }
                }
            }
        }

        // Try to find declaration in the module of the left hand side type:
        if (left_hand_side.type.has_value())
        {
            Type_reference const& left_hand_side_type = left_hand_side.type.value();
            if (std::holds_alternative<Custom_type_reference>(left_hand_side_type.data))
            {
                Custom_type_reference const& custom_type_reference = std::get<Custom_type_reference>(left_hand_side_type.data);
                std::string_view const module_name = find_module_name(core_module, custom_type_reference.module_reference);

                std::optional<Declaration> const declaration = find_declaration(declaration_database, module_name, expression.member_name);
                if (declaration.has_value() && (std::holds_alternative<Function_constructor const*>(declaration.value().data) || std::holds_alternative<Function_declaration const*>(declaration.value().data)))
                {
                    Type_reference const access_type = create_custom_type_reference(module_name, expression.member_name);

                    return Value_and_type
                    {
                        .name = expression.member_name,
                        .value = nullptr,
                        .type = access_type
                    };
                }
            }
        }

        throw std::runtime_error{ format_error("Could not process access expression!", parameters.source_position) };
    }

    static void create_bounds_check_instructions(
        llvm::Value* const index_value,
        llvm::Value* const size_value,
        std::string_view const error_message,
        Expression_parameters const& parameters
    )
    {
        llvm::LLVMContext& llvm_context = parameters.llvm_context;
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;

        llvm::Value* const index_i64 = llvm_builder.CreateIntCast(
            index_value,
            llvm::Type::getInt64Ty(llvm_context),
            false,
            "bounds_check_index"
        );

        llvm::Value* const in_bounds = llvm_builder.CreateICmpULT(index_i64, size_value, "bounds_check_in_bounds");

        llvm::BasicBlock* const pass_block = llvm::BasicBlock::Create(llvm_context, "bounds_check_pass", parameters.llvm_parent_function);
        llvm::BasicBlock* const fail_block = llvm::BasicBlock::Create(llvm_context, "bounds_check_fail", parameters.llvm_parent_function);

        llvm_builder.CreateCondBr(in_bounds, pass_block, fail_block);

        llvm_builder.SetInsertPoint(fail_block);
        create_log_error_instruction(llvm_context, parameters.llvm_module, llvm_builder, error_message);
        create_abort_instruction(llvm_context, parameters.llvm_module, llvm_builder);
        llvm_builder.CreateUnreachable();

        llvm_builder.SetInsertPoint(pass_block);
    }

    Value_and_type create_access_array_expression_value(
        Access_array_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        llvm::LLVMContext& llvm_context = parameters.llvm_context;
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;
        llvm::DataLayout const& llvm_data_layout = parameters.llvm_data_layout;
        Module const core_module = parameters.core_module;
        Type_database const& type_database = parameters.type_database;

        Value_and_type const index_value = create_loaded_expression_value(expression.index.expression_index, statement, parameters);
        llvm::Value* const index_llvm_value = index_value.value;

        iris::Expression const& indexed_expression = statement.expressions[expression.expression.expression_index];
        if (std::holds_alternative<Dereference_and_access_expression>(indexed_expression.data))
        {
            Dereference_and_access_expression const& access_expression = std::get<Dereference_and_access_expression>(indexed_expression.data);
            Value_and_type const soa_value = create_expression_value(access_expression.expression.expression_index, statement, parameters);

            if (soa_value.type.has_value() && std::holds_alternative<Soa_array_type>(soa_value.type->data))
            {
                Soa_array_type_info const soa_info = get_soa_array_type_info(soa_value.type.value(), parameters);

                auto const member_location = std::find(
                    soa_info.struct_declaration->member_names.begin(),
                    soa_info.struct_declaration->member_names.end(),
                    access_expression.member_name
                );
                if (member_location == soa_info.struct_declaration->member_names.end())
                    throw std::runtime_error{ format_error(std::format("'{}' does not exist in struct type '{}'.", access_expression.member_name, soa_info.struct_declaration->name), parameters.source_position) };

                std::size_t const member_index = static_cast<std::size_t>(std::distance(soa_info.struct_declaration->member_names.begin(), member_location));
                Type_reference const& member_type = soa_info.struct_declaration->member_types[member_index];
                llvm::Type* const member_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, member_type, type_database);

                llvm::Value* const data_pointer = load_soa_data_pointer(soa_value, parameters);

                if (parameters.enable_bounds_checks)
                {
                    llvm::Value* const size_value = llvm_builder.getInt64(soa_info.soa_array_type->size);
                    std::string const error_message = std::format("Out-of-bounds SOA array access in '{}.{}'!", parameters.core_module.name, parameters.function_declaration.has_value() && *parameters.function_declaration ? (*parameters.function_declaration)->name : "?");
                    create_bounds_check_instructions(index_llvm_value, size_value, error_message, parameters);
                }

                llvm::Value* const element_pointer = create_soa_member_element_pointer(
                    data_pointer,
                    soa_info.layout.members[member_index],
                    member_llvm_type,
                    index_llvm_value,
                    parameters
                );

                return Value_and_type
                {
                    .name = "",
                    .value = element_pointer,
                    .type = member_type
                };
            }
            else if (soa_value.type.has_value() && std::holds_alternative<Soa_array_view_type>(soa_value.type->data))
            {
                Soa_array_view_type_info const soa_info = get_soa_array_view_type_info(soa_value.type.value(), parameters);

                auto const member_location = std::find(
                    soa_info.struct_declaration->member_names.begin(),
                    soa_info.struct_declaration->member_names.end(),
                    access_expression.member_name
                );
                if (member_location == soa_info.struct_declaration->member_names.end())
                    throw std::runtime_error{ format_error(std::format("'{}' does not exist in struct type '{}'.", access_expression.member_name, soa_info.struct_declaration->name), parameters.source_position) };

                std::size_t const member_index = static_cast<std::size_t>(std::distance(soa_info.struct_declaration->member_names.begin(), member_location));
                Type_reference const& member_type = soa_info.struct_declaration->member_types[member_index];
                llvm::Type* const member_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, member_type, type_database);

                llvm::Value* const data_pointer = load_soa_array_view_data_pointer(soa_value, parameters);
                llvm::Value* const length_value = load_soa_array_view_length(soa_value, parameters);

                if (parameters.enable_bounds_checks)
                {
                    std::string const error_message = std::format("Out-of-bounds SOA array view access in '{}.{}'!", parameters.core_module.name, parameters.function_declaration.has_value() && *parameters.function_declaration ? (*parameters.function_declaration)->name : "?");
                    create_bounds_check_instructions(index_llvm_value, length_value, error_message, parameters);
                }

                llvm::Value* const adjusted_index = create_soa_array_view_adjusted_index(soa_value, index_llvm_value, parameters);
                llvm::Value* const element_pointer = create_soa_member_element_pointer(
                    data_pointer,
                    soa_info.layout.members,
                    member_index,
                    length_value,
                    member_llvm_type,
                    adjusted_index,
                    parameters
                );

                return Value_and_type
                {
                    .name = "",
                    .value = element_pointer,
                    .type = member_type
                };
            }
        }

        Value_and_type const left_hand_side_expression_value = create_expression_value(expression.expression.expression_index, statement, parameters);
        if (!left_hand_side_expression_value.type.has_value())
            throw std::runtime_error{ format_error("Could not deduce type of left hand side.", parameters.source_position) };

        if (std::holds_alternative<Soa_array_type>(left_hand_side_expression_value.type->data))
        {
            Soa_array_type_info const soa_info = get_soa_array_type_info(left_hand_side_expression_value.type.value(), parameters);

            if (parameters.enable_bounds_checks)
            {
                llvm::Value* const size_value = llvm_builder.getInt64(soa_info.soa_array_type->size);
                std::string const error_message = std::format("Out-of-bounds SOA array access in '{}.{}'!", parameters.core_module.name, parameters.function_declaration.has_value() && *parameters.function_declaration ? (*parameters.function_declaration)->name : "?");
                create_bounds_check_instructions(index_llvm_value, size_value, error_message, parameters);
            }

            llvm::Value* const data_pointer = load_soa_data_pointer(left_hand_side_expression_value, parameters);

            llvm::Type* const element_llvm_type = type_reference_to_llvm_type(
                llvm_context,
                llvm_data_layout,
                *soa_info.element_type,
                type_database
            );

            llvm::AllocaInst* const struct_alloca = create_alloca_instruction(
                llvm_builder,
                llvm_data_layout,
                *parameters.llvm_parent_function,
                element_llvm_type,
                "soa_element"
            );

            for (std::size_t member_index = 0; member_index < soa_info.struct_declaration->member_names.size(); ++member_index)
            {
                Type_reference const& member_type = soa_info.struct_declaration->member_types[member_index];
                llvm::Type* const member_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, member_type, type_database);

                llvm::Value* const source_pointer = create_soa_member_element_pointer(
                    data_pointer,
                    soa_info.layout.members[member_index],
                    member_llvm_type,
                    index_llvm_value,
                    parameters
                );
                llvm::Value* const loaded_member_value = create_load_instruction(llvm_builder, llvm_data_layout, member_llvm_type, source_pointer);

                generate_store_struct_member_instructions(
                    parameters.clang_module_data,
                    parameters.llvm_context,
                    parameters.llvm_builder,
                    parameters.llvm_data_layout,
                    struct_alloca,
                    soa_info.struct_declaration->member_names[member_index],
                    soa_info.module_name,
                    *soa_info.struct_declaration,
                    Value_and_type
                    {
                        .name = "",
                        .value = loaded_member_value,
                        .type = member_type
                    },
                    parameters.type_database
                );
            }

            return Value_and_type
            {
                .name = "",
                .value = struct_alloca,
                .type = *soa_info.element_type
            };
        }
        else if (std::holds_alternative<Soa_array_view_type>(left_hand_side_expression_value.type->data))
        {
            Soa_array_view_type_info const soa_info = get_soa_array_view_type_info(left_hand_side_expression_value.type.value(), parameters);
            llvm::Value* const data_pointer = load_soa_array_view_data_pointer(left_hand_side_expression_value, parameters);
            llvm::Value* const length_value = load_soa_array_view_length(left_hand_side_expression_value, parameters);

            if (parameters.enable_bounds_checks)
            {
                std::string const error_message = std::format("Out-of-bounds SOA array view access in '{}.{}'!", parameters.core_module.name, parameters.function_declaration.has_value() && *parameters.function_declaration ? (*parameters.function_declaration)->name : "?");
                create_bounds_check_instructions(index_llvm_value, length_value, error_message, parameters);
            }

            llvm::Value* const adjusted_index = create_soa_array_view_adjusted_index(left_hand_side_expression_value, index_llvm_value, parameters);

            llvm::Type* const element_llvm_type = type_reference_to_llvm_type(
                llvm_context,
                llvm_data_layout,
                *soa_info.element_type,
                type_database
            );

            llvm::AllocaInst* const struct_alloca = create_alloca_instruction(
                llvm_builder,
                llvm_data_layout,
                *parameters.llvm_parent_function,
                element_llvm_type,
                "soa_element"
            );

            for (std::size_t member_index = 0; member_index < soa_info.struct_declaration->member_names.size(); ++member_index)
            {
                Type_reference const& member_type = soa_info.struct_declaration->member_types[member_index];
                llvm::Type* const member_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, member_type, type_database);

                llvm::Value* const source_pointer = create_soa_member_element_pointer(
                    data_pointer,
                    soa_info.layout.members,
                    member_index,
                    length_value,
                    member_llvm_type,
                    adjusted_index,
                    parameters
                );
                llvm::Value* const loaded_member_value = create_load_instruction(llvm_builder, llvm_data_layout, member_llvm_type, source_pointer);

                generate_store_struct_member_instructions(
                    parameters.clang_module_data,
                    parameters.llvm_context,
                    parameters.llvm_builder,
                    parameters.llvm_data_layout,
                    struct_alloca,
                    soa_info.struct_declaration->member_names[member_index],
                    soa_info.module_name,
                    *soa_info.struct_declaration,
                    Value_and_type
                    {
                        .name = "",
                        .value = loaded_member_value,
                        .type = member_type
                    },
                    parameters.type_database
                );
            }

            return Value_and_type
            {
                .name = "",
                .value = struct_alloca,
                .type = *soa_info.element_type
            };
        }

        std::optional<Type_reference> element_type = get_element_or_pointee_type(*left_hand_side_expression_value.type);
        if (!element_type.has_value())
            throw std::runtime_error{ format_error("Cannot find element type of array access.", parameters.source_position) };
        
        if (is_array_slice_type_reference(*left_hand_side_expression_value.type))
        {
            llvm::Type* const array_slice_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, left_hand_side_expression_value.type.value(), type_database);

            if (parameters.enable_bounds_checks)
            {
                llvm::Value* const pointer_to_length = llvm_builder.CreateStructGEP(array_slice_llvm_type, left_hand_side_expression_value.value, 1);
                llvm::Value* const length = create_load_instruction(llvm_builder, llvm_data_layout, llvm::Type::getInt64Ty(llvm_context), pointer_to_length);
                std::string const error_message = std::format("Out-of-bounds array slice access in '{}.{}'!", parameters.core_module.name, parameters.function_declaration.has_value() && *parameters.function_declaration ? (*parameters.function_declaration)->name : "?");
                create_bounds_check_instructions(index_llvm_value, length, error_message, parameters);
            }

            llvm::Value* const pointer_to_data_pointer = llvm_builder.CreateStructGEP(
                array_slice_llvm_type,
                left_hand_side_expression_value.value,
                0
            );

            llvm::Value* const data_pointer = create_load_instruction(llvm_builder, llvm_data_layout, llvm::PointerType::get(llvm_context, 0), pointer_to_data_pointer);
            
            llvm::Type* const element_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, element_type.value(), type_database);

            llvm::Value* const element_pointer = llvm_builder.CreateGEP(
                element_llvm_type,
                data_pointer,
                llvm::ArrayRef<llvm::Value*>{index_llvm_value},
                "array_slice_element_pointer"
            );

            return Value_and_type
            {
                .name = "",
                .value = element_pointer,
                .type = element_type
            };
        }

        bool const using_pointer = is_pointer(*left_hand_side_expression_value.type);

        if (parameters.enable_bounds_checks && !using_pointer)
        {
            Constant_array_type const& constant_array_type = std::get<Constant_array_type>(left_hand_side_expression_value.type->data);
            llvm::Value* const size_value = llvm_builder.getInt64(constant_array_type.size);
            std::string const error_message = std::format("Out-of-bounds constant array access in '{}.{}'!", parameters.core_module.name, parameters.function_declaration.has_value() && *parameters.function_declaration ? (*parameters.function_declaration)->name : "?");
            create_bounds_check_instructions(index_llvm_value, size_value, error_message, parameters);
        }

        Type_reference const& type_reference_to_use =
            using_pointer ?
            element_type.value() :
            *left_hand_side_expression_value.type;
        llvm::Type* const array_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, type_reference_to_use, type_database);
        llvm::Value* const array_pointer =
            using_pointer ?
            create_load_instruction(llvm_builder, llvm_data_layout, llvm::PointerType::get(llvm_context, 0), left_hand_side_expression_value.value) :
            left_hand_side_expression_value.value;
        
        llvm::Value* const element_pointer = llvm_builder.CreateGEP(
            array_llvm_type,
            array_pointer,
            using_pointer ? llvm::ArrayRef<llvm::Value*>{index_llvm_value} : llvm::ArrayRef<llvm::Value*>{llvm_builder.getInt32(0), index_llvm_value},
            "array_element_pointer"
        );
        
        return Value_and_type
        {
            .name = "",
            .value = element_pointer,
            .type = element_type
        };
    }

    Value_and_type create_binary_operation_instruction(
        llvm::IRBuilder<>& llvm_builder,
        Value_and_type const& left_hand_side,
        Value_and_type const& right_hand_side,
        Binary_operation operation,
        Declaration_database const& declaration_database,
        std::optional<Source_position> const& source_position
    );

    Value_and_type create_assignment_additional_operation_instruction(
        Expression_index const left_hand_side,
        Expression_index const right_hand_side,
        Type_reference const& expression_type,
        std::optional<Binary_operation> const additional_operation,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        if (additional_operation.has_value())
        {
            llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;

            Binary_operation const operation = additional_operation.value();

            Value_and_type const left_hand_side_value = create_loaded_expression_value(left_hand_side.expression_index, statement, parameters);

            Expression_parameters right_hand_side_parameters = parameters;
            right_hand_side_parameters.expression_type = left_hand_side_value.type;
            Value_and_type const right_hand_side_value = create_loaded_expression_value(right_hand_side.expression_index, statement, right_hand_side_parameters);

            Value_and_type const result = create_binary_operation_instruction(llvm_builder, left_hand_side_value, right_hand_side_value, operation, parameters.declaration_database, parameters.source_position);

            return result;
        }
        else
        {
            Expression_parameters right_hand_side_parameters = parameters;
            right_hand_side_parameters.expression_type = expression_type;
            Value_and_type const right_hand_side_value = create_loaded_expression_value(right_hand_side.expression_index, statement, right_hand_side_parameters);
            return right_hand_side_value;
        }
    }

    Value_and_type create_assert_expression_value(
        Assert_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        iris::Function_condition const condition = {
            .description = expression.message.value_or(""),
            .condition = expression.statement
        };

        create_check_condition_instructions(
            parameters.llvm_context,
            parameters.llvm_module,
            *parameters.llvm_parent_function,
            parameters.llvm_builder,
            parameters.core_module,
            *parameters.function_declaration.value(),
            condition,
            Condition_type::Assert,
            parameters
        );

        return
        {
            .name = "",
            .value = nullptr,
            .type = std::nullopt
        };
    }

    Value_and_type create_assignment_expression_value(
        Assignment_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;
        llvm::DataLayout const& llvm_data_layout = parameters.llvm_data_layout;

        Value_and_type const left_hand_side = create_expression_value(expression.left_hand_side.expression_index, statement, parameters);
        
        Value_and_type const result = create_assignment_additional_operation_instruction(
            expression.left_hand_side,
            expression.right_hand_side,
            left_hand_side.type.value(),
            expression.additional_operation,
            statement,
            parameters
        );

        if (parameters.debug_info != nullptr)
            set_debug_location(parameters.llvm_builder, *parameters.debug_info, parameters.source_position);
        
        Expression const left_hand_side_expression = statement.expressions[expression.left_hand_side.expression_index];
        if (left_hand_side.value != nullptr && std::holds_alternative<Access_expression>(left_hand_side_expression.data))
        {
            Access_expression const& access_expression = std::get<Access_expression>(left_hand_side_expression.data);

            Value_and_type const access_left_hand_side = create_expression_value(access_expression.expression.expression_index, statement, parameters);
            std::optional<Custom_type_reference> custom_type_reference = get_custom_type_reference_from_access_expression(access_expression, access_left_hand_side, statement, parameters.core_module.name);

            if (custom_type_reference.has_value())
            {
                std::string_view const module_name = custom_type_reference.value().module_reference.name;
                std::string_view const declaration_name = custom_type_reference.value().name;

                std::optional<Declaration> const declaration = find_declaration(parameters.declaration_database, module_name, declaration_name);
                if (declaration.has_value())
                {
                    Declaration const& declaration_value = declaration.value();

                    if (std::holds_alternative<Struct_declaration const*>(declaration_value.data))
                    {
                        Struct_declaration const& struct_declaration = *std::get<Struct_declaration const*>(declaration_value.data);

                        return generate_store_struct_member_instructions(
                            parameters.clang_module_data,
                            parameters.llvm_context,
                            parameters.llvm_builder,
                            parameters.llvm_data_layout,
                            access_left_hand_side.value,
                            access_expression.member_name,
                            module_name,
                            struct_declaration,
                            result,
                            parameters.type_database
                        );
                    }
                }
            }
        }

        // Check if this is an indexed SOA array assignment: particles[1] = {...}
        if (std::holds_alternative<Access_array_expression>(left_hand_side_expression.data))
        {
            Access_array_expression const& access_array_expression = std::get<Access_array_expression>(left_hand_side_expression.data);

            Scope const scope = create_type_analysis_scope(parameters);
            std::optional<Type_reference> const array_base_expression_type = get_expression_type(
                parameters.core_module.name,
                parameters.function_declaration.has_value() ? parameters.function_declaration.value() : nullptr,
                scope,
                statement,
                statement.expressions[access_array_expression.expression.expression_index],
                std::nullopt,
                parameters.declaration_database
            );

            if (
                array_base_expression_type.has_value() &&
                (
                    std::holds_alternative<Soa_array_type>(array_base_expression_type->data) ||
                    std::holds_alternative<Soa_array_view_type>(array_base_expression_type->data)
                )
            )
            {
                // This is a direct indexed access like particles[index]
                Value_and_type const array_value = create_expression_value(access_array_expression.expression.expression_index, statement, parameters);

                if (array_value.type.has_value() && std::holds_alternative<Soa_array_type>(array_value.type->data))
                {
                    // This is an indexed SOA array assignment
                    Soa_array_type_info const soa_info = get_soa_array_type_info(array_value.type.value(), parameters);
                    llvm::Value* const data_pointer = load_soa_data_pointer(array_value, parameters);

                    // Get the index value
                    Value_and_type const index_value = create_loaded_expression_value(access_array_expression.index.expression_index, statement, parameters);

                    // Create a temporary alloca to store the RHS struct value
                    llvm::Type* const element_llvm_type = type_reference_to_llvm_type(
                        parameters.llvm_context,
                        parameters.llvm_data_layout,
                        *soa_info.element_type,
                        parameters.type_database
                    );

                    llvm::AllocaInst* const struct_alloca = create_alloca_instruction(
                        parameters.llvm_builder,
                        parameters.llvm_data_layout,
                        *parameters.llvm_parent_function,
                        element_llvm_type,
                        "soa_assignment_struct"
                    );

                    // Store the RHS struct value to temporary alloca
                    create_store_instruction(parameters.llvm_builder, parameters.llvm_data_layout, result.value, struct_alloca);

                    // For each member of the struct, extract from temp alloca and store to SOA layout
                    for (std::size_t member_index = 0; member_index < soa_info.struct_declaration->member_names.size(); ++member_index)
                    {
                        // Extract the member value from the temporary struct alloca
                        Value_and_type const member_value = generate_load_struct_member_instructions(
                            parameters.clang_module_data,
                            parameters.llvm_context,
                            parameters.llvm_builder,
                            parameters.llvm_data_layout,
                            struct_alloca,
                            soa_info.struct_declaration->member_names[member_index],
                            soa_info.module_name,
                            *soa_info.struct_declaration,
                            parameters.type_database
                        );

                        // Store the member to the SOA layout
                        store_soa_member_value(
                            data_pointer,
                            soa_info,
                            member_index,
                            index_value.value,
                            member_value,
                            parameters
                        );
                    }

                    return Value_and_type
                    {
                        .name = "",
                        .value = nullptr,
                        .type = std::nullopt
                    };
                }
                else if (array_value.type.has_value() && std::holds_alternative<Soa_array_view_type>(array_value.type->data))
                {
                    Soa_array_view_type_info const soa_info = get_soa_array_view_type_info(array_value.type.value(), parameters);
                    llvm::Value* const data_pointer = load_soa_array_view_data_pointer(array_value, parameters);
                    llvm::Value* const length_value = load_soa_array_view_length(array_value, parameters);

                    Value_and_type const index_value = create_loaded_expression_value(access_array_expression.index.expression_index, statement, parameters);
                    llvm::Value* const adjusted_index = create_soa_array_view_adjusted_index(array_value, index_value.value, parameters);

                    llvm::Type* const element_llvm_type = type_reference_to_llvm_type(
                        parameters.llvm_context,
                        parameters.llvm_data_layout,
                        *soa_info.element_type,
                        parameters.type_database
                    );

                    llvm::AllocaInst* const struct_alloca = create_alloca_instruction(
                        parameters.llvm_builder,
                        parameters.llvm_data_layout,
                        *parameters.llvm_parent_function,
                        element_llvm_type,
                        "soa_assignment_struct"
                    );

                    create_store_instruction(parameters.llvm_builder, parameters.llvm_data_layout, result.value, struct_alloca);

                    for (std::size_t member_index = 0; member_index < soa_info.struct_declaration->member_names.size(); ++member_index)
                    {
                        Value_and_type const member_value = generate_load_struct_member_instructions(
                            parameters.clang_module_data,
                            parameters.llvm_context,
                            parameters.llvm_builder,
                            parameters.llvm_data_layout,
                            struct_alloca,
                            soa_info.struct_declaration->member_names[member_index],
                            soa_info.module_name,
                            *soa_info.struct_declaration,
                            parameters.type_database
                        );

                        Type_reference const& member_type = soa_info.struct_declaration->member_types[member_index];
                        llvm::Type* const member_llvm_type = type_reference_to_llvm_type(
                            parameters.llvm_context,
                            parameters.llvm_data_layout,
                            member_type,
                            parameters.type_database
                        );

                        llvm::Value* const destination_pointer = create_soa_member_element_pointer(
                            data_pointer,
                            soa_info.layout.members,
                            member_index,
                            length_value,
                            member_llvm_type,
                            adjusted_index,
                            parameters
                        );

                        llvm::Value* value_to_store = member_value.value;
                        if (value_to_store != nullptr && value_to_store->getType()->isPointerTy())
                            value_to_store = create_load_instruction(parameters.llvm_builder, parameters.llvm_data_layout, member_llvm_type, value_to_store);

                        create_store_instruction(parameters.llvm_builder, parameters.llvm_data_layout, value_to_store, destination_pointer);
                    }

                    return Value_and_type
                    {
                        .name = "",
                        .value = nullptr,
                        .type = std::nullopt
                    };
                }
            }
        }

        llvm::Value* store_instruction = create_store_instruction(llvm_builder, llvm_data_layout, result.value, left_hand_side.value);

        return
        {
            .name = "",
            .value = store_instruction,
            .type = std::nullopt
        };
    }

    bool are_types_compatible(
        Declaration_database const& declaration_database,
        Type_reference const& first,
        Type_reference const& second
    )
    {
        std::optional<Type_reference> const underlying_first_optional = get_underlying_type(declaration_database, first);
        std::optional<Type_reference> const underlying_second_optional = get_underlying_type(declaration_database, second);
        if (underlying_first_optional.has_value() && underlying_second_optional.has_value())
        {
            Type_reference const& underlying_first = underlying_first_optional.value();
            Type_reference const& underlying_second = underlying_second_optional.value();

            if ((is_pointer(underlying_first) && is_null_pointer_type(underlying_second)) || (is_null_pointer_type(underlying_first) && is_pointer(underlying_second)))
                return true;

            if ((is_function_pointer(underlying_first) && is_null_pointer_type(underlying_second)) || (is_null_pointer_type(underlying_first) && is_function_pointer(underlying_second)))
                return true;

            return underlying_first == underlying_second;
        }

        return first == second;
    }

    std::int64_t compute_decimal_scale(std::uint32_t const n)
    {
        std::int64_t v = 1;
        for (std::uint32_t i = 0; i < n; ++i)
            v *= 10;
        return v;
    }

    Value_and_type create_binary_operation_instruction(
        llvm::IRBuilder<>& llvm_builder,
        Value_and_type const& left_hand_side,
        Value_and_type const& right_hand_side,
        Binary_operation const operation,
        Declaration_database const& declaration_database,
        std::optional<Source_position> const& source_position
    )
    {
        if (!left_hand_side.type.has_value() || !right_hand_side.type.has_value())
            throw std::runtime_error{ format_error("Left or right side type is null!", source_position) };

        if (!are_types_compatible(declaration_database, *left_hand_side.type, *right_hand_side.type))
            throw std::runtime_error{ format_error("Left and right side types do not match!", source_position) };

        std::optional<Type_reference> const underling_type = get_underlying_type(declaration_database, left_hand_side.type.value());
        Type_reference const& type = underling_type.has_value() ? underling_type.value() : left_hand_side.type.value();

        auto const create_bool_value = [&llvm_builder](llvm::Value* condition_value) -> Value_and_type
        {
            llvm::Value* result_value = condition_value;
            if (condition_value->getType()->isIntegerTy() && !condition_value->getType()->isIntegerTy(1))
            {
                llvm::Value* const zero_value = llvm::ConstantInt::get(condition_value->getType(), 0);
                result_value = llvm_builder.CreateICmpNE(condition_value, zero_value);
            }

            return Value_and_type
            {
                .name = "",
                .value = result_value,
                .type = create_bool_type_reference()
            };
        };

        switch (operation)
        {
        case Binary_operation::Add: {
            if (is_integer(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateAdd(left_hand_side.value, right_hand_side.value),
                    .type = type
                };
            }
            else if (is_floating_point(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateFAdd(left_hand_side.value, right_hand_side.value),
                    .type = type
                };
            }
            else if (is_decimal(type))
            {
                // Decimal add/sub: same backing integer operation, no scaling needed
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateAdd(left_hand_side.value, right_hand_side.value),
                    .type = type
                };
            }
            break;
        }
        case Binary_operation::Subtract: {
            if (is_integer(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateSub(left_hand_side.value, right_hand_side.value),
                    .type = type
                };
            }
            else if (is_floating_point(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateFSub(left_hand_side.value, right_hand_side.value),
                    .type = type
                };
            }
            else if (is_decimal(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateSub(left_hand_side.value, right_hand_side.value),
                    .type = type
                };
            }
            break;
        }
        case Binary_operation::Multiply: {
            if (is_integer(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateMul(left_hand_side.value, right_hand_side.value),
                    .type = type
                };
            }
            else if (is_floating_point(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateFMul(left_hand_side.value, right_hand_side.value),
                    .type = type
                };
            }
            else if (is_decimal(type))
            {
                // Decimal(N) * Decimal(N): widen, multiply, divide by 10^N, truncate
                Decimal_type const& decimal = std::get<Decimal_type>(type.data);
                std::uint32_t const backing_bits = decimal.scale <= 6 ? 32 : 64;
                std::uint32_t const wide_bits = backing_bits == 32 ? 64 : 128;

                llvm::LLVMContext& llvm_context = llvm_builder.getContext();
                llvm::Type* const wide_type = llvm::Type::getIntNTy(llvm_context, wide_bits);
                llvm::Type* const backing_type = llvm::Type::getIntNTy(llvm_context, backing_bits);
                
                // Widen
                llvm::Value* const lhs_wide = llvm_builder.CreateSExt(left_hand_side.value, wide_type);
                llvm::Value* const rhs_wide = llvm_builder.CreateSExt(right_hand_side.value, wide_type);
                
                // Multiply
                llvm::Value* const product = llvm_builder.CreateMul(lhs_wide, rhs_wide);
                
                // Divide by 10^N
                std::int64_t const scale_value = compute_decimal_scale(decimal.scale);
                llvm::APInt const scale_ap{ wide_bits, static_cast<std::uint64_t>(scale_value), true };
                llvm::Value* const scale_const = llvm::ConstantInt::get(wide_type, scale_ap);
                llvm::Value* const divided = llvm_builder.CreateSDiv(product, scale_const);
                
                // Truncate
                llvm::Value* const result = llvm_builder.CreateTrunc(divided, backing_type);
                
                return Value_and_type { .name = "", .value = result, .type = type };
            }
            break;
        }
        case Binary_operation::Divide: {
            if (is_integer(type))
            {
                if (is_signed_integer(type))
                {
                    return Value_and_type
                    {
                        .name = "",
                        .value = llvm_builder.CreateSDiv(left_hand_side.value, right_hand_side.value),
                        .type = type
                    };
                }
                else
                {
                    return Value_and_type
                    {
                        .name = "",
                        .value = llvm_builder.CreateUDiv(left_hand_side.value, right_hand_side.value),
                        .type = type
                    };
                }
            }
            else if (is_floating_point(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateFDiv(left_hand_side.value, right_hand_side.value),
                    .type = type
                };
            }
            else if (is_decimal(type))
            {
                // Decimal(N) / Decimal(N): widen, multiply lhs by 10^N, divide by rhs, truncate
                Decimal_type const& decimal = std::get<Decimal_type>(type.data);
                std::uint32_t const backing_bits = decimal.scale <= 6 ? 32 : 64;
                std::uint32_t const wide_bits = backing_bits == 32 ? 64 : 128;

                llvm::LLVMContext& llvm_context = llvm_builder.getContext();
                llvm::Type* const wide_type = llvm::Type::getIntNTy(llvm_context, wide_bits);
                llvm::Type* const backing_type = llvm::Type::getIntNTy(llvm_context, backing_bits);
                
                // Widen
                llvm::Value* const lhs_wide = llvm_builder.CreateSExt(left_hand_side.value, wide_type);
                llvm::Value* const rhs_wide = llvm_builder.CreateSExt(right_hand_side.value, wide_type);
                
                // Multiply lhs by 10^N
                std::int64_t const scale_value = compute_decimal_scale(decimal.scale);
                llvm::APInt const scale_ap{ wide_bits, static_cast<std::uint64_t>(scale_value), true };
                llvm::Value* const scale_const = llvm::ConstantInt::get(wide_type, scale_ap);
                llvm::Value* const lhs_scaled = llvm_builder.CreateMul(lhs_wide, scale_const);
                
                // Divide by rhs
                llvm::Value* const divided = llvm_builder.CreateSDiv(lhs_scaled, rhs_wide);
                
                // Truncate
                llvm::Value* const result = llvm_builder.CreateTrunc(divided, backing_type);
                
                return Value_and_type { .name = "", .value = result, .type = type };
            }
            break;
        }
        case Binary_operation::Modulus: {
            if (is_integer(type))
            {
                if (is_signed_integer(type))
                {
                    return Value_and_type
                    {
                        .name = "",
                        .value = llvm_builder.CreateSRem(left_hand_side.value, right_hand_side.value),
                        .type = type
                    };
                }
                else
                {
                    return Value_and_type
                    {
                        .name = "",
                        .value = llvm_builder.CreateURem(left_hand_side.value, right_hand_side.value),
                        .type = type
                    };
                }
            }
            else if (is_floating_point(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateFRem(left_hand_side.value, right_hand_side.value),
                    .type = type
                };
            }
            break;
        }
        case Binary_operation::Equal: {
            if (is_bool(type) || is_integer(type) || is_decimal(type) || is_enum_type(type, left_hand_side.value) || is_pointer(type) || is_function_pointer(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateICmpEQ(left_hand_side.value, right_hand_side.value),
                    .type = create_bool_type_reference()
                };
            }
            else if (is_floating_point(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateFCmpOEQ(left_hand_side.value, right_hand_side.value),
                    .type = create_bool_type_reference()
                };
            }
            break;
        }
        case Binary_operation::Not_equal: {
            if (is_bool(type) || is_integer(type) || is_decimal(type) || is_enum_type(type, left_hand_side.value) || is_pointer(type) || is_function_pointer(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateICmpNE(left_hand_side.value, right_hand_side.value),
                    .type = create_bool_type_reference()
                };
            }
            else if (is_floating_point(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateFCmpONE(left_hand_side.value, right_hand_side.value),
                    .type = create_bool_type_reference()
                };
            }
            break;
        }
        case Binary_operation::Less_than: {
            if (is_integer(type))
            {
                if (is_signed_integer(type))
                {
                    return Value_and_type
                    {
                        .name = "",
                        .value = llvm_builder.CreateICmpSLT(left_hand_side.value, right_hand_side.value),
                        .type = create_bool_type_reference()
                    };
                }
                else
                {
                    return Value_and_type
                    {
                        .name = "",
                        .value = llvm_builder.CreateICmpULT(left_hand_side.value, right_hand_side.value),
                        .type = create_bool_type_reference()
                    };
                }
            }
            else if (is_floating_point(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateFCmpOLT(left_hand_side.value, right_hand_side.value),
                    .type = create_bool_type_reference()
                };
            }
            else if (is_decimal(type))
            {
                // Decimal compares as signed integer (backing storage is always signed)
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateICmpSLT(left_hand_side.value, right_hand_side.value),
                    .type = create_bool_type_reference()
                };
            }
            break;
        }
        case Binary_operation::Less_than_or_equal_to: {
            if (is_integer(type))
            {
                if (is_signed_integer(type))
                {
                    return Value_and_type
                    {
                        .name = "",
                        .value = llvm_builder.CreateICmpSLE(left_hand_side.value, right_hand_side.value),
                        .type = create_bool_type_reference()
                    };
                }
                else
                {
                    return Value_and_type
                    {
                        .name = "",
                        .value = llvm_builder.CreateICmpULE(left_hand_side.value, right_hand_side.value),
                        .type = create_bool_type_reference()
                    };
                }
            }
            else if (is_floating_point(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateFCmpOLE(left_hand_side.value, right_hand_side.value),
                    .type = create_bool_type_reference()
                };
            }
            else if (is_decimal(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateICmpSLE(left_hand_side.value, right_hand_side.value),
                    .type = create_bool_type_reference()
                };
            }
            break;
        }
        case Binary_operation::Greater_than: {
            if (is_integer(type))
            {
                if (is_signed_integer(type))
                {
                    return Value_and_type
                    {
                        .name = "",
                        .value = llvm_builder.CreateICmpSGT(left_hand_side.value, right_hand_side.value),
                        .type = create_bool_type_reference()
                    };
                }
                else
                {
                    return Value_and_type
                    {
                        .name = "",
                        .value = llvm_builder.CreateICmpUGT(left_hand_side.value, right_hand_side.value),
                        .type = create_bool_type_reference()
                    };
                }
            }
            else if (is_floating_point(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateFCmpOGT(left_hand_side.value, right_hand_side.value),
                    .type = create_bool_type_reference()
                };
            }
            else if (is_decimal(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateICmpSGT(left_hand_side.value, right_hand_side.value),
                    .type = create_bool_type_reference()
                };
            }
            break;
        }
        case Binary_operation::Greater_than_or_equal_to: {
            if (is_integer(type))
            {
                if (is_signed_integer(type))
                {
                    return Value_and_type
                    {
                        .name = "",
                        .value = llvm_builder.CreateICmpSGE(left_hand_side.value, right_hand_side.value),
                        .type = create_bool_type_reference()
                    };
                }
                else
                {
                    return Value_and_type
                    {
                        .name = "",
                        .value = llvm_builder.CreateICmpUGE(left_hand_side.value, right_hand_side.value),
                        .type = create_bool_type_reference()
                    };
                }
            }
            else if (is_floating_point(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateFCmpOGE(left_hand_side.value, right_hand_side.value),
                    .type = create_bool_type_reference()
                };
            }
            else if (is_decimal(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateICmpSGE(left_hand_side.value, right_hand_side.value),
                    .type = create_bool_type_reference()
                };
            }
            break;
        }
        case Binary_operation::Logical_and: {
            Value_and_type const left_bool_value = create_bool_value(left_hand_side.value);
            Value_and_type const right_bool_value = create_bool_value(right_hand_side.value);

            return Value_and_type
            {
                .name = "",
                .value = llvm_builder.CreateAnd(left_bool_value.value, right_bool_value.value),
                .type = create_bool_type_reference()
            };
        }
        case Binary_operation::Logical_or: {
            Value_and_type const left_bool_value = create_bool_value(left_hand_side.value);
            Value_and_type const right_bool_value = create_bool_value(right_hand_side.value);

            return Value_and_type
            {
                .name = "",
                .value = llvm_builder.CreateOr(left_bool_value.value, right_bool_value.value),
                .type = create_bool_type_reference()
            };
        }
        case Binary_operation::Bitwise_and: {
            if (is_integer(type) || is_enum_type(type, left_hand_side.value))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateAnd(left_hand_side.value, right_hand_side.value),
                    .type = type
                };
            }
            break;
        }
        case Binary_operation::Bitwise_or: {
            if (is_integer(type) || is_enum_type(type, left_hand_side.value))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateOr(left_hand_side.value, right_hand_side.value),
                    .type = type
                };
            }
            break;
        }
        case Binary_operation::Bitwise_xor: {
            if (is_integer(type) || is_enum_type(type, left_hand_side.value))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateXor(left_hand_side.value, right_hand_side.value),
                    .type = type
                };
            }
            break;
        }
        case Binary_operation::Bit_shift_left: {
            if (is_integer(type))
            {
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateShl(left_hand_side.value, right_hand_side.value),
                    .type = type
                };
            }
            break;
        }
        case Binary_operation::Bit_shift_right: {
            if (is_integer(type))
            {
                if (is_signed_integer(type))
                {
                    return Value_and_type
                    {
                        .name = "",
                        .value = llvm_builder.CreateAShr(left_hand_side.value, right_hand_side.value),
                        .type = type
                    };
                }
                else
                {
                    return Value_and_type
                    {
                        .name = "",
                        .value = llvm_builder.CreateLShr(left_hand_side.value, right_hand_side.value),
                        .type = type
                    };
                }
            }
            break;
        }
        case Binary_operation::Has: {
            if (is_enum_type(type, left_hand_side.value))
            {
                llvm::Value* const and_value = llvm_builder.CreateAnd(left_hand_side.value, right_hand_side.value);

                unsigned const integer_bit_width = left_hand_side.value->getType()->getIntegerBitWidth();
                llvm::Value* const zero_value = llvm_builder.getIntN(integer_bit_width, 0);

                llvm::Value* const compare_value = llvm_builder.CreateICmpUGT(and_value, zero_value);

                return Value_and_type
                {
                    .name = "",
                    .value = compare_value,
                    .type = create_bool_type_reference()
                };
            }
            break;
        }
        }

        throw std::runtime_error{ format_error(std::format("Binary operation '{}' not implemented!", static_cast<std::uint32_t>(operation)), source_position) };
    }

    Value_and_type create_binary_expression_value(
        Binary_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;

        Value_and_type const& left_hand_side = create_loaded_expression_value(expression.left_hand_side.expression_index, statement, parameters);
        Value_and_type const& right_hand_side = create_loaded_expression_value(expression.right_hand_side.expression_index, statement, parameters);
        Binary_operation const operation = expression.operation;

        Value_and_type value = create_binary_operation_instruction(llvm_builder, left_hand_side, right_hand_side, operation, parameters.declaration_database, parameters.source_position);
        return value;
    }

    Value_and_type create_block_expression_value(
        Block_expression const& block_expression,
        Expression_parameters const& parameters
    )
    {
        std::span<Statement const> statements = block_expression.statements;

        if (parameters.debug_info != nullptr && parameters.source_position.has_value())
            push_debug_lexical_block_scope(*parameters.debug_info, *parameters.source_position);

        std::pmr::vector<Block_info> all_block_infos{ parameters.blocks.begin(), parameters.blocks.end() };
        all_block_infos.push_back(Block_info{ .block_type = Block_type::None });
        std::pmr::vector<std::pmr::vector<Statement>> defer_expressions_per_block = create_defer_block(parameters.defer_expressions_per_block);

        Expression_parameters block_parameters = parameters;
        block_parameters.blocks = all_block_infos;
        block_parameters.defer_expressions_per_block = defer_expressions_per_block;

        create_statement_values(
            statements,
            block_parameters,
            true
        );

        if (parameters.debug_info != nullptr && parameters.source_position.has_value())
            pop_debug_scope(*parameters.debug_info);

        return Value_and_type
        {
            .name = "",
            .value = nullptr,
            .type = std::nullopt
        };
    }

    Value_and_type create_break_expression_value(
        Break_expression const& break_expression,
        llvm::IRBuilder<>& llvm_builder,
        std::span<Block_info const> const blocks,
        Expression_parameters const& parameters
    )
    {
        std::array<Block_type, 3> const target_block_types
        {
            Block_type::For_loop,
            Block_type::Switch,
            Block_type::While_loop,
        };
        std::pair<Block_info const*, Blocks_to_pop_count> const target_block_result = 
            find_target_block(blocks, break_expression.loop_count, target_block_types, parameters.source_position);
        llvm::BasicBlock* const target_block = target_block_result.first->after_block;
        Blocks_to_pop_count const blocks_to_pop_count = target_block_result.second;

        create_instructions_pop_blocks(parameters, blocks_to_pop_count);

        if (parameters.debug_info != nullptr)
            set_debug_location(parameters.llvm_builder, *parameters.debug_info, parameters.source_position);

        llvm_builder.CreateBr(target_block);

        return Value_and_type
        {
            .name = "",
            .value = nullptr,
            .type = std::nullopt
        };
    }

    Value_and_type create_call_expression_value_common(
        std::pmr::vector<bool> const& is_expression_address_of,
        std::span<std::optional<Type_reference> const> const argument_types,
        llvm::Value* const llvm_function_callee,
        llvm::FunctionType* const llvm_function_type,
        std::span<llvm::Value* const> const llvm_arguments,
        Function_pointer_type const& function_pointer_type,
        std::optional<Source_position> const& source_position,
        Expression_parameters const& parameters
    )
    {
        if (parameters.debug_info != nullptr)
            set_debug_location(parameters.llvm_builder, *parameters.debug_info, source_position);

        llvm::Value* call_instruction = generate_function_call(
            is_expression_address_of,
            argument_types,
            parameters.llvm_context,
            parameters.llvm_builder,
            parameters.llvm_data_layout,
            parameters.llvm_module,
            *parameters.llvm_parent_function,
            parameters.clang_module_data,
            parameters.core_module,
            function_pointer_type.type,
            *llvm_function_type,
            *llvm_function_callee,
            llvm_arguments,
            parameters.declaration_database,
            parameters.type_database
        );

        std::optional<Type_reference> function_output_type_reference = get_function_output_type_reference(function_pointer_type.type, parameters.core_module);

        return
        {
            .name = "",
            .value = call_instruction,
            .type = std::move(function_output_type_reference)
        };
    }

    bool is_member_of_struct(
        Struct_declaration const& struct_declaration,
        std::string_view const member_name
    )
    {
        auto const location = std::find_if(
            struct_declaration.member_names.begin(),
            struct_declaration.member_names.end(),
            [&](std::pmr::string const& current_member_name) -> bool {
                return current_member_name == member_name;
            }
        );
        return location != struct_declaration.member_names.end();
    }

    static Soa_array_type_info get_soa_array_type_info(
        Type_reference const& type_reference,
        Expression_parameters const& parameters
    )
    {
        if (!std::holds_alternative<Soa_array_type>(type_reference.data))
            throw std::runtime_error{ format_error("Expected Soa_array type.", parameters.source_position) };

        Soa_array_type const& soa_array_type = std::get<Soa_array_type>(type_reference.data);
        if (soa_array_type.value_type.empty())
            throw std::runtime_error{ format_error("Soa_array value_type is not specified.", parameters.source_position) };

        Type_reference const& element_type = soa_array_type.value_type.front();
        Custom_type_reference const* const custom_type_reference = std::get_if<Custom_type_reference>(&element_type.data);
        if (custom_type_reference == nullptr)
            throw std::runtime_error{ format_error("Soa_array element type must be a struct type.", parameters.source_position) };

        std::optional<Declaration> const declaration = find_declaration(
            parameters.declaration_database,
            custom_type_reference->module_reference.name,
            custom_type_reference->name
        );
        if (!declaration.has_value() || !std::holds_alternative<Struct_declaration const*>(declaration->data))
            throw std::runtime_error{ format_error("Soa_array element type must resolve to a struct declaration.", parameters.source_position) };

        Struct_declaration const& struct_declaration = *std::get<Struct_declaration const*>(declaration->data);

        return Soa_array_type_info
        {
            .soa_array_type = &soa_array_type,
            .element_type = &element_type,
            .struct_declaration = &struct_declaration,
            .module_name = custom_type_reference->module_reference.name,
            .layout = calculate_soa_layout(
                parameters.llvm_data_layout,
                parameters.type_database,
                custom_type_reference->module_reference.name,
                custom_type_reference->name,
                soa_array_type.size
            )
        };
    }

    static Soa_array_view_type_info get_soa_array_view_type_info(
        Type_reference const& type_reference,
        Expression_parameters const& parameters
    )
    {
        if (!std::holds_alternative<Soa_array_view_type>(type_reference.data))
            throw std::runtime_error{ format_error("Expected Soa_array_view type.", parameters.source_position) };

        Soa_array_view_type const& soa_array_view_type = std::get<Soa_array_view_type>(type_reference.data);
        if (soa_array_view_type.value_type.empty())
            throw std::runtime_error{ format_error("Soa_array_view value_type is not specified.", parameters.source_position) };

        Type_reference const& element_type = soa_array_view_type.value_type.front();
        Custom_type_reference const* const custom_type_reference = std::get_if<Custom_type_reference>(&element_type.data);
        if (custom_type_reference == nullptr)
            throw std::runtime_error{ format_error("Soa_array_view element type must be a struct type.", parameters.source_position) };

        std::optional<Declaration> const declaration = find_declaration(
            parameters.declaration_database,
            custom_type_reference->module_reference.name,
            custom_type_reference->name
        );
        if (!declaration.has_value() || !std::holds_alternative<Struct_declaration const*>(declaration->data))
            throw std::runtime_error{ format_error("Soa_array_view element type must resolve to a struct declaration.", parameters.source_position) };

        Struct_declaration const& struct_declaration = *std::get<Struct_declaration const*>(declaration->data);

        return Soa_array_view_type_info
        {
            .soa_array_view_type = &soa_array_view_type,
            .element_type = &element_type,
            .struct_declaration = &struct_declaration,
            .module_name = custom_type_reference->module_reference.name,
            .layout = calculate_soa_layout(
                parameters.llvm_data_layout,
                parameters.type_database,
                custom_type_reference->module_reference.name,
                custom_type_reference->name,
                1
            )
        };
    }

    static std::pair<llvm::AllocaInst*, llvm::Value*> create_soa_array_storage(
        Type_reference const& soa_type_reference,
        Soa_layout const& layout,
        Expression_parameters const& parameters,
        std::string_view const name
    )
    {
        if (parameters.llvm_parent_function == nullptr)
            throw std::runtime_error{ format_error("Soa_array local storage requires a parent function.", parameters.source_position) };

        llvm::Type* const soa_llvm_type = type_reference_to_llvm_type(
            parameters.llvm_context,
            parameters.llvm_data_layout,
            soa_type_reference,
            parameters.type_database
        );

        llvm::AllocaInst* const soa_alloca = create_alloca_instruction(
            parameters.llvm_builder,
            parameters.llvm_data_layout,
            *parameters.llvm_parent_function,
            soa_llvm_type,
            name
        );

        llvm::Value* data_pointer = llvm::ConstantPointerNull::get(llvm::Type::getInt8Ty(parameters.llvm_context)->getPointerTo());

        if (layout.size > 0)
        {
            llvm::ArrayType* const storage_type = llvm::ArrayType::get(llvm::Type::getInt8Ty(parameters.llvm_context), layout.size);
            llvm::AllocaInst* const storage_alloca = create_alloca_instruction(
                parameters.llvm_builder,
                parameters.llvm_data_layout,
                *parameters.llvm_parent_function,
                storage_type,
                std::format("{}_storage", name)
            );

            if (layout.alignment > 1)
                storage_alloca->setAlignment(llvm::Align(layout.alignment));

            data_pointer = parameters.llvm_builder.CreateGEP(
                storage_type,
                storage_alloca,
                { parameters.llvm_builder.getInt32(0), parameters.llvm_builder.getInt32(0) },
                std::format("{}_data", name)
            );
        }

        llvm::Value* const data_pointer_field = parameters.llvm_builder.CreateStructGEP(soa_llvm_type, soa_alloca, 0);
        create_store_instruction(parameters.llvm_builder, parameters.llvm_data_layout, data_pointer, data_pointer_field);

        return { soa_alloca, data_pointer };
    }

    static llvm::Value* create_soa_member_element_pointer(
        llvm::Value* const data_pointer,
        Soa_member_layout const& member_layout,
        llvm::Type* const member_llvm_type,
        llvm::Value* const index_value,
        Expression_parameters const& parameters
    )
    {
        llvm::Value* const member_base_pointer = parameters.llvm_builder.CreateGEP(
            llvm::Type::getInt8Ty(parameters.llvm_context),
            data_pointer,
            parameters.llvm_builder.getInt64(member_layout.block_offset),
            "soa_member_base_pointer"
        );

        llvm::Value* const typed_member_base_pointer = parameters.llvm_builder.CreateBitCast(
            member_base_pointer,
            member_llvm_type->getPointerTo(),
            "soa_typed_member_base_pointer"
        );

        return parameters.llvm_builder.CreateGEP(
            member_llvm_type,
            typed_member_base_pointer,
            index_value,
            "soa_member_element_pointer"
        );
    }

    static llvm::Value* create_runtime_aligned_soa_offset(
        llvm::Value* const offset_value,
        std::uint64_t const alignment,
        Expression_parameters const& parameters
    )
    {
        if (alignment <= 1)
            return offset_value;

        llvm::Value* const adjusted_offset = parameters.llvm_builder.CreateAdd(
            offset_value,
            parameters.llvm_builder.getInt64(alignment - 1),
            "soa_offset_adjusted"
        );

        return parameters.llvm_builder.CreateAnd(
            adjusted_offset,
            parameters.llvm_builder.getInt64(~(alignment - 1)),
            "soa_offset_aligned"
        );
    }

    static llvm::Value* create_soa_member_element_pointer(
        llvm::Value* const data_pointer,
        std::span<Soa_member_layout const> const member_layouts,
        std::size_t const member_index,
        llvm::Value* const length_value,
        llvm::Type* const member_llvm_type,
        llvm::Value* const index_value,
        Expression_parameters const& parameters
    )
    {
        if (member_index >= member_layouts.size())
            throw std::runtime_error{ format_error("Soa_array_view member index out of bounds.", parameters.source_position) };

        llvm::Value* block_offset = parameters.llvm_builder.getInt64(0);
        for (std::size_t previous_member_index = 0; previous_member_index < member_index; ++previous_member_index)
        {
            Soa_member_layout const& previous_member = member_layouts[previous_member_index];
            block_offset = create_runtime_aligned_soa_offset(block_offset, previous_member.element_alignment, parameters);

            llvm::Value* const block_size = parameters.llvm_builder.CreateMul(
                length_value,
                parameters.llvm_builder.getInt64(previous_member.element_size),
                "soa_member_block_size"
            );
            block_offset = parameters.llvm_builder.CreateAdd(block_offset, block_size, "soa_member_block_offset");
        }

        block_offset = create_runtime_aligned_soa_offset(block_offset, member_layouts[member_index].element_alignment, parameters);

        llvm::Value* const member_base_pointer = parameters.llvm_builder.CreateGEP(
            llvm::Type::getInt8Ty(parameters.llvm_context),
            data_pointer,
            block_offset,
            "soa_member_base_pointer"
        );

        llvm::Value* const typed_member_base_pointer = parameters.llvm_builder.CreateBitCast(
            member_base_pointer,
            member_llvm_type->getPointerTo(),
            "soa_typed_member_base_pointer"
        );

        return parameters.llvm_builder.CreateGEP(
            member_llvm_type,
            typed_member_base_pointer,
            index_value,
            "soa_member_element_pointer"
        );
    }

    static llvm::Value* load_soa_data_pointer(
        Value_and_type const& soa_value,
        Expression_parameters const& parameters
    )
    {
        if (!soa_value.type.has_value() || soa_value.value == nullptr)
            throw std::runtime_error{ format_error("Cannot load Soa_array data pointer.", parameters.source_position) };

        llvm::Type* const soa_llvm_type = type_reference_to_llvm_type(
            parameters.llvm_context,
            parameters.llvm_data_layout,
            soa_value.type.value(),
            parameters.type_database
        );

        llvm::Value* const data_pointer_field = parameters.llvm_builder.CreateStructGEP(soa_llvm_type, soa_value.value, 0);
        return create_load_instruction(
            parameters.llvm_builder,
            parameters.llvm_data_layout,
            llvm::Type::getInt8Ty(parameters.llvm_context)->getPointerTo(),
            data_pointer_field
        );
    }

    static llvm::Value* load_soa_array_view_field(
        Value_and_type const& soa_value,
        std::uint32_t const field_index,
        llvm::Type* const field_type,
        Expression_parameters const& parameters
    )
    {
        if (!soa_value.type.has_value() || soa_value.value == nullptr)
            throw std::runtime_error{ format_error("Cannot load Soa_array_view field.", parameters.source_position) };

        llvm::Type* const soa_llvm_type = type_reference_to_llvm_type(
            parameters.llvm_context,
            parameters.llvm_data_layout,
            soa_value.type.value(),
            parameters.type_database
        );

        llvm::Value* const field_pointer = parameters.llvm_builder.CreateStructGEP(soa_llvm_type, soa_value.value, field_index);
        return create_load_instruction(
            parameters.llvm_builder,
            parameters.llvm_data_layout,
            field_type,
            field_pointer
        );
    }

    static llvm::Value* load_soa_array_view_data_pointer(
        Value_and_type const& soa_value,
        Expression_parameters const& parameters
    )
    {
        return load_soa_array_view_field(
            soa_value,
            3,
            llvm::Type::getInt8Ty(parameters.llvm_context)->getPointerTo(),
            parameters
        );
    }

    static llvm::Value* load_soa_array_view_length(
        Value_and_type const& soa_value,
        Expression_parameters const& parameters
    )
    {
        return load_soa_array_view_field(
            soa_value,
            2,
            llvm::Type::getInt64Ty(parameters.llvm_context),
            parameters
        );
    }

    static llvm::Value* load_soa_array_view_start_index(
        Value_and_type const& soa_value,
        Expression_parameters const& parameters
    )
    {
        return load_soa_array_view_field(
            soa_value,
            0,
            llvm::Type::getInt64Ty(parameters.llvm_context),
            parameters
        );
    }

    static llvm::Value* create_soa_array_view_adjusted_index(
        Value_and_type const& soa_value,
        llvm::Value* const index_value,
        Expression_parameters const& parameters
    )
    {
        llvm::Value* const start_index = load_soa_array_view_start_index(soa_value, parameters);
        llvm::Value* const converted_index = parameters.llvm_builder.CreateIntCast(
            index_value,
            llvm::Type::getInt64Ty(parameters.llvm_context),
            false,
            "soa_index_i64"
        );

        return parameters.llvm_builder.CreateAdd(start_index, converted_index, "soa_adjusted_index");
    }

    static void store_soa_member_value(
        llvm::Value* const data_pointer,
        Soa_array_type_info const& soa_info,
        std::size_t const member_index,
        llvm::Value* const index_value,
        Value_and_type const& member_value,
        Expression_parameters const& parameters
    )
    {
        Type_reference const& member_type = soa_info.struct_declaration->member_types[member_index];
        llvm::Type* const member_llvm_type = type_reference_to_llvm_type(
            parameters.llvm_context,
            parameters.llvm_data_layout,
            member_type,
            parameters.type_database
        );

        llvm::Value* const destination_pointer = create_soa_member_element_pointer(
            data_pointer,
            soa_info.layout.members[member_index],
            member_llvm_type,
            index_value,
            parameters
        );

        llvm::Value* value_to_store = member_value.value;
        if (value_to_store != nullptr && value_to_store->getType()->isPointerTy())
            value_to_store = create_load_instruction(parameters.llvm_builder, parameters.llvm_data_layout, member_llvm_type, value_to_store);

        create_store_instruction(parameters.llvm_builder, parameters.llvm_data_layout, value_to_store, destination_pointer);
    }

    static void initialize_default_soa_array_storage(
        llvm::Value* const data_pointer,
        Soa_array_type_info const& soa_info,
        Expression_parameters const& parameters
    )
    {
        iris::Module const& struct_core_module =
            soa_info.module_name == parameters.core_module.name ?
            parameters.core_module :
            *parameters.core_module_dependencies.at(soa_info.module_name.data());

        for (std::uint64_t element_index = 0; element_index < soa_info.soa_array_type->size; ++element_index)
        {
            llvm::Value* const index_value = parameters.llvm_builder.getInt64(element_index);

            for (std::size_t member_index = 0; member_index < soa_info.struct_declaration->member_names.size(); ++member_index)
            {
                Expression_parameters new_parameters = set_core_module(parameters, struct_core_module);
                new_parameters.expression_type = soa_info.struct_declaration->member_types[member_index];

                Value_and_type const member_value = create_loaded_statement_value(
                    soa_info.struct_declaration->member_default_values[member_index],
                    new_parameters
                );

                store_soa_member_value(data_pointer, soa_info, member_index, index_value, member_value, parameters);
            }
        }
    }

    static void initialize_soa_array_storage_from_values(
        llvm::Value* const data_pointer,
        std::span<Value_and_type const> const element_values,
        Soa_array_type_info const& soa_info,
        Expression_parameters const& parameters
    )
    {
        llvm::Type* const element_llvm_type = type_reference_to_llvm_type(
            parameters.llvm_context,
            parameters.llvm_data_layout,
            *soa_info.element_type,
            parameters.type_database
        );

        llvm::AllocaInst* const element_alloca = create_alloca_instruction(
            parameters.llvm_builder,
            parameters.llvm_data_layout,
            *parameters.llvm_parent_function,
            element_llvm_type,
            "soa_element"
        );

        for (std::size_t element_index = 0; element_index < element_values.size(); ++element_index)
        {
            llvm::Value* const index_value = parameters.llvm_builder.getInt64(element_index);
            create_store_instruction(parameters.llvm_builder, parameters.llvm_data_layout, element_values[element_index].value, element_alloca);

            for (std::size_t member_index = 0; member_index < soa_info.struct_declaration->member_names.size(); ++member_index)
            {
                Value_and_type const member_value = generate_load_struct_member_instructions(
                    parameters.clang_module_data,
                    parameters.llvm_context,
                    parameters.llvm_builder,
                    parameters.llvm_data_layout,
                    element_alloca,
                    soa_info.struct_declaration->member_names[member_index],
                    soa_info.module_name,
                    *soa_info.struct_declaration,
                    parameters.type_database
                );

                store_soa_member_value(data_pointer, soa_info, member_index, index_value, member_value, parameters);
            }
        }
    }

    Value_and_type instantiate_array_slice(
        std::pmr::vector<iris::Type_reference> const& element_type,
        Value_and_type const& data_value,
        Value_and_type const& length_value,
        Expression_parameters const& parameters
    )
    {
        iris::Type_reference const array_slice_type_reference = iris::create_array_slice_type_reference(element_type, true);
        llvm::Type* const llvm_array_slice_type = type_reference_to_llvm_type(parameters.llvm_context, parameters.llvm_data_layout, array_slice_type_reference, parameters.type_database);

        llvm::AllocaInst* const struct_alloca = create_alloca_instruction(parameters.llvm_builder, parameters.llvm_data_layout, *parameters.llvm_parent_function, llvm_array_slice_type);

        iris::Struct_declaration const struct_declaration = iris::create_array_slice_type_struct_declaration(element_type);

        generate_store_struct_member_instructions(
            parameters.clang_module_data,
            parameters.llvm_context,
            parameters.llvm_builder,
            parameters.llvm_data_layout,
            struct_alloca,
            "data",
            "iris.builtin",
            struct_declaration,
            data_value,
            parameters.type_database
        );

        generate_store_struct_member_instructions(
            parameters.clang_module_data,
            parameters.llvm_context,
            parameters.llvm_builder,
            parameters.llvm_data_layout,
            struct_alloca,
            "length",
            "iris.builtin",
            struct_declaration,
            length_value,
            parameters.type_database
        );

        return Value_and_type
        {
            .name = "",
            .value = struct_alloca,
            .type = array_slice_type_reference
        };
    }

    Value_and_type allocate_stack_array(
        Call_expression const& call_expression,
        iris::Instance_call_expression const& instance_call_expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        Value_and_type const element_type_value = create_statement_value(instance_call_expression.arguments[0], parameters);
        llvm::Type* const element_llvm_type = type_reference_to_llvm_type(parameters.llvm_context, parameters.llvm_data_layout, element_type_value.type.value(), parameters.type_database);

        Value_and_type const array_length_value = create_loaded_expression_value(call_expression.arguments[0].expression_index, statement, parameters);
        
        Block_info& current_block = parameters.blocks.back();

        llvm::AllocaInst* const stack_array_alloca = create_alloca_dynamic_array_instruction(current_block.stack_save_pointer, parameters.llvm_builder, parameters.llvm_data_layout, parameters.llvm_module, element_llvm_type, "stack_array", array_length_value.value);
        Value_and_type const stack_array_value
        {
            .name = "",
            .value = stack_array_alloca,
            .type = create_pointer_type_type_reference({element_type_value.type.value()}, true)
        };

        return instantiate_array_slice(
            {element_type_value.type.value()},
            stack_array_value,
            array_length_value,
            parameters
        );
    }

    std::optional<Value_and_type> create_builtin_call_expression_value(
        Call_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        iris::Expression const& left_hand_side = statement.expressions[expression.expression.expression_index];

        if (std::holds_alternative<iris::Access_expression>(left_hand_side.data))
        {
            iris::Access_expression const& access_expression = std::get<iris::Access_expression>(left_hand_side.data);

            if (access_expression.member_name == "view")
            {
                if (expression.arguments.size() != 0 && expression.arguments.size() != 2)
                    throw std::runtime_error{ format_error("Soa_array.view() expects 0 or 2 arguments.", parameters.source_position) };

                Value_and_type const soa_value = create_expression_value(access_expression.expression.expression_index, statement, parameters);
                if (!soa_value.type.has_value())
                    throw std::runtime_error{ format_error("Cannot deduce Soa_array receiver type for view().", parameters.source_position) };

                std::optional<Type_reference> const underlying_receiver_type = get_underlying_type(
                    parameters.declaration_database,
                    soa_value.type.value()
                );
                if (!underlying_receiver_type.has_value() || !std::holds_alternative<Soa_array_type>(underlying_receiver_type->data))
                    throw std::runtime_error{ format_error("view() receiver must be Soa_array.", parameters.source_position) };

                Soa_array_type const& soa_array_type = std::get<Soa_array_type>(underlying_receiver_type->data);
                if (soa_array_type.value_type.empty())
                    throw std::runtime_error{ format_error("Soa_array value_type is not specified for view().", parameters.source_position) };

                bool is_view_mutable = false;
                if (parameters.expression_type.has_value() && std::holds_alternative<Soa_array_view_type>(parameters.expression_type->data))
                    is_view_mutable = std::get<Soa_array_view_type>(parameters.expression_type->data).is_mutable;

                Type_reference const soa_array_view_type_reference
                {
                    .data = Soa_array_view_type
                    {
                        .value_type = {soa_array_type.value_type[0]},
                        .is_mutable = is_view_mutable,
                    }
                };

                llvm::Type* const soa_view_llvm_type = type_reference_to_llvm_type(
                    parameters.llvm_context,
                    parameters.llvm_data_layout,
                    soa_array_view_type_reference,
                    parameters.type_database
                );

                if (parameters.llvm_parent_function == nullptr)
                    throw std::runtime_error{ format_error("Soa_array.view() requires a parent function.", parameters.source_position) };
                llvm::AllocaInst* const soa_view_alloca = create_alloca_instruction(
                    parameters.llvm_builder,
                    parameters.llvm_data_layout,
                    *parameters.llvm_parent_function,
                    soa_view_llvm_type,
                    "soa_array_view"
                );

                llvm::Value* const data_pointer = load_soa_data_pointer(soa_value, parameters);
                llvm::Value* const total_length = llvm::ConstantInt::get(llvm::Type::getInt64Ty(parameters.llvm_context), soa_array_type.size);

                llvm::Value* start_index_value = llvm::ConstantInt::get(llvm::Type::getInt64Ty(parameters.llvm_context), 0);
                llvm::Value* end_index_value = total_length;

                if (expression.arguments.size() == 2)
                {
                    Expression_parameters argument_parameters = parameters;
                    argument_parameters.expression_type = create_integer_type_type_reference(64, false);

                    Value_and_type const start_index_argument = create_loaded_expression_value(expression.arguments[0].expression_index, statement, argument_parameters);
                    Value_and_type const end_index_argument = create_loaded_expression_value(expression.arguments[1].expression_index, statement, argument_parameters);

                    start_index_value = start_index_argument.value;
                    end_index_value = end_index_argument.value;
                }

                llvm::Value* const start_index_field = parameters.llvm_builder.CreateStructGEP(soa_view_llvm_type, soa_view_alloca, 0);
                create_store_instruction(parameters.llvm_builder, parameters.llvm_data_layout, start_index_value, start_index_field);

                llvm::Value* const end_index_field = parameters.llvm_builder.CreateStructGEP(soa_view_llvm_type, soa_view_alloca, 1);
                create_store_instruction(parameters.llvm_builder, parameters.llvm_data_layout, end_index_value, end_index_field);

                llvm::Value* const length_field = parameters.llvm_builder.CreateStructGEP(soa_view_llvm_type, soa_view_alloca, 2);
                create_store_instruction(parameters.llvm_builder, parameters.llvm_data_layout, total_length, length_field);

                llvm::Value* const data_field = parameters.llvm_builder.CreateStructGEP(soa_view_llvm_type, soa_view_alloca, 3);
                create_store_instruction(parameters.llvm_builder, parameters.llvm_data_layout, data_pointer, data_field);

                return Value_and_type
                {
                    .name = "",
                    .value = soa_view_alloca,
                    .type = soa_array_view_type_reference
                };
            }
        }

        if (std::holds_alternative<iris::Instance_call_expression>(left_hand_side.data))
        {
            iris::Instance_call_expression const& instance_call_expression = std::get<iris::Instance_call_expression>(left_hand_side.data);

            iris::Expression const& instance_call_left_expression = statement.expressions[instance_call_expression.left_hand_side.expression_index];
            if (std::holds_alternative<iris::Variable_expression>(instance_call_left_expression.data))
            {
                iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(instance_call_left_expression.data);
                
                if (variable_expression.name == "create_stack_array_uninitialized")
                {
                    return allocate_stack_array(
                        expression,
                        instance_call_expression,
                        statement,
                        parameters
                    );
                }
                else if (variable_expression.name == "reinterpret_as")
                {
                    Value_and_type const loaded_value = create_loaded_expression_value(expression.arguments[0].expression_index, statement, parameters);
                    Value_and_type const destination_type_value = create_statement_value(instance_call_expression.arguments[0], parameters);

                    return Value_and_type
                    {
                        .name = "",
                        .value = loaded_value.value,
                        .type = destination_type_value.type
                    };
                }
                else if (variable_expression.name == "create_soa_array_view_from_pointer")
                {
                    if (expression.arguments.size() != 2)
                        throw std::runtime_error{ format_error("create_soa_array_view_from_pointer() expects two arguments!", parameters.source_position) };

                    Value_and_type const element_type_value = create_statement_value(instance_call_expression.arguments[0], parameters);

                    bool is_view_mutable = false;
                    if (parameters.expression_type.has_value() && std::holds_alternative<Soa_array_view_type>(parameters.expression_type->data))
                        is_view_mutable = std::get<Soa_array_view_type>(parameters.expression_type->data).is_mutable;

                    iris::Type_reference const soa_array_view_type_reference
                    {
                        .data = Soa_array_view_type
                        {
                            .value_type = {element_type_value.type.value()},
                            .is_mutable = is_view_mutable,
                        }
                    };

                    llvm::Type* const soa_view_llvm_type = type_reference_to_llvm_type(
                        parameters.llvm_context,
                        parameters.llvm_data_layout,
                        soa_array_view_type_reference,
                        parameters.type_database
                    );

                    llvm::AllocaInst* const soa_view_alloca = create_alloca_instruction(
                        parameters.llvm_builder,
                        parameters.llvm_data_layout,
                        *parameters.llvm_parent_function,
                        soa_view_llvm_type,
                        "soa_array_view"
                    );

                    Value_and_type const data_value = create_loaded_expression_value(expression.arguments[0].expression_index, statement, parameters);
                    Value_and_type const length_value = create_loaded_expression_value(expression.arguments[1].expression_index, statement, parameters);

                    llvm::Value* const start_index_field = parameters.llvm_builder.CreateStructGEP(soa_view_llvm_type, soa_view_alloca, 0);
                    create_store_instruction(parameters.llvm_builder, parameters.llvm_data_layout, parameters.llvm_builder.getInt64(0), start_index_field);

                    llvm::Value* const end_index_field = parameters.llvm_builder.CreateStructGEP(soa_view_llvm_type, soa_view_alloca, 1);
                    create_store_instruction(parameters.llvm_builder, parameters.llvm_data_layout, length_value.value, end_index_field);

                    llvm::Value* const length_field = parameters.llvm_builder.CreateStructGEP(soa_view_llvm_type, soa_view_alloca, 2);
                    create_store_instruction(parameters.llvm_builder, parameters.llvm_data_layout, length_value.value, length_field);

                    llvm::Value* const data_field = parameters.llvm_builder.CreateStructGEP(soa_view_llvm_type, soa_view_alloca, 3);
                    create_store_instruction(parameters.llvm_builder, parameters.llvm_data_layout, data_value.value, data_field);

                    return Value_and_type
                    {
                        .name = "",
                        .value = soa_view_alloca,
                        .type = soa_array_view_type_reference
                    };
                }
                else if (variable_expression.name == "calculate_soa_array_size_bytes")
                {
                    if (expression.arguments.size() != 1)
                        throw std::runtime_error{ format_error("calculate_soa_array_size_bytes() expects one argument!", parameters.source_position) };

                    Value_and_type const element_type_value = create_statement_value(instance_call_expression.arguments[0], parameters);

                    iris::Type_reference const& element_type = element_type_value.type.value();
                    iris::Custom_type_reference const* const custom_type_reference = std::get_if<iris::Custom_type_reference>(&element_type.data);
                    if (custom_type_reference == nullptr)
                        throw std::runtime_error{ format_error("calculate_soa_array_size_bytes() element type must be a struct type.", parameters.source_position) };

                    Soa_layout const layout = calculate_soa_layout(
                        parameters.llvm_data_layout,
                        parameters.type_database,
                        custom_type_reference->module_reference.name,
                        custom_type_reference->name,
                        1
                    );

                    Value_and_type const capacity_value = create_loaded_expression_value(expression.arguments[0].expression_index, statement, parameters);

                    llvm::Value* total_size = parameters.llvm_builder.getInt64(0);
                    for (Soa_member_layout const& member : layout.members)
                    {
                        total_size = create_runtime_aligned_soa_offset(total_size, member.element_alignment, parameters);
                        llvm::Value* const block_size = parameters.llvm_builder.CreateMul(
                            capacity_value.value,
                            parameters.llvm_builder.getInt64(member.element_size),
                            "soa_member_block_size"
                        );
                        total_size = parameters.llvm_builder.CreateAdd(total_size, block_size, "soa_member_block_offset");
                    }

                    return Value_and_type
                    {
                        .name = "",
                        .value = total_size,
                        .type = iris::create_integer_type_type_reference(64, true)
                    };
                }
            }
        }
        else if (std::holds_alternative<iris::Variable_expression>(left_hand_side.data))
        {
            iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(left_hand_side.data);
            
            if (variable_expression.name == "check")
            {
                if (expression.arguments.size() != 1)
                    throw std::runtime_error{ format_error("check() expects one argument!", parameters.source_position) };

                std::pmr::string const source_file_path =
                    parameters.core_module.source_file_path.has_value() ? 
                    std::pmr::string{parameters.core_module.source_file_path->generic_string()} :
                    std::pmr::string{};

                std::optional<Source_position> const statement_source_position = get_statement_source_position(statement);
                std::uint64_t const line = statement_source_position.has_value() ? statement_source_position->line : 0;

                if (parameters.debug_info != nullptr)
                    set_debug_location(parameters.llvm_builder, *parameters.debug_info, statement_source_position);
                
                iris::Function_pointer_type const function_pointer_type = create_test_check_function_pointer_type();
                
                llvm::FunctionType* const llvm_function_type = convert_to_llvm_function_type(
                    parameters.clang_module_data,
                    parameters.declaration_database,
                    function_pointer_type.type
                );

                std::pmr::vector<bool> const is_expression_address_of{false, false, false};
                llvm::Function* const llvm_function_callee = parameters.llvm_module.getFunction("iris_test_check");

                std::pmr::vector<llvm::Value*> llvm_arguments{parameters.temporaries_allocator};
                llvm_arguments.resize(3);

                std::pmr::vector<std::optional<Type_reference>> argument_types{parameters.temporaries_allocator};
                argument_types.resize(3);

                {
                    Expression_parameters new_parameters = parameters;
                    new_parameters.expression_type = iris::create_bool_type_reference();
                    new_parameters.source_position = statement_source_position;
                    Value_and_type const condition_value = create_expression_value(expression.arguments[0].expression_index, statement, new_parameters);
                    llvm_arguments[0] = condition_value.value;
                    argument_types[0] = condition_value.type;
                }
                                
                llvm_arguments[1] = create_c_string_constant(parameters.llvm_context, parameters.llvm_module, source_file_path, "iris_test_source_file_path");
                llvm_arguments[2] = llvm::ConstantInt::get(llvm::Type::getIntNTy(parameters.llvm_context, 64), line);

                Value_and_type result = create_call_expression_value_common(
                    is_expression_address_of,
                    argument_types,
                    llvm_function_callee,
                    llvm_function_type,
                    llvm_arguments,
                    function_pointer_type,
                    statement_source_position,
                    parameters
                );

                return result;
            }
            else if (variable_expression.name == "create_array_slice_from_pointer")
            {
                if (expression.arguments.size() != 2)
                    throw std::runtime_error{ format_error("create_array_slice_from_pointer() expects two arguments!", parameters.source_position) };

                Value_and_type const data_value = create_loaded_expression_value(expression.arguments[0].expression_index, statement, parameters);
                if (!data_value.type.has_value())
                    throw std::runtime_error{ format_error("Cannot find deduce argument 0 type of create_array_slice_from_pointer()", parameters.source_position) };

                std::optional<iris::Type_reference> element_type_optional = remove_pointer(data_value.type.value());
                std::pmr::vector<iris::Type_reference> const element_type = element_type_optional.has_value() ? std::pmr::vector<iris::Type_reference>{element_type_optional.value()} : std::pmr::vector<iris::Type_reference>{};

                Value_and_type const length_value = create_loaded_expression_value(expression.arguments[1].expression_index, statement, parameters);

                return instantiate_array_slice(
                    element_type,
                    data_value,
                    length_value,
                    parameters
                );
            }
            else if (variable_expression.name == "offset_pointer")
            {
                if (expression.arguments.size() != 2)
                    throw std::runtime_error{ format_error("offset_pointer() expects two arguments!", parameters.source_position) };

                Value_and_type const pointer_value = create_loaded_expression_value(expression.arguments[0].expression_index, statement, parameters);
                if (!pointer_value.type.has_value())
                    throw std::runtime_error{ format_error("Cannot find deduce argument 0 type of offset_pointer()", parameters.source_position) };

                if (!iris::is_pointer(pointer_value.type.value()))
                    throw std::runtime_error{ format_error("Argument 0 type of offset_pointer() must be a pointer!", parameters.source_position) };

                auto const get_type_alloc_size = [&]() -> std::uint64_t
                {
                    std::optional<iris::Type_reference> const value_type = remove_pointer(pointer_value.type.value());

                    // For *Void assume 1 byte offsets:
                    if (!value_type.has_value())
                        return 1;

                    llvm::Type* const value_llvm_type = type_reference_to_llvm_type(parameters.llvm_context, parameters.llvm_data_layout, value_type.value(), parameters.type_database);

                    std::uint64_t const alloc_size_in_bytes = parameters.llvm_data_layout.getTypeAllocSize(value_llvm_type);
                    return alloc_size_in_bytes;
                };

                std::uint64_t const alloc_size_in_bytes = get_type_alloc_size();
                llvm::Value* const alloc_size_in_bytes_value = llvm::ConstantInt::get(llvm::Type::getInt64Ty(parameters.llvm_context), alloc_size_in_bytes);
                Value_and_type const offset_value = create_loaded_expression_value(expression.arguments[1].expression_index, statement, parameters);

                llvm::Value* const total_offset_value = parameters.llvm_builder.CreateMul(alloc_size_in_bytes_value, offset_value.value);
                llvm::Value* const offseted_pointer_value = parameters.llvm_builder.CreatePtrAdd(pointer_value.value, total_offset_value);

                return Value_and_type
                {
                    .name = "",
                    .value = offseted_pointer_value,
                    .type = pointer_value.type
                };
            }
        }

        return std::nullopt;
    }

    static bool is_reinterpret_as_expression(
        iris::Statement const& statement,
        iris::Expression const& expression
    )
    {
        if (!std::holds_alternative<iris::Call_expression>(expression.data))
            return false;

        iris::Call_expression const& call_expression = std::get<iris::Call_expression>(expression.data);

        iris::Expression const& left_call_expression = statement.expressions[call_expression.expression.expression_index];
        if (!std::holds_alternative<iris::Instance_call_expression>(left_call_expression.data))
            return false;

        iris::Instance_call_expression const& instance_call_expression = std::get<iris::Instance_call_expression>(left_call_expression.data);

        iris::Expression const& instance_call_left_expression = statement.expressions[instance_call_expression.left_hand_side.expression_index];
        if (!std::holds_alternative<iris::Variable_expression>(instance_call_left_expression.data))
            return false;

        iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(instance_call_left_expression.data);
        return variable_expression.name == "reinterpret_as";
    }

    bool is_taking_address_of_expression(
        Statement const& statement,
        iris::Expression const& expression
    )
    {
        return iris::is_expression_address_of(expression) || iris::is_offset_pointer(statement, expression) || is_reinterpret_as_expression(statement, expression);
    }

    std::pmr::vector<bool> create_is_taking_address_of_expressions_array(
        Call_expression const& expression,
        Statement const& statement
    )
    {
        std::pmr::vector<bool> output;
        output.reserve(expression.arguments.size());
        
        for (unsigned i = 0; i < expression.arguments.size(); ++i)
        {
            iris::Expression const& argument_expression = statement.expressions[expression.arguments[i].expression_index];
            bool const is_taking_address_of = is_taking_address_of_expression(statement, argument_expression);
            output.push_back(is_taking_address_of);
        }

        return output;
    }

    Value_and_type create_call_expression_value(
        Call_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        if (parameters.llvm_parent_function == nullptr)
        {
            std::string position = "unknown";
            if (parameters.source_position.has_value())
            {
                position = std::format(
                    "{}:{}",
                    parameters.source_position->line,
                    parameters.source_position->column
                );
            }

            throw std::runtime_error{
                std::format(
                    "Can only create calls inside functions! module='{}' position='{}'",
                    parameters.core_module.name,
                    position
                )
            };
        }

        {
            std::optional<Value_and_type> const value = create_builtin_call_expression_value(expression, statement, parameters);
            if (value.has_value())
                return value.value();
        }

        std::pmr::polymorphic_allocator<> const& temporaries_allocator = parameters.temporaries_allocator;

        Value_and_type const left_hand_side = create_loaded_expression_value(expression.expression.expression_index, statement, parameters);
        std::optional<Type_reference> const resolved_lhs_type = left_hand_side.type.has_value() ? get_underlying_type(parameters.declaration_database, left_hand_side.type.value()) : std::nullopt;
        if (!resolved_lhs_type.has_value() || !std::holds_alternative<Function_pointer_type>(resolved_lhs_type.value().data))
            throw std::runtime_error{ format_error(std::format("Left hand side of call expression is not a function!"), parameters.source_position) };

        Function_pointer_type const& function_pointer_type = std::get<Function_pointer_type>(resolved_lhs_type.value().data);

        llvm::FunctionType* const llvm_function_type = convert_to_llvm_function_type(
            parameters.clang_module_data,
            parameters.declaration_database,
            function_pointer_type.type
        );

        std::pmr::vector<llvm::Value*> llvm_arguments{ temporaries_allocator };
        llvm_arguments.resize(expression.arguments.size());

        std::pmr::vector<std::optional<Type_reference>> argument_types{temporaries_allocator};
        argument_types.resize(expression.arguments.size());

        for (unsigned i = 0; i < expression.arguments.size(); ++i)
        {
            std::uint64_t const expression_index = expression.arguments[i].expression_index;

            Expression_parameters new_parameters = parameters;
            new_parameters.expression_type = i < function_pointer_type.type.input_parameter_types.size() ? function_pointer_type.type.input_parameter_types[i] : std::optional<Type_reference>{};

            Value_and_type const temporary = create_expression_value(expression_index, statement, new_parameters);

            std::size_t const output_index = i;
            llvm_arguments[output_index] = temporary.value;
            argument_types[output_index] = temporary.type;
        }

        std::pmr::vector<bool> const is_taking_address_of_array = create_is_taking_address_of_expressions_array(
            expression,
            statement
        );

        return create_call_expression_value_common(
            is_taking_address_of_array,
            argument_types,
            left_hand_side.value,
            llvm_function_type,
            llvm_arguments,
            function_pointer_type,
            parameters.source_position,
            parameters
        );
    }

    std::optional<llvm::Instruction::CastOps> get_cast_type(
        Type_reference const& source_core_type,
        llvm::Type const& source_llvm_type,
        Type_reference const& destination_core_type,
        llvm::Type const& destination_llvm_type,
        std::optional<Source_position> const& source_position
    )
    {
        if (source_llvm_type.isIntegerTy())
        {
            if (destination_llvm_type.isIntegerTy())
            {
                // Both are integers

                bool const is_source_larger = source_llvm_type.getIntegerBitWidth() > destination_llvm_type.getIntegerBitWidth();

                if (is_source_larger)
                {
                    return llvm::Instruction::CastOps::Trunc;
                }
                else
                {
                    if (is_signed_integer(source_core_type) && is_signed_integer(destination_core_type))
                        return llvm::Instruction::CastOps::SExt;
                    else
                        return llvm::Instruction::CastOps::ZExt;
                }
            }
            else if (destination_llvm_type.isHalfTy() || destination_llvm_type.isFloatTy() || destination_llvm_type.isDoubleTy())
            {
                // Source is integer, destination is floating point

                if (is_signed_integer(source_core_type))
                    return llvm::Instruction::CastOps::SIToFP;
                else
                    return llvm::Instruction::CastOps::UIToFP;
            }
        }
        else if (source_llvm_type.isHalfTy() || source_llvm_type.isFloatTy() || source_llvm_type.isDoubleTy())
        {
            if (destination_llvm_type.isIntegerTy())
            {
                // Source is floating point, destination is integer

                if (is_signed_integer(destination_core_type))
                    return llvm::Instruction::CastOps::FPToSI;
                else
                    return llvm::Instruction::CastOps::FPToUI;
            }
            else if (destination_llvm_type.isHalfTy() || destination_llvm_type.isFloatTy() || destination_llvm_type.isDoubleTy())
            {
                // Both are floating point

                bool const is_source_larger = source_llvm_type.getFPMantissaWidth() > destination_llvm_type.getFPMantissaWidth();

                if (is_source_larger)
                    return llvm::Instruction::CastOps::FPTrunc;
                else
                    return llvm::Instruction::CastOps::FPExt;
            }
        }
        else if (is_pointer(source_core_type) && is_pointer(destination_core_type))
        {
            return std::nullopt;
        }

        throw std::runtime_error{ format_error("Invalid cast!", source_position) };
    }

    static Value_and_type create_decimal_cast_expression_value(
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        Value_and_type const& source,
        Type_reference const& source_type,
        Type_reference const& destination_type,
        llvm::Type* const source_llvm_type,
        llvm::Type* const destination_llvm_type,
        std::optional<Source_position> const& source_position
    )
    {
        bool const source_is_decimal = is_decimal(source_type);
        bool const destination_is_decimal = is_decimal(destination_type);

        if (source_is_decimal && destination_is_decimal)
        {
            // Decimal(N) -> Decimal(M)
            std::uint32_t const source_scale = get_decimal_scale(source_type);
            std::uint32_t const destination_scale = get_decimal_scale(destination_type);
            std::uint32_t const source_bits = source_scale <= 6 ? 32 : 64;
            std::uint32_t const destination_bits = destination_scale <= 6 ? 32 : 64;
            std::uint32_t const wide_bits = std::max(source_bits, destination_bits) == 64 ? 128 : 64;

            llvm::Type* const wide_type = llvm::Type::getIntNTy(llvm_context, wide_bits);
            llvm::Value* const src_wide = llvm_builder.CreateSExt(source.value, wide_type);

            llvm::Value* result_wide = nullptr;
            if (destination_scale > source_scale)
            {
                // Scale up: multiply by 10^(dst-src)
                std::int64_t const ratio = compute_decimal_scale(destination_scale - source_scale);
                llvm::APInt const ratio_ap{ wide_bits, static_cast<std::uint64_t>(ratio), true };
                llvm::Value* const ratio_const = llvm::ConstantInt::get(wide_type, ratio_ap);
                result_wide = llvm_builder.CreateMul(src_wide, ratio_const);
            }
            else
            {
                // Scale down: divide by 10^(src-dst) — truncation toward zero
                std::int64_t const ratio = compute_decimal_scale(source_scale - destination_scale);
                llvm::APInt const ratio_ap{ wide_bits, static_cast<std::uint64_t>(ratio), true };
                llvm::Value* const ratio_const = llvm::ConstantInt::get(wide_type, ratio_ap);
                result_wide = llvm_builder.CreateSDiv(src_wide, ratio_const);
            }

            llvm::Type* const destination_llvm = llvm::Type::getIntNTy(llvm_context, destination_bits);
            llvm::Value* const result = llvm_builder.CreateTrunc(result_wide, destination_llvm);
            return { .name = "", .value = result, .type = destination_type };
        }
        else if (source_is_decimal)
        {
            // Decimal(N) -> Integer or Float
            std::uint32_t const source_scale = get_decimal_scale(source_type);
            std::int64_t const scale_val = compute_decimal_scale(source_scale);

            if (destination_llvm_type->isIntegerTy())
            {
                // Round half away from zero: adj = backing >= 0 ? +half : -half; result = (backing + adj) / scale
                std::uint32_t const backing_bits = source_scale <= 6 ? 32 : 64;
                std::uint32_t const wide_bits = backing_bits == 32 ? 64 : 128;
                llvm::Type* const wide_type = llvm::Type::getIntNTy(llvm_context, wide_bits);

                llvm::Value* const backing_wide = llvm_builder.CreateSExt(source.value, wide_type);
                llvm::Value* const zero_wide = llvm::ConstantInt::get(wide_type, 0);

                std::int64_t const half_scale = scale_val / 2;
                llvm::APInt const half_pos_ap{ wide_bits, static_cast<std::uint64_t>(half_scale), true };
                llvm::APInt const half_neg_ap{ wide_bits, static_cast<std::uint64_t>(-half_scale), true };
                llvm::Value* const half_pos = llvm::ConstantInt::get(wide_type, half_pos_ap);
                llvm::Value* const half_neg = llvm::ConstantInt::get(wide_type, half_neg_ap);

                llvm::Value* const is_non_neg = llvm_builder.CreateICmpSGE(backing_wide, zero_wide);
                llvm::Value* const adjustment = llvm_builder.CreateSelect(is_non_neg, half_pos, half_neg);
                llvm::Value* const adjusted = llvm_builder.CreateAdd(backing_wide, adjustment);

                llvm::APInt const scale_ap{ wide_bits, static_cast<std::uint64_t>(scale_val), true };
                llvm::Value* const scale_const = llvm::ConstantInt::get(wide_type, scale_ap);
                llvm::Value* const divided = llvm_builder.CreateSDiv(adjusted, scale_const);

                // Cast to destination integer size
                llvm::Value* const result = llvm_builder.CreateTrunc(divided, destination_llvm_type);
                return { .name = "", .value = result, .type = destination_type };
            }
            else if (destination_llvm_type->isHalfTy() || destination_llvm_type->isFloatTy() || destination_llvm_type->isDoubleTy())
            {
                // Decimal(N) -> Float: cast backing int to float, divide by float(10^N)
                llvm::Value* const backing_fp = llvm_builder.CreateSIToFP(source.value, destination_llvm_type);
                llvm::Value* const scale_fp = llvm::ConstantFP::get(destination_llvm_type, static_cast<double>(scale_val));
                llvm::Value* const result = llvm_builder.CreateFDiv(backing_fp, scale_fp);
                return { .name = "", .value = result, .type = destination_type };
            }
        }
        else
        {
            // Integer or Float -> Decimal(N)
            std::uint32_t const destination_scale = get_decimal_scale(destination_type);
            std::int64_t const scale_val = compute_decimal_scale(destination_scale);
            std::uint32_t const destination_bits = destination_scale <= 6 ? 32 : 64;
            llvm::Type* const destination_llvm = llvm::Type::getIntNTy(llvm_context, destination_bits);

            if (source_llvm_type->isIntegerTy())
            {
                // Integer -> Decimal(N): extend/trunc source, then multiply by 10^N
                llvm::Value* const src_extended = llvm_builder.CreateSExtOrTrunc(source.value, destination_llvm);
                llvm::APInt const scale_ap{ destination_bits, static_cast<std::uint64_t>(scale_val), true };
                llvm::Value* const scale_const = llvm::ConstantInt::get(destination_llvm, scale_ap);
                llvm::Value* const result = llvm_builder.CreateMul(src_extended, scale_const);
                return { .name = "", .value = result, .type = destination_type };
            }
            else if (source_llvm_type->isHalfTy() || source_llvm_type->isFloatTy() || source_llvm_type->isDoubleTy())
            {
                // Float -> Decimal(N): multiply by 10^N, round half away from zero, convert to int
                llvm::Value* const scale_fp = llvm::ConstantFP::get(source_llvm_type, static_cast<double>(scale_val));
                llvm::Value* const scaled_fp = llvm_builder.CreateFMul(source.value, scale_fp);

                // Round half away from zero using FP: add +0.5 or -0.5 based on sign, then truncate
                llvm::Value* const zero_fp = llvm::ConstantFP::get(source_llvm_type, 0.0);
                llvm::Value* const half_fp = llvm::ConstantFP::get(source_llvm_type, 0.5);
                llvm::Value* const neg_half_fp = llvm::ConstantFP::get(source_llvm_type, -0.5);
                llvm::Value* const is_non_neg = llvm_builder.CreateFCmpOGE(scaled_fp, zero_fp);
                llvm::Value* const adjustment = llvm_builder.CreateSelect(is_non_neg, half_fp, neg_half_fp);
                llvm::Value* const rounded_fp = llvm_builder.CreateFAdd(scaled_fp, adjustment);
                llvm::Value* const result = llvm_builder.CreateFPToSI(rounded_fp, destination_llvm);
                return { .name = "", .value = result, .type = destination_type };
            }
        }

        throw std::runtime_error{ format_error("Decimal cast not fully implemented for requested type combination.", source_position) };
    }

    Value_and_type create_cast_expression_value(
        Cast_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        llvm::LLVMContext& llvm_context = parameters.llvm_context;
        llvm::DataLayout const& llvm_data_layout = parameters.llvm_data_layout;
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;
        Type_database const& type_database = parameters.type_database;

        Value_and_type const source = create_loaded_expression_value(expression.source.expression_index, statement, parameters);
        if (!source.type.has_value())
            throw std::runtime_error{ format_error("Source type is void!", parameters.source_position) };

        std::optional<Type_reference> const source_type = get_underlying_type(parameters.declaration_database, source.type.value());
        std::optional<Type_reference> const destination_type = get_underlying_type(parameters.declaration_database, expression.destination_type);

        if (!source_type.has_value() || !destination_type.has_value())
            throw std::runtime_error{ format_error("Could not find underlyng source and/or destination types!", parameters.source_position) };
        if (source_type == destination_type)
        {
            return
            {
                .name = "",
                .value = source.value,
                .type = destination_type
            };
        }

        llvm::Type* const source_llvm_type = source.value->getType();
        llvm::Type* const destination_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, destination_type.value(), type_database);

        // Handle Decimal casts specially before falling through to the generic integer/float path.
        bool const source_is_decimal = is_decimal(source_type.value());
        bool const destination_is_decimal = is_decimal(destination_type.value());

        if (source_is_decimal || destination_is_decimal)
        {
            return create_decimal_cast_expression_value(
                llvm_context,
                llvm_builder,
                source,
                source_type.value(),
                destination_type.value(),
                source_llvm_type,
                destination_llvm_type,
                parameters.source_position
            );
        }

        std::optional<llvm::Instruction::CastOps> const cast_type = get_cast_type(source_type.value(), *source_llvm_type, destination_type.value(), *destination_llvm_type, parameters.source_position);
        if (!cast_type.has_value())
        {
            return
            {
                .name = "",
                .value = source.value,
                .type = destination_type
            };
        }

        llvm::Value* const cast_instruction = llvm_builder.CreateCast(cast_type.value(), source.value, destination_llvm_type);

        return
        {
            .name = "",
            .value = cast_instruction,
            .type = destination_type
        };
    }

    bool is_fundamental_type_signed(Fundamental_type const fundamental_type)
    {
        switch (fundamental_type)
        {
        case Fundamental_type::C_char:
        case Fundamental_type::C_schar:
        case Fundamental_type::C_short:
        case Fundamental_type::C_int:
        case Fundamental_type::C_long:
        case Fundamental_type::C_longlong: {
            return true;
        }
        case Fundamental_type::C_bool:
        case Fundamental_type::C_uchar:
        case Fundamental_type::C_ushort:
        case Fundamental_type::C_uint:
        case Fundamental_type::C_ulong:
        case Fundamental_type::C_ulonglong: {
            return false;
        }
        default: {
            return false;
        }
        }
    }

    Value_and_type create_constant_expression_value(
        Constant_expression const& expression,
        llvm::LLVMContext& llvm_context,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Module& llvm_module,
        Module const& core_module,
        Declaration_database const& declaration_database,
        Type_database const& type_database,
        std::optional<Source_position> const& source_position
    )
    {
        std::optional<Type_reference> const underlying_type_optional = get_underlying_type(declaration_database, expression.type);
        if (!underlying_type_optional.has_value())
            throw std::runtime_error{ format_error("Could not find underlying constant type!", source_position) };

        Type_reference const& type = underlying_type_optional.value();

        if (std::holds_alternative<Fundamental_type>(type.data))
        {
            Fundamental_type const fundamental_type = std::get<Fundamental_type>(type.data);

            switch (fundamental_type)
            {
            case Fundamental_type::Bool: {
                llvm::Type* const llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, type, type_database);

                std::uint8_t const data = expression.data == "true" ? 1 : 0;
                llvm::APInt const value{ 8, data, false };

                llvm::Value* const instruction = llvm::ConstantInt::get(llvm_type, value);

                return
                {
                    .name = "",
                    .value = instruction,
                    .type = type
                };
            }
            case Fundamental_type::Float16: {
                llvm::Type* const llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, type, type_database);

                char* end;
                float const value = std::strtof(expression.data.c_str(), &end);

                llvm::Value* const instruction = llvm::ConstantFP::get(llvm_type, value);

                return
                {
                    .name = "",
                    .value = instruction,
                    .type = type
                };
            }
            case Fundamental_type::Float32: {
                llvm::Type* const llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, type, type_database);

                char* end;
                float const value = std::strtof(expression.data.c_str(), &end);

                llvm::Value* const instruction = llvm::ConstantFP::get(llvm_type, value);

                return
                {
                    .name = "",
                    .value = instruction,
                    .type = type
                };
            }
            case Fundamental_type::Float64: {
                llvm::Type* const llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, type, type_database);

                char* end;
                double const value = std::strtod(expression.data.c_str(), &end);

                llvm::Value* const instruction = llvm::ConstantFP::get(llvm_type, value);

                return
                {
                    .name = "",
                    .value = instruction,
                    .type = type
                };
            }
            case Fundamental_type::C_char: {
                llvm::Type* const llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, type, type_database);

                std::string_view const char_data = expression.data;
                std::uint64_t char_value = 0;
                if (!char_data.empty() && char_data[0] == '\\' && char_data.size() == 2)
                {
                    switch (char_data[1])
                    {
                    case '\\': char_value = 92; break;
                    case '\'':  char_value = 39; break;
                    case 'n':  char_value = 10; break;
                    case 't':  char_value = 9;  break;
                    case 'r':  char_value = 13; break;
                    default:   char_value = static_cast<std::uint64_t>(static_cast<unsigned char>(char_data[1])); break;
                    }
                }
                else if (!char_data.empty())
                {
                    char_value = static_cast<std::uint64_t>(static_cast<unsigned char>(char_data[0]));
                }

                unsigned const number_of_bits = llvm_type->getIntegerBitWidth();
                llvm::APInt const char_int_value{ number_of_bits, char_value, true };

                llvm::Value* const instruction = llvm::ConstantInt::get(llvm_type, char_int_value);

                return
                {
                    .name = "",
                    .value = instruction,
                    .type = type
                };
            }
            case Fundamental_type::C_bool:
            case Fundamental_type::C_schar:
            case Fundamental_type::C_uchar:
            case Fundamental_type::C_short:
            case Fundamental_type::C_ushort:
            case Fundamental_type::C_int:
            case Fundamental_type::C_uint:
            case Fundamental_type::C_long:
            case Fundamental_type::C_ulong:
            case Fundamental_type::C_longlong:
            case Fundamental_type::C_ulonglong: {
                llvm::Type* const llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, type, type_database);

                char* end;
                std::uint64_t const data = std::strtoull(expression.data.c_str(), &end, 0);

                unsigned const number_of_bits = llvm_type->getIntegerBitWidth();
                bool const is_signed = is_fundamental_type_signed(fundamental_type);
                llvm::APInt const value{ number_of_bits, data, is_signed };

                llvm::Value* const instruction = llvm::ConstantInt::get(llvm_type, value);

                return
                {
                    .name = "",
                    .value = instruction,
                    .type = type
                };
            }
            default:
                break;
            }
        }
        else if (std::holds_alternative<Integer_type>(type.data))
        {
            Integer_type const& integer_type = std::get<Integer_type>(type.data);

            llvm::Type* const llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, type, type_database);

            char* end;
            std::uint64_t const data = std::strtoull(expression.data.c_str(), &end, 0);
            llvm::APInt const value{ integer_type.number_of_bits, data, integer_type.is_signed };

            llvm::Value* const instruction = llvm::ConstantInt::get(llvm_type, value);

            return
            {
                .name = "",
                .value = instruction,
                .type = type
            };
        }
        else if (std::holds_alternative<Decimal_type>(type.data))
        {
            Decimal_type const& decimal_type = std::get<Decimal_type>(type.data);
            std::uint32_t const bits = decimal_type.scale <= 6 ? 32 : 64;
            llvm::Type* const llvm_type = llvm::Type::getIntNTy(llvm_context, bits);

            // Compute 10^scale multiplier
            double multiplier = 1.0;
            for (std::uint32_t i = 0; i < decimal_type.scale; ++i)
                multiplier *= 10.0;

            // Parse the source decimal string and scale to backing integer
            char* end_ptr = nullptr;
            double const float_value = std::strtod(expression.data.c_str(), &end_ptr);

            // Round half away from zero: for positive add 0.5, for negative subtract 0.5
            double const scaled = float_value * multiplier;

            std::int64_t const min_value = bits < 64
                ? -(std::int64_t(1) << (bits - 1))
                : std::numeric_limits<std::int64_t>::min();
            std::int64_t const max_value = bits < 64
                ? (std::int64_t(1) << (bits - 1)) - 1
                : std::numeric_limits<std::int64_t>::max();
            if (scaled < static_cast<double>(min_value) || scaled > static_cast<double>(max_value))
                throw std::runtime_error{ format_error(
                    std::format("Decimal literal '{}' overflows the {}-bit backing integer (scaled value {:.0f} is out of range [{}, {}])",
                        expression.data, bits, scaled, min_value, max_value),
                    source_position) };

            std::int64_t const integer_value =
                scaled >= 0.0 ?
                static_cast<std::int64_t>(scaled + 0.5) :
                static_cast<std::int64_t>(scaled - 0.5);

            llvm::APInt const value{ bits, static_cast<std::uint64_t>(integer_value), true };
            llvm::Value* const instruction = llvm::ConstantInt::get(llvm_type, value);

            return
            {
                .name = "",
                .value = instruction,
                .type = type
            };
        }
        else if (is_c_string(type))
        {
            llvm::Value* const instruction = create_c_string_constant(llvm_context, llvm_module, expression.data);

            return
            {
                .name = "",
                .value = instruction,
                .type = type
            };
        }

        throw std::runtime_error{ format_error("Constant expression not handled!", source_position) };
    }

    static std::optional<Value_and_type> create_constant_array_with_constant_elements(
        std::span<Value_and_type const> const array_data_values,
        Type_reference const& element_type,
        llvm::ArrayType* const array_type,
        std::uint64_t const array_length
    )
    {
        for (std::size_t index = 0; index < array_data_values.size(); ++index)
        {
            if (!llvm::Constant::classof(array_data_values[index].value))
                return std::nullopt;
        }

        std::pmr::vector<llvm::Constant*> constant_values;
        constant_values.resize(array_data_values.size());
        for (std::size_t index = 0; index < array_data_values.size(); ++index)
            constant_values[index] = static_cast<llvm::Constant*>(array_data_values[index].value);

        return Value_and_type
        {
            .name = "",
            .value = llvm::ConstantArray::get(array_type, constant_values),
            .type = create_constant_array_type_reference({element_type}, array_length),
        };
    }

    Value_and_type create_constant_array_expression_value(
        Constant_array_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        llvm::LLVMContext& llvm_context = parameters.llvm_context;
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;
        llvm::DataLayout const& llvm_data_layout = parameters.llvm_data_layout;
        Module const core_module = parameters.core_module;
        Type_database const& type_database = parameters.type_database;

        if (parameters.expression_type.has_value() && std::holds_alternative<Soa_array_type>(parameters.expression_type->data))
        {
            Soa_array_type_info const soa_info = get_soa_array_type_info(parameters.expression_type.value(), parameters);
            if (parameters.llvm_parent_function == nullptr)
                throw std::runtime_error{ format_error("Soa_array global initialization is not implemented yet.", parameters.source_position) };

            if (soa_info.soa_array_type->size != expression.array_data.size() && !expression.array_data.empty())
                throw std::runtime_error{ format_error(std::format("Expected initializer list with size {} but got {} elements.", soa_info.soa_array_type->size, expression.array_data.size()), parameters.source_position) };

            auto const [soa_alloca, data_pointer] = create_soa_array_storage(
                parameters.expression_type.value(),
                soa_info.layout,
                parameters,
                "soa_array"
            );

            if (expression.array_data.empty())
            {
                initialize_default_soa_array_storage(data_pointer, soa_info, parameters);
            }
            else
            {
                std::pmr::vector<Value_and_type> element_values;
                element_values.resize(expression.array_data.size());

                for (std::size_t index = 0; index < expression.array_data.size(); ++index)
                {
                    Expression_parameters new_parameters = parameters;
                    new_parameters.expression_type = *soa_info.element_type;

                    element_values[index] = create_loaded_statement_value(
                        expression.array_data[index],
                        new_parameters
                    );

                    if (!element_values[index].type.has_value() || element_values[index].type.value() != *soa_info.element_type)
                        throw std::runtime_error{ format_error("Type mismatch between Soa_array initializer element and Soa_array element type.", parameters.source_position) };
                }

                initialize_soa_array_storage_from_values(data_pointer, element_values, soa_info, parameters);
            }

            return Value_and_type
            {
                .name = "",
                .value = soa_alloca,
                .type = parameters.expression_type.value(),
            };
        }

        if (parameters.expression_type.has_value() && std::holds_alternative<Soa_array_view_type>(parameters.expression_type->data))
        {
            if (parameters.llvm_parent_function == nullptr)
                throw std::runtime_error{ format_error("Soa_array_view global initialization is not implemented yet.", parameters.source_position) };

            llvm::Type* const soa_view_llvm_type = type_reference_to_llvm_type(
                llvm_context,
                llvm_data_layout,
                parameters.expression_type.value(),
                type_database
            );

            llvm::AllocaInst* const soa_view_alloca = create_alloca_instruction(
                llvm_builder,
                llvm_data_layout,
                *parameters.llvm_parent_function,
                soa_view_llvm_type,
                "soa_array_view"
            );

            std::uint64_t const view_size = llvm_data_layout.getTypeAllocSize(soa_view_llvm_type);
            llvm::Align const view_alignment = llvm_data_layout.getABITypeAlign(soa_view_llvm_type);
            create_memset_to_0_call(llvm_builder, soa_view_alloca, view_size, view_alignment);

            return Value_and_type
            {
                .name = "",
                .value = soa_view_alloca,
                .type = parameters.expression_type.value(),
            };
        }

        std::pmr::vector<Value_and_type> array_data_values;
        array_data_values.resize(expression.array_data.size());
        for (std::size_t index = 0; index < expression.array_data.size(); ++index)
        {
            array_data_values[index] = create_loaded_statement_value(
                expression.array_data[index],
                parameters
            );
        }

        if (!array_data_values.empty() && !array_data_values[0].type.has_value())
            throw std::runtime_error{ format_error("Could not deduce element type of initializer list.", parameters.source_position) };

        for (std::size_t index = 1; index < array_data_values.size(); ++index)
        {
            if (array_data_values[0].type != array_data_values[index].type)
                throw std::runtime_error{ format_error("Type mismatch between elements of the initializer list.", parameters.source_position) };
        }

        if (parameters.expression_type.has_value() && !is_array_slice_type_reference(parameters.expression_type.value()))
        {
            if (!std::holds_alternative<Constant_array_type>(parameters.expression_type->data))
                throw std::runtime_error{ format_error("Cannot assign initializer list to type.", parameters.source_position) };

            Constant_array_type const& requested_array_type = std::get<Constant_array_type>(parameters.expression_type->data);
            if (requested_array_type.size > 0 && expression.array_data.size() == 0 && !requested_array_type.value_type.empty())
            {
                Type_reference const& element_type = requested_array_type.value_type[0];
                llvm::Type* const llvm_element_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, element_type, type_database);
                std::uint64_t const array_length = requested_array_type.size;

                std::uint64_t const element_alloc_size_in_bytes = parameters.llvm_data_layout.getTypeAllocSize(llvm_element_type);
                llvm::Align const alignment = parameters.llvm_data_layout.getABITypeAlign(llvm_element_type);
                std::uint64_t const array_alloc_size_in_bytes = array_length*element_alloc_size_in_bytes;

                llvm::ArrayType* const array_type = llvm::ArrayType::get(llvm_element_type, array_length);
                llvm::ConstantInt* const array_length_constant = llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvm_context), array_length);

                llvm::AllocaInst* const array_alloca = create_alloca_instruction(llvm_builder, llvm_data_layout, *parameters.llvm_parent_function, array_type, "array", array_length_constant);
                create_memset_to_0_call(parameters.llvm_builder, array_alloca, array_alloc_size_in_bytes, alignment);

                return Value_and_type
                {
                    .name = "",
                    .value = array_alloca,
                    .type = create_constant_array_type_reference({element_type}, array_length),
                };
            }

            if (requested_array_type.size != expression.array_data.size())
                throw std::runtime_error{ format_error(std::format("Expected initializer list with size {} but got {} elements.", requested_array_type.size, expression.array_data.size()), parameters.source_position) };
        }
        
        if (expression.array_data.empty())
        {
            llvm::Type* const llvm_int32_type = llvm::Type::getInt32Ty(llvm_context);
            llvm::ArrayType* const llvm_array_type = llvm::ArrayType::get(llvm_int32_type, 0);
            llvm::Value* const llvm_undef_array = llvm::UndefValue::get(llvm_array_type);

            Value_and_type const constant_array_value
            {
                .name = "",
                .value = llvm_undef_array,
                .type = create_constant_array_type_reference({create_integer_type_type_reference(32, true)}, 0),
            };

            return convert_to_expected_type_if_needed(
                constant_array_value,
                parameters
            );
        }

        Type_reference const& element_type = *array_data_values[0].type;
        llvm::Type* const llvm_element_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, element_type, type_database);
        std::uint64_t const array_length = expression.array_data.size();

        llvm::ArrayType* const array_type = llvm::ArrayType::get(llvm_element_type, array_length);
        
        if (parameters.llvm_parent_function == nullptr)
        {
            std::optional<Value_and_type> const constant_array = create_constant_array_with_constant_elements(array_data_values, element_type, array_type, array_length);
            if (constant_array.has_value())
                return constant_array.value();
        }
    
        llvm::ConstantInt* const array_length_constant = llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvm_context), array_length);
        llvm::AllocaInst* const array_alloca = create_alloca_instruction(llvm_builder, llvm_data_layout, *parameters.llvm_parent_function, array_type, "array", array_length_constant);

        for (std::uint64_t index = 0; index < array_length; ++index)
        {
            llvm::Value* const index_value = llvm_builder.getInt32(index);
            llvm::Value* const element_pointer = llvm_builder.CreateGEP(array_type, array_alloca, {llvm_builder.getInt32(0), index_value}, "array_element_pointer");

            llvm::Value* const value = array_data_values[index].value;
            create_store_instruction(llvm_builder, llvm_data_layout, value, element_pointer);
        }

        Value_and_type const constant_array_value
        {
            .name = "",
            .value = array_alloca,
            .type = create_constant_array_type_reference({element_type}, array_length),
        };

        return convert_to_expected_type_if_needed(
            constant_array_value,
            parameters
        );
    }

    Value_and_type create_continue_expression_value(
        Continue_expression const& continue_expression,
        llvm::IRBuilder<>& llvm_builder,
        std::span<Block_info const> const block_infos,
        Expression_parameters const& parameters
    )
    {
        std::array<Block_type, 2> const target_block_types
        {
            Block_type::For_loop,
            Block_type::While_loop,
        };
        std::pair<Block_info const*, Blocks_to_pop_count> const target_block_result = 
            find_target_block(block_infos, 1, target_block_types, parameters.source_position);
        llvm::BasicBlock* const target_block = target_block_result.first->repeat_block;
        Blocks_to_pop_count const blocks_to_pop_count = target_block_result.second;

        create_instructions_pop_blocks(parameters, blocks_to_pop_count);

        if (parameters.debug_info != nullptr)
            set_debug_location(parameters.llvm_builder, *parameters.debug_info, parameters.source_position);

        llvm_builder.CreateBr(target_block);

        return Value_and_type
        {
            .name = "",
            .value = nullptr,
            .type = std::nullopt
        };
    }

    Value_and_type create_dereference_and_access_expression_value(
        Dereference_and_access_expression const& dereference_and_access_expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        Value_and_type const left_hand_side_expression = create_expression_value(dereference_and_access_expression.expression.expression_index, statement, parameters);

        if (left_hand_side_expression.value != nullptr && left_hand_side_expression.type.has_value())
        {
            Type_reference const& type_reference = left_hand_side_expression.type.value();
            if (is_non_void_pointer(type_reference))
            {
                std::optional<Type_reference> const value_type = remove_pointer(type_reference);
                if (value_type.has_value())
                {
                    llvm::Value* const load_address = create_load_instruction(parameters.llvm_builder, parameters.llvm_data_layout, llvm::PointerType::get(parameters.llvm_context, 0), left_hand_side_expression.value);
                    Value_and_type const loaded_left_hand_side
                    {
                        .name = "",
                        .value = load_address,
                        .type = value_type.value()
                    };

                    if (std::holds_alternative<Custom_type_reference>(value_type.value().data))
                    {
                        Custom_type_reference const& custom_type_reference = std::get<Custom_type_reference>(value_type.value().data);

                        std::string_view const module_name = find_module_name(parameters.core_module, custom_type_reference.module_reference);
                        std::string_view const declaration_name = custom_type_reference.name;
            
                        std::optional<Declaration> const declaration = find_underlying_declaration(parameters.declaration_database, module_name, declaration_name);
            
                        if (declaration.has_value())
                        {
                            Declaration const& declaration_value = declaration.value();
            
                            if (std::holds_alternative<Struct_declaration const*>(declaration_value.data))
                            {
                                Struct_declaration const& struct_declaration = *std::get<Struct_declaration const*>(declaration_value.data);
                                return create_access_struct_member(
                                    loaded_left_hand_side,
                                    dereference_and_access_expression.member_name,
                                    module_name,
                                    struct_declaration,
                                    parameters
                                );
                            }
                            else if (std::holds_alternative<Union_declaration const*>(declaration_value.data))
                            {
                                Union_declaration const& union_declaration = *std::get<Union_declaration const*>(declaration_value.data);
                                return create_access_union_member(
                                    loaded_left_hand_side,
                                    dereference_and_access_expression.member_name,
                                    module_name,
                                    union_declaration,
                                    parameters
                                );
                            }
                        }
                    }
                }
            }
        }

        throw std::runtime_error{ format_error("Could not create dereference and access expression value!", parameters.source_position) };
    }

    Value_and_type create_for_loop_expression_value(
        For_loop_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        if (parameters.llvm_parent_function == nullptr)
            throw std::runtime_error{ format_error("Can only create for loops inside functions!", parameters.source_position) };

        llvm::LLVMContext& llvm_context = parameters.llvm_context;
        llvm::DataLayout const& llvm_data_layout = parameters.llvm_data_layout;
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;
        llvm::Module& llvm_module = parameters.llvm_module;
        llvm::Function* const llvm_parent_function = parameters.llvm_parent_function;
        Module const& core_module = parameters.core_module;
        Type_database const& type_database = parameters.type_database;
        std::span<Block_info const> block_infos = parameters.blocks;
        std::span<Value_and_type const> const local_variables = parameters.local_variables;

        if (parameters.debug_info != nullptr && parameters.source_position.has_value())
            push_debug_lexical_block_scope(*parameters.debug_info, *parameters.source_position);

        Value_and_type const& range_begin_temporary = create_loaded_expression_value(expression.range_begin.expression_index, statement, parameters);

        if (parameters.debug_info != nullptr)
            set_debug_location(parameters.llvm_builder, *parameters.debug_info, parameters.source_position);

        // Loop variable declaration:
        Type_reference const& variable_type = range_begin_temporary.type.value();
        llvm::Type* const variable_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, variable_type, type_database);
        llvm::AllocaInst* const variable_alloca = create_alloca_instruction(llvm_builder, llvm_data_layout, *parameters.llvm_parent_function, variable_llvm_type, expression.variable_name.c_str());
        if (parameters.debug_info != nullptr)
            create_local_variable_debug_description(*parameters.debug_info, parameters, expression.variable_name.c_str(), variable_alloca, variable_type);
        create_store_instruction(llvm_builder, llvm_data_layout, range_begin_temporary.value, variable_alloca);
        Value_and_type const variable_value = { .name = expression.variable_name, .value = variable_alloca, .type = variable_type };

        llvm::BasicBlock* const condition_block = llvm::BasicBlock::Create(llvm_context, "for_loop_condition", llvm_parent_function);
        llvm::BasicBlock* const then_block = llvm::BasicBlock::Create(llvm_context, "for_loop_then", llvm_parent_function);
        llvm::BasicBlock* const update_index_block = llvm::BasicBlock::Create(llvm_context, "for_loop_update_index", llvm_parent_function);
        llvm::BasicBlock* const after_block = llvm::BasicBlock::Create(llvm_context, "for_loop_after", llvm_parent_function);

        llvm_builder.CreateBr(condition_block);

        // Loop condition:
        {
            llvm_builder.SetInsertPoint(condition_block);

            Value_and_type const& range_end_value = create_loaded_statement_value(
                expression.range_end,
                parameters
            );

            if (parameters.debug_info != nullptr)
                set_debug_location(parameters.llvm_builder, *parameters.debug_info, parameters.source_position);

            Value_and_type const loaded_variable_value
            {
                .name = expression.variable_name,
                .value = create_load_instruction(llvm_builder, llvm_data_layout, variable_llvm_type, variable_alloca),
                .type = variable_type,
            };

            Binary_operation const compare_operation = expression.range_comparison_operation;
            Value_and_type const condition_value = create_binary_operation_instruction(llvm_builder, loaded_variable_value, range_end_value, compare_operation, parameters.declaration_database, parameters.source_position);

            llvm_builder.CreateCondBr(condition_value.value, then_block, after_block);
        }

        // Loop body:
        {
            llvm_builder.SetInsertPoint(then_block);

            std::pmr::vector<Value_and_type> all_local_variables{ local_variables.begin(), local_variables.end() };
            all_local_variables.push_back(variable_value);

            std::pmr::vector<Block_info> all_block_infos{ block_infos.begin(), block_infos.end() };
            all_block_infos.push_back(Block_info{ .block_type = Block_type::For_loop, .repeat_block = update_index_block, .after_block = after_block });

            std::pmr::vector<std::pmr::vector<Statement>> defer_expressions_per_block = create_defer_block(parameters.defer_expressions_per_block);

            Expression_parameters new_parameters = parameters;
            new_parameters.local_variables = all_local_variables;
            new_parameters.blocks = all_block_infos;
            new_parameters.defer_expressions_per_block = defer_expressions_per_block;

            create_statement_values(
                expression.then_statements,
                new_parameters,
                true
            );

            if (!ends_with_terminator_statement(expression.then_statements))
                llvm_builder.CreateBr(update_index_block);
        }

        // Update loop variable:
        {
            llvm_builder.SetInsertPoint(update_index_block);

            Constant_expression const default_step_constant
            {
                .type = variable_type,
                .data =
                    (expression.range_comparison_operation == Binary_operation::Less_than) || (expression.range_comparison_operation == Binary_operation::Less_than_or_equal_to) ?
                    "1" :
                    "-1"
            };

            Value_and_type const step_by_value =
                expression.step_by.has_value() ?
                create_loaded_expression_value(expression.step_by.value().expression_index, statement, parameters) :
                create_constant_expression_value(default_step_constant, llvm_context, llvm_data_layout, llvm_module, core_module, parameters.declaration_database, type_database, parameters.source_position);

            if (parameters.debug_info != nullptr)
                set_debug_location(parameters.llvm_builder, *parameters.debug_info, parameters.source_position);

            llvm::Value* const loaded_value_value = create_load_instruction(llvm_builder, llvm_data_layout, variable_llvm_type, variable_value.value);
            llvm::Value* new_variable_value = llvm_builder.CreateAdd(loaded_value_value, step_by_value.value);
            create_store_instruction(llvm_builder, llvm_data_layout, new_variable_value, variable_value.value);

            llvm_builder.CreateBr(condition_block);
        }

        if (parameters.debug_info != nullptr && parameters.source_position.has_value())
            pop_debug_scope(*parameters.debug_info);

        // After the loop:
        llvm_builder.SetInsertPoint(after_block);

        return Value_and_type
        {
            .name = "",
            .value = nullptr,
            .type = std::nullopt
        };
    }

    Value_and_type create_if_expression_value(
        If_expression const& expression,
        Expression_parameters const& parameters
    )
    {
        llvm::LLVMContext& llvm_context = parameters.llvm_context;
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;
        llvm::Function* const llvm_parent_function = parameters.llvm_parent_function;
        std::span<Block_info const> block_infos = parameters.blocks;

        auto const calculate_number_of_blocks = [](std::span<Condition_statement_pair const> const series) -> std::uint32_t
        {
            if (series.size() == 1)
                return 2;

            Condition_statement_pair const& last_serie = series.back();
            bool const is_else_if = last_serie.condition.has_value();

            std::uint32_t const blocks_except_last = 2 * (series.size() - 1);
            std::uint32_t const last = is_else_if ? 2 : 1;
            std::uint32_t const total = blocks_except_last + last;
            return total;
        };

        auto get_block_name = [](std::size_t const index, std::size_t const last_index) -> std::string
        {
            if (index == 0)
                return std::format("if_s{}_then", index);
            else if (index == last_index)
                return std::format("if_s{}_after", index);
            else if (index % 2 != 0)
                return std::format("if_s{}_else", index);
            else
                return std::format("if_s{}_then", index);
        };

        std::uint32_t const number_of_blocks = calculate_number_of_blocks(expression.series);

        std::pmr::vector<llvm::BasicBlock*> blocks;
        blocks.resize(number_of_blocks);

        for (std::size_t index = 0; index < blocks.size(); ++index)
        {
            std::string const block_name = get_block_name(index, blocks.size() - 1);
            blocks[index] = llvm::BasicBlock::Create(llvm_context, block_name, llvm_parent_function);
        }

        llvm::BasicBlock* const end_if_block = blocks.back();

        for (std::size_t serie_index = 0; serie_index < expression.series.size(); ++serie_index)
        {
            Condition_statement_pair const& serie = expression.series[serie_index];

            std::pmr::vector<Block_info> all_block_infos{ block_infos.begin(), block_infos.end() };
            all_block_infos.push_back(Block_info{ .block_type = Block_type::If });
            std::pmr::vector<std::pmr::vector<Statement>> defer_expressions_per_block = create_defer_block(parameters.defer_expressions_per_block);

            Expression_parameters then_block_parameters = parameters;
            then_block_parameters.blocks = all_block_infos;
            then_block_parameters.defer_expressions_per_block = defer_expressions_per_block;

            // if: current, then, end_if
            // if,else_if: current, then, else, then, end_if
            // if,else: current, then, else, end_if
            // if,else_if,else: current, then, else, then, else, end_if

            if (serie.condition.has_value())
            {
                if (parameters.debug_info != nullptr)
                    set_debug_location_at_statement(parameters.llvm_builder, *parameters.debug_info, serie.condition.value());

                Value_and_type const& condition_value = create_loaded_statement_value(
                    serie.condition.value(),
                    parameters
                );

                std::size_t const block_index = 2 * serie_index;
                llvm::BasicBlock* const then_block = blocks[block_index];
                llvm::BasicBlock* const else_block = blocks[block_index + 1];

                llvm::Value* const condition_converted_value = convert_to_boolean(llvm_context, llvm_builder, condition_value.value, condition_value.type);

                llvm_builder.CreateCondBr(condition_converted_value, then_block, else_block);

                llvm_builder.SetInsertPoint(then_block);

                if (parameters.debug_info != nullptr && serie.block_source_range.has_value())
                    push_debug_lexical_block_scope(*parameters.debug_info, serie.block_source_range->start);

                create_statement_values(
                    serie.then_statements,
                    then_block_parameters,
                    true
                );

                if (!ends_with_terminator_statement(serie.then_statements))
                    llvm_builder.CreateBr(end_if_block);

                if (parameters.debug_info != nullptr && serie.block_source_range.has_value())
                    pop_debug_scope(*parameters.debug_info);

                llvm_builder.SetInsertPoint(else_block);
            }
            else
            {
                if (parameters.debug_info != nullptr && serie.block_source_range.has_value())
                    push_debug_lexical_block_scope(*parameters.debug_info, serie.block_source_range->start);

                create_statement_values(
                    serie.then_statements,
                    then_block_parameters,
                    true
                );

                if (!ends_with_terminator_statement(serie.then_statements))
                    llvm_builder.CreateBr(end_if_block);

                if (parameters.debug_info != nullptr && serie.block_source_range.has_value())
                    pop_debug_scope(*parameters.debug_info);

                llvm_builder.SetInsertPoint(end_if_block);
            }
        }

        return Value_and_type
        {
            .name = "",
            .value = end_if_block,
            .type = std::nullopt
        };
    }

    static llvm::Value* convert_constant(
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::Type* const storage_type,
        unsigned const storage_size_bits,
        llvm::Value* const source_value,
        llvm::Type* const source_type,
        unsigned const source_size_bits
    )
    {
        if (source_size_bits == storage_size_bits)
            return llvm_builder.CreateBitCast(source_value, storage_type);

        if (source_type->isIntegerTy())
        {
            llvm::Type* const destination_type = llvm::Type::getIntNTy(llvm_context, storage_size_bits < 64 ? storage_size_bits : 64);
            if (source_size_bits < storage_size_bits)
            {
                return llvm_builder.CreateZExt(source_value, destination_type);
            }
            else
            {
                return llvm_builder.CreateTrunc(source_value, destination_type);
            }
        }

        if (llvm::StructType* struct_type = llvm::dyn_cast<llvm::StructType>(source_type))
        {
            if (struct_type->getNumElements() == 1)
            {
                llvm::Value* const inner = llvm_builder.CreateExtractValue(source_value, {0});
                llvm::Type* const inner_type = inner->getType();
                return convert_constant(llvm_context, llvm_builder, storage_type, storage_size_bits, inner, inner_type, source_size_bits);
            }
        }

        return llvm_builder.CreateBitCast(source_value, storage_type);
    }

    static llvm::Value* create_struct_instance_constant_value(
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        Statement const& statement,
        Instantiate_expression const& expression,
        Expression_parameters const& parameters,
        Struct_declaration const& struct_declaration,
        llvm::Type* const llvm_struct_type
    )
    {
        // If there are no explicit member values we can return a
        // zero‑initialized aggregate immediately.  This covers both the
        // "default" and "zero_initialized"/"uninitialized" cases at
        // compile time because globals are guaranteed to be zeroed by
        // the loader.
        if (expression.members.empty())
        {
            return llvm::Constant::getNullValue(llvm_struct_type);
        }

        // Build up the structure value by inserting one element at a time.
        llvm::Value* struct_value = llvm::UndefValue::get(llvm_struct_type);

        for (std::size_t member_index = 0; member_index < struct_declaration.member_names.size(); ++member_index)
        {
            std::string_view const member_name = struct_declaration.member_names[member_index];
            Type_reference const& member_type = struct_declaration.member_types[member_index];
            llvm::Type* const storage_type = llvm_struct_type->getStructElementType(member_index);

            // Try to find an explicit initializer for this member.
            llvm::Value* element_value = nullptr;
            auto const it = std::find_if(
                expression.members.begin(),
                expression.members.end(),
                [&](Instantiate_member_value_pair const& pair) { return pair.member_name == member_name; }
            );

            if (it != expression.members.end())
            {
                Expression_parameters new_parameters = parameters;
                new_parameters.expression_type = member_type;
                Value_and_type const member_value = create_expression_value(it->value.expression_index, statement, new_parameters);

                llvm::Value* const source_value = member_value.value;
                llvm::Type* const source_type = source_value->getType();

                unsigned const storage_size_bits = parameters.llvm_data_layout.getTypeSizeInBits(storage_type);
                unsigned const source_size_bits = parameters.llvm_data_layout.getTypeSizeInBits(source_type);

                element_value = convert_constant(llvm_context, llvm_builder, storage_type, storage_size_bits, source_value, source_type, source_size_bits);
            }
            else
            {
                // No initializer for this member -> zero initialize.
                element_value = llvm::Constant::getNullValue(storage_type);
            }

            struct_value = llvm_builder.CreateInsertValue(struct_value, element_value, member_index);
        }

        return struct_value;
    }

    Value_and_type create_instantiate_struct_expression_value(
        Statement const& statement,
        Instantiate_expression const& expression,
        Expression_parameters const& parameters,
        std::string_view const module_name,
        Struct_declaration const& struct_declaration,
        Type_reference const& struct_type_reference
    )
    {
        llvm::LLVMContext& llvm_context = parameters.llvm_context;
        llvm::DataLayout const& llvm_data_layout = parameters.llvm_data_layout;
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;
        Type_database const& type_database = parameters.type_database;

        llvm::Type* const llvm_struct_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, struct_type_reference, type_database);

        if (parameters.llvm_parent_function == nullptr)
        {
            llvm::Value* const struct_instance = create_struct_instance_constant_value(
                llvm_context,
                llvm_builder,
                statement,
                expression,
                parameters,
                struct_declaration,
                llvm_struct_type
            );

            return Value_and_type
            {
                .name = "",
                .value = struct_instance,
                .type = struct_type_reference
            };
        }

        llvm::AllocaInst* const struct_alloca = create_alloca_instruction(llvm_builder, llvm_data_layout, *parameters.llvm_parent_function, llvm_struct_type);

        if (expression.type == Instantiate_expression_type::Default)
        {
            for (std::size_t member_index = 0; member_index < struct_declaration.member_names.size(); ++member_index)
            {
                std::string_view const member_name = struct_declaration.member_names[member_index];
                Type_reference const& member_type = struct_declaration.member_types[member_index];

                auto const expression_pair_location = std::find_if(expression.members.begin(), expression.members.end(), [member_name](Instantiate_member_value_pair const& pair) { return pair.member_name == member_name; });

                if (expression_pair_location != expression.members.end())
                {
                    Expression_parameters new_parameters = parameters;
                    new_parameters.expression_type = member_type;
                    Value_and_type const member_value = create_loaded_expression_value(expression_pair_location->value.expression_index, statement, new_parameters);

                    generate_store_struct_member_instructions(
                        parameters.clang_module_data,
                        parameters.llvm_context,
                        parameters.llvm_builder,
                        parameters.llvm_data_layout,
                        struct_alloca,
                        member_name,
                        module_name,
                        struct_declaration,
                        member_value,
                        parameters.type_database
                    );
                }
                else
                {
                    iris::Module const& struct_core_module =
                        module_name == parameters.core_module.name ?
                        parameters.core_module :
                        *parameters.core_module_dependencies.at(module_name.data());
                    Expression_parameters new_parameters = set_core_module(parameters, struct_core_module);
                    new_parameters.expression_type = member_type;

                    Value_and_type const member_value = create_loaded_statement_value(struct_declaration.member_default_values[member_index], new_parameters);

                    generate_store_struct_member_instructions(
                        parameters.clang_module_data,
                        parameters.llvm_context,
                        parameters.llvm_builder,
                        parameters.llvm_data_layout,
                        struct_alloca,
                        member_name,
                        module_name,
                        struct_declaration,
                        member_value,
                        parameters.type_database
                    );
                }
            }

            return Value_and_type
            {
                .name = "",
                .value = struct_alloca,
                .type = struct_type_reference
            };
        }
        else if (expression.type == Instantiate_expression_type::Explicit)
        {
            for (std::size_t member_index = 0; member_index < struct_declaration.member_names.size(); ++member_index)
            {
                std::string_view const member_name = struct_declaration.member_names[member_index];
                Type_reference const& member_type = struct_declaration.member_types[member_index];

                if (member_index >= expression.members.size())
                    throw std::runtime_error{ format_error(std::format("The struct member '{}' of struct '{}.{}' is not explicitly initialized!", member_name, module_name, struct_declaration.name), parameters.source_position) };

                Instantiate_member_value_pair const& pair = expression.members[member_index];

                if (pair.member_name != member_name)
                    throw std::runtime_error{ format_error(std::format("Expected struct member '{}' of struct '{}.{}' instead of '{}' while instantiating struct!", member_name, module_name, struct_declaration.name, pair.member_name), parameters.source_position) };

                Expression_parameters new_parameters = parameters;
                new_parameters.expression_type = member_type;
                Value_and_type const member_value = create_loaded_expression_value(pair.value.expression_index, statement, new_parameters);

                generate_store_struct_member_instructions(
                    parameters.clang_module_data,
                    parameters.llvm_context,
                    parameters.llvm_builder,
                    parameters.llvm_data_layout,
                    struct_alloca,
                    member_name,
                    module_name,
                    struct_declaration,
                    member_value,
                    parameters.type_database
                );
            }

            return Value_and_type
            {
                .name = "",
                .value = struct_alloca,
                .type = struct_type_reference
            };
        }
        else if (expression.type == Instantiate_expression_type::Uninitialized)
        {
            return Value_and_type
            {
                .name = "",
                .value = struct_alloca,
                .type = struct_type_reference
            };
        }
        else if (expression.type == Instantiate_expression_type::Zero_initialized)
        {
            std::uint64_t const alloc_size_in_bytes = parameters.llvm_data_layout.getTypeAllocSize(llvm_struct_type);
            llvm::Align const alignment = parameters.llvm_data_layout.getABITypeAlign(llvm_struct_type);

            create_memset_to_0_call(parameters.llvm_builder, struct_alloca, alloc_size_in_bytes, alignment);

            return Value_and_type
            {
                .name = "",
                .value = struct_alloca,
                .type = struct_type_reference
            };
        }
        else
        {
            throw std::runtime_error{ format_error("Instantiate_expression_type not handled!", parameters.source_position) };
        }
    }

    static llvm::Value* create_union_instance_constant_value(
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        Statement const& statement,
        Instantiate_expression const& expression,
        Expression_parameters const& parameters,
        Union_declaration const& union_declaration,
        llvm::Type* const llvm_union_type
    )
    {
        if (expression.members.empty())
        {
            llvm::Value* const union_value = llvm::Constant::getNullValue(llvm_union_type);
            return union_value;
        }

        Instantiate_member_value_pair const& member_value_pair = expression.members[0];

        auto const member_name_location = std::find_if(
            union_declaration.member_names.begin(),
            union_declaration.member_names.end(),
            [&member_value_pair](std::pmr::string const& member_name) { return member_name == member_value_pair.member_name; }
        );
        if (member_name_location == union_declaration.member_names.end())
            throw std::runtime_error{ format_error(std::format("Could not find member '{}' while instantiating union ''!", member_value_pair.member_name, union_declaration.name), parameters.source_position) };

        auto const member_index = std::distance(union_declaration.member_names.begin(), member_name_location);
        Type_reference const& member_type = union_declaration.member_types[member_index];

        Expression_parameters new_parameters = parameters;
        new_parameters.expression_type = member_type;
        Value_and_type const member_value = create_expression_value(member_value_pair.value.expression_index, statement, new_parameters);

        llvm::Type* const storage_type = llvm_union_type->getStructElementType(0);
        llvm::Type* const source_type = member_value.value->getType();

        unsigned const storage_size_bits = parameters.llvm_data_layout.getTypeSizeInBits(storage_type);
        unsigned const source_size_bits = parameters.llvm_data_layout.getTypeSizeInBits(member_value.value->getType());

        llvm::Value* const converted_value = convert_constant(llvm_context, llvm_builder, storage_type, storage_size_bits, member_value.value, source_type, source_size_bits);
        
        // If we fail to convert to the correct type, then just return null
        llvm::Type* const element_type = llvm_union_type->getStructElementType(0);
        if (converted_value->getType() != element_type)
            return llvm::Constant::getNullValue(llvm_union_type);
        
        llvm::Constant* const union_value = llvm::UndefValue::get(llvm_union_type);
        return llvm_builder.CreateInsertValue(union_value, converted_value, 0);
    }

    Value_and_type create_instantiate_union_expression_value(
        Statement const& statement,
        Instantiate_expression const& expression,
        Expression_parameters const& parameters,
        std::string_view const module_name,
        Union_declaration const& union_declaration,
        Type_reference const& union_type_reference
    )
    {                
        llvm::LLVMContext& llvm_context = parameters.llvm_context;
        llvm::DataLayout const& llvm_data_layout = parameters.llvm_data_layout;
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;
        Type_database const& type_database = parameters.type_database;

        llvm::Type* const llvm_union_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, union_type_reference, type_database);
        if (!llvm::StructType::classof(llvm_union_type))
            throw std::runtime_error{ format_error("llvm_union_type must be a StructType!", parameters.source_position) };

        if (parameters.llvm_parent_function == nullptr)
        {
            llvm::Value* const union_instance = create_union_instance_constant_value(
                llvm_context,
                llvm_builder,
                statement,
                expression,
                parameters,
                union_declaration,
                llvm_union_type
            );
            
            return Value_and_type
            {
                .name = "",
                .value = union_instance,
                .type = union_type_reference
            };
        }

        if (expression.type != Instantiate_expression_type::Default)
            throw std::runtime_error{ format_error("Unions only support default Instantiate_expression_type!", parameters.source_position) };

        if (expression.members.size() > 1)
            throw std::runtime_error{ format_error("Instantiating a union requires specifying either zero or one member!", parameters.source_position) };

        if (expression.members.empty())
        {
            llvm::AllocaInst* const union_instance = create_alloca_instruction(llvm_builder, llvm_data_layout, *parameters.llvm_parent_function, llvm_union_type);

            std::uint64_t const alloc_size_in_bytes = llvm_data_layout.getTypeAllocSize(llvm_union_type);
            llvm::Align const alignment = llvm_data_layout.getABITypeAlign(llvm_union_type);
            create_memset_to_0_call(llvm_builder, union_instance, alloc_size_in_bytes, alignment);

            return Value_and_type
            {
                .name = "",
                .value = union_instance,
                .type = union_type_reference
            };
        }

        Instantiate_member_value_pair const& member_value_pair = expression.members[0];

        auto const member_name_location = std::find_if(union_declaration.member_names.begin(), union_declaration.member_names.end(), [&member_value_pair](std::pmr::string const& member_name) { return member_name == member_value_pair.member_name; });
        if (member_name_location == union_declaration.member_names.end())
            throw std::runtime_error{ format_error(std::format("Could not find member '{}' while instantiating union ''!", member_value_pair.member_name, union_declaration.name), parameters.source_position) };

        auto const member_index = std::distance(union_declaration.member_names.begin(), member_name_location);
        Type_reference const& member_type = union_declaration.member_types[member_index];

        Expression_parameters new_parameters = parameters;
        new_parameters.expression_type = member_type;
        Value_and_type const member_value = create_loaded_expression_value(member_value_pair.value.expression_index, statement, new_parameters);

        llvm::AllocaInst* const union_instance = create_alloca_instruction(llvm_builder, llvm_data_layout, *parameters.llvm_parent_function, llvm_union_type);
        llvm::Value* const bitcast_instruction = llvm_builder.CreateBitCast(union_instance, member_value.value->getType()->getPointerTo());
        create_store_instruction(llvm_builder, llvm_data_layout, member_value.value, bitcast_instruction);

        return Value_and_type
        {
            .name = "",
            .value = union_instance,
            .type = union_type_reference
        };
    }

    Value_and_type create_instance_call_expression_value(
        Instance_call_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        Value_and_type const left_hand_side_value = create_expression_value(expression.left_hand_side.expression_index, statement, parameters);
        if (!left_hand_side_value.type.has_value() || !std::holds_alternative<Custom_type_reference>(left_hand_side_value.type.value().data))
            throw std::runtime_error{ format_error("Left hand side of instance call is not a custom type reference!", parameters.source_position) };

        Custom_type_reference const& custom_type_reference = std::get<Custom_type_reference>(left_hand_side_value.type.value().data);

        Function_constructor const* const function_constructor = get_function_constructor(
            parameters.declaration_database,
            custom_type_reference
        );
        if (function_constructor == nullptr)
            throw std::runtime_error{ format_error("Could not find function constructor!", parameters.source_position) };

        std::pmr::polymorphic_allocator<> allocator = {}; // TODO

        Instance_call_key const key = {
            .module_name = custom_type_reference.module_reference.name,
            .function_constructor_name = custom_type_reference.name,
            .arguments = expression.arguments
        };

        std::string const mangled_name = mangle_instance_call_name(key);
        llvm::Function* const llvm_function = get_llvm_function(key.module_name, parameters.llvm_module, mangled_name, std::nullopt);
        if (llvm_function == nullptr)
            throw std::runtime_error{ format_error(std::format("Could not find function '{}'", mangled_name), parameters.source_position) };

        std::optional<Function_expression> const function_expression = get_instance_call_function_expression(
            parameters.declaration_database,
            parameters.core_module,
            key
        );
        if (!function_expression.has_value())
            throw std::runtime_error{ format_error("Could not find function expression!", parameters.source_position) };

        Function_declaration const& function_declaration = function_expression->declaration;
        Type_reference type_reference = create_function_type_type_reference(
            function_declaration.type,
            function_declaration.input_parameter_names,
            function_declaration.output_parameter_names
        );

        return Value_and_type
        {
            .name = "",
            .value = llvm_function,
            .type = std::move(type_reference)
        };
    }

    struct Declaration_to_instantiate
    {
        Declaration declaration;
        Custom_type_reference const* type_reference;
    };

    std::optional<Declaration_to_instantiate> get_declaration_type_to_instantiate(
        Declaration_database const& declaration_database,
        Type_reference const& type_reference
    )
    {
        std::optional<Declaration> const declaration = find_declaration(declaration_database, type_reference);
        if (!declaration.has_value())
            return std::nullopt;

        if (std::holds_alternative<Alias_type_declaration const*>(declaration.value().data))
        {
            Alias_type_declaration const& alias_type_declaration = *std::get<Alias_type_declaration const*>(declaration.value().data);
            if (alias_type_declaration.type.empty())
                return std::nullopt;

            return get_declaration_type_to_instantiate(declaration_database, alias_type_declaration.type[0]);
        }
        else if (std::holds_alternative<Struct_declaration const*>(declaration.value().data) || std::holds_alternative<Union_declaration const*>(declaration.value().data))
        {
            Custom_type_reference const* custom_type_reference = find_declaration_type_reference(type_reference);
            if (custom_type_reference == nullptr)
                return std::nullopt;
                
            return Declaration_to_instantiate
            {
                .declaration = declaration.value(),
                .type_reference = custom_type_reference
            };
        }
        else
        {
            return std::nullopt;
        }
    }

    Value_and_type create_instantiate_expression_value(
        Instantiate_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        Declaration_database const& declaration_database = parameters.declaration_database;

        if (!parameters.expression_type.has_value())
            throw std::runtime_error{ format_error("Could not infer type while trying to instantiate!", parameters.source_position) };

        std::optional<Type_reference> const type_reference_optional = get_underlying_type(parameters.declaration_database, parameters.expression_type.value());
        if (!type_reference_optional.has_value())
            throw std::runtime_error{ format_error("Could not find type to instantiate!", parameters.source_position) };
        Type_reference const& type_reference = type_reference_optional.value();

        if (std::holds_alternative<iris::Soa_array_type>(type_reference.data))
        {
            Soa_array_type_info const soa_info = get_soa_array_type_info(type_reference, parameters);
            auto const [soa_alloca, data_pointer] = create_soa_array_storage(
                type_reference,
                soa_info.layout,
                parameters,
                "soa_array"
            );

            if (expression.type == Instantiate_expression_type::Default)
            {
                initialize_default_soa_array_storage(data_pointer, soa_info, parameters);
            }
            else if (expression.type == Instantiate_expression_type::Zero_initialized)
            {
                if (soa_info.layout.size > 0)
                    create_memset_to_0_call(parameters.llvm_builder, data_pointer, soa_info.layout.size, llvm::Align(soa_info.layout.alignment));
            }
            else if (expression.type == Instantiate_expression_type::Explicit)
            {
                throw std::runtime_error{ format_error("Explicit Soa_array brace initialization is not implemented. Use [ ... ] initializer syntax.", parameters.source_position) };
            }
            else if (expression.type != Instantiate_expression_type::Uninitialized)
            {
                throw std::runtime_error{ format_error("Instantiate_expression_type not handled for Soa_array.", parameters.source_position) };
            }

            return Value_and_type
            {
                .name = "",
                .value = soa_alloca,
                .type = type_reference
            };
        }

        if (std::holds_alternative<iris::Soa_array_view_type>(type_reference.data))
        {
            if (parameters.llvm_parent_function == nullptr)
                throw std::runtime_error{ format_error("Soa_array_view local storage requires a parent function.", parameters.source_position) };

            llvm::Type* const soa_view_llvm_type = type_reference_to_llvm_type(
                parameters.llvm_context,
                parameters.llvm_data_layout,
                type_reference,
                parameters.type_database
            );

            llvm::AllocaInst* const soa_view_alloca = create_alloca_instruction(
                parameters.llvm_builder,
                parameters.llvm_data_layout,
                *parameters.llvm_parent_function,
                soa_view_llvm_type,
                "soa_array_view"
            );

            if (expression.type == Instantiate_expression_type::Default || expression.type == Instantiate_expression_type::Zero_initialized)
            {
                std::uint64_t const view_size = parameters.llvm_data_layout.getTypeAllocSize(soa_view_llvm_type);
                llvm::Align const view_alignment = parameters.llvm_data_layout.getABITypeAlign(soa_view_llvm_type);
                create_memset_to_0_call(parameters.llvm_builder, soa_view_alloca, view_size, view_alignment);
            }
            else if (expression.type == Instantiate_expression_type::Explicit)
            {
                throw std::runtime_error{ format_error("Explicit Soa_array_view brace initialization is not implemented.", parameters.source_position) };
            }
            else if (expression.type != Instantiate_expression_type::Uninitialized)
            {
                throw std::runtime_error{ format_error("Instantiate_expression_type not handled for Soa_array_view.", parameters.source_position) };
            }

            return Value_and_type
            {
                .name = "",
                .value = soa_view_alloca,
                .type = type_reference
            };
        }

        if (std::holds_alternative<iris::Array_slice_type>(type_reference.data))
        {
            iris::Array_slice_type const& array_slice_type = std::get<iris::Array_slice_type>(type_reference.data);
            iris::Struct_declaration const struct_declaration = create_array_slice_type_struct_declaration(array_slice_type.element_type);

            return create_instantiate_struct_expression_value(statement, expression, parameters, "iris.builtin", struct_declaration, type_reference);
        }

        if (is_primitive_type(type_reference) || is_constant_array_type_reference(type_reference) || is_enum_type(declaration_database, type_reference))
        {
            if (parameters.llvm_parent_function == nullptr)
                throw std::runtime_error{ format_error("Primitive instantiate expression is not supported as a global constant!", parameters.source_position) };

            llvm::LLVMContext& llvm_context = parameters.llvm_context;
            llvm::DataLayout const& llvm_data_layout = parameters.llvm_data_layout;
            llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;
            Type_database const& type_database = parameters.type_database;

            llvm::Type* const llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, type_reference, type_database);
            llvm::AllocaInst* const alloca = create_alloca_instruction(llvm_builder, llvm_data_layout, *parameters.llvm_parent_function, llvm_type);

            if (expression.type != Instantiate_expression_type::Uninitialized)
            {
                llvm::Constant* zero_value = nullptr;
                if (is_pointer(type_reference) || is_null_pointer_type(type_reference) || is_function_pointer(type_reference))
                {
                    zero_value = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(llvm_type));
                }
                else if (is_floating_point(type_reference))
                {
                    zero_value = llvm::ConstantFP::get(llvm_type, 0.0);
                }
                else if (std::holds_alternative<iris::Constant_array_type>(type_reference.data))
                {
                    zero_value = llvm::ConstantAggregateZero::get(llvm_type);
                }
                else
                {
                    // integers, bool, c_bool, byte, decimal, enums
                    zero_value = llvm::ConstantInt::get(llvm_type, 0);
                }
                create_store_instruction(llvm_builder, llvm_data_layout, zero_value, alloca);
            }

            return Value_and_type
            {
                .name = "",
                .value = alloca,
                .type = type_reference
            };
        }

        std::optional<Declaration_to_instantiate> const found_instance = get_declaration_type_to_instantiate(
            declaration_database,
            type_reference
        );
        if (!found_instance.has_value())
            throw std::runtime_error{ format_error("Could not instantiate type!", parameters.source_position) };
        
        Custom_type_reference const* custom_type_reference = found_instance->type_reference;
        std::string_view const declaration_module_name = custom_type_reference->module_reference.name;

        Declaration const declaration = found_instance->declaration;

        if (std::holds_alternative<Struct_declaration const*>(declaration.data))
        {
            Struct_declaration const& struct_declaration = *std::get<Struct_declaration const*>(declaration.data);
            return create_instantiate_struct_expression_value(statement, expression, parameters, declaration_module_name, struct_declaration, type_reference);
        }
        else if (std::holds_alternative<Union_declaration const*>(declaration.data))
        {
            Union_declaration const& union_declaration = *std::get<Union_declaration const*>(declaration.data);
            return create_instantiate_union_expression_value(statement, expression, parameters, declaration_module_name, union_declaration, type_reference);
        }

        throw std::runtime_error{ format_error(std::format("Instantiate_expression can only be used to instantiate either structs or unions! Tried to instantiate '{}.{}'", declaration_module_name, custom_type_reference->name), parameters.source_position) };
    }

    Value_and_type create_null_pointer_expression_value(
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        llvm::PointerType* const pointer_llvm_type = llvm::PointerType::get(parameters.llvm_context, 0);
        llvm::Constant* const null_pointer_value = llvm::ConstantPointerNull::get(pointer_llvm_type);
        
        return
        {
            .name = "",
            .value = null_pointer_value,
            .type = create_null_pointer_type_type_reference(),
        };
    }

    Value_and_type create_parenthesis_expression_value(
        Parenthesis_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        return create_expression_value(expression.expression.expression_index, statement, parameters);
    }

    Value_and_type create_return_expression_value(
        Return_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;

        if (!expression.expression.has_value())
        {
            create_instructions_at_return(parameters);

            if (parameters.debug_info != nullptr)
                set_debug_location(parameters.llvm_builder, *parameters.debug_info, parameters.source_position);

            llvm::Value* const instruction = llvm_builder.CreateRetVoid();

            return
            {
                .name = "",
                .value = instruction,
                .type = std::nullopt
            };
        }

        Function_type const& function_type = parameters.function_declaration.value()->type;
        std::optional<Type_reference> const function_output_type = get_function_output_type_reference(function_type, parameters.core_module);

        Expression_parameters new_parameters = parameters;
        new_parameters.expression_type = function_output_type.has_value() ? function_output_type.value() : std::optional<Type_reference>{};
        Value_and_type const temporary = create_expression_value(expression.expression->expression_index, statement, new_parameters);

        create_instructions_at_return(parameters);

        if (parameters.contract_options != Contract_options::Disabled && parameters.function_declaration.has_value())
        {
            std::pmr::vector<Value_and_type> return_values;
            return_values.reserve(1);

            iris::Function_declaration const& function_declaration = *parameters.function_declaration.value();
            if (!function_declaration.output_parameter_names.empty())
            {
                if (function_declaration.output_parameter_names.size() > 1)
                    throw std::runtime_error{ format_error("Postconditions do not support multiple return types yet! Not implemented!", parameters.source_position) };

                Value_and_type return_value = temporary;
                return_value.name = function_declaration.output_parameter_names[0];
                return_values.push_back(std::move(return_value));
            }

            Expression_parameters postcondition_parameters = parameters;
            postcondition_parameters.local_variables = return_values;

            create_function_postconditions(
                parameters.llvm_context,
                parameters.llvm_module,
                *parameters.llvm_parent_function,
                parameters.llvm_builder,
                parameters.core_module,
                *parameters.function_declaration.value(),
                postcondition_parameters
            );
        }
        
        if (parameters.debug_info != nullptr)
            set_debug_location(parameters.llvm_builder, *parameters.debug_info, parameters.source_position);

        iris::Expression const& expression_to_return = statement.expressions[expression.expression->expression_index];
        bool const is_taking_address_of = is_taking_address_of_expression(statement, expression_to_return);

        llvm::Value* const instruction = generate_function_return_instruction(
            parameters.llvm_context,
            parameters.llvm_builder,
            parameters.llvm_data_layout,
            parameters.llvm_module,
            parameters.clang_module_data,
            parameters.core_module,
            function_type,
            *parameters.llvm_parent_function,
            parameters.declaration_database,
            parameters.type_database,
            temporary,
            is_taking_address_of
        );

        return
        {
            .name = "",
            .value = instruction,
            .type = std::nullopt
        };
    }

    Value_and_type create_switch_expression_value(
        Switch_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        llvm::LLVMContext& llvm_context = parameters.llvm_context;
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;
        llvm::Function* const llvm_parent_function = parameters.llvm_parent_function;
        std::span<Block_info const> block_infos = parameters.blocks;

        std::pmr::vector<llvm::BasicBlock*> case_blocks;
        case_blocks.resize(expression.cases.size());

        llvm::BasicBlock* const after_block = llvm::BasicBlock::Create(llvm_context, "switch_after", llvm_parent_function);
        llvm::BasicBlock* default_case_block = nullptr;

        for (std::size_t case_index = 0; case_index < expression.cases.size(); ++case_index)
        {
            Switch_case_expression_pair const& switch_case = expression.cases[case_index];

            std::string const block_name = switch_case.case_value.has_value() ? std::format("switch_case_i{}_", case_index) : "switch_case_default";

            llvm::BasicBlock* case_block = llvm::BasicBlock::Create(llvm_context, block_name, llvm_parent_function);

            if (!switch_case.case_value.has_value())
                default_case_block = case_block;

            case_blocks[case_index] = case_block;
        }

        if (default_case_block == nullptr)
            default_case_block = after_block;

        std::uint64_t const number_of_cases = static_cast<std::uint64_t>(expression.cases.size());

        if (parameters.debug_info != nullptr)
            set_debug_location(parameters.llvm_builder, *parameters.debug_info, parameters.source_position);

        Value_and_type const& switch_value = create_loaded_expression_value(expression.value.expression_index, statement, parameters);

        llvm::SwitchInst* switch_instruction = llvm_builder.CreateSwitch(switch_value.value, default_case_block, number_of_cases);

        for (std::size_t case_index = 0; case_index < expression.cases.size(); ++case_index)
        {
            Switch_case_expression_pair const& switch_case = expression.cases[case_index];

            if (switch_case.case_value.has_value())
            {
                llvm::BasicBlock* const case_block = case_blocks[case_index];
                Value_and_type const& case_value = create_loaded_expression_value(switch_case.case_value.value().expression_index, statement, parameters);

                if (!llvm::ConstantInt::classof(case_value.value))
                    throw std::runtime_error("Swith case value is not a ConstantInt!");

                llvm::ConstantInt* const case_value_constant = static_cast<llvm::ConstantInt*>(case_value.value);

                switch_instruction->addCase(case_value_constant, case_block);
            }
        }

        std::pmr::vector<Block_info> all_block_infos{ block_infos.begin(), block_infos.end() };
        all_block_infos.push_back({ .block_type = Block_type::Switch, .repeat_block = nullptr, .after_block = after_block });

        for (std::size_t case_index = 0; case_index < expression.cases.size(); ++case_index)
        {
            Switch_case_expression_pair const& switch_case = expression.cases[case_index];
            llvm::BasicBlock* const case_block = case_blocks[case_index];

            llvm_builder.SetInsertPoint(case_block);

            std::pmr::vector<std::pmr::vector<Statement>> defer_expressions_per_block = create_defer_block(parameters.defer_expressions_per_block);

            Expression_parameters new_parameters = parameters;
            new_parameters.blocks = all_block_infos;
            new_parameters.defer_expressions_per_block = defer_expressions_per_block;

            create_statement_values(
                switch_case.statements,
                new_parameters,
                true
            );

            if (!ends_with_terminator_statement(switch_case.statements))
            {
                // If there is a next case:
                if ((case_index + 1) < expression.cases.size())
                {
                    llvm::BasicBlock* const next_case_block = case_blocks[case_index + 1];
                    llvm_builder.CreateBr(next_case_block);
                }
                else
                {
                    llvm_builder.CreateBr(after_block);
                }
            }
        }

        llvm_builder.SetInsertPoint(after_block);

        return Value_and_type
        {
            .name = "",
            .value = nullptr,
            .type = std::nullopt
        };
    }

    Value_and_type create_ternary_condition_expression_value(
        Ternary_condition_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        llvm::LLVMContext& llvm_context = parameters.llvm_context;
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;
        llvm::Function* const llvm_parent_function = parameters.llvm_parent_function;

        llvm::BasicBlock* const then_block = llvm::BasicBlock::Create(llvm_context, "ternary_condition_then", llvm_parent_function);
        llvm::BasicBlock* const else_block = llvm::BasicBlock::Create(llvm_context, "ternary_condition_else", llvm_parent_function);
        llvm::BasicBlock* const end_block = llvm::BasicBlock::Create(llvm_context, "ternary_condition_end", llvm_parent_function);

        // Condition:
        Value_and_type const& condition_value = create_loaded_expression_value(expression.condition.expression_index, statement, parameters);
        llvm::Value* const condition_converted_value = convert_to_boolean(llvm_context, llvm_builder, condition_value.value, condition_value.type);
        llvm_builder.CreateCondBr(condition_converted_value, then_block, else_block);

        // Then:
        llvm_builder.SetInsertPoint(then_block);
        Value_and_type const& then_value = create_loaded_statement_value(
            expression.then_statement,
            parameters
        );
        llvm_builder.CreateBr(end_block);
        llvm::BasicBlock* const then_end_block = llvm_builder.GetInsertBlock();

        // Else:
        llvm_builder.SetInsertPoint(else_block);
        Value_and_type const& else_value = create_loaded_statement_value(
            expression.else_statement,
            parameters
        );
        llvm_builder.CreateBr(end_block);
        llvm::BasicBlock* const else_end_block = llvm_builder.GetInsertBlock();

        if (then_value.type.has_value() && else_value.type.has_value() && then_value.type.value() != else_value.type.value())
            throw std::runtime_error{ format_error("Ternary condition then and else statements must have the same type!", parameters.source_position) };

        // End:
        llvm_builder.SetInsertPoint(end_block);
        llvm::PHINode* const phi_node = llvm_builder.CreatePHI(then_value.value->getType(), 2);
        phi_node->addIncoming(then_value.value, then_end_block);
        phi_node->addIncoming(else_value.value, else_end_block);

        return Value_and_type
        {
            .name = "",
            .value = phi_node,
            .type = then_value.type
        };
    }

    Value_and_type create_type_expression_value(
        Type_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        return Value_and_type
        {
            .name = "",
            .value = nullptr,
            .type = expression.type
        };
    }

    Value_and_type create_unary_expression_value(
        Unary_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        llvm::LLVMContext& llvm_context = parameters.llvm_context;
        llvm::DataLayout const& llvm_data_layout = parameters.llvm_data_layout;
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;
        std::span<Value_and_type const> const local_variables = parameters.local_variables;
        Type_database const& type_database = parameters.type_database;

        Value_and_type const value_expression = create_expression_value(expression.expression.expression_index, statement, parameters);
        Unary_operation const operation = expression.operation;

        Type_reference const& type = value_expression.type.value();

        switch (operation)
        {
        case Unary_operation::Not: {
            if (is_bool(type) || is_c_bool(type))
            {
                llvm::Value* const loaded_value = load_if_needed(value_expression, expression.expression.expression_index, statement, parameters).value;
                llvm::Type* const value_type = loaded_value->getType();
                
                llvm::Value* const zero = llvm::ConstantInt::get(value_type, 0);
                llvm::Value* const result = llvm_builder.CreateICmpEQ(loaded_value, zero);
                return Value_and_type
                {
                    .name = "",
                    .value = result,
                    .type = type
                };
            }
            break;
        }
        case Unary_operation::Bitwise_not: {
            if (is_integer(type))
            {
                llvm::Value* const loaded_value = load_if_needed(value_expression, expression.expression.expression_index, statement, parameters).value;
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateNot(loaded_value),
                    .type = type
                };
            }
            break;
        }
        case Unary_operation::Minus: {
            if (is_integer(type))
            {
                llvm::Value* const loaded_value = load_if_needed(value_expression, expression.expression.expression_index, statement, parameters).value;
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateNeg(loaded_value),
                    .type = type
                };
            }
            else if (is_floating_point(type))
            {
                llvm::Value* const loaded_value = load_if_needed(value_expression, expression.expression.expression_index, statement, parameters).value;
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateFNeg(loaded_value),
                    .type = type
                };
            }
            else if (is_decimal(type))
            {
                llvm::Value* const loaded_value = load_if_needed(value_expression, expression.expression.expression_index, statement, parameters).value;
                return Value_and_type
                {
                    .name = "",
                    .value = llvm_builder.CreateNeg(loaded_value),
                    .type = type
                };
            }
            break;
        }
        case Unary_operation::Pre_decrement:
        case Unary_operation::Pre_increment:
        case Unary_operation::Post_decrement:
        case Unary_operation::Post_increment: {
            if (is_integer(type))
            {
                llvm::Type* llvm_value_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, type, type_database);

                bool const is_increment = (operation == Unary_operation::Pre_increment) || (operation == Unary_operation::Post_increment);
                bool const is_post = (operation == Unary_operation::Post_decrement) || (operation == Unary_operation::Post_increment);

                llvm::Value* const current_value = create_load_instruction(llvm_builder, llvm_data_layout, llvm_value_type, value_expression.value);

                llvm::Value* const new_value = is_increment ?
                    llvm_builder.CreateAdd(current_value, llvm::ConstantInt::get(current_value->getType(), 1)) :
                    llvm_builder.CreateSub(current_value, llvm::ConstantInt::get(current_value->getType(), 1));

                create_store_instruction(llvm_builder, llvm_data_layout, new_value, value_expression.value);

                llvm::Value* const returned_value = is_post ? current_value : new_value;

                return Value_and_type
                {
                    .name = "",
                    .value = returned_value,
                    .type = type
                };
            }
            break;
        }
        case Unary_operation::Indirection: {
            if (is_non_void_pointer(type))
            {
                Type_reference const core_pointee_type = remove_pointer(type).value();

                llvm::Value* const load_address = create_load_instruction(llvm_builder, llvm_data_layout, value_expression.value->getType(), value_expression.value);

                return Value_and_type
                {
                    .name = "",
                    .value = load_address,
                    .type = core_pointee_type
                };
            }
            break;
        }
        case Unary_operation::Address_of: {
            std::string_view const variable_name = value_expression.name;

            // Try local variable:
            {
                std::optional<Value_and_type> location = search_in_function_scope(variable_name, parameters.function_arguments, local_variables);
                if (location.has_value())
                {
                    Value_and_type const& variable_declaration = location.value();
                    return Value_and_type
                    {
                        .name = "",
                        .value = variable_declaration.value,
                        .type = create_pointer_type_type_reference({ variable_declaration.type.value() }, false)
                    };
                }
            }

            // Try global variable:
            {
                std::optional<Global_variable_declaration const*> const declaration = find_global_variable_declaration(parameters.core_module, variable_name);
                if (declaration.has_value())
                {
                    Global_variable_declaration const& global_variable_declaration = *declaration.value();

                    if (global_variable_declaration.global_type != Global_variable_type::Macro)
                    {
                        std::optional<Value_and_type> const global_variable = get_global_variable_value_and_type(
                            parameters.core_module,
                            global_variable_declaration,
                            parameters
                        );
                        if (global_variable.has_value())
                        {
                            if (!global_variable->type.has_value())
                                throw std::runtime_error{ format_error(std::format("Could not deduce type of global variable '{}'", global_variable_declaration.name), parameters.source_position) };
                            
                            return Value_and_type
                            {
                                .name = "",
                                .value = global_variable->value,
                                .type = create_pointer_type_type_reference({ global_variable->type.value() }, false)
                            };
                        }
                    }
                }
            }

            // Try access expressions:
            {
                iris::Expression const& expression_to_get_address_of = statement.expressions[expression.expression.expression_index];

                if (std::holds_alternative<iris::Access_expression>(expression_to_get_address_of.data) || std::holds_alternative<iris::Access_array_expression>(expression_to_get_address_of.data) || std::holds_alternative<iris::Dereference_and_access_expression>(expression_to_get_address_of.data))
                {
                    std::pmr::vector<Type_reference> element_type;
                    if (value_expression.type.has_value())
                        element_type.push_back(value_expression.type.value());

                    return
                    {
                        .name = "",
                        .value = value_expression.value,
                        .type = create_pointer_type_type_reference(std::move(element_type), true)
                    };
                }
            }
        }
        }

        throw std::runtime_error{ format_error(std::format("Unary operation '{}' not implemented!", static_cast<std::uint32_t>(operation)), parameters.source_position) };
    }

    Value_and_type create_variable_declaration_expression_value(
        Variable_declaration_expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        if (parameters.llvm_parent_function == nullptr)
            throw std::runtime_error{ format_error("Can only create variables inside functions!", parameters.source_position) };

        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;
        llvm::DataLayout const& llvm_data_layout = parameters.llvm_data_layout;

        Value_and_type const& right_hand_side = create_loaded_expression_value(expression.right_hand_side.expression_index, statement, parameters);

        if (parameters.debug_info != nullptr)
            set_debug_location(parameters.llvm_builder, *parameters.debug_info, parameters.source_position);

        llvm::AllocaInst* const alloca = create_alloca_instruction(llvm_builder, llvm_data_layout, *parameters.llvm_parent_function, right_hand_side.value->getType(), expression.name.c_str());
        if (alloca == nullptr)
        {
            return Value_and_type
            {
                .name = expression.name,
                .value = nullptr,
                .type = right_hand_side.type
            };    
        }

        if (parameters.debug_info != nullptr)
            create_local_variable_debug_description(*parameters.debug_info, parameters, expression.name, alloca, *right_hand_side.type);

        if (can_store(right_hand_side.type))
            create_store_instruction(llvm_builder, llvm_data_layout, right_hand_side.value, alloca);

        return Value_and_type
        {
            .name = expression.name,
            .value = alloca,
            .type = right_hand_side.type
        };
    }

    Value_and_type create_variable_declaration_with_type_expression_value(
        Variable_declaration_with_type_expression const& expression,
        iris::Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        if (parameters.llvm_parent_function == nullptr)
            throw std::runtime_error{ format_error("Can only create variables inside functions!", parameters.source_position) };

        llvm::LLVMContext& llvm_context = parameters.llvm_context;
        llvm::DataLayout const& llvm_data_layout = parameters.llvm_data_layout;
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;
        Type_database& type_database = parameters.type_database;

        std::optional<Type_reference> const core_type_optional = iris::get_variable_declaration_with_type_expression_type(statement, expression);
        if (!core_type_optional.has_value())
            throw std::runtime_error{ format_error("Variable declaration with type has invalid type expression!", parameters.source_position) };

        Type_reference const& core_type = core_type_optional.value();

        llvm::Type* const llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, core_type, type_database);

        iris::Expression const& right_hand_side_expression = statement.expressions[expression.right_hand_side.expression_index];
        bool const is_right_side_instantiate_expression = std::holds_alternative<iris::Instantiate_expression>(right_hand_side_expression.data);
        if (is_right_side_instantiate_expression)
        {
            if (parameters.debug_info != nullptr)
                set_debug_location(parameters.llvm_builder, *parameters.debug_info, parameters.source_position);
        }

        Expression_parameters new_parameters = parameters;
        new_parameters.expression_type = core_type;

        Value_and_type const right_hand_side = create_expression_value(
            expression.right_hand_side.expression_index,
            statement,
            new_parameters
        );

        if (is_right_side_instantiate_expression && llvm::AllocaInst::classof(right_hand_side.value))
        {
            llvm::AllocaInst* const alloca_instruction = static_cast<llvm::AllocaInst*>(right_hand_side.value);
            alloca_instruction->setName(expression.name.c_str());

            if (parameters.debug_info != nullptr)
                create_local_variable_debug_description(*parameters.debug_info, parameters, expression.name, alloca_instruction, core_type);

            return Value_and_type
            {
                .name = expression.name,
                .value = alloca_instruction,
                .type = std::move(core_type)
            };
        }

        bool const store_value = can_store(core_type);
        llvm::Value* const loaded_value = store_value ? load_if_needed(right_hand_side, expression.right_hand_side.expression_index, statement, new_parameters).value : right_hand_side.value;

        llvm::AllocaInst* const alloca = create_alloca_instruction(llvm_builder, llvm_data_layout, *parameters.llvm_parent_function, llvm_type, expression.name.c_str());

        if (parameters.debug_info != nullptr)
            create_local_variable_debug_description(*parameters.debug_info, parameters, expression.name, alloca, core_type);

        if (store_value)
        {
            if (parameters.debug_info != nullptr)
                set_debug_location(parameters.llvm_builder, *parameters.debug_info, parameters.source_position);
            
            create_store_instruction(llvm_builder, llvm_data_layout, loaded_value, alloca);
        }

        return Value_and_type
        {
            .name = expression.name,
            .value = alloca,
            .type = std::move(core_type)
        };
    }

    Value_and_type convert_to_expected_type_if_needed(
        Value_and_type const& value,
        Expression_parameters const& parameters
    )
    {
        // Do implicit convertions:
        if (value.type.has_value() && parameters.expression_type.has_value())
        {
            Type_reference const& source_type = value.type.value();
            Type_reference const& expected_type = parameters.expression_type.value();
            
            if (is_constant_array_type_reference(source_type) && is_array_slice_type_reference(expected_type))
            {
                llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;

                std::uint64_t const array_length = get_constant_array_type_size(source_type);

                if (array_length == 0)
                {
                    Value_and_type const data_value
                    {
                        .name = "",
                        .value = llvm::ConstantPointerNull::get(llvm::PointerType::get(parameters.llvm_context, 0)),
                        .type = std::nullopt,
                    };

                    Value_and_type const length_value
                    {
                        .name = "",
                        .value = llvm_builder.getInt64(array_length),
                        .type = create_integer_type_type_reference(64, false),
                    };

                    std::optional<Type_reference> const expected_element_type = get_element_or_pointee_type(expected_type);
                    std::pmr::vector<iris::Type_reference> const element_type{expected_element_type.value()};

                    Value_and_type const array_slice = instantiate_array_slice(
                        element_type,
                        data_value,
                        length_value,
                        parameters
                    );

                    return array_slice;
                }

                std::optional<Type_reference> const expected_element_type = get_element_or_pointee_type(expected_type);

                std::pmr::vector<iris::Type_reference> const element_type{expected_element_type.value()};

                llvm::Type* const constant_array_llvm_type = type_reference_to_llvm_type(parameters.llvm_context, parameters.llvm_data_layout, value.type.value(), parameters.type_database);

                Value_and_type const data_value
                {
                    .name = "",
                    .value = llvm_builder.CreateGEP(
                        constant_array_llvm_type,
                        value.value,
                        {llvm_builder.getInt32(0), llvm_builder.getInt32(0)},
                        "data_pointer"
                    ),
                    .type = std::nullopt,
                };

                Value_and_type const length_value
                {
                    .name = "",
                    .value = llvm_builder.getInt64(array_length),
                    .type = create_integer_type_type_reference(64, false),
                };

                Value_and_type const array_slice = instantiate_array_slice(
                    element_type,
                    data_value,
                    length_value,
                    parameters
                );

                return array_slice;
            }
        }

        return value;
    }

    Value_and_type create_variable_expression_value(
        Variable_expression const& expression,
        Expression_parameters const& parameters
    )
    {
        std::string_view const variable_name = expression.name;

        auto const is_variable = [variable_name](Value_and_type const& element) -> bool
        {
            return element.name == variable_name;
        };

        // Search in local variables and function arguments:
        {
            std::optional<Value_and_type> location = search_in_function_scope(variable_name, parameters.function_arguments, parameters.local_variables);

            if (location.has_value())
            {
                return convert_to_expected_type_if_needed(location.value(), parameters);
            }
        }

        // Search in function arguments:
        {
            auto const location = std::find_if(parameters.function_arguments.begin(), parameters.function_arguments.end(), is_variable);
            if (location != parameters.function_arguments.end())
            {
                return convert_to_expected_type_if_needed(*location, parameters);
            }
        }

        // Search for functions in this module:
        {
            // TODO review this...
            std::optional<Function_declaration const*> const function_declaration = find_function_declaration(parameters.core_module, variable_name);
            if (function_declaration.has_value())
            {
                Function_declaration const* const function_declaration_value = function_declaration.value();

                llvm::Function* llvm_function = get_llvm_function(parameters.core_module, parameters.llvm_module, variable_name);
                if (llvm_function == nullptr)
                {
                    Type_reference const function_type_reference = create_function_type_type_reference(
                        function_declaration_value->type,
                        function_declaration_value->input_parameter_names,
                        function_declaration_value->output_parameter_names
                    );

                    Function_pointer_type const& function_pointer_type = std::get<Function_pointer_type>(function_type_reference.data);
                    llvm::FunctionType* const llvm_function_type = convert_to_llvm_function_type(
                        parameters.clang_module_data,
                        parameters.declaration_database,
                        function_pointer_type.type
                    );

                    llvm_function = &to_function(
                        parameters.llvm_context,
                        parameters.llvm_data_layout,
                        parameters.clang_module_data,
                        parameters.core_module.name,
                        *llvm_function_type,
                        *function_declaration.value(),
                        parameters.type_database,
                        parameters.declaration_database
                    );

                    parameters.llvm_module.getFunctionList().push_back(llvm_function);
                }

                Type_reference type = create_function_type_type_reference(function_declaration_value->type, function_declaration_value->input_parameter_names, function_declaration_value->output_parameter_names);

                return Value_and_type
                {
                    .name = expression.name,
                    .value = llvm_function,
                    .type = std::move(type)
                };
            }
        }

        // Search for alias in this module:
        {
            std::optional<Alias_type_declaration const*> const declaration = find_alias_type_declaration(parameters.core_module, variable_name);
            if (declaration.has_value())
            {
                std::optional<Type_reference> type = get_underlying_type(parameters.declaration_database, *declaration.value());

                return Value_and_type
                {
                    .name = expression.name,
                    .value = nullptr,
                    .type = std::move(type)
                };
            }
        }

        // Search for enums in this module:
        {
            std::optional<Enum_declaration const*> const declaration = find_enum_declaration(parameters.core_module, variable_name);
            if (declaration.has_value())
            {
                Enum_declaration const& enum_declaration = *declaration.value();
                Type_reference type = create_custom_type_reference(parameters.core_module.name, enum_declaration.name);

                return Value_and_type
                {
                    .name = expression.name,
                    .value = nullptr,
                    .type = std::move(type)
                };
            }
        }

        // Search for global variables:
        {
            std::optional<Global_variable_declaration const*> const declaration = find_global_variable_declaration(parameters.core_module, variable_name);
            if (declaration.has_value())
            {
                Global_variable_declaration const& global_variable_declaration = *declaration.value();

                std::optional<Value_and_type> const global_variable = get_global_variable_value_and_type(
                    parameters.core_module,
                    global_variable_declaration,
                    parameters
                );
                if (global_variable.has_value())
                    return *global_variable;
            }
        }

        // Search for module dependencies:
        {
            std::optional<std::string_view> const module_name = get_module_name_from_alias(parameters.core_module, variable_name);
            if (module_name.has_value())
            {
                return Value_and_type
                {
                    .name = expression.name,
                    .value = nullptr,
                    .type = std::nullopt
                };
            }
        }

        // Search for builtin types:
        {
            if (variable_name == "Type_kind")
            {
                Type_reference type = create_custom_type_reference("iris.builtin", "Type_kind");

                return Value_and_type
                {
                    .name = expression.name,
                    .value = nullptr,
                    .type = std::move(type)
                };
            }
        }

        throw std::runtime_error{ format_error(std::format("Undefined variable '{}'", variable_name), parameters.source_position) };
    }

    bool is_true_constant(iris::Statement const& statement)
    {
        if (statement.expressions.size() != 1)
            return false;

        iris::Expression const& expression = statement.expressions[0];
        if (std::holds_alternative<iris::Constant_expression>(expression.data))
        {
            iris::Constant_expression const& constant_expression = std::get<iris::Constant_expression>(expression.data);
            if (is_bool(constant_expression.type))
            {
                return constant_expression.data == "true";
            }
            else if (is_c_bool(constant_expression.type))
            {
                if (constant_expression.data == "false")
                    return false;
                return constant_expression.data != "0";
            }
        }

        return false;
    }

    void create_while_loop_then_block(
        While_loop_expression const& expression,
        Expression_parameters const& parameters,
        std::span<Block_info> const all_block_infos,
        llvm::BasicBlock* const condition_block,
        llvm::BasicBlock* const then_block,
        llvm::BasicBlock* const after_block
    )
    {
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;

        std::pmr::vector<std::pmr::vector<Statement>> defer_expressions_per_block = create_defer_block(parameters.defer_expressions_per_block);

        Expression_parameters then_block_parameters = parameters;
        then_block_parameters.blocks = all_block_infos;
        then_block_parameters.defer_expressions_per_block = defer_expressions_per_block;

        llvm_builder.SetInsertPoint(then_block);

        if (parameters.debug_info != nullptr && parameters.source_position.has_value())
            push_debug_lexical_block_scope(*parameters.debug_info, *parameters.source_position);

        create_statement_values(
            expression.then_statements,
            then_block_parameters,
            true
        );
        if (!ends_with_terminator_statement(expression.then_statements))
        {
            iris::Expression const& condition_expression = expression.condition.expressions[0];
            if (parameters.debug_info != nullptr && condition_expression.source_range.has_value())
                set_debug_location(parameters.llvm_builder, *parameters.debug_info, condition_expression.source_range->start.line, condition_expression.source_range->start.column);

            llvm_builder.CreateBr(condition_block);
        }

        if (parameters.debug_info != nullptr && parameters.source_position.has_value())
            pop_debug_scope(*parameters.debug_info);

        llvm_builder.SetInsertPoint(after_block);
    }


    Value_and_type create_while_loop_expression_value(
        While_loop_expression const& expression,
        Expression_parameters const& parameters
    )
    {
        llvm::LLVMContext& llvm_context = parameters.llvm_context;
        llvm::IRBuilder<>& llvm_builder = parameters.llvm_builder;
        llvm::Function* const llvm_parent_function = parameters.llvm_parent_function;
        std::span<Block_info const> block_infos = parameters.blocks;

        if (is_true_constant(expression.condition))
        {
            llvm::BasicBlock* const then_block = llvm::BasicBlock::Create(llvm_context, "while_loop_then", llvm_parent_function);
            llvm::BasicBlock* const after_block = llvm::BasicBlock::Create(llvm_context, "while_loop_after", llvm_parent_function);

            std::pmr::vector<Block_info> all_block_infos{ block_infos.begin(), block_infos.end() };
            all_block_infos.push_back(
                Block_info
                {
                    .block_type = Block_type::While_loop,
                    .repeat_block = then_block,
                    .after_block = after_block,
                }
            );

            iris::Expression const& condition_expression = expression.condition.expressions[0];
            if (parameters.debug_info != nullptr && condition_expression.source_range.has_value())
                set_debug_location(parameters.llvm_builder, *parameters.debug_info, condition_expression.source_range->start.line, condition_expression.source_range->start.column);

            llvm_builder.CreateBr(then_block);

            create_while_loop_then_block(
                expression,
                parameters,
                all_block_infos,
                then_block,
                then_block,
                after_block
            );

            return Value_and_type
            {
                .name = "",
                .value = after_block,
                .type = std::nullopt
            };
        }

        llvm::BasicBlock* const condition_block = llvm::BasicBlock::Create(llvm_context, "while_loop_condition", llvm_parent_function);
        llvm::BasicBlock* const then_block = llvm::BasicBlock::Create(llvm_context, "while_loop_then", llvm_parent_function);
        llvm::BasicBlock* const after_block = llvm::BasicBlock::Create(llvm_context, "while_loop_after", llvm_parent_function);

        std::pmr::vector<Block_info> all_block_infos{ block_infos.begin(), block_infos.end() };
        all_block_infos.push_back(
            Block_info
            {
                .block_type = Block_type::While_loop,
                .repeat_block = condition_block,
                .after_block = after_block,
            }
            );

        llvm_builder.CreateBr(condition_block);
        llvm_builder.SetInsertPoint(condition_block);
        Value_and_type const& condition_value = create_loaded_statement_value(
            expression.condition,
            parameters
        );
        llvm::Value* const condition_converted_value = convert_to_boolean(llvm_context, llvm_builder, condition_value.value, condition_value.type);
        llvm_builder.CreateCondBr(condition_converted_value, then_block, after_block);

        create_while_loop_then_block(
            expression,
            parameters,
            all_block_infos,
            condition_block,
            then_block,
            after_block
        );

        return Value_and_type
        {
            .name = "",
            .value = after_block,
            .type = std::nullopt
        };
    }

    Value_and_type create_expression_value(
        Expression const& expression,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        Expression_parameters new_parameters = parameters;

        if (expression.source_range.has_value())
        {
            Source_position const source_position = expression.source_range->start;
            new_parameters.source_position = source_position;
        }
        else
        {
            new_parameters.source_position = std::nullopt;
        }

        if (std::holds_alternative<Access_expression>(expression.data))
        {
            Access_expression const& data = std::get<Access_expression>(expression.data);
            return create_access_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Access_array_expression>(expression.data))
        {
            Access_array_expression const& data = std::get<Access_array_expression>(expression.data);
            return create_access_array_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Assert_expression>(expression.data))
        {
            Assert_expression const& data = std::get<Assert_expression>(expression.data);
            return create_assert_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Assignment_expression>(expression.data))
        {
            Assignment_expression const& data = std::get<Assignment_expression>(expression.data);
            return create_assignment_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Binary_expression>(expression.data))
        {
            Binary_expression const& data = std::get<Binary_expression>(expression.data);
            return create_binary_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Block_expression>(expression.data))
        {
            Block_expression const& data = std::get<Block_expression>(expression.data);
            return create_block_expression_value(data, new_parameters);
        }
        else if (std::holds_alternative<Break_expression>(expression.data))
        {
            Break_expression const& data = std::get<Break_expression>(expression.data);
            return create_break_expression_value(data, new_parameters.llvm_builder, new_parameters.blocks, new_parameters);
        }
        else if (std::holds_alternative<Call_expression>(expression.data))
        {
            Call_expression const& data = std::get<Call_expression>(expression.data);
            return create_call_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Cast_expression>(expression.data))
        {
            Cast_expression const& data = std::get<Cast_expression>(expression.data);
            return create_cast_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Constant_expression>(expression.data))
        {
            Constant_expression const& data = std::get<Constant_expression>(expression.data);
            return create_constant_expression_value(data, new_parameters.llvm_context, new_parameters.llvm_data_layout, new_parameters.llvm_module, new_parameters.core_module, new_parameters.declaration_database, new_parameters.type_database, new_parameters.source_position);
        }
        else if (std::holds_alternative<Constant_array_expression>(expression.data))
        {
            Constant_array_expression const& data = std::get<Constant_array_expression>(expression.data);
            return create_constant_array_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Continue_expression>(expression.data))
        {
            Continue_expression const& data = std::get<Continue_expression>(expression.data);
            return create_continue_expression_value(data, new_parameters.llvm_builder, new_parameters.blocks, new_parameters);
        }
        else if (std::holds_alternative<Defer_expression>(expression.data))
        {
            if (parameters.defer_expressions_per_block.empty())
                throw std::runtime_error{ format_error("Can only have defer expressions inside function blocks!", parameters.source_position) };

            std::pmr::vector<Statement>& current_block_defer_expressions = parameters.defer_expressions_per_block.back();
            current_block_defer_expressions.push_back(statement);
            return {};
        }
        else if (std::holds_alternative<Dereference_and_access_expression>(expression.data))
        {
            Dereference_and_access_expression const& data = std::get<Dereference_and_access_expression>(expression.data);
            return create_dereference_and_access_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<For_loop_expression>(expression.data))
        {
            For_loop_expression const& data = std::get<For_loop_expression>(expression.data);
            return create_for_loop_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<If_expression>(expression.data))
        {
            If_expression const& data = std::get<If_expression>(expression.data);
            return create_if_expression_value(data, new_parameters);
        }
        else if (std::holds_alternative<Instance_call_expression>(expression.data))
        {
            Instance_call_expression const& data = std::get<Instance_call_expression>(expression.data);
            return create_instance_call_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Instantiate_expression>(expression.data))
        {
            Instantiate_expression const& data = std::get<Instantiate_expression>(expression.data);
            return create_instantiate_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Null_pointer_expression>(expression.data))
        {
            return create_null_pointer_expression_value(statement, new_parameters);
        }
        else if (std::holds_alternative<Parenthesis_expression>(expression.data))
        {
            Parenthesis_expression const& data = std::get<Parenthesis_expression>(expression.data);
            return create_parenthesis_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Reflection_expression>(expression.data))
        {
            throw std::runtime_error{ format_error("Reflection_expression should have been handled in the Compile_time_pass!", parameters.source_position) };
        }
        else if (std::holds_alternative<Return_expression>(expression.data))
        {
            Return_expression const& data = std::get<Return_expression>(expression.data);
            return create_return_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Switch_expression>(expression.data))
        {
            Switch_expression const& data = std::get<Switch_expression>(expression.data);
            return create_switch_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Ternary_condition_expression>(expression.data))
        {
            Ternary_condition_expression const& data = std::get<Ternary_condition_expression>(expression.data);
            return create_ternary_condition_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Type_expression>(expression.data))
        {
            Type_expression const& data = std::get<Type_expression>(expression.data);
            return create_type_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Unary_expression>(expression.data))
        {
            Unary_expression const& data = std::get<Unary_expression>(expression.data);
            return create_unary_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Variable_declaration_expression>(expression.data))
        {
            Variable_declaration_expression const& data = std::get<Variable_declaration_expression>(expression.data);
            return create_variable_declaration_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Variable_declaration_with_type_expression>(expression.data))
        {
            Variable_declaration_with_type_expression const& data = std::get<Variable_declaration_with_type_expression>(expression.data);
            return create_variable_declaration_with_type_expression_value(data, statement, new_parameters);
        }
        else if (std::holds_alternative<Variable_expression>(expression.data))
        {
            Variable_expression const& data = std::get<Variable_expression>(expression.data);
            return create_variable_expression_value(data, new_parameters);
        }
        else if (std::holds_alternative<While_loop_expression>(expression.data))
        {
            While_loop_expression const& data = std::get<While_loop_expression>(expression.data);
            return create_while_loop_expression_value(data, new_parameters);
        }
        else
        {
            //static_assert(always_false_v<Expression_type>, "non-exhaustive visitor!");
            throw std::runtime_error{ format_error("Did not handle expression type!", parameters.source_position) };
        }
    }

    bool is_comment(
        Statement const& statement
    )
    {
        if (statement.expressions.size() == 1)
        {
            return std::holds_alternative<Comment_expression>(statement.expressions[0].data);
        }

        return false;
    }

    static bool is_global_array_variable(llvm::Value* const value)
    {
        if (!llvm::GlobalVariable::classof(value))
            return false;

        llvm::GlobalVariable const* const global_variable = static_cast<llvm::GlobalVariable*>(value);
        return global_variable->getValueType()->isArrayTy();
    }

    Value_and_type load_if_needed(
        Value_and_type const& value,
        std::size_t const expression_index,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        if (value.value != nullptr && value.type.has_value() && value.value->getType()->isPointerTy())
        {
            if (std::holds_alternative<Null_pointer_type>(value.type->data) && !llvm::AllocaInst::classof(value.value) && !llvm::GetElementPtrInst::classof(value.value) && !llvm::GlobalVariable::classof(value.value))
                return value;

            iris::Expression const& expression = statement.expressions[expression_index];

            if (llvm::AllocaInst::classof(value.value) || llvm::GetElementPtrInst::classof(value.value) || llvm::GlobalVariable::classof(value.value))
            {
                if (iris::is_expression_address_of(expression) || iris::is_offset_pointer(statement, expression) || is_reinterpret_as_expression(statement, expression) || is_global_array_variable(value.value))
                {
                    return value;
                }
                else
                {
                    llvm::Type* const destination_llvm_type = type_reference_to_llvm_type(parameters.llvm_context, parameters.llvm_data_layout, value.type.value(), parameters.type_database);
                    llvm::Value* const loaded_value = create_load_instruction(parameters.llvm_builder, parameters.llvm_data_layout, destination_llvm_type, value.value);
                    return Value_and_type
                    {
                        .name = value.name,
                        .value = loaded_value,
                        .type = value.type
                    };
                }
            }

            llvm::Type* const llvm_type = type_reference_to_llvm_type(parameters.llvm_context, parameters.llvm_data_layout, value.type.value(), parameters.type_database);
            if (llvm_type == value.value->getType() || llvm_type->isFunctionTy())
            {
                return value;
            }

            llvm::Value* const loaded_value = parameters.llvm_builder.CreateLoad(llvm_type, value.value);
            return Value_and_type
            {
                .name = value.name,
                .value = loaded_value,
                .type = value.type
            };
        }
        else
        {
            return value;
        }
    }

    Value_and_type create_loaded_expression_value(
        std::size_t const expression_index,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        Value_and_type value = create_expression_value(expression_index, statement, parameters);
        return load_if_needed(value, expression_index, statement, parameters);
    }

    Value_and_type create_expression_value(
        std::size_t const expression_index,
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        return create_expression_value(statement.expressions[expression_index], statement, parameters);
    }

    Value_and_type create_statement_value(
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        return create_expression_value(0, statement, parameters);
    }

    Value_and_type create_loaded_statement_value(
        Statement const& statement,
        Expression_parameters const& parameters
    )
    {
        return create_loaded_expression_value(0, statement, parameters);
    }

    void create_statement_values(
        std::span<Statement const> const statements,
        Expression_parameters const& parameters,
        bool const create_defer_expressions_at_end
    )
    {
        std::pmr::vector<Value_and_type> all_local_variables;
        all_local_variables.reserve(parameters.local_variables.size() + statements.size());
        all_local_variables.insert(all_local_variables.begin(), parameters.local_variables.begin(), parameters.local_variables.end());

        Expression_parameters new_parameters = parameters;

        for (Statement const& statement : statements)
        {
            if (!is_comment(statement) && !statement.expressions.empty())
            {
                new_parameters.local_variables = all_local_variables;

                Value_and_type statement_value = create_statement_value(
                    statement,
                    new_parameters
                );

                if (!statement_value.name.empty())
                    all_local_variables.push_back(statement_value);
            }
        }

        if (create_defer_expressions_at_end && !ends_with_terminator_statement(statements))
        {
            new_parameters.local_variables = all_local_variables;
            create_instructions_at_end_of_block(new_parameters);
        }
    }
    
    void create_defer_instructions_at_end_of_block(
        Expression_parameters const& parameters,
        std::size_t const block_index
    )
    {
        llvm::DebugLoc const previous_debug_location = parameters.llvm_builder.getCurrentDebugLocation();

        std::span<Statement const> const block_defer_expressions = parameters.defer_expressions_per_block[block_index];

        for (std::size_t expression_index = 0; expression_index < block_defer_expressions.size(); ++expression_index)
        {
            std::size_t const reverse_expression_index = block_defer_expressions.size() - 1 - expression_index;
            Statement const& defer_expression_statement = block_defer_expressions[reverse_expression_index];

            if (!defer_expression_statement.expressions.empty())
            {
                Expression const& first_expression = defer_expression_statement.expressions[0];
                if (std::holds_alternative<Defer_expression>(first_expression.data))
                {
                    Defer_expression const defer_expression = std::get<Defer_expression>(first_expression.data);
                    
                    create_expression_value(
                        defer_expression.expression_to_defer.expression_index,
                        defer_expression_statement,
                        parameters
                    ); 
                }
            }
        }

        parameters.llvm_builder.SetCurrentDebugLocation(previous_debug_location);
    }

    void create_instructions_at_end_of_block(
        Expression_parameters const& parameters,
        std::size_t const block_index
    )
    {
        create_defer_instructions_at_end_of_block(parameters, block_index);

        Block_info const& block = parameters.blocks[block_index];
        if (block.stack_save_pointer != nullptr)
            create_free_dynamic_array_instruction(block.stack_save_pointer, parameters.llvm_builder, parameters.llvm_module);
    }

    void create_instructions_at_end_of_block(
        Expression_parameters const& parameters
    )
    {
        assert(parameters.blocks.size() == parameters.defer_expressions_per_block.size());
        
        if (!parameters.blocks.empty())
            create_instructions_at_end_of_block(parameters, parameters.blocks.size() - 1);
    }

    void create_instructions_pop_blocks(
        Expression_parameters const& parameters,
        std::size_t const blocks_to_pop_count
    )
    {
        assert(parameters.blocks.size() == parameters.defer_expressions_per_block.size());

        for (std::size_t block_index = 0; block_index < blocks_to_pop_count; ++block_index)
        {
            std::size_t const reverse_block_index = parameters.blocks.size() - 1 - block_index;
            create_instructions_at_end_of_block(parameters, reverse_block_index);
        }
    }

    void create_instructions_at_return(
        Expression_parameters const& parameters
    )
    {
        assert(parameters.blocks.size() == parameters.defer_expressions_per_block.size());

        create_instructions_pop_blocks(parameters, parameters.blocks.size());
    }

    std::string_view condition_type_to_string(Condition_type const type)
    {
        switch (type)
        {
        case Condition_type::Assert: {
            return "assert";
        }
        case Condition_type::Precondition: {
            return "precondition";
        }
        case Condition_type::Postcondition:
        default: {
            return "postcondition";
        }
        }
    }

    void create_check_condition_instructions(
        llvm::LLVMContext& llvm_context,
        llvm::Module& llvm_module,
        llvm::Function& llvm_function,
        llvm::IRBuilder<>& llvm_builder,
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        iris::Function_condition const& function_condition,
        Condition_type const condition_type,
        Expression_parameters const& expression_parameters
    )
    {
        Value_and_type const condition_value = create_statement_value(
            function_condition.condition,
            expression_parameters
        );

        bool const is_boolean_expression = condition_value.type.has_value() && (is_bool(*condition_value.type) || is_c_bool(*condition_value.type));
        if (!is_boolean_expression)
            throw std::runtime_error{ format_error(std::format("In function '{}', condition '{}', expression does not evaluate to a boolean value.", function_declaration.name, function_condition.description), expression_parameters.source_position) };
        
        llvm::BasicBlock* const success_block = llvm::BasicBlock::Create(llvm_context, "condition_success", &llvm_function);
        llvm::BasicBlock* const fail_block = llvm::BasicBlock::Create(llvm_context, "condition_fail", &llvm_function);

        llvm::Value* const condition_converted_value = convert_to_boolean(llvm_context, llvm_builder, condition_value.value, condition_value.type);
        llvm_builder.CreateCondBr(condition_converted_value, success_block, fail_block);

        llvm_builder.SetInsertPoint(fail_block);

        std::string const error_message = std::format("In function '{}.{}' {} '{}' failed!", core_module.name, function_declaration.name, condition_type_to_string(condition_type), function_condition.description);
        create_log_error_instruction(llvm_context, llvm_module, llvm_builder, error_message.c_str());
        create_abort_instruction(llvm_context, llvm_module, llvm_builder);
        llvm_builder.CreateUnreachable();

        llvm_builder.SetInsertPoint(success_block);
    }

    void create_function_preconditions(
        llvm::LLVMContext& llvm_context,
        llvm::Module& llvm_module,
        llvm::Function& llvm_function,
        llvm::IRBuilder<>& llvm_builder,
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        Expression_parameters const& expression_parameters
    )
    {
        for (Function_condition const& precondition : function_declaration.preconditions)
        {
            create_check_condition_instructions(
                llvm_context,
                llvm_module,
                llvm_function,
                llvm_builder,
                core_module,
                function_declaration,
                precondition,
                Condition_type::Precondition,
                expression_parameters
            );
        }
    }

    void create_function_postconditions(
        llvm::LLVMContext& llvm_context,
        llvm::Module& llvm_module,
        llvm::Function& llvm_function,
        llvm::IRBuilder<>& llvm_builder,
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        Expression_parameters const& expression_parameters
    )
    {
        for (Function_condition const& postcondition : function_declaration.postconditions)
        {
            create_check_condition_instructions(
                llvm_context,
                llvm_module,
                llvm_function,
                llvm_builder,
                core_module,
                function_declaration,
                postcondition,
                Condition_type::Postcondition,
                expression_parameters
            );
        }
    }
}
