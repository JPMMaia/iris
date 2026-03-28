module h.compiler.implicit_function_pass;

import std;

import h.compiler.analysis;
import h.core;
import h.core.declarations;
import h.core.expressions;
import h.core.types;

namespace h::compiler
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
        std::optional<h::Custom_type_reference> const type_instance_reference = unmangle_type_instance_name(declaration_name);
        if (type_instance_reference.has_value())
            return type_instance_reference->module_reference.name;
        return std::pmr::string{declaration_module_name};
    }

    static std::optional<Implicit_function_data> find_implicit_function_auxiliary(
        h::Statement const& statement,
        std::size_t const call_expression_index,
        std::size_t const left_access_expression_index,
        std::string_view const access_expression_member_name,
        bool const dereference,
        h::Module const& core_module,
        Scope const& scope,
        h::Declaration_database const& declaration_database
    )
    {
        h::Expression const& left_access_expression = statement.expressions[left_access_expression_index];

        if (std::holds_alternative<h::Variable_expression>(left_access_expression.data))
        {
            h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(left_access_expression.data);
            Variable const* variable = find_variable_from_scope(scope, variable_expression.name);
            if (variable == nullptr)
                return std::nullopt;

            std::optional<h::Type_reference> const declaration_type =
                dereference ?
                std::optional<h::Type_reference>{remove_pointer(variable->type)} :
                std::optional<h::Type_reference>{variable->type};
            if (!declaration_type.has_value())
                return std::nullopt;

            std::optional<Declaration> const declaration = find_underlying_declaration(declaration_database, declaration_type.value());
            if (declaration.has_value())
            {
                if (std::holds_alternative<h::Struct_declaration const*>(declaration->data))
                {
                    h::Struct_declaration const& struct_declaration = *std::get<h::Struct_declaration const*>(declaration->data);

                    auto const member_location = std::find(
                        struct_declaration.member_names.begin(),
                        struct_declaration.member_names.end(),
                        access_expression_member_name
                    );

                    if (member_location != struct_declaration.member_names.end())
                        return std::nullopt;

                    std::pmr::string const declaration_module_name = get_type_module_name(declaration->module_name, struct_declaration.name);
                    h::Import_module_with_alias const* const import_module_with_alias = h::find_import_module_with_module_name(
                        core_module,
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
        h::Statement const& statement,
        h::Expression const& expression,
        std::size_t const expression_index,
        h::Module const& core_module,
        Scope const& scope,
        h::Declaration_database const& declaration_database
    )
    {
        if (std::holds_alternative<h::Call_expression>(expression.data))
        {
            h::Call_expression const& data = std::get<h::Call_expression>(expression.data);
            h::Expression const& left_call_expression = statement.expressions[data.expression.expression_index];

            if (std::holds_alternative<h::Access_expression>(left_call_expression.data))
            {
                h::Access_expression const& access_expression = std::get<h::Access_expression>(left_call_expression.data);

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
            else if (std::holds_alternative<h::Dereference_and_access_expression>(left_call_expression.data))
            {
                h::Dereference_and_access_expression const& access_expression = std::get<h::Dereference_and_access_expression>(left_call_expression.data);
                
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

    static h::Statement transform_statement_with_implicit_function(
        h::Statement const& statement,
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

        h::Call_expression const& call_expression = std::get<h::Call_expression>(statement.expressions[call_expression_index].data);

        std::pmr::vector<h::Expression> new_expressions = statement.expressions;
        new_expressions.reserve(statement.expressions.size() + 3);

        std::size_t new_left_access_expression_index = 0;

        if (import_alias.has_value())
        {
            Expression_reference<h::Access_expression> access_alias_expression = create_expression_inside_statement<h::Access_expression>(new_expressions);
            access_alias_expression.value->member_name = std::pmr::string{function_name};
            
            Expression_reference<h::Variable_expression> alias_name_expression = create_expression_inside_statement<h::Variable_expression>(new_expressions);
            alias_name_expression.value->name = std::pmr::string{import_alias.value()};
            access_alias_expression.value->expression = {.expression_index = alias_name_expression.index};

            new_left_access_expression_index = access_alias_expression.index;
        }
        else
        {
            Expression_reference<h::Variable_expression> function_name_expression = create_expression_inside_statement<h::Variable_expression>(new_expressions);
            function_name_expression.value->name = std::pmr::string{function_name};

            new_left_access_expression_index = function_name_expression.index;
        }

        Expression_reference<h::Call_expression> new_call_expression = create_expression_reference<h::Call_expression>(new_expressions, call_expression_index);
        
        std::size_t new_call_left_side_index = left_access_expression_index;
        if (!dereference)
        {
            Expression_reference<h::Unary_expression> new_take_address_expression = create_expression_inside_statement<h::Unary_expression>(new_expressions);
            *new_take_address_expression.value = {
                .expression = {.expression_index = left_access_expression_index },
                .operation = h::Unary_operation::Address_of
            };
            new_call_left_side_index = new_take_address_expression.index;
        }

        *new_call_expression.value = {
            .expression = { .expression_index = new_left_access_expression_index },
            .arguments = {}
        };
        new_call_expression.value->arguments.reserve(1 + call_expression.arguments.size());
        new_call_expression.value->arguments.push_back(
            h::Expression_index{.expression_index = new_call_left_side_index }
        );
        new_call_expression.value->arguments.insert(
            new_call_expression.value->arguments.end(),
            call_expression.arguments.begin(),
            call_expression.arguments.end()
        );

        return h::Statement
        {
            .expressions = std::move(new_expressions)
        };
    }

    /*static bool try_transform_statement_with_implicit_function(
        h::Statement& statement,
        h::Module& core_module,
        Scope const& scope,
        h::Declaration_database const& declaration_database
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
                h::Expression const& expression = statement.expressions[reverse_index];

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
        h::Module& core_module,
        h::Declaration_database const& declaration_database,
        h::Function_declaration const& function_declaration,
        h::Function_definition& function_definition
    )
    {
        Scope scope{};

        add_parameters_to_scope(
            scope,
            function_declaration.input_parameter_names,
            function_declaration.type.input_parameter_types,
            function_declaration.input_parameter_source_positions
        );

        auto const callback = [&](h::Statement const& statement, Scope const& scope) -> void
        {
            auto const replace_implicit_functions = [&](h::Expression const& expression, h::Statement const& statement) -> bool
            {
                if (std::holds_alternative<h::Call_expression>(expression.data))
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
                            add_import_usage(core_module, implicit_function->import_alias.value(), implicit_function->function_name);

                        h::Statement new_statement = transform_statement_with_implicit_function(
                            statement,
                            implicit_function->import_alias,
                            implicit_function->function_name,
                            implicit_function->call_expression_index,
                            implicit_function->left_access_expression_index,
                            implicit_function->dereference
                        );
                        h::Statement& mutable_statement = const_cast<h::Statement&>(statement);
                        mutable_statement = std::move(new_statement);
                    }
                }

                return false;
            };

            h::visit_expressions(statement, replace_implicit_functions);
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
        h::Module& core_module,
        h::Declaration_database const& declaration_database
    )
    {
        for (h::Function_definition& function_definition : core_module.definitions.function_definitions)
        {
            std::optional<Function_declaration const*> const function_declaration = find_function_declaration(core_module, function_definition.name);
            if (!function_declaration.has_value())
                continue;

            run_implicit_function_pass_on_function(
                core_module,
                declaration_database,
                *function_declaration.value(),
                function_definition
            );
        }
    }
}
