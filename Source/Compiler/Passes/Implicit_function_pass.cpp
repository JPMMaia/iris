module iris.compiler.implicit_function_pass;

import std;

import iris.compiler.analysis;
import iris.core;
import iris.core.declarations;
import iris.core.expressions;
import iris.core.types;

namespace iris::compiler
{
    struct Implicit_function_data
    {
        std::optional<std::string_view> const import_alias;
        std::string_view const function_name;
        std::size_t const call_expression_index;
        std::size_t const left_access_expression_index;
        bool dereference;
    };

    static std::pmr::string get_type_module_name(
        std::string_view const declaration_module_name,
        std::string_view const declaration_name
    )
    {
        std::optional<iris::Custom_type_reference> const type_instance_reference = unmangle_type_instance_name(declaration_name);
        if (type_instance_reference.has_value())
            return type_instance_reference->module_reference.name;
        return std::pmr::string{declaration_module_name};
    }

    static std::optional<Implicit_function_data> find_implicit_function_auxiliary(
        iris::Statement const& statement,
        std::size_t const call_expression_index,
        std::size_t const left_access_expression_index,
        std::string_view const access_expression_member_name,
        bool const dereference,
        iris::Module const& core_module,
        Scope const& scope,
        iris::Declaration_database const& declaration_database
    )
    {
        iris::Expression const& left_access_expression = statement.expressions[left_access_expression_index];

        if (std::holds_alternative<iris::Variable_expression>(left_access_expression.data))
        {
            iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(left_access_expression.data);
            Variable const* variable = find_variable_from_scope(scope, variable_expression.name);
            if (variable == nullptr)
                return std::nullopt;

            std::optional<iris::Type_reference> const declaration_type =
                dereference ?
                std::optional<iris::Type_reference>{remove_pointer(variable->type)} :
                std::optional<iris::Type_reference>{variable->type};
            if (!declaration_type.has_value())
                return std::nullopt;

            std::optional<Declaration> const declaration = find_underlying_declaration(declaration_database, declaration_type.value());
            if (declaration.has_value())
            {
                if (std::holds_alternative<iris::Struct_declaration const*>(declaration->data))
                {
                    iris::Struct_declaration const& struct_declaration = *std::get<iris::Struct_declaration const*>(declaration->data);

                    auto const member_location = std::find(
                        struct_declaration.member_names.begin(),
                        struct_declaration.member_names.end(),
                        access_expression_member_name
                    );

                    if (member_location != struct_declaration.member_names.end())
                        return std::nullopt;

                    std::pmr::string const declaration_module_name = get_type_module_name(declaration->module_name, struct_declaration.name);
                    iris::Import_module_with_alias const* const import_module_with_alias = iris::find_import_module_with_module_name(
                        core_module.dependencies,
                        declaration_module_name
                    );
                    std::optional<std::string_view> const import_alias = 
                        import_module_with_alias != nullptr ?
                        std::optional<std::string_view>{import_module_with_alias->alias} :
                        std::optional<std::string_view>{};

                    return Implicit_function_data
                    {
                        .import_alias = import_alias,
                        .function_name = access_expression_member_name,
                        .call_expression_index = call_expression_index,
                        .left_access_expression_index = left_access_expression_index,
                        .dereference = dereference,
                    };
                }
            }
        }

        return std::nullopt;
    }

    static std::optional<Implicit_function_data> find_implicit_function(
        iris::Statement const& statement,
        iris::Expression const& expression,
        std::size_t const expression_index,
        iris::Module const& core_module,
        Scope const& scope,
        iris::Declaration_database const& declaration_database
    )
    {
        if (std::holds_alternative<iris::Call_expression>(expression.data))
        {
            iris::Call_expression const& data = std::get<iris::Call_expression>(expression.data);
            iris::Expression const& left_call_expression = statement.expressions[data.expression.expression_index];

            if (std::holds_alternative<iris::Access_expression>(left_call_expression.data))
            {
                iris::Access_expression const& access_expression = std::get<iris::Access_expression>(left_call_expression.data);

                return find_implicit_function_auxiliary(
                    statement,
                    expression_index,
                    access_expression.expression.expression_index,
                    access_expression.member_name,
                    false,
                    core_module,
                    scope,
                    declaration_database
                );
            }
            else if (std::holds_alternative<iris::Dereference_and_access_expression>(left_call_expression.data))
            {
                iris::Dereference_and_access_expression const& access_expression = std::get<iris::Dereference_and_access_expression>(left_call_expression.data);
                
                return find_implicit_function_auxiliary(
                    statement,
                    expression_index,
                    access_expression.expression.expression_index,
                    access_expression.member_name,
                    true,
                    core_module,
                    scope,
                    declaration_database
                );
            }
        }

        return std::nullopt;
    }

    static iris::Statement transform_statement_with_implicit_function(
        iris::Statement const& statement,
        std::optional<std::string_view> const import_alias,
        std::string_view const function_name,
        std::size_t const call_expression_index,
        std::size_t const left_access_expression_index,
        bool const dereference
    )
    {
        // import external_module as em;
        // var instance: em.My_struct = ...;
        // instance.get_v0() -> em.get_v0(&instance)
        // var pointer = &instance;
        // pointer->get_v0() -> em.get_v0(pointer);

        iris::Call_expression const& call_expression = std::get<iris::Call_expression>(statement.expressions[call_expression_index].data);

        std::pmr::vector<iris::Expression> new_expressions = statement.expressions;
        new_expressions.reserve(statement.expressions.size() + 3);

        std::size_t new_left_access_expression_index = 0;

        if (import_alias.has_value())
        {
            Expression_reference<iris::Access_expression> access_alias_expression = create_expression_inside_statement<iris::Access_expression>(new_expressions);
            access_alias_expression.value->member_name = std::pmr::string{function_name};
            
            Expression_reference<iris::Variable_expression> alias_name_expression = create_expression_inside_statement<iris::Variable_expression>(new_expressions);
            alias_name_expression.value->name = std::pmr::string{import_alias.value()};
            access_alias_expression.value->expression = {.expression_index = alias_name_expression.index};

            new_left_access_expression_index = access_alias_expression.index;
        }
        else
        {
            Expression_reference<iris::Variable_expression> function_name_expression = create_expression_inside_statement<iris::Variable_expression>(new_expressions);
            function_name_expression.value->name = std::pmr::string{function_name};

            new_left_access_expression_index = function_name_expression.index;
        }

        Expression_reference<iris::Call_expression> new_call_expression = create_expression_reference<iris::Call_expression>(new_expressions, call_expression_index);
        
        std::size_t new_call_left_side_index = left_access_expression_index;
        if (!dereference)
        {
            Expression_reference<iris::Unary_expression> new_take_address_expression = create_expression_inside_statement<iris::Unary_expression>(new_expressions);
            *new_take_address_expression.value = {
                .expression = {.expression_index = left_access_expression_index },
                .operation = iris::Unary_operation::Address_of
            };
            new_call_left_side_index = new_take_address_expression.index;
        }

        *new_call_expression.value = {
            .expression = { .expression_index = new_left_access_expression_index },
            .arguments = {}
        };
        new_call_expression.value->arguments.reserve(1 + call_expression.arguments.size());
        new_call_expression.value->arguments.push_back(
            iris::Expression_index{.expression_index = new_call_left_side_index }
        );
        new_call_expression.value->arguments.insert(
            new_call_expression.value->arguments.end(),
            call_expression.arguments.begin(),
            call_expression.arguments.end()
        );

        return iris::Statement
        {
            .expressions = std::move(new_expressions)
        };
    }

    /*static bool try_transform_statement_with_implicit_function(
        iris::Statement& statement,
        iris::Module& core_module,
        Scope const& scope,
        iris::Declaration_database const& declaration_database
    )
    {
        bool transformed = false;

        while (true)
        {
            bool transformed_in_iteration = false;

            std::size_t const count = statement.expressions.size();
            for (std::size_t index = 0; index < count; ++index)
            {
                std::size_t const reverse_index = count - index - 1;
                iris::Expression const& expression = statement.expressions[reverse_index];

                std::optional<Implicit_function_data> const implicit_function = find_implicit_function(
                    statement,
                    expression,
                    reverse_index,
                    core_module,
                    scope,
                    declaration_database
                );
                if (!implicit_function.has_value())
                    continue;

                if (implicit_function->import_alias.has_value())
                    add_import_usage(core_module, implicit_function->import_alias.value(), implicit_function->function_name);

                statement = transform_statement_with_implicit_function(
                    statement,
                    implicit_function->import_alias,
                    implicit_function->function_name,
                    implicit_function->call_expression_index,
                    implicit_function->left_access_expression_index,
                    implicit_function->dereference
                );

                transformed = true;
                transformed_in_iteration = true;
                break;
            }

            if (!transformed_in_iteration)
                break;
        }

        return transformed;
    }*/

    void run_implicit_function_pass_on_function(
        iris::Module const& core_module,
        iris::Module_dependencies& dependencies,
        iris::Declaration_database const& declaration_database,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition& function_definition
    )
    {
        Scope scope{};

        add_parameters_to_scope(
            scope,
            function_declaration.input_parameter_names,
            function_declaration.type.input_parameter_types,
            function_declaration.input_parameter_source_positions
        );

        auto const callback = [&](iris::Statement const& statement, Scope const& scope) -> void
        {
            auto const replace_implicit_functions = [&](iris::Expression const& expression, iris::Statement const& statement) -> bool
            {
                if (std::holds_alternative<iris::Call_expression>(expression.data))
                {
                    std::size_t const expression_index = find_expression_index(statement, expression);

                    std::optional<Implicit_function_data> const implicit_function = find_implicit_function(
                        statement,
                        expression,
                        expression_index,
                        core_module,
                        scope,
                        declaration_database
                    );
                    if (implicit_function.has_value())
                    {
                        if (implicit_function->import_alias.has_value())
                            add_import_usage(dependencies, implicit_function->import_alias.value(), implicit_function->function_name);

                        iris::Statement new_statement = transform_statement_with_implicit_function(
                            statement,
                            implicit_function->import_alias,
                            implicit_function->function_name,
                            implicit_function->call_expression_index,
                            implicit_function->left_access_expression_index,
                            implicit_function->dereference
                        );
                        iris::Statement& mutable_statement = const_cast<iris::Statement&>(statement);
                        mutable_statement = std::move(new_statement);
                    }
                }

                return false;
            };

            iris::visit_expressions(statement, replace_implicit_functions);
        };

        visit_statements_using_scope(
            core_module,
            &function_declaration,
            scope,
            function_definition.statements,
            declaration_database,
            callback
        );
    }

    void run_implicit_function_pass_on_module(
        iris::Module& core_module,
        iris::Declaration_database const& declaration_database
    )
    {
        for (iris::Function_definition& function_definition : core_module.definitions.function_definitions)
        {
            std::optional<Function_declaration const*> const function_declaration = find_function_declaration(core_module, function_definition.name);
            if (!function_declaration.has_value())
                continue;

            run_implicit_function_pass_on_function(
                core_module,
                core_module.dependencies,
                declaration_database,
                *function_declaration.value(),
                function_definition
            );
        }
    }
}
