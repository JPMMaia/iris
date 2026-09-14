module iris.compiler.lambda_pass;

import std;

import iris.compiler.analysis;
import iris.compiler.common;
import iris.compiler.lambda_database;
import iris.compiler.validation;
import iris.core;
import iris.core.declarations;
import iris.core.expressions;
import iris.core.types;

namespace iris::compiler
{
    static std::pmr::string get_lambda_function_name(
        std::string_view const parent_function_name,
        std::size_t const lambda_index
    )
    {
        return std::pmr::string{ std::format("{}__lambda{}", parent_function_name, lambda_index) };
    }

    // The lambda body as the statements of a function: a block body contributes its own
    // statements, an inline body becomes a single return of the body expression.
    static std::pmr::vector<iris::Statement> create_lambda_body_statements(
        iris::Statement const& body,
        bool const has_output
    )
    {
        if (body.expressions.size() == 1 && std::holds_alternative<iris::Block_expression>(body.expressions[0].data))
        {
            iris::Block_expression const& block_expression = std::get<iris::Block_expression>(body.expressions[0].data);
            return { block_expression.statements.begin(), block_expression.statements.end() };
        }

        iris::Statement statement;

        Expression_reference<iris::Return_expression> const return_expression = create_expression_inside_statement<iris::Return_expression>(statement.expressions);
        if (has_output)
            return_expression.value->expression = iris::Expression_index{ .expression_index = 1 };
        else
            return_expression.value->expression = std::nullopt;

        add_expressions_to_expressions(statement.expressions, body.expressions);

        std::pmr::vector<iris::Statement> statements;
        statements.push_back(std::move(statement));
        return statements;
    }

    // One statement per captured variable, reading it out of the environment pointer so the
    // body's references to the enclosing scope resolve to locals.
    static iris::Statement create_capture_load_statement(std::string_view const captured_name)
    {
        iris::Statement statement;

        // create_expression_inside_statement hands back a pointer into the vector, so reserve
        // first: a reallocation would leave the earlier references dangling.
        statement.expressions.reserve(3);

        Expression_reference<iris::Variable_declaration_expression> const declaration_expression =
            create_expression_inside_statement<iris::Variable_declaration_expression>(statement.expressions);
        Expression_reference<iris::Dereference_and_access_expression> const access_expression =
            create_expression_inside_statement<iris::Dereference_and_access_expression>(statement.expressions);
        Expression_reference<iris::Variable_expression> const variable_expression =
            create_expression_inside_statement<iris::Variable_expression>(statement.expressions);

        *declaration_expression.value = {
            .name = std::pmr::string{ captured_name },
            .is_mutable = false,
            .right_hand_side = { .expression_index = access_expression.index },
        };

        *access_expression.value = {
            .expression = { .expression_index = variable_expression.index },
            .member_name = std::pmr::string{ captured_name },
        };

        variable_expression.value->name = std::pmr::string{ "user_data" };

        return statement;
    }

    struct Lambda_signature
    {
        std::pmr::vector<iris::Type_reference> input_parameter_types;
        std::pmr::vector<iris::Type_reference> output_parameter_types;
    };

    static std::optional<Lambda_signature> resolve_lambda_signature(
        std::string_view const module_name,
        iris::Function_declaration const* const function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& expression,
        std::optional<iris::Type_reference> const& expected_type,
        iris::Declaration_database const& declaration_database
    )
    {
        std::optional<Type_info> const type_info = get_expression_type_info(
            module_name,
            function_declaration,
            scope,
            statement,
            expression,
            expected_type,
            declaration_database
        );

        if (!type_info.has_value())
            return std::nullopt;

        std::optional<iris::Lambda_type> const lambda_type = resolve_lambda_type(declaration_database, type_info->type);
        if (!lambda_type.has_value())
            return std::nullopt;

        return Lambda_signature
        {
            .input_parameter_types = lambda_type->input_parameter_types,
            .output_parameter_types = lambda_type->output_parameter_types,
        };
    }

    struct Lambda_lowering_result
    {
        iris::Function_declaration function_declaration;
        iris::Function_definition function_definition;
        std::optional<iris::Struct_declaration> environment_struct_declaration;
        std::pmr::string lambda_key;
        std::pmr::string generated_function_name;
    };

    static std::optional<Lambda_lowering_result> lower_lambda_expression(
        std::string_view const module_name,
        iris::Function_declaration const* const parent_function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& expression,
        iris::Lambda_expression& lambda_expression,
        std::string_view const parent_function_name,
        std::size_t const lambda_index,
        std::optional<iris::Type_reference> const& expected_type,
        iris::Declaration_database const& declaration_database
    )
    {
        std::optional<Lambda_signature> const signature = resolve_lambda_signature(
            module_name,
            parent_function_declaration,
            scope,
            statement,
            expression,
            expected_type,
            declaration_database
        );

        // Validation has already reported an unresolvable lambda; there is nothing to lower.
        if (!signature.has_value() || signature->input_parameter_types.size() != lambda_expression.parameter_names.size())
            return std::nullopt;

        std::pmr::string const generated_function_name = get_lambda_function_name(parent_function_name, lambda_index);

        // Code generation runs without a preceding analysis phase in some entry points, so the
        // capture set is computed here whenever it has not been filled in yet.
        if (!lambda_expression.captured_variables.has_value())
        {
            std::pmr::vector<std::pmr::string> enclosing_names;
            enclosing_names.reserve(scope.variables.size());
            for (Variable const& variable : scope.variables)
                enclosing_names.push_back(variable.name);

            compute_lambda_captures(lambda_expression, enclosing_names);
        }

        std::span<std::pmr::string const> const captured_names =
            lambda_expression.captured_variables.has_value() ?
            std::span<std::pmr::string const>{ lambda_expression.captured_variables.value() } :
            std::span<std::pmr::string const>{};

        // The environment is a plain struct of the captured values, passed by pointer as the
        // trailing parameter. Without captures the parameter stays an untyped pointer.
        std::optional<iris::Struct_declaration> environment_struct_declaration;
        iris::Type_reference user_data_type = create_pointer_type_type_reference({}, true);

        if (!captured_names.empty())
        {
            std::pmr::vector<iris::Type_reference> member_types;
            std::pmr::vector<std::pmr::string> member_names;

            for (std::pmr::string const& captured_name : captured_names)
            {
                Variable const* const variable = find_variable_from_scope(scope, captured_name);
                if (variable == nullptr)
                    return std::nullopt;

                member_types.push_back(variable->type);
                member_names.push_back(captured_name);
            }

            std::pmr::string const environment_name = get_lambda_environment_struct_name(generated_function_name);

            std::pmr::vector<std::optional<std::uint32_t>> member_bit_fields;
            member_bit_fields.resize(member_names.size(), std::nullopt);

            std::pmr::vector<iris::Statement> member_default_values;
            member_default_values.resize(member_names.size());

            environment_struct_declaration = iris::Struct_declaration
            {
                .name = environment_name,
                .unique_name = std::nullopt,
                .member_types = std::move(member_types),
                .member_names = std::move(member_names),
                .member_bit_fields = std::move(member_bit_fields),
                .member_default_values = std::move(member_default_values),
                .is_packed = false,
                .is_literal = false,
            };

            user_data_type = create_pointer_type_type_reference(
                { create_custom_type_reference(module_name, environment_name) },
                true
            );
        }

        std::pmr::vector<iris::Type_reference> input_parameter_types = signature->input_parameter_types;
        input_parameter_types.push_back(user_data_type);

        std::pmr::vector<std::pmr::string> input_parameter_names = lambda_expression.parameter_names;
        input_parameter_names.push_back(std::pmr::string{ "user_data" });

        std::pmr::vector<std::pmr::string> output_parameter_names;
        output_parameter_names.reserve(signature->output_parameter_types.size());
        for (std::size_t index = 0; index < signature->output_parameter_types.size(); ++index)
            output_parameter_names.push_back(std::pmr::string{ std::format("result_{}", index) });

        iris::Function_declaration function_declaration
        {
            .name = generated_function_name,
            .unique_name = std::nullopt,
            .type = iris::Function_type
            {
                .input_parameter_types = std::move(input_parameter_types),
                .output_parameter_types = signature->output_parameter_types,
                .is_variadic = false,
            },
            .input_parameter_names = std::move(input_parameter_names),
            .output_parameter_names = std::move(output_parameter_names),
            .linkage = iris::Linkage::Private,
            .is_test = false,
        };

        std::pmr::vector<iris::Statement> statements;
        for (std::pmr::string const& captured_name : captured_names)
            statements.push_back(create_capture_load_statement(captured_name));

        std::pmr::vector<iris::Statement> body_statements = create_lambda_body_statements(
            lambda_expression.body,
            !signature->output_parameter_types.empty()
        );
        statements.insert(statements.end(), std::make_move_iterator(body_statements.begin()), std::make_move_iterator(body_statements.end()));

        iris::Function_definition function_definition
        {
            .name = generated_function_name,
            .statements = std::move(statements),
        };

        return Lambda_lowering_result
        {
            .function_declaration = std::move(function_declaration),
            .function_definition = std::move(function_definition),
            .environment_struct_declaration = std::move(environment_struct_declaration),
            .lambda_key = create_lambda_key(module_name, parent_function_name, lambda_expression),
            .generated_function_name = generated_function_name,
        };
    }

    void run_lambda_pass_on_function(
        iris::Module& core_module,
        iris::Declaration_database& declaration_database,
        Lambda_database& lambda_database,
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

        std::pmr::vector<Lambda_lowering_result> results;
        std::size_t lambda_index = 0;

        auto const callback = [&](iris::Statement const& statement, Scope const& statement_scope) -> void
        {
            for (std::size_t expression_index = 0; expression_index < statement.expressions.size(); ++expression_index)
            {
                iris::Expression const& expression = statement.expressions[expression_index];
                if (!std::holds_alternative<iris::Lambda_expression>(expression.data))
                    continue;

                iris::Lambda_expression& lambda_expression = std::get<iris::Lambda_expression>(const_cast<iris::Expression&>(expression).data);

                // Already lowered by an earlier run of the pass over this module.
                std::pmr::string const lambda_key = create_lambda_key(core_module.name, function_declaration.name, lambda_expression);
                if (find_generated_lambda_function_name(lambda_database, lambda_key).has_value())
                    continue;

                std::optional<iris::Type_reference> const expected_type = get_expected_expression_type(
                    core_module.name,
                    &function_declaration,
                    statement_scope,
                    declaration_database,
                    statement,
                    std::nullopt,
                    expression_index
                );

                std::optional<Lambda_lowering_result> result = lower_lambda_expression(
                    core_module.name,
                    &function_declaration,
                    statement_scope,
                    statement,
                    expression,
                    lambda_expression,
                    function_declaration.name,
                    lambda_index,
                    expected_type,
                    declaration_database
                );

                if (!result.has_value())
                    continue;

                lambda_index += 1;
                results.push_back(std::move(result.value()));
            }
        };

        visit_statements_using_scope(
            core_module.name,
            &function_declaration,
            scope,
            function_definition.statements,
            declaration_database,
            callback
        );

        for (Lambda_lowering_result& result : results)
        {
            if (result.environment_struct_declaration.has_value())
            {
                core_module.internal_declarations.struct_declarations.push_back(std::move(result.environment_struct_declaration.value()));
                add_struct_declaration(declaration_database, core_module.name, false, core_module.internal_declarations.struct_declarations.back());
            }

            core_module.internal_declarations.function_declarations.push_back(std::move(result.function_declaration));
            add_function_declaration(declaration_database, core_module.name, false, core_module.internal_declarations.function_declarations.back());

            core_module.definitions.function_definitions.push_back(std::move(result.function_definition));

            add_generated_lambda_function_name(lambda_database, std::move(result.lambda_key), std::move(result.generated_function_name));
        }
    }

    void run_lambda_pass_on_module(
        iris::Module& core_module,
        iris::Declaration_database& declaration_database,
        Lambda_database& lambda_database,
        bool const is_test_mode
    )
    {
        // Lowering a lambda appends a new function definition, whose body may itself contain
        // lambdas, so keep walking until the list stops growing.
        for (std::size_t index = 0; index < core_module.definitions.function_definitions.size(); ++index)
        {
            std::pmr::string const definition_name = core_module.definitions.function_definitions[index].name;

            std::optional<Function_declaration const*> const function_declaration = find_function_declaration(core_module, definition_name);
            if (!function_declaration.has_value())
                continue;

            if (function_declaration.value()->is_test && !is_test_mode)
                continue;

            // The pass appends to both vectors, which invalidates any reference held across the
            // call, so work on copies and put the definition back afterwards.
            iris::Function_declaration const declaration_copy = *function_declaration.value();
            iris::Function_definition definition_copy = std::move(core_module.definitions.function_definitions[index]);

            run_lambda_pass_on_function(
                core_module,
                declaration_database,
                lambda_database,
                declaration_copy,
                definition_copy
            );

            core_module.definitions.function_definitions[index] = std::move(definition_copy);
        }
    }
}
