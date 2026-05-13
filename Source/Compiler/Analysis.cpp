module;

#include <assert.h>

module iris.compiler.analysis;

import std;

import iris.compiler.diagnostic;
import iris.compiler.validation;
import iris.core;
import iris.core.declarations;
import iris.core.types;

namespace iris::compiler
{
    Variable create_variable(
        std::pmr::string name,
        iris::Type_reference type,
        bool is_mutable,
        bool is_compile_time,
        std::optional<iris::Source_position> source_position
    )
    {
        return
        {
            .name = std::move(name),
            .type = std::move(type),
            .is_mutable = is_mutable,
            .is_compile_time = is_compile_time,
            .source_position = source_position,
        };
    }

    Variable create_variable(
        std::pmr::string name,
        iris::Type_reference type,
        bool is_mutable,
        bool is_compile_time,
        std::optional<iris::Source_range> source_range
    )
    {
        return
        {
            .name = std::move(name),
            .type = std::move(type),
            .is_mutable = is_mutable,
            .is_compile_time = is_compile_time,
            .source_position = source_range.has_value() ? std::optional<iris::Source_position>{source_range->start} : std::optional<iris::Source_position>{std::nullopt},
        };
    }

    Analysis_result process_module(
        iris::Module& core_module,
        iris::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        Analysis_result result;

        if (options.validate)
        {
            std::pmr::vector<iris::compiler::Diagnostic> const diagnostics = validate_module(
                core_module,
                declaration_database,
                temporaries_allocator
            );
            if (!diagnostics.empty())
            {
                result.diagnostics.insert(result.diagnostics.end(), diagnostics.begin(), diagnostics.end());
                return result;
            }
        }

        process_declarations(result, core_module, core_module.export_declarations, core_module.definitions, declaration_database, options, temporaries_allocator);
        process_declarations(result, core_module, core_module.internal_declarations, core_module.definitions, declaration_database, options, temporaries_allocator);
        return result;
    }

    void process_declarations(
        Analysis_result& result,
        iris::Module& core_module,
        Module_declarations& declarations,
        Module_definitions& definitions,
        iris::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        for (iris::Function_declaration& declaration : declarations.function_declarations)
        {
            auto location = std::find_if(
                definitions.function_definitions.begin(),
                definitions.function_definitions.end(),
                [&declaration](Function_definition const& definition) {
                    return definition.name == declaration.name;
                });

            if (location != definitions.function_definitions.end())
                process_function(result, core_module, declaration, *location, declaration_database, options, temporaries_allocator);
        }
    }

    void add_parameters_to_scope(
        Scope& scope,
        std::span<std::pmr::string const> const parameter_names,
        std::span<iris::Type_reference const> const parameter_types,
        std::optional<std::pmr::vector<Source_position>> const parameter_source_positions
    )
    {
        for (std::size_t parameter_index = 0; parameter_index < parameter_names.size(); ++parameter_index)
        {
            scope.variables.push_back(
                create_variable(
                    parameter_names[parameter_index],
                    parameter_types[parameter_index],
                    false,
                    false,
                    parameter_source_positions.has_value() ? std::optional<iris::Source_position>{(*parameter_source_positions)[parameter_index]} : std::optional<iris::Source_position>{std::nullopt}
                )
            );
        }
    }

    void process_function(
        Analysis_result& result,
        iris::Module& core_module,
        iris::Function_declaration& function_declaration,
        iris::Function_definition& function_definition,
        iris::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        Scope scope{ .variables = std::pmr::vector<Variable>{temporaries_allocator} };
        scope.variables.reserve(64);

        add_parameters_to_scope(scope, function_declaration.input_parameter_names, function_declaration.type.input_parameter_types, function_declaration.input_parameter_source_positions);

        for (iris::Function_condition& condition : function_declaration.preconditions)
        {
            process_statement(
                result,
                core_module,
                &function_declaration,
                scope,
                condition.condition,
                std::nullopt,
                declaration_database,
                options,
                temporaries_allocator
            );
        }

        {
            add_parameters_to_scope(scope, function_declaration.output_parameter_names, function_declaration.type.output_parameter_types, function_declaration.output_parameter_source_positions);

            for (iris::Function_condition& condition : function_declaration.postconditions)
            {
                process_statement(
                    result,
                    core_module,
                    &function_declaration,
                    scope,
                    condition.condition,
                    std::nullopt,
                    declaration_database,
                    options,
                    temporaries_allocator
                );
            }

            for (std::size_t index = 0; index < function_declaration.output_parameter_names.size(); ++index)
            {
                scope.variables.pop_back();
            }
        }

        process_block(
            result,
            core_module,
            &function_declaration,
            scope,
            function_definition.statements,
            declaration_database,
            options,
            temporaries_allocator
        );
    }

    void process_block(
        Analysis_result& result,
        iris::Module& core_module,
        iris::Function_declaration const* const function_declaration,
        Scope& scope,
        std::span<Statement> const statements,
        iris::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::size_t const original_count = scope.variables.size();

        for (Statement& statement : statements)
        {
            process_statement(
                result,
                core_module,
                function_declaration,
                scope,
                statement,
                std::nullopt,
                declaration_database,
                options,
                temporaries_allocator
            );
        }

        while (scope.variables.size() > original_count)
        {
            scope.variables.pop_back();
        }
    }

    void process_statements(
        Analysis_result& result,
        iris::Module& core_module,
        iris::Function_declaration const* const function_declaration,
        Scope& scope,
        std::span<Statement> const statements,
        iris::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        for (Statement& statement : statements)
        {
            process_statement(
                result,
                core_module,
                function_declaration,
                scope,
                statement,
                std::nullopt,
                declaration_database,
                options,
                temporaries_allocator
            );
        }
    }

    void process_statement(
        Analysis_result& result,
        iris::Module& core_module,
        iris::Function_declaration const* const function_declaration,
        Scope& scope,
        iris::Statement& statement,
        std::optional<iris::Type_reference> const& expected_statement_type,
        iris::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::size_t count = statement.expressions.size();
        for (std::size_t index = 0; index < count; ++index)
        {
            std::size_t reverse_index = count - index - 1;
            iris::Expression& expression = statement.expressions[reverse_index];

            std::optional<iris::Statement> new_statement = process_expression(
                result,
                core_module,
                function_declaration,
                scope,
                statement,
                expression,
                reverse_index,
                declaration_database,
                options,
                temporaries_allocator
            );
            if (new_statement.has_value())
            {
                process_statement(
                    result,
                    core_module,
                    function_declaration,
                    scope,
                    new_statement.value(),
                    expected_statement_type,
                    declaration_database,
                    options,
                    temporaries_allocator
                );

                statement = new_statement.value();
                return;
            }
        }
    }

    std::optional<iris::Statement> process_expression(
        Analysis_result& result,
        iris::Module& core_module,
        iris::Function_declaration const* const function_declaration,
        Scope& scope,
        iris::Statement& statement,
        iris::Expression& expression,
        std::size_t const expression_index,
        iris::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        if (std::holds_alternative<iris::Block_expression>(expression.data))
        {
            iris::Block_expression& data = std::get<iris::Block_expression>(expression.data);
            process_block(
                result,
                core_module,
                function_declaration,
                scope,
                data.statements,
                declaration_database,
                options,
                temporaries_allocator
            );
        }
        else if (std::holds_alternative<iris::Constant_array_expression>(expression.data))
        {
            iris::Constant_array_expression& data = std::get<iris::Constant_array_expression>(expression.data);
            process_statements(
                result,
                core_module,
                function_declaration,
                scope,
                data.array_data,
                declaration_database,
                options,
                temporaries_allocator
            );
        }
        else if (std::holds_alternative<iris::For_loop_expression>(expression.data))
        {
            iris::For_loop_expression& data = std::get<iris::For_loop_expression>(expression.data);

            std::optional<iris::Type_reference> const type_reference = get_expression_type(core_module.name, nullptr, scope, statement, statement.expressions[data.range_begin.expression_index], std::nullopt, declaration_database);
            if (type_reference.has_value())
            {
                scope.variables.push_back(
                    create_variable(data.variable_name, type_reference.value(), true, false, expression.source_range)
                );
            }

            process_statement(
                result,
                core_module,
                function_declaration,
                scope,
                data.range_end,
                std::nullopt,
                declaration_database,
                options,
                temporaries_allocator
            );
            process_block(
                result,
                core_module,
                function_declaration,
                scope,
                data.then_statements,
                declaration_database,
                options,
                temporaries_allocator
            );
        }
        else if (std::holds_alternative<iris::Function_expression>(expression.data))
        {
            iris::Function_expression& data = std::get<iris::Function_expression>(expression.data);
            process_function(
                result,
                core_module,
                data.declaration,
                data.definition, // TODO pass scope?
                declaration_database,
                options,
                temporaries_allocator
            );
        }
        else if (std::holds_alternative<iris::Instance_call_expression>(expression.data))
        {
            iris::Instance_call_expression& data = std::get<iris::Instance_call_expression>(expression.data);
            process_statements(
                result,
                core_module,
                function_declaration,
                scope,
                data.arguments,
                declaration_database,
                options,
                temporaries_allocator
            );
        }
        else if (std::holds_alternative<iris::If_expression>(expression.data))
        {
            iris::If_expression& data = std::get<iris::If_expression>(expression.data);
            for (iris::Condition_statement_pair& serie : data.series)
            {
                if (serie.condition.has_value())
                {
                    process_statement(
                        result,
                        core_module,
                        function_declaration,
                        scope,
                        serie.condition.value(),
                        std::nullopt,
                        declaration_database,
                        options,
                        temporaries_allocator
                    );
                }

                process_block(
                    result,
                    core_module,
                    function_declaration,
                    scope,
                    serie.then_statements,
                    declaration_database,
                    options,
                    temporaries_allocator
                );
            }
        }
        else if (std::holds_alternative<iris::Switch_expression>(expression.data))
        {
            iris::Switch_expression& data = std::get<iris::Switch_expression>(expression.data);
            for (iris::Switch_case_expression_pair& serie : data.cases)
            {
                process_block(
                    result,
                    core_module,
                    function_declaration,
                    scope,
                    serie.statements,
                    declaration_database,
                    options,
                    temporaries_allocator
                );
            }
        }
        else if (std::holds_alternative<iris::Ternary_condition_expression>(expression.data))
        {
            iris::Ternary_condition_expression& data = std::get<iris::Ternary_condition_expression>(expression.data);
            process_statement(
                result,
                core_module,
                function_declaration,
                scope,
                data.then_statement,
                std::nullopt,
                declaration_database,
                options,
                temporaries_allocator
            );
            process_statement(
                result,
                core_module,
                function_declaration,
                scope,
                data.else_statement,
                std::nullopt,
                declaration_database,
                options,
                temporaries_allocator
            );
        }
        else if (std::holds_alternative<iris::Variable_declaration_expression>(expression.data))
        {
            iris::Variable_declaration_expression& data = std::get<iris::Variable_declaration_expression>(expression.data);
            std::optional<iris::Type_reference> const type_reference = get_expression_type(core_module.name, nullptr, scope, statement, statement.expressions[data.right_hand_side.expression_index], std::nullopt, declaration_database);
            if (type_reference.has_value())
                scope.variables.push_back(
                    create_variable(data.name, type_reference.value(), data.is_mutable, false, expression.source_range)
                );
            
            // TODO error if type is nullopt
        }
        else if (std::holds_alternative<iris::Variable_declaration_with_type_expression>(expression.data))
        {
            iris::Variable_declaration_with_type_expression& data = std::get<iris::Variable_declaration_with_type_expression>(expression.data);

            std::optional<iris::Type_reference> const variable_type = iris::get_variable_declaration_with_type_expression_type(statement, data);
            if (variable_type.has_value())
            {
                scope.variables.push_back(
                    create_variable(data.name, std::move(variable_type.value()), data.is_mutable, false, expression.source_range)
                );
            }
        }
        else if (std::holds_alternative<iris::While_loop_expression>(expression.data))
        {
            iris::While_loop_expression& data = std::get<iris::While_loop_expression>(expression.data);
            process_statement(
                result,
                core_module,
                function_declaration,
                scope,
                data.condition,
                std::nullopt,
                declaration_database,
                options,
                temporaries_allocator
            );
            process_block(
                result,
                core_module,
                function_declaration,
                scope,
                data.then_statements,
                declaration_database,
                options,
                temporaries_allocator
            );
        }

        return std::nullopt;
    }

    std::optional<iris::Type_reference> get_expression_type(
        std::string_view const module_name,
        iris::Function_declaration const* const function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        std::optional<iris::Type_reference> const& expected_statement_type,
        iris::Declaration_database const& declaration_database
    )
    {
        if (statement.expressions.empty())
            return std::nullopt;

        return get_expression_type(
            module_name,
            function_declaration,
            scope,
            statement,
            statement.expressions[0],
            expected_statement_type,
            declaration_database
        );
    }

    
    iris::Function_declaration const* get_function_declaration_to_call(
        std::string_view const module_name,
        iris::Statement const& statement,
        iris::Expression const& expression,
        iris::Declaration_database const& declaration_database
    )
    {
        if (std::holds_alternative<iris::Access_expression>(expression.data))
        {
            //iris::Access_expression const& access_expression = std::get<iris::Access_expression>(expression.data); // TODO
            assert(false);
        }
        else if (std::holds_alternative<iris::Variable_expression>(expression.data))
        {
            iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(expression.data);

            std::optional<Declaration> const declaration = find_declaration(declaration_database, module_name, variable_expression.name);
            if (declaration.has_value() && std::holds_alternative<iris::Function_declaration const*>(declaration->data))
                return std::get<iris::Function_declaration const*>(declaration->data);
        }

        return nullptr;
    }

    std::optional<iris::Type_reference> get_type_to_instantiate(
        std::string_view const module_name,
        iris::Function_declaration const* const function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& expression,
        iris::Instantiate_expression const& instantiate_expression,
        iris::Declaration_database const& declaration_database
    )
    {
        std::uint64_t expression_index = statement.expressions.size();
        for (std::size_t index = 0; index < statement.expressions.size(); ++index)
        {
            if (&statement.expressions[index] == &expression)
            {
                expression_index = index;
                break;
            }
        }

        if (expression_index == statement.expressions.size())
            return std::nullopt;

        for (std::size_t index = 0; index < statement.expressions.size(); ++index)
        {
            iris::Expression const& current_expression = statement.expressions[index];
            
            if (std::holds_alternative<iris::Assignment_expression>(current_expression.data))
            {
                iris::Assignment_expression const& assignment_expression = std::get<iris::Assignment_expression>(current_expression.data);

                if (assignment_expression.right_hand_side.expression_index == expression_index)
                {
                    std::optional<iris::Type_reference> const left_hand_side_type = get_expression_type(
                        module_name,
                        function_declaration,
                        scope,
                        statement,
                        statement.expressions[assignment_expression.left_hand_side.expression_index],
                        std::nullopt,
                        declaration_database
                    );
                    return left_hand_side_type;
                }
            }
            else if (std::holds_alternative<iris::Call_expression>(current_expression.data))
            {
                iris::Call_expression const& call_expression = std::get<iris::Call_expression>(current_expression.data);

                for (std::size_t argument_index = 0; argument_index < call_expression.arguments.size(); ++argument_index)
                {
                    iris::Expression_index const& argument_expression = call_expression.arguments[argument_index];
                    if (argument_expression.expression_index == expression_index)
                    {
                        iris::Function_declaration const* const function_declaration_to_call = get_function_declaration_to_call(
                            module_name,
                            statement,
                            statement.expressions[call_expression.expression.expression_index],
                            declaration_database
                        );
                        if (function_declaration_to_call != nullptr && argument_index < function_declaration_to_call->type.input_parameter_types.size())
                            return function_declaration_to_call->type.input_parameter_types[argument_index];
                        else
                            return std::nullopt;
                    }
                }
            }
            else if (std::holds_alternative<iris::Instantiate_expression>(current_expression.data))
            {
                iris::Instantiate_expression const& parent_instantiate_expression = std::get<iris::Instantiate_expression>(current_expression.data);

                for (iris::Instantiate_member_value_pair const& member : parent_instantiate_expression.members)
                {
                    if (member.value.expression_index == expression_index)
                    {
                        std::optional<iris::Type_reference> const current_expression_type = get_expression_type(
                            module_name,
                            function_declaration,
                            scope,
                            statement,
                            current_expression,
                            std::nullopt,
                            declaration_database
                        );
                        if (!current_expression_type.has_value())
                            return std::nullopt;

                        std::optional<Declaration> const& declaration = find_underlying_declaration(declaration_database, current_expression_type.value());
                        if (!declaration.has_value())
                            return std::nullopt;

                        std::optional<iris::Type_reference> const current_member_type = get_declaration_member_type(
                            declaration.value(),
                            member.member_name
                        );

                        return current_member_type;
                    }
                }
            }
            else if (std::holds_alternative<iris::Return_expression>(current_expression.data))
            {
                iris::Return_expression const& return_expression = std::get<iris::Return_expression>(current_expression.data);
                if (return_expression.expression.has_value() && return_expression.expression->expression_index == expression_index)
                {
                    if (function_declaration != nullptr && !function_declaration->type.output_parameter_types.empty())
                        return function_declaration->type.output_parameter_types[0];
                    else
                        return std::nullopt;
                }
            }
            else if (std::holds_alternative<iris::Variable_declaration_with_type_expression>(current_expression.data))
            {
                iris::Variable_declaration_with_type_expression const& declaration_expression = std::get<iris::Variable_declaration_with_type_expression>(current_expression.data);
                if (declaration_expression.right_hand_side.expression_index == expression_index)
                    return iris::get_variable_declaration_with_type_expression_type(statement, declaration_expression);
            }
        }

        return std::nullopt;
    }

    Type_info create_type_info(iris::Type_reference type, bool is_mutable)
    {
        return Type_info
        {
            .type = std::move(type),
            .is_mutable = is_mutable,
        };
    }

    std::optional<Type_info> create_type_info(std::optional<iris::Type_reference> type, bool is_mutable)
    {
        if (!type.has_value())
            return std::nullopt;

        return Type_info
        {
            .type = std::move(type.value()),
            .is_mutable = is_mutable,
        };
    }

    std::optional<Type_info> get_implicit_function_type(
        iris::Declaration_database const& declaration_database,
        std::string_view const declaration_module_name,
        std::string_view const member_name
    )
    {
        std::optional<Declaration> const implicit_declaration = find_underlying_declaration(
            declaration_database,
            declaration_module_name,
            member_name
        );
        if (!implicit_declaration.has_value())
            return std::nullopt;

        if (std::holds_alternative<iris::Function_declaration const*>(implicit_declaration->data))
        {
            iris::Function_declaration const& implicit_function_declaration = *std::get<iris::Function_declaration const*>(implicit_declaration->data);
            return Type_info
            {
                .type = create_function_type_type_reference(implicit_function_declaration.type, implicit_function_declaration.input_parameter_names, implicit_function_declaration.output_parameter_names),
                .is_mutable = false,
            };
        }

        return std::nullopt;
    }

     std::optional<Type_info> get_instanced_function_type(
        iris::Declaration_database const& declaration_database,
        Instance_call_key const& key
    )
    {
        Function_constructor const* const function_constructor = get_function_constructor(declaration_database, key.module_name, key.function_constructor_name);
        if (function_constructor == nullptr)
            return std::nullopt;

        Function_expression const* function_expression = nullptr;
        std::size_t function_expression_count = 0;

        auto const process_expression = [&](Expression const& expression, [[maybe_unused]] Statement const& statement) -> bool {
            if (std::holds_alternative<Function_expression>(expression.data))
            {
                ++function_expression_count;
                function_expression = &std::get<Function_expression>(expression.data);
            }

            return false;
        };
        visit_expressions(function_constructor->statements, process_expression);

        if (function_expression_count != 1 || function_expression == nullptr)
            return std::nullopt;

        Function_type function_type = function_expression->declaration.type;
        if (!replace_parameter_types_by_instance_arguments(function_type, function_constructor->parameters, key.arguments))
            return std::nullopt;

        return Type_info
        {
            .type = create_function_type_type_reference(
                std::move(function_type),
                function_expression->declaration.input_parameter_names,
                function_expression->declaration.output_parameter_names
            ),
            .is_mutable = false,
        };
    }

    std::optional<Type_info> get_expression_type_info(
        std::string_view const module_name,
        iris::Function_declaration const* const function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& expression,
        std::optional<iris::Type_reference> const& expected_expression_type,
        iris::Declaration_database const& declaration_database
    )
    {
        if (std::holds_alternative<iris::Access_expression>(expression.data))
        {
            Access_expression const& data = std::get<iris::Access_expression>(expression.data);
            
            std::optional<Type_info> const type_info = get_expression_type_info(module_name, nullptr, scope, statement, statement.expressions[data.expression.expression_index], std::nullopt, declaration_database);
            std::optional<iris::Type_reference> const type_reference = type_info.has_value() ? std::optional<iris::Type_reference>{type_info->type} : std::optional<iris::Type_reference>{std::nullopt};

            bool const is_import_alias_or_enum_name = !type_reference.has_value();
            if (is_import_alias_or_enum_name)
            {
                iris::Expression const& left_hand_side_expression = statement.expressions[data.expression.expression_index];
                
                if (std::holds_alternative<iris::Variable_expression>(left_hand_side_expression.data))
                {
                    iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(left_hand_side_expression.data);

                    // Try import alias:
                    {
                        std::optional<Declaration> const declaration_optional = find_declaration_using_import_alias(
                            declaration_database,
                            module_name,
                            variable_expression.name,
                            data.member_name
                        );

                        if (declaration_optional.has_value())
                        {
                            std::optional<Declaration> const underling_declaration_optional = get_underlying_declaration(declaration_database, declaration_optional.value());
                            if (underling_declaration_optional.has_value())
                            {
                                Declaration const& declaration = underling_declaration_optional.value();

                                if (std::holds_alternative<Function_declaration const*>(declaration.data))
                                {
                                    Function_declaration const& function_declaration = *std::get<Function_declaration const*>(declaration.data);
                                    return Type_info
                                    {
                                        .type = create_function_type_type_reference(function_declaration.type, function_declaration.input_parameter_names, function_declaration.output_parameter_names),
                                        .is_mutable = false,
                                    };
                                }
                                else if (std::holds_alternative<Global_variable_declaration const*>(declaration.data))
                                {
                                    Global_variable_declaration const& global_variable_declaration = *std::get<Global_variable_declaration const*>(declaration.data);
                                    if (global_variable_declaration.type.has_value())
                                    {
                                        return Type_info
                                        {
                                            .type = global_variable_declaration.type.value(),
                                            .is_mutable = global_variable_declaration.global_type == Global_variable_type::Mutable,
                                        };    
                                    }

                                    std::optional<iris::Type_reference> value_type = get_expression_type(module_name, nullptr, scope, global_variable_declaration.initial_value, std::nullopt, declaration_database);
                                    if (!value_type.has_value())
                                        return std::nullopt;

                                    return Type_info
                                    {
                                        .type = std::move(value_type.value()),
                                        .is_mutable = global_variable_declaration.global_type == Global_variable_type::Mutable,
                                    };
                                }

                                Import_module_with_alias const* import_alias = find_import_module_with_alias(
                                    get_module_dependencies(declaration_database, module_name),
                                    variable_expression.name
                                );
                                assert(import_alias != nullptr);

                                return Type_info
                                {
                                    .type = create_custom_type_reference(import_alias->module_name, data.member_name),
                                    .is_mutable = false,
                                };
                            }
                        }
                    }

                    // Try enum
                    {
                        std::optional<Declaration> const declaration_optional = find_underlying_declaration(
                            declaration_database,
                            module_name,
                            variable_expression.name
                        );

                        if (declaration_optional.has_value())
                        {
                            if (std::holds_alternative<Enum_declaration const*>(declaration_optional->data))
                            {
                                return Type_info
                                {
                                    .type = create_custom_type_reference(module_name, variable_expression.name),
                                    .is_mutable = false,
                                };
                            }
                        }
                    }
                }

                return std::nullopt;
            }
            else if (std::holds_alternative<iris::Array_slice_type>(type_reference.value().data))
            {
                iris::Array_slice_type const& array_slice_type = std::get<iris::Array_slice_type>(type_reference.value().data);

                if (data.member_name == "data")
                {
                    return Type_info
                    {
                        .type = create_pointer_type_type_reference(array_slice_type.element_type, array_slice_type.is_mutable),
                        .is_mutable = false,
                    };
                }
                else if (data.member_name == "length")
                {
                    return Type_info
                    {
                        .type = iris::create_integer_type_type_reference(64, false),
                        .is_mutable = false,
                    };
                }

                return std::nullopt;
            }
            else if (std::holds_alternative<iris::Soa_array_type>(type_reference.value().data))
            {
                if (data.member_name == "data")
                {
                    return Type_info
                    {
                        .type = create_pointer_type_type_reference({}, type_info->is_mutable),
                        .is_mutable = false,
                    };
                }
                else if (data.member_name == "length")
                {
                    return Type_info
                    {
                        .type = iris::create_integer_type_type_reference(64, false),
                        .is_mutable = false,
                    };
                }
                else if (data.member_name == "view")
                {
                    return Type_info
                    {
                        .type = create_builtin_type_reference("soa_array_view"),
                        .is_mutable = false,
                    };
                }

                return std::nullopt;
            }
            else if (std::holds_alternative<iris::Soa_array_view_type>(type_reference.value().data))
            {
                if (data.member_name == "data")
                {
                    return Type_info
                    {
                        .type = create_pointer_type_type_reference({}, type_info->is_mutable),
                        .is_mutable = false,
                    };
                }
                else if (data.member_name == "length" || data.member_name == "start_index" || data.member_name == "end_index")
                {
                    return Type_info
                    {
                        .type = iris::create_integer_type_type_reference(64, false),
                        .is_mutable = false,
                    };
                }

                return std::nullopt;
            }
            else if (std::holds_alternative<iris::Custom_type_reference>(type_reference.value().data))
            {
                iris::Custom_type_reference const custom_type_reference = std::get<iris::Custom_type_reference>(type_reference.value().data);

                std::optional<Type_reference> const underlying_type = get_underlying_type(
                    declaration_database,
                    type_reference.value()
                );
                if (underlying_type.has_value() && std::holds_alternative<iris::Type_instance>(underlying_type->data))
                {
                    Type_instance const& type_instance = std::get<iris::Type_instance>(underlying_type->data);
                    Declaration_instance_storage const storage = instantiate_type_instance(declaration_database, type_instance);
                    if (std::holds_alternative<iris::Struct_declaration>(storage.data))
                    {
                        Struct_declaration const& struct_declaration = std::get<iris::Struct_declaration>(storage.data);

                        auto const location = std::find_if(
                            struct_declaration.member_names.begin(),
                            struct_declaration.member_names.end(),
                            [&](std::pmr::string const& member_name) -> bool {
                                return member_name == data.member_name;
                            }
                        );
                        if (location != struct_declaration.member_names.end())
                        {
                            std::size_t const member_index = std::distance(struct_declaration.member_names.begin(), location);
                            return Type_info
                            {
                                .type = struct_declaration.member_types[member_index],
                                .is_mutable = type_info->is_mutable,
                            };
                        }
                    }
                }
                
                std::optional<Declaration> const declaration = find_underlying_declaration(
                    declaration_database,
                    type_reference.value()
                );
                if (!declaration.has_value())
                    return std::nullopt;

                std::optional<Type_reference> const member_type = get_declaration_member_type(
                    declaration.value(),
                    data.member_name
                );
                if (member_type.has_value())
                {
                    return Type_info
                    {
                        .type = member_type.value(),
                        .is_mutable = type_info->is_mutable,
                    };
                }

                std::optional<Type_info> const implicit_function_type = get_implicit_function_type(
                    declaration_database,
                    declaration->module_name,
                    data.member_name
                );
                if (implicit_function_type.has_value())
                    return implicit_function_type;

                std::optional<iris::Custom_type_reference> const type_instance_name = unmangle_type_instance_name(custom_type_reference.name);
                if (type_instance_name.has_value())
                {
                    iris::Type_reference const& implicit_custom_type_reference = create_custom_type_reference(type_instance_name->module_reference.name, data.member_name);
                    std::optional<Declaration> const implicit_declaration = find_declaration(
                        declaration_database,
                        type_instance_name->module_reference.name,
                        data.member_name
                    );
                    if (implicit_declaration.has_value())
                    {
                        return Type_info
                        {
                            .type = implicit_custom_type_reference,
                            .is_mutable = false,
                        };
                    }
                }

                return std::nullopt;
            }
            else if (std::holds_alternative<iris::Type_instance>(type_reference.value().data))
            {
                Type_instance const& type_instance = std::get<iris::Type_instance>(type_reference.value().data);
                Declaration_instance_storage const storage = instantiate_type_instance(declaration_database, type_instance);
                if (std::holds_alternative<iris::Struct_declaration>(storage.data))
                {
                    Struct_declaration const& struct_declaration = std::get<iris::Struct_declaration>(storage.data);
                    
                    auto const location = std::find_if(
                        struct_declaration.member_names.begin(),
                        struct_declaration.member_names.end(),
                        [&](std::pmr::string const& member_name) -> bool {
                            return member_name == data.member_name;
                        }
                    );
                    if (location != struct_declaration.member_names.end())
                    {
                        std::size_t const member_index = std::distance(struct_declaration.member_names.begin(), location);
                        return Type_info
                        {
                            .type = struct_declaration.member_types[member_index],
                            .is_mutable = false,
                        };
                    }
                    
                    // If member is not found, then try to find function in the module
                    iris::Type_reference const& custom_type_reference = create_custom_type_reference(type_instance.type_constructor.module_reference.name, data.member_name);
                    std::optional<Declaration> const declaration = find_declaration(
                        declaration_database,
                        custom_type_reference
                    );
                    if (!declaration.has_value())
                        return std::nullopt;

                    return Type_info
                    {
                        .type = custom_type_reference,
                        .is_mutable = false,
                    };
                }
            }

            return std::nullopt; // TODO@instances
        }
        else if (std::holds_alternative<iris::Access_array_expression>(expression.data))
        {
            iris::Access_array_expression const& data = std::get<iris::Access_array_expression>(expression.data);

            iris::Expression const& left_hand_side_expression = statement.expressions[data.expression.expression_index];
            if (std::holds_alternative<iris::Dereference_and_access_expression>(left_hand_side_expression.data))
            {
                iris::Dereference_and_access_expression const& dereference_and_access_expression = std::get<iris::Dereference_and_access_expression>(left_hand_side_expression.data);

                std::optional<Type_info> const soa_type_info = get_expression_type_info(
                    module_name,
                    nullptr,
                    scope,
                    statement,
                    statement.expressions[dereference_and_access_expression.expression.expression_index],
                    std::nullopt,
                    declaration_database
                );
                if (
                    soa_type_info.has_value() &&
                    (
                        std::holds_alternative<iris::Soa_array_type>(soa_type_info->type.data) ||
                        std::holds_alternative<iris::Soa_array_view_type>(soa_type_info->type.data)
                    )
                )
                {
                    std::optional<iris::Type_reference> const element_type = iris::get_element_or_pointee_type(soa_type_info->type);
                    if (!element_type.has_value())
                        return std::nullopt;

                    std::optional<Declaration> const declaration = find_declaration(
                        declaration_database,
                        element_type.value()
                    );
                    if (!declaration.has_value())
                        return std::nullopt;

                    std::optional<iris::Type_reference> const member_type = get_declaration_member_type(
                        declaration.value(),
                        dereference_and_access_expression.member_name
                    );
                    if (!member_type.has_value())
                        return std::nullopt;

                    bool const is_member_mutable =
                        std::holds_alternative<iris::Soa_array_view_type>(soa_type_info->type.data) ?
                        std::get<iris::Soa_array_view_type>(soa_type_info->type.data).is_mutable :
                        soa_type_info->is_mutable;

                    return Type_info
                    {
                        .type = member_type.value(),
                        .is_mutable = is_member_mutable,
                    };
                }
            }

            std::optional<Type_info> const lhs_type_info = get_expression_type_info(module_name, nullptr, scope, statement, statement.expressions[data.expression.expression_index], std::nullopt, declaration_database);;
            std::optional<iris::Type_reference> const lhs_type_reference = lhs_type_info.has_value() ? std::optional<iris::Type_reference>{lhs_type_info->type} : std::optional<iris::Type_reference>{std::nullopt};
            if (!lhs_type_reference.has_value())
                return std::nullopt;

            if (std::holds_alternative<iris::Array_slice_type>(lhs_type_reference->data))
            {
                iris::Array_slice_type const& array_type = std::get<iris::Array_slice_type>(lhs_type_reference->data);
                if (array_type.element_type.empty())
                    return std::nullopt;

                return Type_info
                {
                    .type = array_type.element_type[0],
                    .is_mutable = array_type.is_mutable,
                };
            }
            else if (std::holds_alternative<iris::Constant_array_type>(lhs_type_reference->data))
            {
                iris::Constant_array_type const& array_type = std::get<iris::Constant_array_type>(lhs_type_reference->data);
                if (array_type.value_type.empty())
                    return std::nullopt;

                return Type_info
                {
                    .type = array_type.value_type[0],
                    .is_mutable = lhs_type_info->is_mutable,
                };
            }
            else if (std::holds_alternative<iris::Soa_array_type>(lhs_type_reference->data))
            {
                iris::Soa_array_type const& array_type = std::get<iris::Soa_array_type>(lhs_type_reference->data);
                if (array_type.value_type.empty())
                    return std::nullopt;

                return Type_info
                {
                    .type = array_type.value_type[0],
                    .is_mutable = lhs_type_info->is_mutable,
                };
            }
            else if (std::holds_alternative<iris::Soa_array_view_type>(lhs_type_reference->data))
            {
                iris::Soa_array_view_type const& array_type = std::get<iris::Soa_array_view_type>(lhs_type_reference->data);
                if (array_type.value_type.empty())
                    return std::nullopt;

                return Type_info
                {
                    .type = array_type.value_type[0],
                    .is_mutable = array_type.is_mutable,
                };
            }
            else if (std::holds_alternative<iris::Pointer_type>(lhs_type_reference->data))
            {
                iris::Pointer_type const& pointer_type = std::get<iris::Pointer_type>(lhs_type_reference->data);
                if (pointer_type.element_type.empty())
                    return std::nullopt;
                
                return Type_info
                {
                    .type = pointer_type.element_type[0],
                    .is_mutable = pointer_type.is_mutable,
                };
            }

            return std::nullopt;
        }
        else if (std::holds_alternative<iris::Binary_expression>(expression.data))
        {
            Binary_expression const& data = std::get<iris::Binary_expression>(expression.data);

            switch (data.operation)
            {
                case iris::Binary_operation::Equal:
                case iris::Binary_operation::Not_equal:
                case iris::Binary_operation::Less_than:
                case iris::Binary_operation::Less_than_or_equal_to:
                case iris::Binary_operation::Greater_than:
                case iris::Binary_operation::Greater_than_or_equal_to:
                case iris::Binary_operation::Logical_and:
                case iris::Binary_operation::Logical_or:
                case iris::Binary_operation::Has: {
                    return Type_info
                    {
                        .type = create_bool_type_reference(),
                        .is_mutable = false,
                    };
                }
                case iris::Binary_operation::Add:
                case iris::Binary_operation::Subtract:
                case iris::Binary_operation::Multiply:
                case iris::Binary_operation::Divide:
                case iris::Binary_operation::Modulus:
                case iris::Binary_operation::Bitwise_and:
                case iris::Binary_operation::Bitwise_or:
                case iris::Binary_operation::Bitwise_xor:
                case iris::Binary_operation::Bit_shift_left:
                case iris::Binary_operation::Bit_shift_right:
                default: {
                    std::optional<iris::Type_reference> type = get_expression_type(module_name, nullptr, scope, statement, statement.expressions[data.left_hand_side.expression_index], std::nullopt, declaration_database);
                    if (!type.has_value())
                        return std::nullopt;

                    return Type_info
                    {
                        .type = std::move(type.value()),
                        .is_mutable = false,
                    };
                }
            }
        }
        else if (std::holds_alternative<iris::Call_expression>(expression.data))
        {
            Call_expression const& data = std::get<iris::Call_expression>(expression.data);

            std::optional<iris::Type_reference> const type_reference = get_expression_type(module_name, nullptr, scope, statement, statement.expressions[data.expression.expression_index], std::nullopt, declaration_database);

            if (type_reference.has_value() && std::holds_alternative<iris::Builtin_type_reference>(type_reference.value().data))
            {
                iris::Builtin_type_reference const& builtin_type_reference = std::get<iris::Builtin_type_reference>(type_reference.value().data);
                
                if (builtin_type_reference.value == "check")
                {
                    return std::nullopt;
                }
                else if (builtin_type_reference.value == "create_array_slice_from_pointer")
                {
                    std::pmr::vector<iris::Type_reference> element_type;
                    bool is_mutable = false;

                    if (data.arguments.size() > 0)
                    {
                        std::optional<Type_info> const first_argument_type_info = get_expression_type_info(module_name, nullptr, scope, statement, statement.expressions[data.arguments[0].expression_index], std::nullopt, declaration_database);
                        if (first_argument_type_info.has_value() && std::holds_alternative<Pointer_type>(first_argument_type_info->type.data))
                        {
                            Pointer_type const& pointer_type = std::get<Pointer_type>(first_argument_type_info->type.data);

                            if (!pointer_type.element_type.empty())
                                element_type.push_back(pointer_type.element_type[0]);

                            is_mutable = pointer_type.is_mutable;
                        }
                    }

                    return Type_info
                    {
                        .type = create_array_slice_type_reference(element_type, is_mutable),
                        .is_mutable = false,
                    };
                }
                else if (builtin_type_reference.value == "create_stack_array_uninitialized")
                {
                    // This will generate a validation error as there is no element type.
                    return Type_info
                    {
                        .type = create_array_slice_type_reference({}, true),
                        .is_mutable = false,
                    };
                }
                else if (builtin_type_reference.value == "offset_pointer")
                {
                    if (data.arguments.size() == 0)
                        return std::nullopt;
                    
                    std::optional<Type_info> const first_argument_type_info = get_expression_type_info(module_name, nullptr, scope, statement, statement.expressions[data.arguments[0].expression_index], std::nullopt, declaration_database);
                    return first_argument_type_info;
                }
                else if (builtin_type_reference.value == "reinterpret_as")
                {
                    // This will generate a validation error as there is no element type.
                    return Type_info
                    {
                        .type = {},
                        .is_mutable = false,
                    };
                }
                else if (builtin_type_reference.value == "soa_array_view")
                {
                    iris::Expression const& callable_expression = statement.expressions[data.expression.expression_index];

                    std::optional<Type_info> receiver_type_info = std::nullopt;
                    if (std::holds_alternative<iris::Access_expression>(callable_expression.data))
                    {
                        iris::Access_expression const& access_expression = std::get<iris::Access_expression>(callable_expression.data);
                        receiver_type_info = get_expression_type_info(
                            module_name,
                            nullptr,
                            scope,
                            statement,
                            statement.expressions[access_expression.expression.expression_index],
                            std::nullopt,
                            declaration_database
                        );
                    }
                    else if (std::holds_alternative<iris::Dereference_and_access_expression>(callable_expression.data))
                    {
                        iris::Dereference_and_access_expression const& access_expression = std::get<iris::Dereference_and_access_expression>(callable_expression.data);
                        receiver_type_info = get_expression_type_info(
                            module_name,
                            nullptr,
                            scope,
                            statement,
                            statement.expressions[access_expression.expression.expression_index],
                            std::nullopt,
                            declaration_database
                        );
                    }

                    if (!receiver_type_info.has_value())
                        return std::nullopt;

                    std::optional<iris::Type_reference> const underlying_receiver_type = get_underlying_type(
                        declaration_database,
                        receiver_type_info->type
                    );
                    if (!underlying_receiver_type.has_value() || !std::holds_alternative<iris::Soa_array_type>(underlying_receiver_type->data))
                        return std::nullopt;

                    iris::Soa_array_type const& soa_array_type = std::get<iris::Soa_array_type>(underlying_receiver_type->data);
                    if (soa_array_type.value_type.empty())
                        return std::nullopt;

                    iris::Type_reference const soa_array_view_type =
                    {
                        .data = iris::Soa_array_view_type
                        {
                            .value_type = {soa_array_type.value_type[0]},
                            .is_mutable = receiver_type_info->is_mutable,
                        }
                    };

                    return Type_info
                    {
                        .type = soa_array_view_type,
                        .is_mutable = false,
                    };
                }
            }
            else if (!type_reference.has_value() || !std::holds_alternative<iris::Function_pointer_type>(type_reference.value().data))
            {
                std::optional<Deduced_instance_call> const deduced_instance_call = deduce_instance_call_arguments(
                    declaration_database,
                    module_name,
                    scope,
                    statement,
                    data,
                    {}
                );
                if (deduced_instance_call.has_value())
                {
                    Instance_call_key const key = {
                        .module_name = deduced_instance_call->custom_type_reference.module_reference.name,
                        .function_constructor_name = deduced_instance_call->custom_type_reference.name,
                        .arguments = deduced_instance_call->arguments
                    };

                    Function_expression const call_instance = create_instance_call_expression_value(
                        deduced_instance_call->function_constructor,
                        deduced_instance_call->arguments,
                        key
                    );

                    if (!call_instance.declaration.type.output_parameter_types.empty())
                    {
                        return Type_info
                        {
                            .type = call_instance.declaration.type.output_parameter_types[0],
                            .is_mutable = false,
                        };
                    }
                }

                return std::nullopt;
            }

            iris::Function_pointer_type const& function_pointer_type = std::get<iris::Function_pointer_type>(type_reference.value().data);

            if (function_pointer_type.type.output_parameter_types.empty())
                return std::nullopt;

            return Type_info
            {
                .type = function_pointer_type.type.output_parameter_types[0],
                .is_mutable = false,
            };
        }
        else if (std::holds_alternative<iris::Cast_expression>(expression.data))
        {
            Cast_expression const& data = std::get<iris::Cast_expression>(expression.data);
            return Type_info
            {
                .type = data.destination_type,
                .is_mutable = false,
            };
        }
        else if (std::holds_alternative<iris::Constant_expression>(expression.data))
        {
            Constant_expression const& data = std::get<iris::Constant_expression>(expression.data);
            return Type_info
            {
                .type = data.type,
                .is_mutable = false,
            };
        }
        else if (std::holds_alternative<iris::Constant_array_expression>(expression.data))
        {
            Constant_array_expression const& data = std::get<iris::Constant_array_expression>(expression.data);
            if (data.array_data.empty())
            {
                if (expected_expression_type.has_value())
                {
                    return Type_info
                    {
                        .type = expected_expression_type.value(),
                        .is_mutable = false,
                    };
                }

                // For empty array and no expected type, we assume it's an empty array of Int32:
                return Type_info
                {
                    .type = create_constant_array_type_reference({create_integer_type_type_reference(32, true)}, data.array_data.size()),
                    .is_mutable = false,
                };
            }

            std::optional<iris::Type_reference> const element_type = get_expression_type(module_name, nullptr, scope, data.array_data[0], std::nullopt, declaration_database);
            if (!element_type.has_value())
                return std::nullopt;

            return Type_info
            {
                .type = create_constant_array_type_reference({element_type.value()}, data.array_data.size()),
                .is_mutable = false,
            };
        }
        else if (std::holds_alternative<iris::Defer_expression>(expression.data))
        {
            Defer_expression const& data = std::get<iris::Defer_expression>(expression.data);
            std::optional<Type_reference> type = get_expression_type(module_name, nullptr, scope, statement, statement.expressions[data.expression_to_defer.expression_index], std::nullopt, declaration_database);
            if (!type.has_value())
                return std::nullopt;

            return Type_info
            {
                .type = std::move(type.value()),
                .is_mutable = false,
            };
        }
        else if (std::holds_alternative<iris::Dereference_and_access_expression>(expression.data))
        {
            Dereference_and_access_expression const& data = std::get<iris::Dereference_and_access_expression>(expression.data);

            std::optional<Type_reference> const left_side_type = get_expression_type(module_name, nullptr, scope, statement, statement.expressions[data.expression.expression_index], std::nullopt, declaration_database);
            if (!left_side_type.has_value() || !is_non_void_pointer(left_side_type.value()) || !is_pointer(left_side_type.value()))
                return std::nullopt;

            iris::Pointer_type const& pointer_type = std::get<iris::Pointer_type>(left_side_type->data);

            std::optional<Type_reference> const left_side_value_type = remove_pointer(left_side_type.value());
            if (!left_side_value_type.has_value())
                return std::nullopt;

            if (std::holds_alternative<iris::Custom_type_reference>(left_side_value_type->data))
            {
                std::optional<Type_reference> const underlying_type = get_underlying_type(
                    declaration_database,
                    left_side_value_type.value()
                );
                if (underlying_type.has_value() && std::holds_alternative<iris::Type_instance>(underlying_type->data))
                {
                    Type_instance const& type_instance = std::get<iris::Type_instance>(underlying_type->data);
                    Declaration_instance_storage const storage = instantiate_type_instance(declaration_database, type_instance);
                    if (std::holds_alternative<iris::Struct_declaration>(storage.data))
                    {
                        Struct_declaration const& struct_declaration = std::get<iris::Struct_declaration>(storage.data);
                        auto const location = std::find_if(
                            struct_declaration.member_names.begin(),
                            struct_declaration.member_names.end(),
                            [&](std::pmr::string const& member_name) -> bool {
                                return member_name == data.member_name;
                            }
                        );
                        if (location != struct_declaration.member_names.end())
                        {
                            std::size_t const member_index = std::distance(struct_declaration.member_names.begin(), location);
                            return Type_info
                            {
                                .type = struct_declaration.member_types[member_index],
                                .is_mutable = pointer_type.is_mutable,
                            };
                        }
                    }
                }
            }

            std::optional<Declaration> const declaration = find_declaration(declaration_database, left_side_value_type.value());
            if (!declaration.has_value())
                return std::nullopt;

            std::optional<iris::Type_reference> member_type = get_declaration_member_type(
                declaration.value(),
                data.member_name
            );
            if (!member_type.has_value())
            {
                std::optional<Type_info> const implicit_function_type = get_implicit_function_type(
                    declaration_database,
                    declaration->module_name,
                    data.member_name
                );
                return implicit_function_type;
            }

            return Type_info
            {
                .type = std::move(member_type.value()),
                .is_mutable = pointer_type.is_mutable,
            };
        }
        else if (std::holds_alternative<iris::Instance_call_expression>(expression.data))
        {
            Instance_call_expression const& data = std::get<iris::Instance_call_expression>(expression.data);

            // Check builtin functions:
            {
                iris::Expression const& left_hand_side = statement.expressions[data.left_hand_side.expression_index];
                if (std::holds_alternative<iris::Variable_expression>(left_hand_side.data))
                {
                    iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(left_hand_side.data);
                    
                    if (variable_expression.name == "create_stack_array_uninitialized")
                    {
                        std::pmr::vector<iris::Type_reference> element_type;
                        if (data.arguments.size() > 0)
                        {
                            iris::Statement const argument = data.arguments[0];
                            if (!argument.expressions.empty() && std::holds_alternative<iris::Type_expression>(argument.expressions[0].data))
                            {
                                iris::Type_expression const& type_expression = std::get<iris::Type_expression>(argument.expressions[0].data);
                                element_type.push_back(type_expression.type);
                            }
                        }

                        iris::Function_type function_type
                        {
                            .input_parameter_types = {create_integer_type_type_reference(64, false)},
                            .output_parameter_types = {create_array_slice_type_reference(element_type, true)},
                            .is_variadic = false,
                        };

                        return Type_info
                        {
                            .type = create_function_type_type_reference(std::move(function_type), {"length"}, {"stack_array"}),
                            .is_mutable = false,
                        };
                    }
                    else if (variable_expression.name == "reinterpret_as")
                    {
                        std::pmr::vector<iris::Type_reference> element_type;
                        if (data.arguments.size() > 0)
                        {
                            iris::Statement const argument = data.arguments[0];
                            if (!argument.expressions.empty() && std::holds_alternative<iris::Type_expression>(argument.expressions[0].data))
                            {
                                iris::Type_expression const& type_expression = std::get<iris::Type_expression>(argument.expressions[0].data);
                                element_type.push_back(type_expression.type);
                            }
                        }

                        iris::Function_type function_type
                        {
                            .input_parameter_types = { create_fundamental_type_type_reference(Fundamental_type::Any_type) },
                            .output_parameter_types = { element_type },
                            .is_variadic = false,
                        };

                        return Type_info
                        {
                            .type = create_function_type_type_reference(std::move(function_type), {"value"}, {"result"}),
                            .is_mutable = false,
                        };
                    }
                }
            }

            std::optional<Custom_type_reference> custom_type_reference = get_function_constructor_type_reference(
                declaration_database,
                module_name,
                statement.expressions[data.left_hand_side.expression_index],
                statement
            );
            if (!custom_type_reference.has_value())
                return std::nullopt;

            Instance_call_key const key = {
                .module_name = custom_type_reference->module_reference.name,
                .function_constructor_name = custom_type_reference->name,
                .arguments = data.arguments
            };

            return get_instanced_function_type(declaration_database, key);
        }
        else if (std::holds_alternative<iris::Instantiate_expression>(expression.data))
        {
            iris::Instantiate_expression const& instantiate_expression = std::get<iris::Instantiate_expression>(expression.data);

            std::optional<iris::Type_reference> const type_to_instantiate =
                expected_expression_type.has_value() ?
                expected_expression_type.value() :            
                get_type_to_instantiate(
                    module_name,
                    function_declaration,
                    scope,
                    statement,
                    expression,
                    instantiate_expression,
                    declaration_database
                );

            if (!type_to_instantiate.has_value())
                return std::nullopt;

            if (std::holds_alternative<iris::Array_slice_type>(type_to_instantiate->data))
            {
                return Type_info
                {
                    .type = type_to_instantiate.value(),
                    .is_mutable = false,
                };
            }

            if (std::holds_alternative<iris::Soa_array_type>(type_to_instantiate->data))
            {
                return Type_info
                {
                    .type = type_to_instantiate.value(),
                    .is_mutable = false,
                };
            }

            if (std::holds_alternative<iris::Soa_array_view_type>(type_to_instantiate->data))
            {
                return Type_info
                {
                    .type = type_to_instantiate.value(),
                    .is_mutable = false,
                };
            }

            std::optional<Declaration> const declaration = find_underlying_declaration(
                declaration_database,
                type_to_instantiate.value()
            );
            if (!declaration.has_value())
                return std::nullopt;

            if (std::holds_alternative<iris::Struct_declaration const*>(declaration->data) || std::holds_alternative<iris::Union_declaration const*>(declaration->data) || std::holds_alternative<iris::Type_constructor const*>(declaration->data))
            {
                return Type_info
                {
                    .type = type_to_instantiate.value(),
                    .is_mutable = false,
                };
            }

            return std::nullopt;
        }
        else if (std::holds_alternative<iris::Null_pointer_expression>(expression.data))
        {
            return Type_info
            {
                .type = create_null_pointer_type_type_reference(),
                .is_mutable = false,
            };
        }
        else if (std::holds_alternative<iris::Parenthesis_expression>(expression.data))
        {
            Parenthesis_expression const& data = std::get<iris::Parenthesis_expression>(expression.data);
            std::optional<iris::Type_reference> type = get_expression_type(module_name, nullptr, scope, statement, statement.expressions[data.expression.expression_index], std::nullopt, declaration_database);
            if (!type.has_value())
                return std::nullopt;

            return Type_info
            {
                .type = std::move(type.value()),
                .is_mutable = false,
            };
        }
        else if (std::holds_alternative<iris::Reflection_expression>(expression.data))
        {
            Reflection_expression const& data = std::get<iris::Reflection_expression>(expression.data);

            if (data.name == "size_of" || data.name == "alignment_of")
            {
                return Type_info
                {
                    .type = iris::create_integer_type_type_reference(64, false),
                    .is_mutable = false,
                };   
            }

            return std::nullopt;
        }
        else if (std::holds_alternative<iris::Ternary_condition_expression>(expression.data))
        {
            Ternary_condition_expression const& data = std::get<iris::Ternary_condition_expression>(expression.data);
            std::optional<iris::Type_reference> type = get_expression_type(module_name, nullptr, scope, data.then_statement, std::nullopt, declaration_database);
            if (!type.has_value())
                return std::nullopt;

            return Type_info
            {
                .type = std::move(type.value()),
                .is_mutable = false,
            };
        }
        else if (std::holds_alternative<iris::Unary_expression>(expression.data))
        {
            Unary_expression const& data = std::get<iris::Unary_expression>(expression.data);

            switch (data.operation)
            {
                case iris::Unary_operation::Not:
                {
                    return Type_info
                    {
                        .type = create_bool_type_reference(),
                        .is_mutable = false,
                    };
                }
                case iris::Unary_operation::Bitwise_not:
                case iris::Unary_operation::Minus:
                case iris::Unary_operation::Pre_increment:
                case iris::Unary_operation::Post_increment:
                case iris::Unary_operation::Pre_decrement:
                case iris::Unary_operation::Post_decrement:
                {
                    std::optional<iris::Type_reference> type = get_expression_type(module_name, nullptr, scope, statement, statement.expressions[data.expression.expression_index], std::nullopt, declaration_database);
                    if (!type.has_value())
                        return std::nullopt;

                    return Type_info
                    {
                        .type = std::move(type.value()),
                        .is_mutable = false,
                    };
                }
                case iris::Unary_operation::Indirection:
                {
                    std::optional<iris::Type_reference> const expression_type = get_expression_type(module_name, nullptr, scope, statement, statement.expressions[data.expression.expression_index], std::nullopt, declaration_database);
                    if (!expression_type.has_value())
                        return std::nullopt;

                    if (is_pointer(expression_type.value()))
                    {
                        Pointer_type const& pointer_type = std::get<Pointer_type>(expression_type.value().data);
                        if (pointer_type.element_type.empty())
                            return std::nullopt;

                        return Type_info
                        {
                            .type = pointer_type.element_type[0],
                            .is_mutable = pointer_type.is_mutable,
                        };
                    }
                    else
                    {
                        return std::nullopt;
                    }
                }
                case iris::Unary_operation::Address_of:
                {
                    std::optional<Type_info> const expression_type_info = get_expression_type_info(module_name, nullptr, scope, statement, statement.expressions[data.expression.expression_index], std::nullopt, declaration_database);
                    if (!expression_type_info.has_value())
                    {
                        return Type_info
                        {
                            .type = create_pointer_type_type_reference({}, true),
                            .is_mutable = false,
                        };
                    }

                    return Type_info
                    {
                        .type = create_pointer_type_type_reference({expression_type_info->type}, expression_type_info->is_mutable),
                        .is_mutable = false,
                    };
                }
            }

            return std::nullopt;
        }
        else if (std::holds_alternative<iris::Variable_expression>(expression.data))
        {
            Variable_expression const& data = std::get<iris::Variable_expression>(expression.data);

            // Try variable:
            {
                auto const location = std::find_if(
                    scope.variables.begin(),
                    scope.variables.end(),
                    [&](Variable const& variable) -> bool {
                        return variable.name == data.name;
                    }
                );
                if (location != scope.variables.end())
                {
                    return Type_info
                    {
                        .type = location->type,
                        .is_mutable = location->is_mutable,
                    };
                }
            }

            // Try declarations:
            {
                std::optional<Declaration> const declaration_optional = find_declaration(
                    declaration_database,
                    module_name,
                    data.name
                );
                if (declaration_optional.has_value())
                {
                    if (std::holds_alternative<Global_variable_declaration const*>(declaration_optional->data))
                    {
                        Global_variable_declaration const& global_variable_declaration = *std::get<Global_variable_declaration const*>(declaration_optional->data);
                        if (global_variable_declaration.type.has_value())
                        {
                            return Type_info
                            {
                                .type = global_variable_declaration.type.value(),
                                .is_mutable = global_variable_declaration.global_type == Global_variable_type::Mutable,
                            };
                        }

                        std::optional<iris::Type_reference> type = get_expression_type(module_name, nullptr, scope, global_variable_declaration.initial_value, std::nullopt, declaration_database);
                        if (!type.has_value())
                            return std::nullopt;
                    
                        return Type_info
                        {
                            .type = std::move(type.value()),
                            .is_mutable = global_variable_declaration.global_type == Global_variable_type::Mutable,
                        };
                    }
                    else if (std::holds_alternative<Function_declaration const*>(declaration_optional->data))
                    {
                        Function_declaration const& function_declaration = *std::get<Function_declaration const*>(declaration_optional->data);
                        
                        return Type_info
                        {
                            .type = create_function_type_type_reference(
                                function_declaration.type,
                                function_declaration.input_parameter_names,
                                function_declaration.output_parameter_names
                            ),
                            .is_mutable = false,
                        };
                    }
                }
            }

            // Try builtins:
            {
                if (is_builtin_function_name(data.name))
                {
                    return Type_info
                    {
                        .type = create_builtin_type_reference(data.name),
                        .is_mutable = false,
                    };
                }

                if (data.name == "Type_kind")
                {
                    std::optional<Declaration> const declaration_optional = find_underlying_declaration(
                        declaration_database,
                        "iris.builtin",
                        "Type_kind"
                    );

                    if (declaration_optional.has_value())
                    {
                        if (std::holds_alternative<Enum_declaration const*>(declaration_optional->data))
                        {
                            return Type_info
                            {
                                .type = create_custom_type_reference("iris.builtin", "Type_kind"),
                                .is_mutable = false,
                            };
                        }
                    }
                }
            }

            return std::nullopt;
        }
        else
        {
            return std::nullopt;
        }
    }

    std::optional<iris::Type_reference> get_expression_type(
        std::string_view const module_name,
        iris::Function_declaration const* const function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& expression,
        std::optional<iris::Type_reference> const& expected_expression_type,
        iris::Declaration_database const& declaration_database
    )
    {
        std::optional<Type_info> type_info = get_expression_type_info(
            module_name,
            function_declaration,
            scope,
            statement,
            expression,
            expected_expression_type,
            declaration_database
        );
        if (!type_info.has_value())
            return std::nullopt;

        return type_info->type;
    }

    std::optional<Custom_type_reference> get_function_constructor_type_reference_using_scope(
        Declaration_database const& declaration_database,
        std::string_view const module_name,
        Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& expression
    )
    {
        std::optional<iris::Type_reference> const expression_type = get_expression_type(
            module_name,
            nullptr,
            scope,
            statement,
            expression,
            std::nullopt,
            declaration_database
        );
        if (expression_type.has_value() && std::holds_alternative<iris::Custom_type_reference>(expression_type.value().data))
        {
            return std::get<iris::Custom_type_reference>(expression_type.value().data);
        }

        return get_function_constructor_type_reference(
            declaration_database,
            module_name,
            expression,
            statement
        );
    }

    std::optional<iris::Type_reference> get_instance_call_implicit_first_argument(
        std::string_view const module_name,
        Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& left_hand_side,
        iris::Declaration_database const& declaration_database
    )
    {
        if (std::holds_alternative<iris::Access_expression>(left_hand_side.data))
        {
            Access_expression const& data = std::get<iris::Access_expression>(left_hand_side.data);
            std::optional<iris::Type_reference> const type_reference = get_expression_type(
                module_name,
                nullptr,
                scope,
                statement,
                statement.expressions[data.expression.expression_index],
                std::nullopt,
                declaration_database
            );
            if (!type_reference.has_value())
                return std::nullopt;
            
            Type_reference pointer_type = create_pointer_type_type_reference({type_reference.value()}, true);
            return pointer_type;
        }

        return std::nullopt;
    }

    void deduce_type(
        iris::Function_constructor const& function_constructor,
        iris::Type_reference const& parameter_type,
        iris::Type_reference const& argument_type,
        std::pmr::vector<std::optional<Type_reference>>& deduced_parameters
    )
    {
        if (std::holds_alternative<Parameter_type>(parameter_type.data))
        {
            Parameter_type const& value = std::get<Parameter_type>(parameter_type.data);

            auto const is_parameter = [&](iris::Function_constructor_parameter const& parameter) -> bool {
                return parameter.name == value.name;
            };

            auto const location = std::find_if(function_constructor.parameters.begin(), function_constructor.parameters.end(), is_parameter);
            if (location == function_constructor.parameters.end())
                throw std::runtime_error{ std::format("Could not find parameter type '{}'", value.name) };
            
            std::size_t const parameter_index = std::distance(function_constructor.parameters.begin(), location);
            deduced_parameters[parameter_index] = argument_type;
        }
        else if (std::holds_alternative<Pointer_type>(parameter_type.data) && std::holds_alternative<Pointer_type>(argument_type.data))
        {
            Pointer_type const& parameter_pointer_type = std::get<Pointer_type>(parameter_type.data);
            Pointer_type const& argument_pointer_type = std::get<Pointer_type>(argument_type.data);
            if (!parameter_pointer_type.element_type.empty() && !argument_pointer_type.element_type.empty())
            {
                deduce_type(
                    function_constructor,
                    parameter_pointer_type.element_type[0],
                    argument_pointer_type.element_type[0],
                    deduced_parameters
                );
            }
        }
        else if (std::holds_alternative<Type_instance>(parameter_type.data) && std::holds_alternative<Type_instance>(argument_type.data))
        {
            Type_instance const& parameter_type_instance = std::get<Type_instance>(parameter_type.data);
            Type_instance const& argument_type_instance = std::get<Type_instance>(argument_type.data);

            if (parameter_type_instance.type_constructor.module_reference.name == argument_type_instance.type_constructor.module_reference.name)
            {
                for (std::size_t index = 0; index < parameter_type_instance.arguments.size(); ++index)
                {
                    Statement const& parameter_statement = parameter_type_instance.arguments[index];
                    Statement const& argument_statement = argument_type_instance.arguments[index];

                    if (parameter_statement.expressions.size() == argument_statement.expressions.size())
                    {
                        Expression const& parameter_expression = parameter_statement.expressions[0];
                        Expression const& argument_expression = argument_statement.expressions[0];

                        if (std::holds_alternative<Type_expression>(parameter_expression.data) && std::holds_alternative<Type_expression>(argument_expression.data))
                        {
                            Type_expression const& parameter_type_expression = std::get<Type_expression>(parameter_expression.data);
                            Type_expression const& argument_type_expression = std::get<Type_expression>(argument_expression.data);

                            deduce_type(
                                function_constructor,
                                parameter_type_expression.type,
                                argument_type_expression.type,
                                deduced_parameters
                            );
                        }
                    }
                }
            }
        }
        else if (parameter_type == argument_type)
        {

        }
    }

    std::optional<Deduced_instance_call> deduce_instance_call_arguments(
        iris::Declaration_database const& declaration_database,
        std::string_view const module_name,
        Scope const& scope,
        iris::Statement const& statement,
        iris::Call_expression const& call_expression,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        iris::Expression const& left_hand_side = statement.expressions[call_expression.expression.expression_index];

        std::optional<Custom_type_reference> const custom_type_reference = get_function_constructor_type_reference_using_scope(
            declaration_database,
            module_name,
            scope,
            statement,
            left_hand_side
        );
        if (!custom_type_reference.has_value())
            return std::nullopt;

        Function_constructor const* function_constructor = get_function_constructor(
            declaration_database,
            *custom_type_reference
        );
        if (function_constructor == nullptr)
            return std::nullopt;

        std::optional<iris::Type_reference> const implicit_first_argument_type = get_instance_call_implicit_first_argument(
            module_name,
            scope,
            statement,
            left_hand_side,
            declaration_database
        );

        std::pmr::vector<Type_reference> argument_types{ temporaries_allocator };
        argument_types.resize(call_expression.arguments.size() + (implicit_first_argument_type.has_value() ? 1 : 0));

        if (implicit_first_argument_type.has_value())
            argument_types[0] = implicit_first_argument_type.value();

        for (std::size_t argument_index = 0; argument_index < call_expression.arguments.size(); ++argument_index)
        {
            iris::Expression const& argument_expression = statement.expressions[call_expression.arguments[argument_index].expression_index];

            std::optional<iris::Type_reference> argument_type = get_expression_type(
                module_name,
                nullptr,
                scope,
                statement,
                argument_expression,
                std::nullopt,
                declaration_database
            );
            if (!argument_type.has_value())
                throw std::runtime_error("Argument type is not valid.");

            std::size_t const output_argument_index = implicit_first_argument_type.has_value() ? argument_index + 1 : argument_index;
            argument_types[output_argument_index] = argument_type.value();
        }

        std::pmr::vector<Function_expression const*> const function_expressions = get_all_possible_function_expressions(
            *function_constructor,
            argument_types.size(),
            temporaries_allocator
        );

        std::pmr::vector<std::optional<Type_reference>> deduced_parameters;

        auto const has_value = [](std::optional<Type_reference> const& type) -> bool {
            return type.has_value();
        };

        for (Function_expression const* function_expression : function_expressions)
        {
            deduced_parameters.clear();
            deduced_parameters.resize(function_constructor->parameters.size());

            if (std::holds_alternative<iris::Instance_call_expression>(left_hand_side.data))
            {
                iris::Instance_call_expression const& instance_call_expression = std::get<iris::Instance_call_expression>(left_hand_side.data);
                for (std::size_t index = 0; index < instance_call_expression.arguments.size(); ++index)
                {
                    iris::Statement const& argument_statement = instance_call_expression.arguments[index];
                    if (argument_statement.expressions.size() == 1)
                    {
                        iris::Expression const& argument_expression = argument_statement.expressions[0];
                        if (std::holds_alternative<iris::Type_expression>(argument_expression.data))
                        {
                            iris::Type_expression const& type_expression = std::get<iris::Type_expression>(argument_expression.data);
                            deduced_parameters[index] = type_expression.type;
                        }
                    }
                }
            }

            for (std::size_t index = 0; index < function_expression->declaration.type.input_parameter_types.size(); ++index)
            {
                Type_reference const& parameter_type = function_expression->declaration.type.input_parameter_types[index];
                Type_reference const& argument_type = argument_types[index];
                
                deduce_type(
                    *function_constructor,
                    parameter_type,
                    argument_type,
                    deduced_parameters
                );

                if (std::all_of(deduced_parameters.begin(), deduced_parameters.end(), has_value))
                {
                    std::pmr::vector<Statement> const deduced_parameter_statements = create_statements_from_type_references(
                        deduced_parameters,
                        temporaries_allocator
                    );

                    return Deduced_instance_call
                    {
                        .custom_type_reference = std::move(custom_type_reference.value()),
                        .function_constructor = *function_constructor,
                        .arguments = std::move(deduced_parameter_statements),
                    };
                }
            }
        }

        throw std::runtime_error("Could not deduce instance call arguments.");
    }

    std::pmr::vector<Function_expression const*> get_all_possible_function_expressions(
        Function_constructor const& function_constructor,
        std::size_t const argument_count,
        std::pmr::polymorphic_allocator<> const& allocator
    )
    {
        std::pmr::vector<Function_expression const*> output{ allocator };
        output.reserve(1);

        auto const process_expression = [&](Expression const& expression, Statement const& statement) -> bool {

            if (std::holds_alternative<Function_expression>(expression.data))
            {
                Function_expression const& function_expression = std::get<Function_expression>(expression.data);
                if (function_expression.declaration.type.input_parameter_types.size() == argument_count)
                    output.push_back(&function_expression);
            }

            return false;
        };

        visit_expressions(function_constructor.statements, process_expression);

        return output;
    }

    std::pmr::vector<Statement> create_statements_from_type_references(
        std::span<std::optional<Type_reference> const> const type_references,
        std::pmr::polymorphic_allocator<> const& allocator
    )
    {
        std::pmr::vector<Statement> statements{allocator};
        statements.reserve(type_references.size());

        for (std::optional<Type_reference> const& type_reference : type_references)
        {
            if (type_reference.has_value())
            {
                std::pmr::vector<Expression> expressions;
                expressions.push_back(
                    Expression
                    {
                        .data = Type_expression{ .type = type_reference.value() },
                        .source_range = std::nullopt
                    }
                );

                statements.push_back(
                    Statement
                    {
                        .expressions = std::move(expressions)
                    }
                );
            }
        }

        return statements;
    }

    std::optional<iris::Type_reference> get_declaration_member_type(
        Declaration const& declaration,
        std::string_view const member_name
    )
    {
        if (std::holds_alternative<iris::Enum_declaration const*>(declaration.data))
        {
            iris::Enum_declaration const& enum_declaration = *std::get<iris::Enum_declaration const*>(declaration.data);

            return iris::create_custom_type_reference(declaration.module_name, enum_declaration.name);
        }
        else if (std::holds_alternative<iris::Struct_declaration const*>(declaration.data))
        {
            iris::Struct_declaration const& struct_declaration = *std::get<iris::Struct_declaration const*>(declaration.data);

            auto const location = std::find_if(
                struct_declaration.member_names.begin(),
                struct_declaration.member_names.end(),
                [&](std::pmr::string const& current_member_name) -> bool
                {
                    return current_member_name == member_name;
                }
            );
            if (location == struct_declaration.member_names.end())
                return std::nullopt;

            std::size_t const member_index = std::distance(struct_declaration.member_names.begin(), location);
            return struct_declaration.member_types[member_index];
        }
        else if (std::holds_alternative<iris::Union_declaration const*>(declaration.data))
        {
            iris::Union_declaration const& union_declaration = *std::get<iris::Union_declaration const*>(declaration.data);
            
            auto const location = std::find_if(
                union_declaration.member_names.begin(),
                union_declaration.member_names.end(),
                [&](std::pmr::string const& current_member_name) -> bool
                {
                    return current_member_name == member_name;
                }
            );
            if (location == union_declaration.member_names.end())
                return std::nullopt;

            std::size_t const member_index = std::distance(union_declaration.member_names.begin(), location);
            return union_declaration.member_types[member_index];
        }
        else
        {
            return std::nullopt;
        }
    }

    std::pmr::vector<Declaration_member_info> get_declaration_member_infos(
        Declaration const& declaration,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<Declaration_member_info> members{output_allocator};

        if (std::holds_alternative<Enum_declaration const*>(declaration.data))
        {
            Enum_declaration const& enum_declaration = *std::get<Enum_declaration const*>(declaration.data);

            members.reserve(enum_declaration.values.size());

            for (std::size_t member_index = 0; member_index < enum_declaration.values.size(); ++member_index)
            {
                iris::Enum_value const& enum_value = enum_declaration.values[member_index];
                Declaration_member_info member_info =
                {
                    .member_name = enum_value.name,
                    .member_type = create_custom_type_reference(declaration.module_name, enum_declaration.name),
                    .member_source_position =
                        enum_value.source_location.has_value() ?
                        std::optional<Source_position>{Source_position{enum_value.source_location->line, enum_value.source_location->column}} :
                        std::optional<Source_position>{std::nullopt},
                };

                members.push_back(std::move(member_info));
            }
        }
        else if (std::holds_alternative<Struct_declaration const*>(declaration.data))
        {
            Struct_declaration const& struct_declaration = *std::get<Struct_declaration const*>(declaration.data);

            members.reserve(struct_declaration.member_types.size());

            for (std::size_t member_index = 0; member_index < struct_declaration.member_types.size(); ++member_index)
            {
                Declaration_member_info member_info =
                {
                    .member_name = struct_declaration.member_names[member_index],
                    .member_type = struct_declaration.member_types[member_index],
                    .member_source_position = 
                        struct_declaration.member_source_positions.has_value() ?
                        std::optional<Source_position>{struct_declaration.member_source_positions.value()[member_index]} : 
                        std::optional<Source_position>{std::nullopt},
                };

                members.push_back(std::move(member_info));
            }
        }
        else if (std::holds_alternative<Union_declaration const*>(declaration.data))
        {
            Union_declaration const& union_declaration = *std::get<Union_declaration const*>(declaration.data);

            members.reserve(union_declaration.member_types.size());

            for (std::size_t member_index = 0; member_index < union_declaration.member_types.size(); ++member_index)
            {
                Declaration_member_info member_info =
                {
                    .member_name = union_declaration.member_names[member_index],
                    .member_type = union_declaration.member_types[member_index],
                    .member_source_position =
                        union_declaration.member_source_positions.has_value() ?
                        std::optional<Source_position>{union_declaration.member_source_positions.value()[member_index]} :
                        std::optional<Source_position>{std::nullopt},
                };

                members.push_back(std::move(member_info));
            }
        }

        return members;
    }

    std::optional<Scope> calculate_scope(
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition const& function_definition,
        iris::Declaration_database const& declaration_database,
        iris::Source_position const& source_position
    )
    {
        std::optional<Scope> output = std::nullopt;
        std::optional<Scope> last_scope = std::nullopt;

         auto const process_statement = [&](iris::Statement const& statement, iris::compiler::Scope const& scope) -> void {

            if (output.has_value())
                return;
            
            if (statement.expressions.empty())
            {
                last_scope = scope;
                return;
            }

            iris::Expression const& first_expression = statement.expressions[0];
            if (first_expression.source_range.has_value())
            {
                if (range_contains_position(first_expression.source_range.value(), source_position) && !is_add_scope_expression(first_expression))
                {
                    output = scope;
                }
                else if (first_expression.source_range->end.line == source_position.line)
                {
                    output = scope;

                    if (std::holds_alternative<iris::Variable_declaration_expression>(first_expression.data))
                    {
                        iris::Variable_declaration_expression const& variable = std::get<iris::Variable_declaration_expression>(first_expression.data);
                        std::optional<iris::Type_reference> const type_reference = get_expression_type(core_module.name, nullptr, scope, statement, statement.expressions[variable.right_hand_side.expression_index], std::nullopt, declaration_database);
                        if (type_reference.has_value())
                            output->variables.push_back(
                                create_variable(variable.name, type_reference.value(), variable.is_mutable, false, first_expression.source_range)
                            );
                    }
                    else if (std::holds_alternative<iris::Variable_declaration_with_type_expression>(first_expression.data))
                    {
                        iris::Variable_declaration_with_type_expression const& variable = std::get<iris::Variable_declaration_with_type_expression>(first_expression.data);

                        std::optional<iris::Type_reference> const variable_type = iris::get_variable_declaration_with_type_expression_type(statement, variable);
                        if (variable_type.has_value())
                        {
                            output->variables.push_back(
                                create_variable(variable.name, std::move(variable_type.value()), variable.is_mutable, false, first_expression.source_range)
                            );
                        }
                    }
                }
            }
        };

        iris::compiler::Scope scope = {};
        iris::compiler::add_parameters_to_scope(
            scope,
            function_declaration.input_parameter_names,
            function_declaration.type.input_parameter_types,
            function_declaration.input_parameter_source_positions
        );

        iris::compiler::visit_statements_using_scope(
            core_module.name,
            &function_declaration,
            scope,
            function_definition.statements,
            declaration_database,
            process_statement
        );

        if (output.has_value())
            return output.value();

        if (last_scope.has_value())
            return last_scope.value();

        return scope;
    }

    Variable const* find_variable_from_scope(
        Scope const& scope,
        std::string_view const name
    )
    {
        auto const location = std::find_if(
            scope.variables.begin(),
            scope.variables.end(),
            [&](Variable const& variable) -> bool { return variable.name == name; }
        );
        if (location == scope.variables.end())
            return nullptr;

        return &(*location);
    }
}
