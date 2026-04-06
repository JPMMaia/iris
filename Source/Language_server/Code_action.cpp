module;

#include <filesystem>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include <lsp/types.h>

module h.language_server.code_action;

import h.compiler.analysis;
import h.compiler.diagnostic;
import h.core;
import h.core.declarations;
import h.core.expressions;
import h.core.formatter;
import h.core.types;
import h.language_server.core;
import h.language_server.location;
import h.parser.parse_tree;

namespace h::language_server
{
    static std::uint32_t count_consecutive_spaces(
        std::u8string_view const text,
        std::uint32_t const start_index
    )
    {
        std::uint32_t count = 0;

        for (std::uint32_t index = start_index; index < text.size(); ++index)
        {
            char8_t const current_character = text[index];

            if (current_character == ' ')
            {
                count += 1;
                continue;
            }

            break;
        }

        return count;
    }

    static std::uint32_t calculate_indendation(
        h::parser::Parse_tree const& parse_tree,
        h::Source_position const& source_position
    )
    {
        h::parser::Parse_node const root_node = get_root_node(parse_tree);

        h::parser::Parse_node const hint_node = h::parser::get_smallest_node_that_contains_position(
            root_node,
            source_position
        );

        std::uint32_t const start_byte = calculate_byte(
            parse_tree,
            hint_node,
            source_position
        );

        std::uint32_t current_byte = start_byte;
        while (current_byte > 0)
        {
            char8_t const current_character = parse_tree.text[current_byte];
            if (current_character == '\n' || current_character == '\r')
            {
                current_byte += 1;
                std::uint32_t const indentation = count_consecutive_spaces(
                    parse_tree.text,
                    current_byte
                );

                return indentation;
            }

            current_byte -= 1;
        }

        return 0;
    }

    static lsp::WorkspaceEdit create_workspace_edit_from_text_edit(
        std::filesystem::path const& source_file_path,
        h::Source_range const& range,
        std::string_view const new_text
    )
    {
        lsp::TextEdit text_edit
        {
            .range = to_lsp_range(range),
            .newText = std::string{new_text},
        };

        lsp::Array<lsp::TextEdit> text_edits
        {
            std::move(text_edit)
        };

        lsp::DocumentUri document_uri = lsp::DocumentUri::fromPath(source_file_path.generic_string());

        lsp::Map<lsp::DocumentUri, lsp::Array<lsp::TextEdit>> changes;
        changes[document_uri] = std::move(text_edits);

        lsp::WorkspaceEdit edit
        {
            .changes = std::move(changes),
        };

        return edit;
    }

    std::optional<std::size_t> find_enum_value_index_from_default_value(
        h::Enum_declaration const& enum_declaration,
        h::Statement const& member_default_value
    )
    {
        if (member_default_value.expressions.empty())
            return std::nullopt;

        h::Expression const& first_expression = member_default_value.expressions[0];
        if (std::holds_alternative<h::Access_expression>(first_expression.data))
        {
            h::Access_expression const& access_expression = std::get<h::Access_expression>(first_expression.data);

            auto const location = std::find_if(
                enum_declaration.values.begin(),
                enum_declaration.values.end(),
                [&](h::Enum_value const& enum_value) -> bool { return enum_value.name == access_expression.member_name; }
            );
            if (location == enum_declaration.values.end())
                return std::nullopt;

            return std::distance(enum_declaration.values.begin(), location);
        }

        return std::nullopt;
    }

    static h::Statement create_instantiate_member_statement_value(
        Declaration_database const& declaration_database,
        h::Module const& core_module,
        h::Struct_declaration const& declaration,
        std::size_t const member_index
    )
    {
        h::Statement const& member_default_value = declaration.member_default_values[member_index];
        
        h::Type_reference const& member_type = declaration.member_types[member_index];
        std::optional<Declaration> const member_declaration = find_underlying_declaration(
            declaration_database,
            member_type
        );
        if (!member_declaration.has_value())
            return member_default_value;

        if (std::holds_alternative<h::Enum_declaration const*>(member_declaration->data))
        {
            std::optional<std::string_view> enum_module_name = get_type_module_name(member_type);
            if (!enum_module_name.has_value() || enum_module_name.value() == core_module.name)
                return member_default_value;

            h::Enum_declaration const& enum_declaration = *std::get<h::Enum_declaration const*>(member_declaration->data);
            std::optional<std::size_t> const enum_value_index = find_enum_value_index_from_default_value(
                enum_declaration,
                member_default_value
            );
            if (!enum_value_index.has_value())
                return member_default_value;

            auto const import_location = std::find_if(
                core_module.dependencies.alias_imports.begin(),
                core_module.dependencies.alias_imports.end(),
                [&](Import_module_with_alias const& import_module) { return import_module.module_name == enum_module_name.value(); }
            );
            if (import_location == core_module.dependencies.alias_imports.end())
                return member_default_value;

            std::string_view const import_alias = import_location->alias;

            h::Statement statement
            {
                .expressions = {
                    h::Expression
                    {
                        .data = h::Access_expression
                        {
                            .expression = {
                                .expression_index = 1,
                            },
                            .member_name = enum_declaration.values[enum_value_index.value()].name
                        }
                    },
                    h::Expression
                    {
                        .data = h::Access_expression
                        {
                            .expression = {
                                .expression_index = 2,
                            },
                            .member_name = enum_declaration.name
                        }
                    },
                    h::Expression
                    {
                        .data = h::Variable_expression
                        {
                            .name = std::pmr::string{import_alias}
                        }
                    }
                }
            };

            return statement;
        }
        else
        {
            return member_default_value;
        }
    }

    static lsp::CodeAction create_add_missing_instantiate_members_code_action(
        Declaration_database const& declaration_database,
        h::parser::Parse_tree const& parse_tree,
        h::Module const& core_module,
        h::Struct_declaration const& declaration,
        h::Statement const& original_statement,
        h::Expression const& original_expression,
        h::Instantiate_expression const& original_instantiate_expression
    )
    {
        h::Instantiate_expression new_instantiate_expression = original_instantiate_expression;
        std::size_t const original_expression_index = find_expression_index(original_statement, original_expression);
        
        Statement statement = original_statement;
        
        for (std::size_t index = 0; index < declaration.member_names.size(); ++index)
        {
            std::string_view const& member_name = declaration.member_names[index];

            auto const location = std::find_if(
                new_instantiate_expression.members.begin(),
                new_instantiate_expression.members.end(),
                [&](Instantiate_member_value_pair const& member) -> bool { return member.member_name == member_name; }
            );

            if (location == new_instantiate_expression.members.end())
            {
                std::size_t const value_expression_index = statement.expressions.size();
                
                h::Statement const member_statement = create_instantiate_member_statement_value(
                    declaration_database,
                    core_module,
                    declaration,
                    index
                );

                add_expressions_to_expressions(statement.expressions, member_statement.expressions);

                new_instantiate_expression.members.push_back(
                    h::Instantiate_member_value_pair
                    {
                        .member_name = std::pmr::string{member_name},
                        .value = {.expression_index = value_expression_index },
                        .source_range = std::nullopt,
                    }
                );
            }
        }

        std::sort(
            new_instantiate_expression.members.begin(),
            new_instantiate_expression.members.end(),
            [&](Instantiate_member_value_pair const& first, Instantiate_member_value_pair const& second)
            {
                for (std::size_t index = 0; index < declaration.member_names.size(); ++index)
                {
                    std::string_view const& member_name = declaration.member_names[index];

                    if (member_name == first.member_name)
                        return true;
                    else if (member_name == second.member_name)
                        return false;
                }

                return false;
            }
        );

        statement.expressions[original_expression_index] = h::Expression{new_instantiate_expression};

        std::uint32_t const indentation = calculate_indendation(
            parse_tree,
            original_expression.source_range->start
        );

        std::pmr::string const new_text = h::format_expression(
            core_module,
            statement,
            statement.expressions[original_expression_index],
            indentation,
            false,
            {},
            {}
        );

        lsp::WorkspaceEdit edit = create_workspace_edit_from_text_edit(
            core_module.source_file_path.value(),
            original_expression.source_range.value(),
            new_text
        );

        lsp::CodeActionKind const kind =
            original_instantiate_expression.type == h::Instantiate_expression_type::Explicit ?
            lsp::CodeActionKind::QuickFix :
            lsp::CodeActionKind::RefactorRewrite;

        return lsp::CodeAction
        {
            .title = "Add missing instantiate members",
            .kind = kind,
            .diagnostics = std::nullopt, // TODO
            .isPreferred = true,
            .edit = std::move(edit),
        };
    }

    static bool is_position_before(
        h::Source_position const& first,
        h::Source_position const& second
    )
    {
        if (first.line < second.line)
            return true;

        if (first.line > second.line)
            return false;

        return first.column < second.column;
    }

    static h::Expression const* find_innermost_instantiate_expression(
        h::Statement const& statement,
        h::Source_position const& source_position
    )
    {
        h::Expression const* innermost = nullptr;

        for (h::Expression const& expression : statement.expressions)
        {
            if (std::holds_alternative<h::Instantiate_expression>(expression.data))
            {
                if (h::range_contains_position_inclusive(expression.source_range.value(), source_position))
                {
                    if (innermost != nullptr)
                    {
                        if (h::range_contains_position_inclusive(innermost->source_range.value(), expression.source_range->start))
                        {
                            innermost = &expression;
                        }
                    }
                    else
                    {
                        innermost = &expression;
                    }
                }
            }
        }

        return innermost;
    }

    lsp::CodeAction create_add_cast_code_action(
        Declaration_database const& declaration_database,
        h::Module const& core_module,
        h::compiler::Diagnostic const& diagnostic,
        h::compiler::Diagnostic_mismatch_type_data const& mismatch_data,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::optional<h::Type_reference> const provided_underlying_type = get_underlying_type(declaration_database, mismatch_data.provided_type);
        std::optional<h::Type_reference> const expected_underlying_type = get_underlying_type(declaration_database, mismatch_data.expected_type);

        std::pmr::string const provided_type_name = h::format_type_reference(core_module, mismatch_data.provided_type, temporaries_allocator, temporaries_allocator);
        std::pmr::string const provided_underlying_type_name = h::format_type_reference(core_module, provided_underlying_type, temporaries_allocator, temporaries_allocator);

        std::pmr::string const expected_type_name = h::format_type_reference(core_module, mismatch_data.expected_type, temporaries_allocator, temporaries_allocator);
        std::pmr::string const expected_underlying_type_name = h::format_type_reference(core_module, expected_underlying_type, temporaries_allocator, temporaries_allocator);

        h::Source_range const source_range = h::create_source_range(
            diagnostic.range.end.line,
            diagnostic.range.end.column,
            diagnostic.range.end.line,
            diagnostic.range.end.column
        );

        std::string const new_text = std::format(" as {}", expected_type_name);

        lsp::WorkspaceEdit edit = create_workspace_edit_from_text_edit(
            core_module.source_file_path.value(),
            source_range,
            new_text
        );

        return lsp::CodeAction
        {
            .title = std::format("Add cast from '{}' to '{}' ('{}' to '{}')", provided_type_name, expected_type_name, provided_underlying_type_name, expected_underlying_type_name),
            .kind = lsp::CodeActionKind::QuickFix,
            .diagnostics = std::nullopt, // TODO
            .isPreferred = true,
            .edit = std::move(edit),
        };
    }

    void add_fix_code_action(
        std::vector<std::variant<lsp::Command, lsp::CodeAction>>& code_actions,
        Declaration_database const& declaration_database,
        h::parser::Parse_tree const& parse_tree,
        h::Module const& core_module,
        std::span<h::compiler::Diagnostic const> const diagnostics,
        lsp::Range const range,
        lsp::CodeActionContext const& context
    )
    {
        h::Source_range const source_range = to_source_range(range);

        for (h::compiler::Diagnostic const& diagnostic : diagnostics)
        {
            if (!range_contains_position_inclusive(diagnostic.range, source_range.start))
                continue;

            if (diagnostic.code.has_value())
            {
                h::compiler::Diagnostic_code const code = diagnostic.code.value();
                if (code == h::compiler::Diagnostic_code::Type_mismatch)
                {
                    h::compiler::Diagnostic_mismatch_type_data const mismatch_data = h::compiler::read_diagnostic_mismatch_type_data(
                        diagnostic.data
                    );

                    lsp::CodeAction code_action = create_add_cast_code_action(
                        declaration_database,
                        core_module,
                        diagnostic,
                        mismatch_data,
                        {}
                    );
                    
                    code_actions.emplace_back(std::move(code_action));
                }
            }
        }
    }

    lsp::TextDocument_CodeActionResult compute_code_actions(
        Declaration_database const& declaration_database,
        h::parser::Parse_tree const& parse_tree,
        h::Module const& core_module,
        std::span<h::compiler::Diagnostic const> const diagnostics,
        lsp::Range const range,
        lsp::CodeActionContext const& context
    )
    {
        std::vector<std::variant<lsp::Command, lsp::CodeAction>> code_actions;

        add_fix_code_action(code_actions, declaration_database, parse_tree, core_module, diagnostics, range, context);

        h::Source_range const source_range = to_source_range(range);

        std::optional<h::Function> const function = find_function_that_contains_source_position(
            core_module,
            source_range.start
        );
        if (function.has_value())
        {
            std::optional<lsp::TextDocument_CodeActionResult> result_optional = std::nullopt;

            auto const process_statement = [&](h::Statement const& statement, h::compiler::Scope const& scope) -> bool
            {
                h::Expression const* expression = find_innermost_instantiate_expression(
                    statement,
                    source_range.start
                );
                if (expression != nullptr)
                {
                    std::optional<h::Type_reference> const type_to_instantiate = get_expression_type(
                        core_module,
                        function->declaration,
                        scope,
                        statement,
                        *expression,
                        std::nullopt,
                        declaration_database
                    );
                    if (type_to_instantiate.has_value())
                    {
                        std::optional<Declaration> const& declaration = find_underlying_declaration(declaration_database, type_to_instantiate.value());
                        if (declaration.has_value() && std::holds_alternative<h::Struct_declaration const*>(declaration->data))
                        {
                            std::pmr::vector<h::compiler::Declaration_member_info> const member_infos = h::compiler::get_declaration_member_infos(
                                declaration.value(),
                                {}
                            );

                            h::Instantiate_expression const& instantiate_expression = std::get<h::Instantiate_expression>(expression->data);
                            if (member_infos.size() != instantiate_expression.members.size())
                            {
                                lsp::CodeAction code_action = create_add_missing_instantiate_members_code_action(
                                    declaration_database,
                                    parse_tree,
                                    core_module,
                                    *std::get<h::Struct_declaration const*>(declaration->data),
                                    statement,
                                    *expression,
                                    instantiate_expression
                                );

                                code_actions.push_back(std::move(code_action));
                                return true;
                            }
                        }
                    }
                }

                return false;
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

        return code_actions;
    }
}
