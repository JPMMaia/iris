module;

#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

#include <lsp/types.h>

module iris.language_server.location;

import iris.compiler.analysis;
import iris.compiler.validation;
import iris.core;
import iris.core.declarations;
import iris.core.types;
import iris.language_server.core;

namespace iris::language_server
{
    std::optional<Declaration> find_declaration_that_contains_source_position(
        Declaration_database const& declaration_database,
        std::string_view const& module_name,
        iris::Source_position const& source_position
    )
    {
        std::optional<Declaration> found_declaration = std::nullopt;

        auto const process_declaration = [&](Declaration const& declaration) -> bool
        {
            std::optional<iris::Source_range_location> const declaration_source_location = get_declaration_source_location(
                declaration
            );

            if (declaration_source_location.has_value())
            {
                iris::Source_range const& range = declaration_source_location->range;

                if (range_contains_position(range, source_position))
                {
                    found_declaration = declaration;
                    return true;
                }
            }

            return false;
        };

        visit_declarations(
            declaration_database,
            module_name,
            process_declaration
        );

        return found_declaration;
    }

    std::optional<iris::Function> find_function_that_contains_source_position(
        iris::Module const& core_module,
        iris::Source_position const& source_position
    )
    {
        for (iris::Function_definition const& definition : core_module.definitions.function_definitions)
        {
            if (definition.source_location.has_value())
            {
                if (range_contains_position(definition.source_location->range, source_position))
                {
                    std::optional<Function_declaration const*> const declaration = iris::find_function_declaration(core_module, definition.name);
                    if (declaration.has_value())
                    {
                        return iris::Function
                        {
                            .declaration = declaration.value(),
                            .definition = &definition
                        };                     
                    }
                    
                    return std::nullopt;
                }
            }
        }

        return std::nullopt;
    }

    std::optional<iris::Type_reference> find_type_that_contains_source_position(
        iris::Type_reference const& type,
        iris::Source_position const& source_position
    )
    {
        if (!type.source_range.has_value())
            return std::nullopt;

        // Inclusive: a cursor sitting immediately after an identifier is still on that identifier,
        // which is how an editor reports the caret at the end of a word.
        if (!range_contains_position_inclusive(type.source_range.value(), source_position))
            return std::nullopt;

        iris::Type_reference const* best = &type;

        auto const process_type = [&](iris::Type_reference const& current) -> bool
        {
            if (current.source_range.has_value())
            {
                if (range_contains_position_inclusive(current.source_range.value(), source_position))
                {
                    best = &current;
                }
            }

            return false;
        };

        visit_type_references_recursively(
            type,
            process_type
        );

        return *best;
    }

    std::optional<Declaration> find_value_declaration_using_expression(
        Declaration_database const& declaration_database,
        iris::Module const& core_module,
        iris::Statement const& statement,
        iris::Expression const& expression
    )
    {
        if (std::holds_alternative<iris::Access_expression>(expression.data))
        {
            iris::Access_expression const& access_expression = std::get<iris::Access_expression>(expression.data);

            iris::Expression const& expression_to_access = statement.expressions[access_expression.expression.expression_index];
            if (std::holds_alternative<iris::Variable_expression>(expression_to_access.data))
            {
                iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(expression_to_access.data);
                
                std::optional<Declaration> const declaration = find_underlying_declaration_using_import_alias(declaration_database, core_module.name, variable_expression.name, access_expression.member_name);
                return declaration;
            }
        }
        else if (std::holds_alternative<iris::Variable_expression>(expression.data))
        {
            iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(expression.data);

            std::optional<Declaration> const declaration = find_underlying_declaration(declaration_database, core_module.name, variable_expression.name);
            return declaration;
        }

        return std::nullopt;
    }

    iris::Enum_declaration const* find_enum_declaration_using_expression(
        Declaration_database const& declaration_database,
        iris::Module const& core_module,
        iris::Statement const& statement,
        iris::Expression const& expression
    )
    {
        std::optional<Declaration> const declaration = find_value_declaration_using_expression(
            declaration_database,
            core_module,
            statement,
            expression
        );
        if (declaration.has_value() && std::holds_alternative<iris::Enum_declaration const*>(declaration->data))
            return std::get<iris::Enum_declaration const*>(declaration->data);

        return nullptr;
    }

    void visit_statements_using_scope_including_lambda_bodies(
        Declaration_database const& declaration_database,
        iris::Module const& core_module,
        iris::Function_declaration const* const function_declaration,
        iris::compiler::Scope& scope,
        std::span<iris::Statement const> const statements,
        std::function<void(iris::Statement const&, iris::compiler::Scope const&)> const& callback
    )
    {
        auto const process_statement = [&](iris::Statement const& statement, iris::compiler::Scope const& statement_scope) -> void
        {
            callback(statement, statement_scope);

            for (std::size_t index = 0; index < statement.expressions.size(); ++index)
            {
                iris::Expression const& expression = statement.expressions[index];
                if (!std::holds_alternative<iris::Lambda_expression>(expression.data))
                    continue;

                iris::Lambda_expression const& lambda_expression = std::get<iris::Lambda_expression>(expression.data);

                std::optional<iris::Type_reference> const expected_type = iris::compiler::get_expected_expression_type(
                    core_module.name,
                    function_declaration,
                    statement_scope,
                    declaration_database,
                    statement,
                    std::nullopt,
                    index
                );

                std::optional<iris::compiler::Type_info> const type_info = iris::compiler::get_expression_type_info(
                    core_module.name,
                    function_declaration,
                    statement_scope,
                    statement,
                    expression,
                    expected_type,
                    declaration_database
                );
                if (!type_info.has_value())
                    continue;

                std::optional<iris::Lambda_type> const lambda_type = iris::compiler::resolve_lambda_type(declaration_database, type_info->type);
                if (!lambda_type.has_value() || lambda_type->input_parameter_types.size() != lambda_expression.parameter_names.size())
                    continue;

                iris::compiler::Scope lambda_scope = statement_scope;

                iris::compiler::add_parameters_to_scope(
                    lambda_scope,
                    lambda_expression.parameter_names,
                    lambda_type->input_parameter_types,
                    std::nullopt
                );

                iris::Statement const& body = lambda_expression.body;

                visit_statements_using_scope_including_lambda_bodies(
                    declaration_database,
                    core_module,
                    function_declaration,
                    lambda_scope,
                    {&body, 1},
                    callback
                );
            }
        };

        iris::compiler::visit_statements_using_scope(
            core_module.name,
            function_declaration,
            scope,
            statements,
            declaration_database,
            process_statement
        );
    }

    void visit_expressions_that_contain_position(
        Declaration_database const& declaration_database,
        iris::Module const& core_module,
        iris::Source_position const& source_position,
        std::function<bool(iris::Function_declaration const* function_declaration, iris::compiler::Scope const& scope, iris::Statement const& statement, iris::Expression const& expression)> const& visitor
    )
    {
        std::optional<iris::Function> const function = find_function_that_contains_source_position(
            core_module,
            source_position
        );

        if (function.has_value())
        {
            auto const process_statement = [&](iris::Statement const& statement, iris::compiler::Scope const& scope) -> bool
            {
                auto const process_expression = [&](iris::Expression const& expression, iris::Statement const& statement) -> bool
                {
                    if (!expression.source_range.has_value())
                        return false;

                    if (iris::range_contains_position_inclusive(expression.source_range.value(), source_position))
                        return visitor(function->declaration, scope, statement, expression);

                    return false;
                };

                if (statement.expressions.empty())
                    return false;

                iris::Expression const& first_expression = statement.expressions[0];
                if (!first_expression.source_range.has_value())
                    return false;

                if (!iris::range_contains_position_inclusive(first_expression.source_range.value(), source_position))
                    return false;

                return visit_expressions(
                    statement,
                    process_expression
                );
            };

            iris::compiler::Scope scope = {};

            iris::compiler::add_parameters_to_scope(
                scope,
                function->declaration->input_parameter_names,
                function->declaration->type.input_parameter_types,
                function->declaration->input_parameter_source_positions
            );

            visit_statements_using_scope_including_lambda_bodies(
                declaration_database,
                core_module,
                function->declaration,
                scope,
                function->definition->statements,
                process_statement
            );
        }
        else
        {
            iris::compiler::Scope const scope = {};

            auto const process_expression = [&](iris::Expression const& expression, iris::Statement const& statement) -> bool
            {
                if (!expression.source_range.has_value())
                    return false;

                if (iris::range_contains_position_inclusive(expression.source_range.value(), source_position))
                    return visitor(nullptr, scope, statement, expression);

                return false;
            };

            visit_expressions(
                core_module,
                process_expression
            );
        }
    }
}
