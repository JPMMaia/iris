module;

#include <format>
#include <memory_resource>
#include <span>
#include <vector>

#include <lsp/types.h>

module iris.language_server.inlay_hints;

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
    // A lambda literal writes the types it wants to leave out: a parameter without an annotation and
    // an omitted return type both get a hint showing what they were resolved to.
    static void add_lambda_inlay_hints(
        std::pmr::vector<lsp::InlayHint>& output,
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        iris::Declaration_database const& declaration_database,
        iris::compiler::Scope const& scope,
        iris::Statement const& statement,
        std::size_t const expression_index,
        iris::Lambda_expression const& lambda_expression,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::optional<iris::Type_reference> const expected_type = iris::compiler::get_expected_expression_type(
            core_module.name,
            &function_declaration,
            scope,
            declaration_database,
            statement,
            std::nullopt,
            expression_index
        );

        std::optional<iris::compiler::Type_info> const type_info = iris::compiler::get_expression_type_info(
            core_module.name,
            &function_declaration,
            scope,
            statement,
            statement.expressions[expression_index],
            expected_type,
            declaration_database
        );
        if (!type_info.has_value())
            return;

        std::optional<iris::Lambda_type> const lambda_type = iris::compiler::resolve_lambda_type(declaration_database, type_info->type);
        if (!lambda_type.has_value())
            return;

        if (lambda_type->input_parameter_types.size() != lambda_expression.parameter_names.size())
            return;

        if (lambda_expression.parameter_source_positions.size() == lambda_expression.parameter_names.size())
        {
            for (std::size_t index = 0; index < lambda_expression.parameter_names.size(); ++index)
            {
                if (index < lambda_expression.parameter_types.size() && lambda_expression.parameter_types[index].has_value())
                    continue;

                iris::Source_position const& parameter_position = lambda_expression.parameter_source_positions[index];

                lsp::Position const position
                {
                    .line = parameter_position.line - 1,
                    .character = parameter_position.column - 1 + static_cast<std::uint32_t>(lambda_expression.parameter_names[index].size()),
                };

                std::vector<lsp::InlayHintLabelPart> label = create_inlay_hint_variable_type_label(
                    core_module,
                    declaration_database,
                    lambda_type->input_parameter_types[index],
                    temporaries_allocator
                );

                output.push_back(
                    lsp::InlayHint
                    {
                        .position = position,
                        .label = std::move(label),
                        .kind = lsp::InlayHintKind::Type,
                        .textEdits = std::nullopt,
                        .tooltip = std::nullopt,
                    }
                );
            }
        }

        if (lambda_expression.return_type.has_value() ||
            !lambda_expression.input_parameters_source_range.has_value() ||
            lambda_type->output_parameter_types.size() != 1)
        {
            return;
        }

        iris::Source_position const& parameters_end = lambda_expression.input_parameters_source_range->end;

        lsp::Position const position
        {
            .line = parameters_end.line - 1,
            .character = parameters_end.column - 1,
        };

        std::vector<lsp::InlayHintLabelPart> label;
        label.push_back(lsp::InlayHintLabelPart{ .value = "-> " });

        create_inlay_hint_variable_type_label_aux(
            label,
            core_module,
            declaration_database,
            lambda_type->output_parameter_types[0],
            temporaries_allocator
        );

        output.push_back(
            lsp::InlayHint
            {
                .position = position,
                .label = std::move(label),
                .kind = lsp::InlayHintKind::Type,
                .textEdits = std::nullopt,
                .tooltip = std::nullopt,
            }
        );
    }

    std::pmr::vector<lsp::InlayHint> create_function_inlay_hints(
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition const& function_definition,
        iris::Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<lsp::InlayHint> output{temporaries_allocator};

        iris::compiler::Scope scope
        {
            .variables{temporaries_allocator}
        };

        add_parameters_to_scope(
            scope,
            function_declaration.input_parameter_names,
            function_declaration.type.input_parameter_types,
            function_declaration.input_parameter_source_positions
        );

        auto const process_statement = [&](iris::Statement const& statement, iris::compiler::Scope const& scope) -> void
        {
            if (statement.expressions.empty())
                return;

            for (std::size_t index = 0; index < statement.expressions.size(); ++index)
            {
                if (!std::holds_alternative<iris::Lambda_expression>(statement.expressions[index].data))
                    continue;

                add_lambda_inlay_hints(
                    output,
                    core_module,
                    function_declaration,
                    declaration_database,
                    scope,
                    statement,
                    index,
                    std::get<iris::Lambda_expression>(statement.expressions[index].data),
                    temporaries_allocator
                );
            }

            iris::Expression const& expression = statement.expressions[0];

            if (!expression.source_range.has_value())
                return;

            if (std::holds_alternative<iris::Variable_declaration_expression>(expression.data))
            {
                iris::Variable_declaration_expression const& variable_declaration = std::get<iris::Variable_declaration_expression>(expression.data);

                std::optional<iris::Type_reference> const variable_type = get_expression_type(
                    core_module.name,
                    &function_declaration,
                    scope,
                    statement,
                    statement.expressions[variable_declaration.right_hand_side.expression_index],
                    std::nullopt,
                    declaration_database
                );
                if (!variable_type.has_value())
                    return;

                std::uint32_t const offset =
                    variable_declaration.is_mutable ?
                    8 + variable_declaration.name.size() :
                    4 + variable_declaration.name.size();

                lsp::Position const position
                {
                    .line = expression.source_range->start.line - 1,
                    .character = expression.source_range->start.column + offset - 1,
                };

                std::vector<lsp::InlayHintLabelPart> label = create_inlay_hint_variable_type_label(
                    core_module,
                    declaration_database,
                    variable_type.value(),
                    temporaries_allocator
                );

                lsp::InlayHint inlay_hint
                {
                    .position = position,
                    .label = std::move(label),
                    .kind = lsp::InlayHintKind::Type,
                    .textEdits = std::nullopt,
                    .tooltip = std::nullopt,
                };

                output.push_back(std::move(inlay_hint));
            }
        };

        visit_statements_using_scope_including_lambda_bodies(
            declaration_database,
            core_module,
            &function_declaration,
            scope,
            function_definition.statements,
            process_statement
        );

        return output;
    }

    void create_inlay_hint_variable_type_label_for_function_parameters(
        std::vector<lsp::InlayHintLabelPart>& parts,
        iris::Module const& core_module,
        iris::Declaration_database const& declaration_database,
        std::span<std::pmr::string const> const parameter_names,
        std::span<iris::Type_reference const> const parameter_types,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        if (parameter_names.size() != parameter_types.size())
            return;

        for (std::size_t index = 0; index < parameter_names.size(); ++index)
        {
            parts.push_back(
                lsp::InlayHintLabelPart
                {
                    .value = std::format("{}: ", parameter_names[index]),
                }
            );

            create_inlay_hint_variable_type_label_aux(
                parts,
                core_module,
                declaration_database,
                parameter_types[index],
                temporaries_allocator
            );

            if (index + 1 < parameter_names.size())
            {
                parts.push_back(
                    lsp::InlayHintLabelPart
                    {
                        .value = ", ",
                    }
                );
            }
        }
    }

    void create_inlay_hint_variable_type_label_aux(
        std::vector<lsp::InlayHintLabelPart>& parts,
        iris::Module const& core_module,
        iris::Declaration_database const& declaration_database,
        std::optional<iris::Type_reference> const& type_optional,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        if (!type_optional.has_value())
        {
            return;
        }

        iris::Type_reference const& type = type_optional.value();

        if (iris::is_custom_type_reference(type))
        {
            std::pmr::string const type_name = iris::format_type_reference(
                core_module.dependencies,
                type,
                temporaries_allocator,
                temporaries_allocator
            );

            lsp::InlayHintLabelPart part
            {
                .value = type_name.data(),
            };

            std::optional<Declaration> const declaration = find_declaration(declaration_database, type);
            if (declaration.has_value())
            {
                std::optional<iris::Source_range_location> const declaration_source_location = get_declaration_source_location(
                    declaration.value()
                );
                if (declaration_source_location.has_value() && declaration_source_location->file_path.has_value())
                {
                    part.location =
                    {
                        .uri = lsp::DocumentUri::fromPath(declaration_source_location->file_path->generic_string()),
                        .range = {
                            .start = {
                                .line = declaration_source_location->range.start.line - 1,
                                .character = declaration_source_location->range.start.column - 1,
                            },
                            .end = {
                                .line = declaration_source_location->range.start.line -1,
                                .character = declaration_source_location->range.start.column -1,
                            }
                        },
                    };
                }
            }

            parts.push_back(std::move(part));
        }
        else if (std::holds_alternative<iris::Constant_array_type>(type.data))
        {
            iris::Constant_array_type const& constant_array_type = std::get<iris::Constant_array_type>(type.data);
            if (constant_array_type.value_type.empty())
                return;

            parts.push_back(
                lsp::InlayHintLabelPart
                {
                    .value = "Constant_array::<",
                }
            );

            create_inlay_hint_variable_type_label_aux(
                parts,
                core_module,
                declaration_database,
                constant_array_type.value_type[0],
                temporaries_allocator
            );

            parts.push_back(
                lsp::InlayHintLabelPart
                {
                    .value = std::format(", {}>", constant_array_type.size),
                }
            );
        }
        else if (std::holds_alternative<iris::Function_pointer_type>(type.data))
        {
            iris::Function_pointer_type const& function_pointer_type = std::get<iris::Function_pointer_type>(type.data);

            parts.push_back(
                lsp::InlayHintLabelPart
                {
                    .value = "function<(",
                }
            );

            create_inlay_hint_variable_type_label_for_function_parameters(
                parts,
                core_module,
                declaration_database,
                function_pointer_type.input_parameter_names,
                function_pointer_type.type.input_parameter_types,
                temporaries_allocator
            );

            parts.push_back(
                lsp::InlayHintLabelPart
                {
                    .value = ") -> (",
                }
            );

            create_inlay_hint_variable_type_label_for_function_parameters(
                parts,
                core_module,
                declaration_database,
                function_pointer_type.output_parameter_names,
                function_pointer_type.type.output_parameter_types,
                temporaries_allocator
            );

            parts.push_back(
                lsp::InlayHintLabelPart
                {
                    .value = ")>",
                }
            );
        }
        else if (std::holds_alternative<iris::Pointer_type>(type.data))
        {
            iris::Pointer_type const& pointer_type = std::get<iris::Pointer_type>(type.data);

            parts.push_back(
                lsp::InlayHintLabelPart
                {
                    .value = "*",
                }
            );

            if (pointer_type.is_mutable)
            {
                parts.push_back(
                    lsp::InlayHintLabelPart
                    {
                        .value = "mutable ",
                    }
                );
            }

            create_inlay_hint_variable_type_label_aux(
                parts,
                core_module,
                declaration_database,
                !pointer_type.element_type.empty() ? std::optional<iris::Type_reference>(pointer_type.element_type[0]) : std::optional<iris::Type_reference>{std::nullopt},
                temporaries_allocator
            );
        }
        else if (std::holds_alternative<iris::Type_instance>(type.data))
        {
            iris::Type_instance const& type_instance = std::get<iris::Type_instance>(type.data);

            create_inlay_hint_variable_type_label_aux(
                parts,
                core_module,
                declaration_database,
                iris::Type_reference{.data = type_instance.type_constructor},
                temporaries_allocator
            );

            parts.push_back(
                lsp::InlayHintLabelPart
                {
                    .value = "<",
                }
            );

            for (std::size_t index = 0; index < type_instance.arguments.size(); ++index)
            {
                iris::Statement const& statement = type_instance.arguments[index];
                if (statement.expressions.empty())
                    continue;

                iris::Expression const& first_expression = statement.expressions[0];

                if (std::holds_alternative<iris::Type_expression>(first_expression.data))
                {
                    iris::Type_expression const& argument = std::get<iris::Type_expression>(first_expression.data);
                    
                    create_inlay_hint_variable_type_label_aux(
                        parts,
                        core_module,
                        declaration_database,
                        argument.type,
                        temporaries_allocator
                    );
                }
                else
                {
                    std::pmr::string const type_name = iris::format_type_reference(
                        core_module.dependencies,
                        type,
                        temporaries_allocator,
                        temporaries_allocator
                    );

                    parts.push_back(
                        lsp::InlayHintLabelPart
                        {
                            .value = type_name.data(),
                        }
                    );
                }

                if (index + 1 < type_instance.arguments.size())
                {
                    parts.push_back(
                        lsp::InlayHintLabelPart
                        {
                            .value = ", ",
                        }
                    );
                }
            }

            parts.push_back(
                lsp::InlayHintLabelPart
                {
                    .value = ">",
                }
            );
        }
        else
        {
            std::pmr::string const type_name = iris::format_type_reference(
                core_module.dependencies,
                type,
                temporaries_allocator,
                temporaries_allocator
            );

            lsp::InlayHintLabelPart part
            {
                .value = type_name.data(),
            };

            parts.push_back(std::move(part));
        }
    }

    std::vector<lsp::InlayHintLabelPart> create_inlay_hint_variable_type_label(
        iris::Module const& core_module,
        iris::Declaration_database const& declaration_database,
        iris::Type_reference const& type,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::vector<lsp::InlayHintLabelPart> parts;

        lsp::InlayHintLabelPart first_part
        {
            .value = ": ",
        };

        parts.push_back(first_part);

        create_inlay_hint_variable_type_label_aux(
            parts,
            core_module,
            declaration_database,
            type,
            temporaries_allocator
        );

        return parts;
    }
}
