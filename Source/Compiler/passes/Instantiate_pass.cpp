module;

#include <cassert>

module iris.compiler.instantiate_pass;

import std;

import iris.core;
import iris.core.declarations;
import iris.core.expressions;
import iris.core.types;
import iris.compiler;
import iris.compiler.all_passes;
import iris.compiler.analysis;
import iris.compiler.clang_code_generation;
import iris.compiler.clang_data;
import iris.compiler.types;

namespace iris::compiler
{
    static void add_instantiated_type_to_module(
        iris::Module& core_module,
        Declaration_database& declaration_database,
        Type_instance const& type_instance
    )
    {
        std::pmr::string const mangled_name = mangle_type_instance_name(type_instance);

        std::optional<Declaration> const existing = find_declaration_in_instanced_module_declarations(
            core_module.instanced_declarations,
            type_instance.type_constructor.module_reference.name,
            mangled_name
        );
        if (existing.has_value())
            return;

        Declaration_instance_storage storage = instantiate_type_instance(declaration_database, type_instance);

        if (std::holds_alternative<Struct_declaration>(storage.data))
        {
            Struct_declaration& struct_declaration = std::get<Struct_declaration>(storage.data);
            core_module.instanced_declarations.struct_declarations.push_back(std::move(struct_declaration));
            
            add_struct_declaration(declaration_database, type_instance.type_constructor.module_reference.name, false, core_module.instanced_declarations.struct_declarations.back());
        }
    }

    static void instantiate_type(
        iris::Module& core_module,
        Declaration_database& declaration_database,
        iris::Type_reference& mutable_type_reference,
        Type_instance const& type_instance
    )
    {
        add_instantiated_type_to_module(core_module, declaration_database, type_instance);

        // Replace `iris::Type_instance` by the actual type
        {
            std::pmr::string const mangled_name = mangle_type_instance_name(type_instance);
        
            mutable_type_reference = iris::create_custom_type_reference(type_instance.type_constructor.module_reference.name, mangled_name);
        }
    }

    static void instantiate_all_types(
        iris::Module& core_module,
        Declaration_database& declaration_database
    )
    {
        auto const instantiate_all = [&](std::string_view const declaration_name, iris::Type_reference const& type_reference) -> bool {

            if (std::holds_alternative<Type_instance>(type_reference.data))
            {
                iris::Type_reference& mutable_type_reference = const_cast<iris::Type_reference&>(type_reference);
                
                Type_instance const& type_instance = std::get<Type_instance>(type_reference.data);
                instantiate_type(core_module, declaration_database, mutable_type_reference, type_instance);
            }

            return false;
        };

        iris::visit_type_references_recursively_with_declaration_name(
            core_module,
            instantiate_all
        );
    }

    static void instantiate_types_in_function(
        iris::Module& core_module,
        Declaration_database& declaration_database,
        iris::Function_declaration& function_declaration,
        iris::Function_definition& function_definition
    )
    {
        auto const instantiate_all = [&](iris::Type_reference const& type_reference) -> bool {

            if (std::holds_alternative<Type_instance>(type_reference.data))
            {
                iris::Type_reference& mutable_type_reference = const_cast<iris::Type_reference&>(type_reference);

                Type_instance const& type_instance = std::get<Type_instance>(type_reference.data);
                instantiate_type(core_module, declaration_database, mutable_type_reference, type_instance);
            }

            return false;
        };

        iris::visit_type_references_recursively(
            function_declaration,
            instantiate_all
        );

        iris::visit_type_references_recursively(
            function_definition,
            instantiate_all
        );
    }

    static bool is_builtin_instance_call(
        iris::Statement const& statement,
        Instance_call_expression const& expression
    )
    {
        iris::Expression const& left_hand_side = statement.expressions[expression.left_hand_side.expression_index];
        if (std::holds_alternative<iris::Variable_expression>(left_hand_side.data))
        {
            iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(left_hand_side.data);
            return is_builtin_function_name(variable_expression.name);
        }

        return false;
    }

    static void instantiate_function(
        iris::Module& core_module,
        iris::Statement& statement,
        iris::Expression const& expression,
        Instance_call_key const key,
        All_passes_parameters const& parameters
    )
    {
        std::pmr::string const mangled_name = std::pmr::string{mangle_instance_call_name(key)};

        std::optional<Declaration> const existing = find_declaration_in_instanced_module_declarations(
            core_module.instanced_declarations,
            key.module_name,
            mangled_name
        );
        if (existing.has_value())
            return;
        
        iris::Function_constructor const* function_constructor = get_function_constructor(
            parameters.declaration_database,
            key.module_name,
            key.function_constructor_name
        );
        if (function_constructor == nullptr)
            throw std::runtime_error{ "Could not find function constructor!" };

        iris::Function_expression function_expression = create_instance_call_expression_value(
            *function_constructor,
            key.arguments,
            key
        );

        run_all_passes_on_function(
            core_module,
            function_expression.declaration,
            function_expression.definition,
            parameters
        );

        core_module.instanced_declarations.function_declarations.push_back(function_expression.declaration);
        core_module.definitions.function_definitions.push_back(function_expression.definition);

        add_function_declaration(parameters.declaration_database, key.module_name, false, core_module.instanced_declarations.function_declarations.back());
    }

    struct Replace_instantiate_function_parameters
    {
        iris::Statement& statement;
        std::size_t expression_index;
        Instance_call_key const key;
    };

    static std::optional<std::size_t> find_parent_call_expression_index(
        iris::Statement const& statement,
        std::size_t const callee_expression_index
    )
    {
        for (std::size_t index = 0; index < statement.expressions.size(); ++index)
        {
            iris::Expression const& expression = statement.expressions[index];
            if (!std::holds_alternative<iris::Call_expression>(expression.data))
                continue;

            iris::Call_expression const& call_expression = std::get<iris::Call_expression>(expression.data);
            if (call_expression.expression.expression_index == callee_expression_index)
                return index;
        }

        return std::nullopt;
    }

    static std::optional<iris::Expression_index> create_implicit_receiver_argument(
        iris::Statement& statement,
        iris::Expression const& callee_expression
    )
    {
        if (std::holds_alternative<iris::Access_expression>(callee_expression.data))
        {
            iris::Access_expression const& access_expression = std::get<iris::Access_expression>(callee_expression.data);

            iris::Expression_index const copied_receiver_expression = copy_expressions_to_new_statement(
                statement,
                statement,
                access_expression.expression
            );

            iris::Expression receiver_expression = {
                .data = iris::Unary_expression{
                    .expression = copied_receiver_expression,
                    .operation = iris::Unary_operation::Address_of
                }
            };

            statement.expressions.push_back(std::move(receiver_expression));
            return iris::Expression_index{ .expression_index = statement.expressions.size() - 1 };
        }

        if (std::holds_alternative<iris::Dereference_and_access_expression>(callee_expression.data))
        {
            iris::Dereference_and_access_expression const& access_expression = std::get<iris::Dereference_and_access_expression>(callee_expression.data);

            iris::Expression_index const copied_receiver_expression = copy_expressions_to_new_statement(
                statement,
                statement,
                access_expression.expression
            );

            return copied_receiver_expression;
        }

        return std::nullopt;
    }

    static void replace_instantiate_function(
        iris::Module& core_module,
        Replace_instantiate_function_parameters const& instantiate_parameters,
        All_passes_parameters const& parameters
    )
    {
        std::pmr::string const mangled_name = std::pmr::string{mangle_instance_call_name(instantiate_parameters.key)};

        std::size_t const callee_expression_index = instantiate_parameters.expression_index;
        iris::Expression const& callee_expression = instantiate_parameters.statement.expressions[callee_expression_index];

        std::optional<std::size_t> const parent_call_expression_index = find_parent_call_expression_index(
            instantiate_parameters.statement,
            callee_expression_index
        );

        if (parent_call_expression_index.has_value())
        {
            std::optional<iris::Expression_index> const receiver_argument = create_implicit_receiver_argument(
                instantiate_parameters.statement,
                callee_expression
            );

            if (receiver_argument.has_value())
            {
                iris::Expression& parent_call_expression = instantiate_parameters.statement.expressions[parent_call_expression_index.value()];
                iris::Call_expression& call_expression = std::get<iris::Call_expression>(parent_call_expression.data);
                call_expression.arguments.insert(call_expression.arguments.begin(), receiver_argument.value());
            }
        }

        iris::Expression const& expression_to_replace = instantiate_parameters.statement.expressions[callee_expression_index];

        std::pmr::vector<iris::Expression> new_expressions
        {
            iris::create_variable_expression(mangled_name)
        };

        iris::Statement const new_statement = iris::create_statement(std::move(new_expressions));

        replace_expression(
            instantiate_parameters.statement,
            expression_to_replace,
            new_statement,
            parameters.temporaries_allocator
        );
    }

    static void visit_deduced_instance_calls(
        iris::Module& core_module,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition& function_definition,
        All_passes_parameters const& parameters,
        std::function<void(iris::Module&, iris::Statement&, iris::Expression const&, Instance_call_key const&, All_passes_parameters const&)> const& callback
    )
    {
        auto const scope_callback = [&](iris::Statement const& statement, iris::compiler::Scope const& scope) -> bool
        {
            auto const instantiate_all = [&](iris::Expression const& expression, iris::Statement const& statement) -> bool
            {
                if (std::holds_alternative<iris::Call_expression>(expression.data))
                {
                    iris::Call_expression const& call_expression = std::get<iris::Call_expression>(expression.data);
                    std::optional<Deduced_instance_call> const deduced_instance_call = deduce_instance_call_arguments(
                        parameters.declaration_database,
                        core_module,
                        scope,
                        statement,
                        call_expression,
                        parameters.temporaries_allocator
                    );
                    if (deduced_instance_call.has_value())
                    {
                        Instance_call_key const key = {
                            .module_name = deduced_instance_call->custom_type_reference.module_reference.name,
                            .function_constructor_name = deduced_instance_call->custom_type_reference.name,
                            .arguments = deduced_instance_call->arguments
                        };

                        iris::Statement& mutable_statement = const_cast<iris::Statement&>(statement);

                        callback(
                            core_module,
                            mutable_statement,
                            statement.expressions[call_expression.expression.expression_index],
                            key,
                            parameters
                        );
                    }
                }
                else if (std::holds_alternative<Instance_call_expression>(expression.data))
                {
                    Instance_call_expression const& instance_call_expression = std::get<Instance_call_expression>(expression.data);

                    if (is_builtin_instance_call(statement, instance_call_expression))
                        return false;

                    Instance_call_key const key = create_instance_call_key(
                        parameters.declaration_database,
                        core_module,
                        instance_call_expression,
                        statement
                    );

                    iris::Statement& mutable_statement = const_cast<iris::Statement&>(statement);

                    callback(
                        core_module,
                        mutable_statement,
                        expression,
                        key,
                        parameters
                    );
                }

                return false;
            };

            iris::visit_expressions(
                statement,
                instantiate_all
            );

            return false;
        };

        Scope scope;
        add_parameters_to_scope(scope, function_declaration.input_parameter_names, function_declaration.type.input_parameter_types, function_declaration.input_parameter_source_positions);

        visit_statements_using_scope(
            core_module,
            &function_declaration,
            scope,
            function_definition.statements,
            parameters.declaration_database,
            scope_callback
        );
    }

    static void instantiate_functions_in_function(
        iris::Module& core_module,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition& function_definition,
        All_passes_parameters const& parameters
    )
    {
        visit_deduced_instance_calls(
            core_module,
            function_declaration,
            function_definition,
            parameters,
            instantiate_function
        );

        if (function_definition.name == "run")
        {
            int i = 0;
        }

        std::pmr::vector<Replace_instantiate_function_parameters> replace_parameters;
        auto const gather_replacements = [&](iris::Module& core_module, iris::Statement& statement, iris::Expression const& expression, Instance_call_key const key, All_passes_parameters const& parameters)
        {
            replace_parameters.push_back({
                statement,
                find_expression_index(statement, expression),
                key
            });
        };

        visit_deduced_instance_calls(
            core_module,
            function_declaration,
            function_definition,
            parameters,
            gather_replacements
        );

        for (Replace_instantiate_function_parameters const value : replace_parameters)
        {
            replace_instantiate_function(core_module, value, parameters);
        }
    }

    static void instantiate_all_functions(
        iris::Module& core_module,
        All_passes_parameters const& parameters
    )
    {
        for (iris::Function_definition& function_definition : core_module.definitions.function_definitions)
        {
            std::optional<Function_declaration const*> const function_declaration = find_function_declaration(core_module, function_definition.name);

            instantiate_functions_in_function(
                core_module,
                *function_declaration.value(),
                function_definition,
                parameters
            );
        }
    }

    static void verify_no_instance_calls(
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& expression,
        iris::Declaration_database const& declaration_database
    )
    {
        if (std::holds_alternative<iris::Access_expression>(expression.data))
        {
            iris::Access_expression const& access_expression = std::get<iris::Access_expression>(expression.data);

            std::optional<iris::Type_reference> const expression_type = get_expression_type(
                core_module,
                &function_declaration,
                scope,
                statement,
                expression,
                std::nullopt,
                declaration_database
            );
            if (expression_type.has_value())
            {
                std::optional<Declaration> const declaration = find_underlying_declaration(declaration_database, expression_type.value());
                if (declaration.has_value())
                {
                    assert(!std::holds_alternative<Function_constructor const*>(declaration.value().data));
                }
            }
        }
        else if (std::holds_alternative<iris::Instance_call_expression>(expression.data))
        {
            iris::Instance_call_expression const& instance_call_expression = std::get<iris::Instance_call_expression>(expression.data);
            iris::Expression const& expression = statement.expressions[instance_call_expression.left_hand_side.expression_index];
            assert(std::holds_alternative<iris::Variable_expression>(expression.data));
            iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(expression.data);
            assert(iris::is_builtin_function_name(variable_expression.name));
        }
    }

    static void verify_no_instance_calls_in_function(
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition const& function_definition,
        iris::Declaration_database const& declaration_database
    )
    {
        auto const scope_callback = [&](iris::Statement const& statement, iris::compiler::Scope const& scope) -> bool
        {
            auto const verify_all = [&](iris::Expression const& expression, iris::Statement const& statement) -> bool
            {
                verify_no_instance_calls(core_module, function_declaration, scope, statement, expression, declaration_database);
                return false;
            };

            iris::visit_expressions(
                statement,
                verify_all
            );

            return false;
        };

        Scope scope;
        add_parameters_to_scope(scope, function_declaration.input_parameter_names, function_declaration.type.input_parameter_types, function_declaration.input_parameter_source_positions);

        visit_statements_using_scope(
            core_module,
            &function_declaration,
            scope,
            function_definition.statements,
            declaration_database,
            scope_callback
        );
    }

    static void verify_no_instance_calls_in_module(
        iris::Module const& core_module,
        iris::Declaration_database const& declaration_database
    )
    {
        for (iris::Function_definition const& definition : core_module.definitions.function_definitions)
        {
            std::optional<Function_declaration const*> const declaration = find_function_declaration(core_module, definition.name);
            if (!declaration.has_value())
                continue;

            verify_no_instance_calls_in_function(
                core_module,
                *declaration.value(),
                definition,
                declaration_database
            );
        }
    }

    static void verify_no_type_instances(
        iris::Type_reference const& type_reference,
        iris::Declaration_database const& declaration_database
    )
    {
        assert(!std::holds_alternative<Type_instance>(type_reference.data));

        if (std::holds_alternative<Custom_type_reference>(type_reference.data))
        {
            Custom_type_reference const& custom_type_reference = std::get<Custom_type_reference>(type_reference.data);
            std::optional<Declaration> const declaration = find_declaration(
                declaration_database,
                custom_type_reference.module_reference.name,
                custom_type_reference.name
            );
            if (declaration.has_value())
            {
                assert(!std::holds_alternative<Function_constructor const*>(declaration->data));
                assert(!std::holds_alternative<Type_constructor const*>(declaration->data));
            }
        }
    }

    static void verify_no_type_instances_in_function(
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition const& function_definition,
        iris::Declaration_database const& declaration_database
    )
    {
        auto const visitor = [&](iris::Type_reference const& type_reference) -> bool
        {
            verify_no_type_instances(type_reference, declaration_database);
            return false;
        };

        iris::visit_type_references_recursively(
            function_declaration,
            visitor
        );

        iris::visit_type_references_recursively(
            function_definition,
            visitor
        );
    }

    static void verify_no_type_instances_in_module(
        iris::Module const& core_module,
        iris::Declaration_database const& declaration_database
    )
    {
        auto const visitor = [&](std::string_view const declaration_name, iris::Type_reference const& type_reference) -> bool
        {
            verify_no_type_instances(type_reference, declaration_database);
            return false;
        };

        iris::visit_type_references_recursively_with_declaration_name(
            core_module,
            visitor
        );
    }

    void run_instantiate_pass_on_function(
        iris::Module& core_module,
        iris::Function_declaration& function_declaration,
        iris::Function_definition& function_definition,
        All_passes_parameters const& parameters
    )
    {
        instantiate_functions_in_function(core_module, function_declaration, function_definition, parameters);
        instantiate_types_in_function(core_module, parameters.declaration_database, function_declaration, function_definition);

        verify_no_instance_calls_in_function(core_module, function_declaration, function_definition, parameters.declaration_database);
        verify_no_type_instances_in_function(core_module, function_declaration, function_definition, parameters.declaration_database);
    }

    void run_instantiate_pass_on_module(
        iris::Module& core_module,
        All_passes_parameters const& parameters
    )
    {
        instantiate_all_functions(core_module, parameters);
        instantiate_all_types(core_module, parameters.declaration_database);

        verify_no_instance_calls_in_module(core_module, parameters.declaration_database);
        verify_no_type_instances_in_module(core_module, parameters.declaration_database);
    }
}
