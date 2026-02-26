module;

#include <format>
#include <memory_resource>
#include <span>
#include <vector>

#include <lsp/types.h>

module h.language_server.inlay_hints;

import h.compiler.analysis;
import h.core;
import h.core.declarations;
import h.core.formatter;
import h.core.types;
import h.language_server.core;

namespace h::language_server
{
    std::pmr::vector<lsp::InlayHint> create_function_inlay_hints(
        h::Module const& core_module,
        h::Function_declaration const& function_declaration,
        h::Function_definition const& function_definition,
        h::Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<lsp::InlayHint> output{temporaries_allocator};

        h::compiler::Scope scope
        {
            .variables{temporaries_allocator}
        };

        add_parameters_to_scope(
            scope,
            function_declaration.input_parameter_names,
            function_declaration.type.input_parameter_types,
            function_declaration.input_parameter_source_positions
        );

        auto const process_statement = [&](h::Statement const& statement, h::compiler::Scope const& scope) -> void
        {
            if (statement.expressions.empty())
                return;

            h::Expression const& expression = statement.expressions[0];

            if (!expression.source_range.has_value())
                return;

            if (std::holds_alternative<h::Variable_declaration_expression>(expression.data))
            {
                h::Variable_declaration_expression const& variable_declaration = std::get<h::Variable_declaration_expression>(expression.data);

                std::optional<h::Type_reference> const variable_type = get_expression_type(
                    core_module,
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

        h::compiler::visit_statements_using_scope(
            core_module,
            &function_declaration,
            scope,
            function_definition.statements,
            declaration_database,
            process_statement
        );

        return output;
    }

    void create_inlay_hint_variable_type_label_for_function_parameters(
        std::vector<lsp::InlayHintLabelPart>& parts,
        h::Module const& core_module,
        h::Declaration_database const& declaration_database,
        std::span<std::pmr::string const> const parameter_names,
        std::span<h::Type_reference const> const parameter_types,
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
        h::Module const& core_module,
        h::Declaration_database const& declaration_database,
        std::optional<h::Type_reference> const& type_optional,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        if (!type_optional.has_value())
        {
            return;
        }

        h::Type_reference const& type = type_optional.value();

        if (h::is_custom_type_reference(type))
        {
            std::pmr::string const type_name = h::format_type_reference(
                core_module,
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
                std::optional<h::Source_range_location> const declaration_source_location = get_declaration_source_location(
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
        else if (std::holds_alternative<h::Constant_array_type>(type.data))
        {
            h::Constant_array_type const& constant_array_type = std::get<h::Constant_array_type>(type.data);
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
        else if (std::holds_alternative<h::Function_pointer_type>(type.data))
        {
            h::Function_pointer_type const& function_pointer_type = std::get<h::Function_pointer_type>(type.data);

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
        else if (std::holds_alternative<h::Pointer_type>(type.data))
        {
            h::Pointer_type const& pointer_type = std::get<h::Pointer_type>(type.data);

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
                !pointer_type.element_type.empty() ? std::optional<h::Type_reference>(pointer_type.element_type[0]) : std::optional<h::Type_reference>{std::nullopt},
                temporaries_allocator
            );
        }
        else if (std::holds_alternative<h::Type_instance>(type.data))
        {
            h::Type_instance const& type_instance = std::get<h::Type_instance>(type.data);

            create_inlay_hint_variable_type_label_aux(
                parts,
                core_module,
                declaration_database,
                h::Type_reference{.data = type_instance.type_constructor},
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
                h::Statement const& statement = type_instance.arguments[index];
                if (statement.expressions.empty())
                    continue;

                h::Expression const& first_expression = statement.expressions[0];

                if (std::holds_alternative<h::Type_expression>(first_expression.data))
                {
                    h::Type_expression const& argument = std::get<h::Type_expression>(first_expression.data);
                    
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
                    std::pmr::string const type_name = h::format_type_reference(
                        core_module,
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
            std::pmr::string const type_name = h::format_type_reference(
                core_module,
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
        h::Module const& core_module,
        h::Declaration_database const& declaration_database,
        h::Type_reference const& type,
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
