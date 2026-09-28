module;

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <lsp/types.h>

module iris.language_server.hover;

import iris.compiler.analysis;
import iris.compiler.validation;
import iris.core;
import iris.core.declarations;
import iris.core.formatter;
import iris.core.types;
import iris.language_server.core;
import iris.language_server.location;

namespace iris::language_server
{
    static lsp::TextDocument_HoverResult create_hover(
        std::vector<std::string> contents
    )
    {
        if (contents.empty())
            return nullptr;

        std::vector<lsp::MarkedString> marked_strings;
        marked_strings.reserve(contents.size());

        for (std::string& content : contents)
            marked_strings.push_back(lsp::MarkedString{std::move(content)});

        lsp::Hover hover;
        hover.contents = std::move(marked_strings);
        return hover;
    }

    static std::string format_type(
        iris::Module const& core_module,
        iris::Type_reference const& type
    )
    {
        std::pmr::string const text = iris::format_type_reference(
            core_module.dependencies,
            type,
            {},
            {}
        );

        return std::string{text.begin(), text.end()};
    }

    static void add_parameter_list(
        std::string& text,
        iris::Module const& core_module,
        std::span<std::pmr::string const> const parameter_names,
        std::span<iris::Type_reference const> const parameter_types
    )
    {
        text += '(';

        for (std::size_t index = 0; index < parameter_types.size(); ++index)
        {
            if (index > 0)
                text += ", ";

            if (index < parameter_names.size())
            {
                text += std::string_view{parameter_names[index]};
                text += ": ";
            }

            text += format_type(core_module, parameter_types[index]);
        }

        text += ')';
    }

    static std::string format_lambda_declaration(
        iris::Module const& core_module,
        iris::Lambda_declaration const& declaration
    )
    {
        std::string text = "lambda ";
        text += std::string_view{declaration.name};

        add_parameter_list(text, core_module, declaration.input_parameter_names, declaration.input_parameter_types);
        text += " -> ";
        add_parameter_list(text, core_module, declaration.output_parameter_names, declaration.output_parameter_types);

        return text;
    }

    // A lambda literal has no name of its own, so it is shown as the signature it resolved to, with
    // the parameter names as they were written.
    static std::string format_lambda_literal(
        iris::Module const& core_module,
        iris::Lambda_expression const& lambda_expression,
        iris::Lambda_type const& lambda_type
    )
    {
        std::string text = "lambda";

        add_parameter_list(text, core_module, lambda_expression.parameter_names, lambda_type.input_parameter_types);
        text += " -> ";
        add_parameter_list(text, core_module, {}, lambda_type.output_parameter_types);

        return text;
    }

    static std::optional<std::string> format_declaration(
        iris::Module const& core_module,
        Declaration const& declaration
    )
    {
        if (std::holds_alternative<iris::Lambda_declaration const*>(declaration.data))
            return format_lambda_declaration(core_module, *std::get<iris::Lambda_declaration const*>(declaration.data));

        if (std::holds_alternative<iris::Struct_declaration const*>(declaration.data))
            return "struct " + std::string{std::string_view{std::get<iris::Struct_declaration const*>(declaration.data)->name}};

        if (std::holds_alternative<iris::Union_declaration const*>(declaration.data))
            return "union " + std::string{std::string_view{std::get<iris::Union_declaration const*>(declaration.data)->name}};

        if (std::holds_alternative<iris::Enum_declaration const*>(declaration.data))
            return "enum " + std::string{std::string_view{std::get<iris::Enum_declaration const*>(declaration.data)->name}};

        if (std::holds_alternative<iris::Alias_type_declaration const*>(declaration.data))
            return "using " + std::string{std::string_view{std::get<iris::Alias_type_declaration const*>(declaration.data)->name}};

        return std::nullopt;
    }

    static lsp::TextDocument_HoverResult create_hover_from_type(
        Declaration_database const& declaration_database,
        iris::Module const& core_module,
        iris::Type_reference const& type
    )
    {
        std::optional<Declaration> const declaration = find_declaration(declaration_database, type);
        if (declaration.has_value())
        {
            std::optional<std::string> text = format_declaration(core_module, declaration.value());
            if (text.has_value())
                return create_hover({std::move(text.value())});
        }

        return create_hover({format_type(core_module, type)});
    }

    lsp::TextDocument_HoverResult compute_hover(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        lsp::Position const position
    )
    {
        (void)parse_tree;

        iris::Source_position const source_position = to_source_position(position);

        std::optional<Declaration> const declaration_optional = find_declaration_that_contains_source_position(
            declaration_database,
            core_module.name,
            source_position
        );
        if (declaration_optional.has_value())
        {
            std::optional<iris::Type_reference> found_type = std::nullopt;

            auto const process_type = [&](iris::Type_reference const& type) -> bool
            {
                found_type = find_type_that_contains_source_position(type, source_position);
                return found_type.has_value();
            };

            auto const process_declaration = [&](auto const* const declaration) -> bool
            {
                return visit_type_references(*declaration, process_type);
            };

            std::visit(process_declaration, declaration_optional->data);

            if (found_type.has_value())
                return create_hover_from_type(declaration_database, core_module, found_type.value());

            // The cursor is on the declaration's own name.
            std::string_view const declaration_name = get_declaration_name(declaration_optional.value());
            std::optional<iris::Source_range_location> const declaration_location = get_declaration_source_location(declaration_optional.value());
            if (declaration_location.has_value())
            {
                std::optional<iris::Source_range> const name_range = create_sub_source_range(declaration_location->range, 0, declaration_name.size());
                if (name_range.has_value() && range_contains_position_inclusive(name_range.value(), source_position))
                {
                    std::optional<std::string> text = format_declaration(core_module, declaration_optional.value());
                    if (text.has_value())
                        return create_hover({std::move(text.value())});
                }
            }
        }

        std::optional<iris::Function> const function = find_function_that_contains_source_position(
            core_module,
            source_position
        );
        if (!function.has_value())
            return nullptr;

        std::optional<iris::Type_reference> found_type = std::nullopt;

        visit_type_references(
            function->definition->statements,
            [&](iris::Type_reference const& type) -> bool
            {
                found_type = find_type_that_contains_source_position(type, source_position);
                return found_type.has_value();
            }
        );

        if (found_type.has_value())
            return create_hover_from_type(declaration_database, core_module, found_type.value());

        std::vector<std::string> contents;

        auto const process_statement = [&](iris::Statement const& statement, iris::compiler::Scope const& scope) -> void
        {
            if (!contents.empty())
                return;

            auto const process_expression = [&](iris::Expression const& expression, iris::Statement const& statement) -> bool
            {
                if (!expression.source_range.has_value())
                    return false;

                if (!range_contains_position_inclusive(expression.source_range.value(), source_position))
                    return false;

                if (std::holds_alternative<iris::Lambda_expression>(expression.data))
                {
                    iris::Lambda_expression const& lambda_expression = std::get<iris::Lambda_expression>(expression.data);

                    // Inside the body the cursor is on whatever the body itself mentions; only the
                    // head of the literal stands for the lambda.
                    bool const is_in_body =
                        !lambda_expression.body.expressions.empty() &&
                        lambda_expression.body.expressions[0].source_range.has_value() &&
                        range_contains_position_inclusive(lambda_expression.body.expressions[0].source_range.value(), source_position);

                    if (is_in_body)
                        return false;

                    std::size_t const expression_index = static_cast<std::size_t>(&expression - statement.expressions.data());

                    std::optional<iris::Type_reference> const expected_type = iris::compiler::get_expected_expression_type(
                        core_module.name,
                        function->declaration,
                        scope,
                        declaration_database,
                        statement,
                        std::nullopt,
                        expression_index
                    );

                    std::optional<iris::compiler::Type_info> const type_info = iris::compiler::get_expression_type_info(
                        core_module.name,
                        function->declaration,
                        scope,
                        statement,
                        expression,
                        expected_type,
                        declaration_database
                    );
                    if (!type_info.has_value())
                        return false;

                    std::optional<iris::Lambda_type> const lambda_type = iris::compiler::resolve_lambda_type(declaration_database, type_info->type);
                    if (!lambda_type.has_value())
                        return false;

                    contents.push_back(format_lambda_literal(core_module, lambda_expression, lambda_type.value()));

                    // Capture analysis is part of the compiler passes, which have not run on a file
                    // that is still being edited, so the capture set is computed here.
                    iris::Lambda_expression lambda_expression_copy = lambda_expression;
                    if (!lambda_expression_copy.captured_variables.has_value())
                    {
                        std::pmr::vector<std::pmr::string> enclosing_names;
                        enclosing_names.reserve(scope.variables.size());
                        for (iris::compiler::Variable const& variable : scope.variables)
                            enclosing_names.push_back(variable.name);

                        iris::compiler::compute_lambda_captures(lambda_expression_copy, enclosing_names);
                    }

                    if (lambda_expression_copy.captured_variables.has_value() && !lambda_expression_copy.captured_variables->empty())
                    {
                        std::string captures = "captures: ";

                        for (std::size_t index = 0; index < lambda_expression_copy.captured_variables->size(); ++index)
                        {
                            if (index > 0)
                                captures += ", ";

                            captures += std::string_view{lambda_expression_copy.captured_variables->at(index)};
                        }

                        contents.push_back(std::move(captures));
                    }

                    return true;
                }
                else if (std::holds_alternative<iris::Variable_expression>(expression.data))
                {
                    iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(expression.data);

                    iris::compiler::Variable const* const variable = iris::compiler::find_variable_from_scope(scope, variable_expression.name);
                    if (variable == nullptr)
                        return false;

                    contents.push_back(std::string{std::string_view{variable->name}} + ": " + format_type(core_module, variable->type));

                    // A lambda-typed value is shown with the signature it can be called with.
                    std::optional<Declaration> const declaration = find_declaration(declaration_database, variable->type);
                    if (declaration.has_value() && std::holds_alternative<iris::Lambda_declaration const*>(declaration->data))
                        contents.push_back(format_lambda_declaration(core_module, *std::get<iris::Lambda_declaration const*>(declaration->data)));

                    return true;
                }

                return false;
            };

            visit_expressions(
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

        return create_hover(std::move(contents));
    }
}
