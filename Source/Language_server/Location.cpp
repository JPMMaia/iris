module;

#include <functional>
#include <optional>
#include <string_view>
#include <variant>

#include <lsp/types.h>

module h.language_server.location;

import h.compiler.analysis;
import h.core;
import h.core.declarations;
import h.core.types;
import h.language_server.core;

namespace h::language_server
{
    std::optional<Declaration> find_declaration_that_contains_source_position(
        Declaration_database const& declaration_database,
        std::string_view const& module_name,
        h::Source_position const& source_position
    )
    {
        std::optional<Declaration> found_declaration = std::nullopt;

        auto const process_declaration = [&](Declaration const& declaration) -> bool
        {
            std::optional<h::Source_range_location> const declaration_source_location = get_declaration_source_location(
                declaration
            );

            if (declaration_source_location.has_value())
            {
                h::Source_range const& range = declaration_source_location->range;

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

    std::optional<h::Function> find_function_that_contains_source_position(
        h::Module const& core_module,
        h::Source_position const& source_position
    )
    {
        for (h::Function_definition const& definition : core_module.definitions.function_definitions)
        {
            if (definition.source_location.has_value())
            {
                if (range_contains_position(definition.source_location->range, source_position))
                {
                    std::optional<Function_declaration const*> const declaration = h::find_function_declaration(core_module, definition.name);
                    if (declaration.has_value())
                    {
                        return h::Function
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

    std::optional<h::Type_reference> find_type_that_contains_source_position(
        h::Type_reference const& type,
        h::Source_position const& source_position
    )
    {
        if (!type.source_range.has_value())
            return std::nullopt;

        if (!range_contains_position(type.source_range.value(), source_position))
            return std::nullopt;

        h::Type_reference const* best = &type;

        auto const process_type = [&](h::Type_reference const& current) -> bool
        {
            if (current.source_range.has_value())
            {
                if (range_contains_position(current.source_range.value(), source_position))
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
        h::Module const& core_module,
        h::Statement const& statement,
        h::Expression const& expression
    )
    {
        if (std::holds_alternative<h::Access_expression>(expression.data))
        {
            h::Access_expression const& access_expression = std::get<h::Access_expression>(expression.data);

            h::Expression const& expression_to_access = statement.expressions[access_expression.expression.expression_index];
            if (std::holds_alternative<h::Variable_expression>(expression_to_access.data))
            {
                h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(expression_to_access.data);
                
                std::optional<Declaration> const declaration = find_underlying_declaration_using_import_alias(declaration_database, core_module, variable_expression.name, access_expression.member_name);
                return declaration;
            }
        }
        else if (std::holds_alternative<h::Variable_expression>(expression.data))
        {
            h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(expression.data);

            std::optional<Declaration> const declaration = find_underlying_declaration(declaration_database, core_module.name, variable_expression.name);
            return declaration;
        }

        return std::nullopt;
    }

    h::Enum_declaration const* find_enum_declaration_using_expression(
        Declaration_database const& declaration_database,
        h::Module const& core_module,
        h::Statement const& statement,
        h::Expression const& expression
    )
    {
        std::optional<Declaration> const declaration = find_value_declaration_using_expression(
            declaration_database,
            core_module,
            statement,
            expression
        );
        if (declaration.has_value() && std::holds_alternative<h::Enum_declaration const*>(declaration->data))
            return std::get<h::Enum_declaration const*>(declaration->data);

        return nullptr;
    }

    void visit_expressions_that_contain_position(
        Declaration_database const& declaration_database,
        h::Module const& core_module,
        h::Source_position const& source_position,
        std::function<bool(h::Function_declaration const* function_declaration, h::compiler::Scope const& scope, h::Statement const& statement, h::Expression const& expression)> const& visitor
    )
    {
        std::optional<h::Function> const function = find_function_that_contains_source_position(
            core_module,
            source_position
        );

        if (function.has_value())
        {
            auto const process_statement = [&](h::Statement const& statement, h::compiler::Scope const& scope) -> bool
            {
                auto const process_expression = [&](h::Expression const& expression, h::Statement const& statement) -> bool
                {
                    if (!expression.source_range.has_value())
                        return false;

                    if (h::range_contains_position_inclusive(expression.source_range.value(), source_position))
                        return visitor(function->declaration, scope, statement, expression);

                    return false;
                };

                if (statement.expressions.empty())
                    return false;

                h::Expression const& first_expression = statement.expressions[0];
                if (!first_expression.source_range.has_value())
                    return false;

                if (!h::range_contains_position_inclusive(first_expression.source_range.value(), source_position))
                    return false;

                return visit_expressions(
                    statement,
                    process_expression
                );
            };

            h::compiler::Scope scope = {};

            h::compiler::add_parameters_to_scope(
                scope,
                function->declaration->input_parameter_names,
                function->declaration->type.input_parameter_types,
                function->declaration->input_parameter_source_positions
            );

            h::compiler::visit_statements_using_scope(
                core_module,
                function->declaration,
                scope,
                function->definition->statements,
                declaration_database,
                process_statement
            );
        }
        else
        {
            h::compiler::Scope const scope = {};

            auto const process_expression = [&](h::Expression const& expression, h::Statement const& statement) -> bool
            {
                if (!expression.source_range.has_value())
                    return false;

                if (h::range_contains_position_inclusive(expression.source_range.value(), source_position))
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
