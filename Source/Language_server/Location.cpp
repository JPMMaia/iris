module;

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <variant>

#include <lsp/types.h>

module iris.language_server.location;

import iris.compiler.analysis;
import iris.compiler.validation;
import iris.core;
import iris.core.declarations;
import iris.core.formatter;
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
        iris::compiler::visit_statements_using_scope_including_lambda_bodies(
            core_module.name,
            function_declaration,
            scope,
            statements,
            declaration_database,
            callback
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

    std::optional<iris::Source_range> create_name_source_range(
        std::optional<iris::Source_position> const& position,
        std::string_view const name
    )
    {
        if (!position.has_value())
            return std::nullopt;

        return iris::Source_range
        {
            .start = position.value(),
            .end = iris::Source_position{ .line = position->line, .column = position->column + static_cast<std::uint32_t>(name.size()) },
        };
    }

    std::string format_type_for_display(
        iris::Module const& core_module,
        iris::Type_reference const& type
    )
    {
        std::pmr::string const text = iris::format_type_reference(core_module.dependencies, type, {}, {});
        return std::string{text.begin(), text.end()};
    }

    iris::compiler::Scope_pipeline const* find_pipeline_reference(
        iris::compiler::Scope const& scope,
        iris::Expression const& expression
    )
    {
        if (!std::holds_alternative<iris::Variable_expression>(expression.data))
            return nullptr;

        std::string_view const name = std::get<iris::Variable_expression>(expression.data).name;
        if (iris::compiler::find_variable_from_scope(scope, name) != nullptr)
            return nullptr;

        return iris::compiler::find_scope_pipeline(scope, name);
    }

    Pipeline_signature create_pipeline_signature(
        iris::Module const& core_module,
        Declaration_database const& declaration_database,
        iris::Function_declaration const* const function_declaration,
        iris::compiler::Scope const& scope,
        iris::Pipeline_expression const& pipeline
    )
    {
        Pipeline_signature signature;
        signature.label = "pipeline ";
        signature.label += std::string_view{pipeline.name};
        signature.label += '(';

        for (std::size_t index = 0; index < pipeline.input_parameter_names.size() && index < pipeline.input_parameter_types.size(); ++index)
        {
            if (index > 0)
                signature.label += ", ";

            std::string parameter{std::string_view{pipeline.input_parameter_names[index]}};
            parameter += ": ";
            parameter += format_type_for_display(core_module, pipeline.input_parameter_types[index]);

            signature.label += parameter;
            signature.parameters.push_back(std::move(parameter));
        }

        signature.label += ')';

        std::optional<iris::Type_reference> const output_type = iris::compiler::get_pipeline_output_type(
            core_module.name,
            function_declaration,
            scope,
            pipeline,
            declaration_database
        );
        if (output_type.has_value())
        {
            signature.label += " => ";
            signature.label += format_type_for_display(core_module, output_type.value());
        }

        return signature;
    }

    std::optional<Pipeline_signature> create_pipeline_sink_signature(
        iris::Module const& core_module,
        Declaration_database const& declaration_database,
        iris::Function_declaration const* const function_declaration,
        iris::compiler::Scope const& scope,
        iris::Statement const& statement,
        iris::compiler::Pipeline_sink_call const& sink_call
    )
    {
        if (sink_call.pipeline == nullptr)
            return std::nullopt;

        std::string_view const sink_name = sink_call.sink_name;
        if (!iris::compiler::is_pipeline_sink_name(sink_name) && sink_name != "separate")
            return std::nullopt;

        iris::Pipeline_expression const& pipeline = *sink_call.pipeline;

        std::optional<iris::Type_reference> const output_type = iris::compiler::get_pipeline_output_type(
            core_module.name,
            function_declaration,
            scope,
            pipeline,
            declaration_database
        );

        std::optional<iris::Type_reference> const sink_type =
            sink_name != "separate" ?
            iris::compiler::get_pipeline_sink_type(core_module.name, function_declaration, scope, statement, sink_call, declaration_database) :
            std::nullopt;

        auto const format_mutable_slice = [&](std::optional<iris::Type_reference> const& element_type) -> std::string
        {
            if (!element_type.has_value())
                return "Array_slice::<mutable T>";

            return format_type_for_display(core_module, iris::create_array_slice_type_reference({ element_type.value() }, true));
        };

        std::vector<std::string> parameters;

        if (sink_name == "into")
        {
            parameters.push_back("out: " + format_mutable_slice(output_type));
        }
        else if (sink_name == "in_place")
        {
            for (std::size_t index = 0; index < pipeline.input_parameter_names.size() && index < pipeline.input_parameter_types.size(); ++index)
                parameters.push_back(std::string{std::string_view{pipeline.input_parameter_names[index]}} + ": " + format_mutable_slice(pipeline.input_parameter_types[index]));
        }
        else if (sink_name == "fold")
        {
            std::string const accumulator_type = sink_type.has_value() ? format_type_for_display(core_module, sink_type.value()) : std::string{"T"};
            std::string const element_type = output_type.has_value() ? format_type_for_display(core_module, output_type.value()) : std::string{"U"};

            parameters.push_back("init: " + accumulator_type);
            parameters.push_back("function: (" + accumulator_type + ", " + element_type + ") -> " + accumulator_type);
        }
        else if (sink_name == "separate")
        {
            parameters.push_back("scratch: Array_slice::<mutable T>");
        }

        Pipeline_signature signature;
        signature.label = std::string{sink_name};
        signature.label += '(';

        for (std::size_t index = 0; index < parameters.size(); ++index)
        {
            if (index > 0)
                signature.label += ", ";

            signature.label += parameters[index];
        }

        signature.label += ')';
        signature.parameters = std::move(parameters);

        if (sink_type.has_value())
        {
            signature.label += " -> ";
            signature.label += format_type_for_display(core_module, sink_type.value());
        }

        return signature;
    }
}
