module;

#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <lsp/types.h>

module iris.language_server.signature_help;

import iris.compiler.analysis;
import iris.core;
import iris.core.declarations;
import iris.core.formatter;
import iris.language_server.core;
import iris.language_server.debug;
import iris.language_server.location;
import iris.parser.convertor;
import iris.parser.parse_tree;

namespace iris::language_server
{
    // Walk up the parse tree to find the nearest Expression_call ancestor.
    static std::optional<iris::parser::Parse_node> find_enclosing_call_node(
        iris::parser::Parse_node const& node
    )
    {
        if (iris::parser::get_node_symbol(node) == "Expression_call")
            return node;

        std::optional<iris::parser::Parse_node> current = iris::parser::get_parent_node(node);
        while (current.has_value())
        {
            std::string_view const current_symbol = iris::parser::get_node_symbol(current.value());
            if (current_symbol == "Expression_call")
                return current;
            current = iris::parser::get_parent_node(current.value());
        }
        return std::nullopt;
    }

    // Count comma tokens in the Expression_call_arguments child that appear before cursor.
    static std::uint32_t count_commas_before_position(
        iris::parser::Parse_tree const& parse_tree,
        iris::parser::Parse_node const& call_node,
        iris::Source_position const& cursor
    )
    {
        std::pmr::polymorphic_allocator<> alloc;

        // Find Expression_call_arguments child by symbol name.
        std::pmr::vector<iris::parser::Parse_node> const call_children =
            iris::parser::get_child_nodes(parse_tree, call_node, alloc);

        std::optional<iris::parser::Parse_node> args_node;
        for (iris::parser::Parse_node const& child : call_children)
        {
            if (iris::parser::get_node_symbol(child) == "Expression_call_arguments")
            {
                args_node = child;
                break;
            }
        }

        if (!args_node.has_value())
            return 0;

        std::pmr::vector<iris::parser::Parse_node> const args_children =
            iris::parser::get_child_nodes(parse_tree, args_node.value(), alloc);

        std::uint32_t comma_count = 0;
        for (iris::parser::Parse_node const& child : args_children)
        {
            if (iris::parser::get_node_symbol(child) != ",")
                continue;

            iris::Source_range const child_range = iris::parser::get_node_source_range(child);

            // The comma must end at or before the cursor to count.
            if (child_range.end.line < cursor.line ||
                (child_range.end.line == cursor.line && child_range.end.column <= cursor.column))
            {
                ++comma_count;
            }
        }

        return comma_count;
    }

    // Resolve the Function_declaration being called from a callee expression.
    static iris::Function_declaration const* find_called_function_declaration(
        Declaration_database const& declaration_database,
        iris::Module const& core_module,
        iris::Statement const& statement,
        iris::Expression const& callee_expr
    )
    {
        if (std::holds_alternative<iris::Variable_expression>(callee_expr.data))
        {
            iris::Variable_expression const& var =
                std::get<iris::Variable_expression>(callee_expr.data);
            std::optional<Declaration> const decl =
                find_underlying_declaration(declaration_database, core_module.name, var.name);
            if (decl.has_value() &&
                std::holds_alternative<iris::Function_declaration const*>(decl->data))
            {
                return std::get<iris::Function_declaration const*>(decl->data);
            }
        }
        else if (std::holds_alternative<iris::Access_expression>(callee_expr.data))
        {
            iris::Access_expression const& access =
                std::get<iris::Access_expression>(callee_expr.data);
            if (access.expression.expression_index < statement.expressions.size())
            {
                iris::Expression const& access_target =
                    statement.expressions[access.expression.expression_index];
                if (std::holds_alternative<iris::Variable_expression>(access_target.data))
                {
                    iris::Variable_expression const& var =
                        std::get<iris::Variable_expression>(access_target.data);
                    std::optional<Declaration> const decl =
                        find_underlying_declaration_using_import_alias(
                            declaration_database, core_module, var.name, access.member_name);
                    if (decl.has_value() &&
                        std::holds_alternative<iris::Function_declaration const*>(decl->data))
                    {
                        return std::get<iris::Function_declaration const*>(decl->data);
                    }
                }
            }
        }

        return nullptr;
    }

    // Resolve the Function_declaration being called from a Call_expression.
    static iris::Function_declaration const* find_called_function_declaration(
        Declaration_database const& declaration_database,
        iris::Module const& core_module,
        iris::Statement const& statement,
        iris::Call_expression const& call_expression
    )
    {
        if (call_expression.expression.expression_index >= statement.expressions.size())
            return nullptr;

        iris::Expression const& callee_expr =
            statement.expressions[call_expression.expression.expression_index];

        return find_called_function_declaration(
            declaration_database,
            core_module,
            statement,
            callee_expr
        );
    }

    static std::optional<iris::parser::Parse_node> find_opening_parenthesis_node(
        iris::parser::Parse_node const& smallest_node,
        iris::Source_position const& cursor
    )
    {
        std::uint32_t paren_depth = 0;

        std::optional<iris::parser::Parse_node> current = smallest_node;
        while (current.has_value())
        {
            std::string_view const symbol = iris::parser::get_node_symbol(current.value());
            iris::Source_range const node_range = iris::parser::get_node_source_range(current.value());

            if (node_range.end.line > cursor.line ||
                (node_range.end.line == cursor.line && node_range.end.column > cursor.column))
            {
                current = iris::parser::get_node_previous_sibling(current.value());
                continue;
            }

            if (symbol == ")")
            {
                ++paren_depth;
            }
            else if (symbol == "(")
            {
                if (paren_depth == 0)
                {
                    return current;
                }

                --paren_depth;
            }

            current = iris::parser::get_node_previous_sibling(current.value());
        }

        return std::nullopt;
    }

    static std::optional<iris::parser::Parse_node> find_enclosing_generic_expression_node(
        iris::parser::Parse_node const& node
    )
    {
        std::optional<iris::parser::Parse_node> current = node;
        while (current.has_value())
        {
            std::string_view const symbol = iris::parser::get_node_symbol(current.value());
            if (symbol == "Generic_expression" || symbol == "Generic_expression_or_instantiate")
                return current;

            current = iris::parser::get_parent_node(current.value());
        }

        return std::nullopt;
    }

    static std::optional<iris::parser::Parse_node> find_enclosing_instantiate_node(
        iris::parser::Parse_node const& node
    )
    {
        std::optional<iris::parser::Parse_node> current = node;
        while (current.has_value())
        {
            if (iris::parser::get_node_symbol(current.value()) == "Expression_instantiate")
                return current;

            current = iris::parser::get_parent_node(current.value());
        }

        return std::nullopt;
    }

    static std::optional<iris::parser::Parse_node> find_call_callee_generic_expression_node(
        iris::parser::Parse_tree const& parse_tree,
        iris::parser::Parse_node const& smallest_node,
        std::optional<iris::parser::Parse_node> const& call_node,
        iris::Source_position const& cursor
    )
    {
        std::optional<iris::parser::Parse_node> callee_node;
        if (call_node.has_value())
        {
            callee_node = iris::parser::get_child_node(parse_tree, call_node.value(), 0);
        }
        else
        {
            std::optional<iris::parser::Parse_node> const opening_parenthesis =
                find_opening_parenthesis_node(smallest_node, cursor);
            if (!opening_parenthesis.has_value())
                return std::nullopt;

            callee_node = iris::parser::find_node_before_source_position(
                parse_tree,
                opening_parenthesis.value(),
                iris::parser::get_node_start_source_position(opening_parenthesis.value())
            );
        }

        if (!callee_node.has_value())
            return std::nullopt;

        return find_enclosing_generic_expression_node(callee_node.value());
    }

    static std::optional<Declaration> find_function_declaration_from_pointer(
        Declaration_database const& declaration_database,
        iris::Function_declaration const* const function_declaration
    )
    {
        if (function_declaration == nullptr)
            return std::nullopt;

        for (auto const& [module_name, declarations] : declaration_database.map)
        {
            (void)module_name;

            for (auto const& [declaration_name, declaration] : declarations)
            {
                (void)declaration_name;

                if (std::holds_alternative<iris::Function_declaration const*>(declaration.data))
                {
                    iris::Function_declaration const* const current =
                        std::get<iris::Function_declaration const*>(declaration.data);
                    if (current == function_declaration)
                        return declaration;
                }
            }
        }

        return std::nullopt;
    }

    static std::optional<Signature_help_name> create_signature_help_name(
        Declaration_database const& declaration_database,
        iris::Function_declaration const* const function_declaration,
        iris::Module const& core_module
    )
    {
        if (function_declaration == nullptr)
            return std::nullopt;

        std::optional<Declaration> const declaration = find_function_declaration_from_pointer(
            declaration_database,
            function_declaration
        );
        if (declaration.has_value())
        {
            return Signature_help_name
            {
                .module_name = declaration->module_name,
                .declaration_name = function_declaration->name,
            };
        }

        return Signature_help_name
        {
            .module_name = core_module.name,
            .declaration_name = function_declaration->name,
        };
    }

    static std::optional<Declaration> find_struct_declaration_from_pointer(
        Declaration_database const& declaration_database,
        iris::Struct_declaration const* const struct_declaration
    )
    {
        if (struct_declaration == nullptr)
            return std::nullopt;

        for (auto const& [module_name, declarations] : declaration_database.map)
        {
            (void)module_name;

            for (auto const& [declaration_name, declaration] : declarations)
            {
                (void)declaration_name;

                if (std::holds_alternative<iris::Struct_declaration const*>(declaration.data))
                {
                    iris::Struct_declaration const* const current =
                        std::get<iris::Struct_declaration const*>(declaration.data);
                    if (current == struct_declaration)
                        return declaration;
                }
            }
        }

        return std::nullopt;
    }

    static std::optional<Signature_help_name> create_struct_signature_help_name(
        Declaration_database const& declaration_database,
        iris::Struct_declaration const* const struct_declaration,
        iris::Module const& core_module
    )
    {
        if (struct_declaration == nullptr)
            return std::nullopt;

        std::optional<Declaration> const declaration = find_struct_declaration_from_pointer(
            declaration_database,
            struct_declaration
        );
        if (declaration.has_value())
        {
            return Signature_help_name
            {
                .module_name = declaration->module_name,
                .declaration_name = struct_declaration->name,
            };
        }

        return Signature_help_name
        {
            .module_name = core_module.name,
            .declaration_name = struct_declaration->name,
        };
    }

    static iris::Struct_declaration const* find_instantiated_struct_declaration(
        Declaration_database const& declaration_database,
        iris::Module const& core_module,
        iris::Function_declaration const* const function_declaration,
        iris::compiler::Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& expression
    )
    {
        if (!std::holds_alternative<iris::Instantiate_expression>(expression.data))
            return nullptr;

        std::optional<iris::Type_reference> const expression_type = iris::compiler::get_expression_type(
            core_module,
            function_declaration,
            scope,
            statement,
            expression,
            std::nullopt,
            declaration_database
        );
        if (!expression_type.has_value())
            return nullptr;

        std::optional<Declaration> const declaration = find_underlying_declaration(
            declaration_database,
            expression_type.value()
        );
        if (!declaration.has_value() ||
            !std::holds_alternative<iris::Struct_declaration const*>(declaration->data))
        {
            return nullptr;
        }

        return std::get<iris::Struct_declaration const*>(declaration->data);
    }

    static iris::Function_declaration const* find_called_function_declaration_from_text_before_cursor(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        iris::Source_position const& source_position
    )
    {
        iris::parser::Parse_node const root_node = iris::parser::get_root_node(parse_tree);
        std::uint32_t const cursor_byte = iris::parser::calculate_byte(parse_tree, root_node, source_position);
        if (cursor_byte == 0)
            return nullptr;

        std::uint32_t opening_parenthesis_byte = 0;
        bool found_opening_parenthesis = false;

        std::uint32_t parenthesis_depth = 0;
        for (std::uint32_t byte_index = cursor_byte; byte_index > 0;)
        {
            --byte_index;

            char8_t const character = parse_tree.text[byte_index];
            if (character == u8')')
            {
                ++parenthesis_depth;
            }
            else if (character == u8'(')
            {
                if (parenthesis_depth == 0)
                {
                    opening_parenthesis_byte = byte_index;
                    found_opening_parenthesis = true;
                    break;
                }

                --parenthesis_depth;
            }
        }

        if (!found_opening_parenthesis || opening_parenthesis_byte == 0)
            return nullptr;

        std::uint32_t token_end = opening_parenthesis_byte;
        while (token_end > 0)
        {
            char8_t const previous = parse_tree.text[token_end - 1];
            if (previous == u8' ' || previous == u8'\t' || previous == u8'\n' || previous == u8'\r')
                --token_end;
            else
                break;
        }

        if (token_end == 0)
            return nullptr;

        auto const is_identifier_char = [](char8_t const value) -> bool
        {
            return (value >= u8'a' && value <= u8'z') ||
                   (value >= u8'A' && value <= u8'Z') ||
                   (value >= u8'0' && value <= u8'9') ||
                   value == u8'_';
        };

        std::uint32_t token_start = token_end;
        while (token_start > 0)
        {
            char8_t const previous = parse_tree.text[token_start - 1];
            if (is_identifier_char(previous) || previous == u8'.')
                --token_start;
            else
                break;
        }

        if (token_start >= token_end)
            return nullptr;

        std::string_view callee_text{
            reinterpret_cast<char const*>(parse_tree.text.data() + token_start),
            static_cast<std::size_t>(token_end - token_start)
        };
        while (!callee_text.empty() && (callee_text.front() == ' ' || callee_text.front() == '\t'))
            callee_text.remove_prefix(1);
        while (!callee_text.empty() && (callee_text.back() == ' ' || callee_text.back() == '\t'))
            callee_text.remove_suffix(1);

        if (callee_text.empty())
            return nullptr;

        std::size_t const dot_index = callee_text.rfind('.');
        if (dot_index == std::string_view::npos)
        {
            std::optional<Declaration> const declaration =
                find_underlying_declaration(declaration_database, core_module.name, callee_text);
            if (declaration.has_value() &&
                std::holds_alternative<iris::Function_declaration const*>(declaration->data))
            {
                return std::get<iris::Function_declaration const*>(declaration->data);
            }

            return nullptr;
        }

        std::string_view const left_hand_side = callee_text.substr(0, dot_index);
        std::string_view const member_name = callee_text.substr(dot_index + 1);
        if (left_hand_side.empty() || member_name.empty())
            return nullptr;

        std::optional<Declaration> const declaration =
            find_underlying_declaration_using_import_alias(
                declaration_database,
                core_module,
                left_hand_side,
                member_name
            );
        if (declaration.has_value() &&
            std::holds_alternative<iris::Function_declaration const*>(declaration->data))
        {
            return std::get<iris::Function_declaration const*>(declaration->data);
        }

        return nullptr;
    }

    static iris::Function_declaration const* find_called_function_declaration_from_parse_tree(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        iris::parser::Parse_node const& smallest_node,
        std::optional<iris::parser::Parse_node> const& call_node,
        iris::Source_position const& cursor
    )
    {
        std::optional<iris::parser::Parse_node> callee_node;
        if (call_node.has_value())
        {
            callee_node = iris::parser::get_child_node(parse_tree, call_node.value(), 0);
        }
        else
        {
            std::optional<iris::parser::Parse_node> const opening_parenthesis =
                find_opening_parenthesis_node(smallest_node, cursor);
            if (!opening_parenthesis.has_value())
                return nullptr;

            callee_node = iris::parser::find_node_before_source_position(
                parse_tree,
                opening_parenthesis.value(),
                iris::parser::get_node_start_source_position(opening_parenthesis.value())
            );
        }

        if (!callee_node.has_value())
            return nullptr;

        std::optional<iris::parser::Parse_node> const generic_expression =
            find_enclosing_generic_expression_node(callee_node.value());
        if (!generic_expression.has_value())
            return nullptr;

        std::string_view callee_text = iris::parser::get_node_value(parse_tree, generic_expression.value());
        while (!callee_text.empty() && (callee_text.front() == ' ' || callee_text.front() == '\t'))
            callee_text.remove_prefix(1);
        while (!callee_text.empty() && (callee_text.back() == ' ' || callee_text.back() == '\t'))
            callee_text.remove_suffix(1);

        std::size_t const dot_index = callee_text.rfind('.');
        if (dot_index == std::string_view::npos)
        {
            std::optional<Declaration> const decl =
                find_underlying_declaration(declaration_database, core_module.name, callee_text);
            if (decl.has_value() &&
                std::holds_alternative<iris::Function_declaration const*>(decl->data))
            {
                return std::get<iris::Function_declaration const*>(decl->data);
            }

            return nullptr;
        }

        std::string_view const left_hand_side = callee_text.substr(0, dot_index);
        std::string_view const member_name = callee_text.substr(dot_index + 1);
        if (left_hand_side.empty() || member_name.empty())
            return nullptr;

        std::optional<Declaration> const decl =
            find_underlying_declaration_using_import_alias(
                declaration_database,
                core_module,
                left_hand_side,
                member_name
            );
        if (decl.has_value() &&
            std::holds_alternative<iris::Function_declaration const*>(decl->data))
        {
            return std::get<iris::Function_declaration const*>(decl->data);
        }

        return nullptr;
    }

    // Extract documentation text (lines before the first @-tagged line) from the comment.
    // Trims leading/trailing whitespace on each line and trailing empty lines from the result.
    static std::optional<std::string> extract_function_doc(std::string_view const comment)
    {
        std::string result;
        bool first_line = true;

        std::size_t pos = 0;
        while (pos <= comment.size())
        {
            std::size_t const end = comment.find('\n', pos);
            std::size_t const line_end = (end == std::string_view::npos) ? comment.size() : end;

            std::string_view line = comment.substr(pos, line_end - pos);

            // Trim leading whitespace.
            while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
                line.remove_prefix(1);
            // Trim trailing whitespace (including \r).
            while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r'))
                line.remove_suffix(1);

            // Stop at the first @-tagged line.
            if (!line.empty() && line.front() == '@')
                break;

            if (!first_line)
                result += '\n';
            result.append(line);
            first_line = false;

            if (end == std::string_view::npos)
                break;
            pos = end + 1;
        }

        // Trim trailing newlines.
        while (!result.empty() && result.back() == '\n')
            result.pop_back();

        if (result.empty())
            return std::nullopt;

        return result;
    }

    // Extract documentation for a specific @input_parameter tag from the comment.
    static std::optional<std::string> extract_parameter_doc(
        std::string_view const comment,
        std::string_view const param_name
    )
    {
        std::string const tag = std::string("@input_parameter ") + std::string(param_name) + ": ";

        std::size_t pos = 0;
        while (pos <= comment.size())
        {
            std::size_t const end = comment.find('\n', pos);
            std::size_t const line_end = (end == std::string_view::npos) ? comment.size() : end;

            std::string_view line = comment.substr(pos, line_end - pos);

            // Trim leading whitespace.
            while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
                line.remove_prefix(1);

            if (line.starts_with(tag))
            {
                std::string doc{line.substr(tag.size())};
                // Trim trailing whitespace.
                while (!doc.empty() && (doc.back() == ' ' || doc.back() == '\t' || doc.back() == '\r'))
                    doc.pop_back();
                return doc;
            }

            if (end == std::string_view::npos)
                break;
            pos = end + 1;
        }

        return std::nullopt;
    }

    // Count commas before cursor for an incomplete/malformed call node (fallback for when
    // find_enclosing_call_node returns nullopt). Traverses backwards through siblings using
    // get_node_previous_sibling, tracking parenthesis nesting to count commas at the correct level.
    // Returns nullopt if opening parenthesis is not found; otherwise returns the comma count.
    static std::optional<std::uint32_t> count_commas_before_position_fallback(
        iris::parser::Parse_node const& smallest_node,
        iris::Source_position const& cursor
    )
    {
        std::uint32_t comma_count = 0;
        std::uint32_t paren_depth = 0;  // Track nesting: ) increments, ( decrements

        std::optional<iris::parser::Parse_node> current = smallest_node;
        
        while (current.has_value())
        {
            std::string_view const symbol = iris::parser::get_node_symbol(current.value());
            iris::Source_range const node_range = iris::parser::get_node_source_range(current.value());

            // Only process nodes that end at or before the cursor
            if (node_range.end.line > cursor.line ||
                (node_range.end.line == cursor.line && node_range.end.column > cursor.column))
            {
                current = iris::parser::get_node_previous_sibling(current.value());
                continue;
            }

            if (symbol == ")")
            {
                // Closing paren: we're entering a nested expression
                ++paren_depth;
            }
            else if (symbol == "(")
            {
                if (paren_depth == 0)
                {
                    // Found the opening paren of the call
                    return comma_count;
                }
                else
                {
                    // Closing a nested expression
                    --paren_depth;
                }
            }
            else if (symbol == "," && paren_depth == 0)
            {
                // Comma at the correct nesting level
                ++comma_count;
            }

            current = iris::parser::get_node_previous_sibling(current.value());
        }

        // Did not find opening parenthesis
        return std::nullopt;
    }

    static std::optional<std::uint32_t> count_commas_before_position_text_fallback(
        iris::parser::Parse_tree const& parse_tree,
        iris::Source_position const& cursor
    )
    {
        iris::parser::Parse_node const root_node = iris::parser::get_root_node(parse_tree);
        std::uint32_t const cursor_byte = iris::parser::calculate_byte(parse_tree, root_node, cursor);
        if (cursor_byte == 0)
            return std::nullopt;

        std::uint32_t comma_count = 0;
        std::uint32_t paren_depth = 0;
        std::optional<std::uint32_t> opening_parenthesis_byte = std::nullopt;

        for (std::uint32_t byte_index = cursor_byte; byte_index > 0;)
        {
            --byte_index;

            char8_t const character = parse_tree.text[byte_index];
            if (character == u8')')
            {
                ++paren_depth;
            }
            else if (character == u8'(')
            {
                if (paren_depth == 0)
                {
                    opening_parenthesis_byte = byte_index;
                    break;
                }

                --paren_depth;
            }
            else if (character == u8',' && paren_depth == 0)
            {
                ++comma_count;
            }
        }

        if (!opening_parenthesis_byte.has_value())
            return std::nullopt;

        auto const is_identifier_character = [](char8_t const value) -> bool
        {
            return (value >= u8'a' && value <= u8'z') ||
                   (value >= u8'A' && value <= u8'Z') ||
                   (value >= u8'0' && value <= u8'9') ||
                   value == u8'_';
        };

        std::uint32_t token_end = opening_parenthesis_byte.value();
        while (token_end > 0)
        {
            char8_t const previous = parse_tree.text[token_end - 1];
            if (previous == u8' ' || previous == u8'\t' || previous == u8'\n' || previous == u8'\r')
                --token_end;
            else
                break;
        }

        if (token_end == 0)
            return std::nullopt;

        char8_t const token_character = parse_tree.text[token_end - 1];
        bool const is_call_like_token =
            is_identifier_character(token_character) ||
            token_character == u8'.' ||
            token_character == u8')' ||
            token_character == u8']';

        if (!is_call_like_token)
            return std::nullopt;

        return comma_count;
    }

    static std::optional<std::uint32_t> count_instantiate_commas_before_position_text_fallback(
        iris::parser::Parse_tree const& parse_tree,
        iris::Source_position const& cursor
    )
    {
        auto const calculate_text_byte = [&](iris::Source_position const& position) -> std::uint32_t
        {
            std::uint32_t line = 0;
            std::uint32_t column = 0;
            for (std::uint32_t i = 0; i < parse_tree.text.size(); ++i)
            {
                if (line == position.line && column == position.column)
                    return i;

                if (parse_tree.text[i] == u8'\n')
                {
                    ++line;
                    column = 0;
                }
                else
                {
                    ++column;
                }
            }

            return static_cast<std::uint32_t>(parse_tree.text.size());
        };

        std::uint32_t const cursor_byte = calculate_text_byte(cursor);
        if (cursor_byte == 0 || cursor_byte > parse_tree.text.size())
            return std::nullopt;

        std::uint32_t comma_count = 0;
        std::uint32_t brace_depth = 0;
        bool found_opening_brace = false;

        for (std::uint32_t byte_index = cursor_byte; byte_index > 0;)
        {
            --byte_index;

            char8_t const character = parse_tree.text[byte_index];
            if (character == u8'}')
            {
                ++brace_depth;
            }
            else if (character == u8'{')
            {
                if (brace_depth == 0)
                {
                    found_opening_brace = true;
                    break;
                }

                --brace_depth;
            }
            else if (character == u8',' && brace_depth == 0)
            {
                ++comma_count;
            }
        }

        if (!found_opening_brace)
            return std::nullopt;

        return comma_count;
    }

    static std::optional<std::uint32_t> count_instantiate_commas_before_position_fallback(
        iris::parser::Parse_node const& smallest_node,
        iris::Source_position const& cursor
    )
    {
        std::uint32_t comma_count = 0;
        std::uint32_t brace_depth = 0;

        std::optional<iris::parser::Parse_node> current = smallest_node;
        while (current.has_value())
        {
            std::string_view const symbol = iris::parser::get_node_symbol(current.value());
            iris::Source_range const node_range = iris::parser::get_node_source_range(current.value());

            if (node_range.end.line > cursor.line ||
                (node_range.end.line == cursor.line && node_range.end.column > cursor.column))
            {
                current = iris::parser::get_node_previous_sibling(current.value());
                continue;
            }

            if (symbol == "}")
            {
                ++brace_depth;
            }
            else if (symbol == "{")
            {
                if (brace_depth == 0)
                    return comma_count;

                --brace_depth;
            }
            else if (symbol == "," && brace_depth == 0)
            {
                ++comma_count;
            }

            current = iris::parser::get_node_previous_sibling(current.value());
        }

        return std::nullopt;
    }

    // Build the signature label string for a function declaration, tracking UTF-16 offsets
    // for each input parameter within the label.  Also populates the parameters vector.
    static std::string build_signature_label(
        iris::Module const& core_module,
        iris::Function_declaration const& decl,
        std::vector<lsp::ParameterInformation>& parameters,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::string label = "function ";
        label += std::string_view(decl.name);
        label += '(';

        for (std::size_t i = 0; i < decl.input_parameter_names.size(); ++i)
        {
            if (i > 0)
                label += ", ";

            lsp::uint const param_start = static_cast<lsp::uint>(label.size());

            label += std::string_view(decl.input_parameter_names[i]);
            label += ": ";

            std::optional<iris::Type_reference> const type_ref =
                (i < decl.type.input_parameter_types.size())
                ? std::optional<iris::Type_reference>(decl.type.input_parameter_types[i])
                : std::nullopt;

            std::pmr::string const type_str = iris::format_type_reference(
                core_module,
                type_ref,
                output_allocator,
                temporaries_allocator
            );
            label.append(type_str.data(), type_str.size());

            lsp::uint const param_end = static_cast<lsp::uint>(label.size());

            lsp::ParameterInformation param_info;
            param_info.label = lsp::Tuple<lsp::uint, lsp::uint>{param_start, param_end};

            if (decl.comment.has_value())
            {
                std::optional<std::string> const param_doc =
                    extract_parameter_doc(decl.comment.value(), decl.input_parameter_names[i]);
                if (param_doc.has_value())
                    param_info.documentation = param_doc.value();
            }

            parameters.push_back(std::move(param_info));
        }

        label += ')';
        label += " -> (";

        for (std::size_t i = 0; i < decl.output_parameter_names.size(); ++i)
        {
            if (i > 0)
                label += ", ";

            label += std::string_view(decl.output_parameter_names[i]);
            label += ": ";

            std::optional<iris::Type_reference> const type_ref =
                (i < decl.type.output_parameter_types.size())
                ? std::optional<iris::Type_reference>(decl.type.output_parameter_types[i])
                : std::nullopt;

            std::pmr::string const type_str = iris::format_type_reference(
                core_module,
                type_ref,
                output_allocator,
                temporaries_allocator
            );
            label.append(type_str.data(), type_str.size());
        }

        label += ')';
        return label;
    }

    // Build the signature label string for a struct declaration, tracking UTF-16 offsets
    // for each member within the label.  Also populates the parameters vector.
    static std::string build_struct_signature_label(
        iris::Module const& core_module,
        iris::Struct_declaration const& struct_decl,
        std::vector<lsp::ParameterInformation>& parameters,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::string label{std::string_view(struct_decl.name)};
        label += " {\n";

        for (std::size_t i = 0; i < struct_decl.member_names.size(); ++i)
        {
            label += "    ";
            lsp::uint const param_start = static_cast<lsp::uint>(label.size());

            label += std::string_view(struct_decl.member_names[i]);
            label += ": ";

            std::optional<iris::Type_reference> const type_ref =
                (i < struct_decl.member_types.size())
                ? std::optional<iris::Type_reference>(struct_decl.member_types[i])
                : std::nullopt;

            std::pmr::string const type_str = iris::format_type_reference(
                core_module,
                type_ref,
                output_allocator,
                temporaries_allocator
            );
            label.append(type_str.data(), type_str.size());

            if (i < struct_decl.member_default_values.size())
            {
                label += " = ";
                std::pmr::string const default_str = iris::format_statement(
                    core_module,
                    struct_decl.member_default_values[i],
                    0,
                    false,
                    output_allocator,
                    temporaries_allocator
                );
                label.append(default_str.data(), default_str.size());
            }

            lsp::uint const param_end = static_cast<lsp::uint>(label.size());

            lsp::ParameterInformation param_info;
            param_info.label = lsp::Tuple<lsp::uint, lsp::uint>{param_start, param_end};

            for (iris::Indexed_comment const& ic : struct_decl.member_comments)
            {
                if (ic.index == static_cast<std::uint64_t>(i))
                {
                    std::optional<std::string> const doc = extract_function_doc(ic.comment);
                    if (doc.has_value())
                        param_info.documentation = doc.value();
                    break;
                }
            }

            parameters.push_back(std::move(param_info));

            if (i + 1 < struct_decl.member_names.size())
                label += ',';
            label += '\n';
        }

        label += '}';
        return label;
    }

    static iris::Struct_declaration const* find_struct_member_struct_declaration(
        Declaration_database const& declaration_database,
        iris::Struct_declaration const& struct_decl,
        std::string_view const member_name
    )
    {
        for (std::size_t i = 0; i < struct_decl.member_names.size(); ++i)
        {
            if (std::string_view(struct_decl.member_names[i]) != member_name)
                continue;

            if (i >= struct_decl.member_types.size())
                return nullptr;

            std::optional<Declaration> const declaration =
                find_underlying_declaration(declaration_database, struct_decl.member_types[i]);
            if (declaration.has_value() &&
                std::holds_alternative<iris::Struct_declaration const*>(declaration->data))
            {
                return std::get<iris::Struct_declaration const*>(declaration->data);
            }

            return nullptr;
        }

        return nullptr;
    }

    static iris::Struct_declaration const* refine_struct_declaration_from_parse_tree_context(
        Declaration_database const& declaration_database,
        iris::Module const& core_module,
        iris::parser::Parse_tree const& parse_tree,
        iris::parser::Parse_node const& smallest_node,
        iris::Source_position const& source_position,
        iris::Struct_declaration const* current_struct
    )
    {
        if (current_struct != nullptr)
        {
            std::vector<iris::parser::Parse_node> instantiate_member_ancestors;

            std::optional<iris::parser::Parse_node> current = smallest_node;
            while (current.has_value())
            {
                if (iris::parser::get_node_symbol(current.value()) == "Expression_instantiate_member")
                    instantiate_member_ancestors.push_back(current.value());

                current = iris::parser::get_parent_node(current.value());
            }

            for (auto iter = instantiate_member_ancestors.rbegin(); iter != instantiate_member_ancestors.rend(); ++iter)
            {
                std::optional<iris::parser::Parse_node> const member_name_node =
                    iris::parser::get_child_node(parse_tree, *iter, 0);
                if (!member_name_node.has_value())
                    continue;

                iris::Struct_declaration const* const nested_struct = find_struct_member_struct_declaration(
                    declaration_database,
                    *current_struct,
                    iris::parser::get_node_value(parse_tree, member_name_node.value())
                );
                if (nested_struct == nullptr)
                    break;

                current_struct = nested_struct;
            }

            if (current_struct != nullptr)
                return current_struct;
        }

        std::optional<Declaration> const declaration = find_declaration_that_contains_source_position(
            declaration_database,
            core_module.name,
            source_position
        );
        if (!declaration.has_value() ||
            !std::holds_alternative<iris::Struct_declaration const*>(declaration->data))
        {
            return current_struct;
        }

        iris::Struct_declaration const& containing_struct =
            *std::get<iris::Struct_declaration const*>(declaration->data);

        for (std::size_t i = 0; i < containing_struct.member_default_values.size(); ++i)
        {
            iris::Statement const& default_value = containing_struct.member_default_values[i];
            if (default_value.expressions.empty())
                continue;

            iris::Expression const& first_expression = default_value.expressions[0];
            if (!first_expression.source_range.has_value() ||
                !iris::range_contains_position_inclusive(first_expression.source_range.value(), source_position))
            {
                continue;
            }

            if (i >= containing_struct.member_types.size())
                return current_struct;

            std::optional<Declaration> const member_declaration =
                find_underlying_declaration(declaration_database, containing_struct.member_types[i]);
            if (member_declaration.has_value() &&
                std::holds_alternative<iris::Struct_declaration const*>(member_declaration->data))
            {
                return std::get<iris::Struct_declaration const*>(member_declaration->data);
            }

            return current_struct;
        }

        return current_struct;
    }

    Signature_help_kind decide_signature_help_kind(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        lsp::Position const position
    )
    {
        (void)declaration_database;
        (void)core_module;

        iris::Source_position const source_position = to_source_position(position);
        iris::Source_position fallback_source_position = source_position;
        ++fallback_source_position.column;
        iris::parser::Parse_node const root_node = iris::parser::get_root_node(parse_tree);
        iris::parser::Parse_node const smallest_node = iris::parser::get_smallest_node_that_contains_position(
            root_node,
            source_position
        );

        std::optional<iris::parser::Parse_node> current = smallest_node;
        while (current.has_value())
        {
            std::string_view const symbol = iris::parser::get_node_symbol(current.value());
            if (symbol == "Expression_call")
                return Signature_help_kind::Function;

            if (symbol == "Expression_instantiate" ||
                symbol == "Expression_instantiate_members" ||
                symbol == "Expression_instantiate_member")
            {
                return Signature_help_kind::Struct;
            }

            current = iris::parser::get_parent_node(current.value());
        }

        std::optional<iris::parser::Parse_node> const parent_node = iris::parser::get_parent_node(smallest_node);
        std::optional<iris::parser::Parse_node> const previous_sibling =
            iris::parser::get_node_previous_sibling(smallest_node);
        if (parent_node.has_value() && previous_sibling.has_value())
        {
            std::string_view const parent_symbol = iris::parser::get_node_symbol(parent_node.value());
            std::string_view const previous_symbol = iris::parser::get_node_symbol(previous_sibling.value());
            if (parent_symbol == "Expression_instantiate_members" &&
                (previous_symbol == "{" || previous_symbol == ","))
            {
                return Signature_help_kind::Struct;
            }
        }

        std::optional<iris::parser::Parse_node> const opening_parenthesis =
            find_opening_parenthesis_node(smallest_node, source_position);
        if (opening_parenthesis.has_value())
        {
            std::optional<iris::parser::Parse_node> const callee_node =
                iris::parser::find_node_before_source_position(
                    parse_tree,
                    opening_parenthesis.value(),
                    iris::parser::get_node_start_source_position(opening_parenthesis.value())
                );
            if (callee_node.has_value())
            {
                std::optional<iris::parser::Parse_node> const generic_expression =
                    find_enclosing_generic_expression_node(callee_node.value());
                if (generic_expression.has_value())
                    return Signature_help_kind::Function;
            }
        }

        return Signature_help_kind::None;
    }

    std::optional<Signature_help_name> find_function_call_module_and_function_name(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        lsp::Position const position
    )
    {
        iris::Source_position const source_position = to_source_position(position);
        iris::parser::Parse_node const root_node = iris::parser::get_root_node(parse_tree);
        iris::parser::Parse_node const smallest_node = iris::parser::get_smallest_node_that_contains_position(
            root_node,
            source_position
        );

        std::optional<iris::parser::Parse_node> const call_node = find_enclosing_call_node(smallest_node);

        std::optional<Signature_help_name> const text_name = create_signature_help_name(
            declaration_database,
            find_called_function_declaration_from_text_before_cursor(
                declaration_database,
                parse_tree,
                core_module,
                source_position
            ),
            core_module
        );
        if (text_name.has_value())
            return text_name;

        iris::Function_declaration const* const parse_tree_declaration =
            find_called_function_declaration_from_parse_tree(
                declaration_database,
                parse_tree,
                core_module,
                smallest_node,
                call_node,
                source_position
            );
        std::optional<Signature_help_name> const parse_tree_name = create_signature_help_name(
            declaration_database,
            parse_tree_declaration,
            core_module
        );
        if (parse_tree_name.has_value())
            return parse_tree_name;

        std::optional<Signature_help_name> semantic_name = std::nullopt;
        std::optional<iris::Source_range> semantic_call_range = std::nullopt;

        auto const is_more_nested_range = [](iris::Source_range const& lhs, iris::Source_range const& rhs) -> bool
        {
            bool const lhs_starts_later =
                lhs.start.line > rhs.start.line ||
                (lhs.start.line == rhs.start.line && lhs.start.column >= rhs.start.column);
            bool const lhs_ends_earlier =
                lhs.end.line < rhs.end.line ||
                (lhs.end.line == rhs.end.line && lhs.end.column <= rhs.end.column);

            return lhs_starts_later && lhs_ends_earlier;
        };

        visit_expressions_that_contain_position(
            declaration_database,
            core_module,
            source_position,
            [&](iris::Function_declaration const* function_declaration, iris::compiler::Scope const& scope, iris::Statement const& statement, iris::Expression const& expression) -> bool
            {
                (void)function_declaration;
                (void)scope;

                if (!std::holds_alternative<iris::Call_expression>(expression.data) ||
                    !expression.source_range.has_value())
                {
                    return false;
                }

                iris::Call_expression const& call_expression = std::get<iris::Call_expression>(expression.data);
                iris::Function_declaration const* const declaration = find_called_function_declaration(
                    declaration_database,
                    core_module,
                    statement,
                    call_expression
                );
                if (declaration == nullptr)
                    return false;

                bool const should_update =
                    !semantic_call_range.has_value() ||
                    is_more_nested_range(expression.source_range.value(), semantic_call_range.value());

                if (!should_update)
                    return false;

                std::optional<Signature_help_name> const current_name = create_signature_help_name(
                    declaration_database,
                    declaration,
                    core_module
                );
                if (!current_name.has_value())
                    return false;

                semantic_call_range = expression.source_range;
                semantic_name = current_name;
                return false;
            }
        );

        if (semantic_name.has_value())
            return semantic_name;

        std::optional<iris::parser::Parse_node> const generic_expression_node =
            find_call_callee_generic_expression_node(
                parse_tree,
                smallest_node,
                call_node,
                source_position
            );
        if (!generic_expression_node.has_value())
            return std::nullopt;

        iris::Statement callee_statement;
        iris::parser::node_to_expression(
            callee_statement,
            iris::parser::create_module_info(core_module),
            parse_tree,
            generic_expression_node.value(),
            {},
            {}
        );
        if (callee_statement.expressions.empty())
            return std::nullopt;

        iris::Expression const& callee_expression = callee_statement.expressions.front();

        iris::Function_declaration const* const expression_declaration = find_called_function_declaration(
            declaration_database,
            core_module,
            callee_statement,
            callee_expression
        );
        std::optional<Signature_help_name> const expression_name = create_signature_help_name(
            declaration_database,
            expression_declaration,
            core_module
        );
        if (expression_name.has_value())
            return expression_name;

        std::optional<iris::Function> const containing_function = find_function_that_contains_source_position(
            core_module,
            source_position
        );
        if (!containing_function.has_value())
            return std::nullopt;

        std::optional<iris::compiler::Scope> const scope = iris::compiler::calculate_scope(
            core_module,
            *containing_function->declaration,
            *containing_function->definition,
            declaration_database,
            source_position
        );
        if (!scope.has_value())
            return std::nullopt;

        std::optional<iris::Type_reference> const expression_type = iris::compiler::get_expression_type(
            core_module,
            containing_function->declaration,
            scope.value(),
            callee_statement,
            callee_expression,
            std::nullopt,
            declaration_database
        );
        if (!expression_type.has_value())
            return std::nullopt;

        iris::Function_declaration const* const typed_declaration = find_called_function_declaration(
            declaration_database,
            core_module,
            callee_statement,
            callee_expression
        );

        return create_signature_help_name(
            declaration_database,
            typed_declaration,
            core_module
        );
    }

    std::optional<std::uint32_t> find_function_call_active_parameter(
        iris::parser::Parse_tree const& parse_tree,
        lsp::Position const position
    )
    {
        iris::Source_position const source_position = to_source_position(position);
        iris::parser::Parse_node const root_node = iris::parser::get_root_node(parse_tree);
        iris::parser::Parse_node const smallest_node = iris::parser::get_smallest_node_that_contains_position(
            root_node,
            source_position
        );

        std::optional<std::uint32_t> best_result = std::nullopt;

        std::optional<iris::parser::Parse_node> const call_node = find_enclosing_call_node(smallest_node);
        if (call_node.has_value())
        {
            best_result = count_commas_before_position(
                parse_tree,
                call_node.value(),
                source_position
            );
        }

        std::optional<std::uint32_t> const sibling_fallback = count_commas_before_position_fallback(
            smallest_node,
            source_position
        );
        if (sibling_fallback.has_value() &&
            (!best_result.has_value() || sibling_fallback.value() > best_result.value()))
        {
            best_result = sibling_fallback;
        }

        std::optional<std::uint32_t> const text_fallback = count_commas_before_position_text_fallback(
            parse_tree,
            source_position
        );
        if (text_fallback.has_value() &&
            (!best_result.has_value() || text_fallback.value() > best_result.value()))
        {
            best_result = text_fallback;
        }

        return best_result;
    }

    lsp::TextDocument_SignatureHelpResult compute_function_signature_help(
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        std::uint32_t const active_parameter
    )
    {
        std::pmr::polymorphic_allocator<> const output_allocator;
        std::pmr::polymorphic_allocator<> const temporaries_allocator;

        std::vector<lsp::ParameterInformation> parameters;
        std::string const label = build_signature_label(
            core_module,
            function_declaration,
            parameters,
            output_allocator,
            temporaries_allocator
        );

        lsp::SignatureInformation signature_info;
        signature_info.label = label;
        signature_info.parameters = std::move(parameters);

        if (function_declaration.comment.has_value())
        {
            std::optional<std::string> const doc = extract_function_doc(function_declaration.comment.value());
            if (doc.has_value())
                signature_info.documentation = doc.value();
        }

        lsp::SignatureHelp signature_help;
        signature_help.signatures.push_back(std::move(signature_info));
        signature_help.activeSignature = 0;
        signature_help.activeParameter = active_parameter;
        return signature_help;
    }

    std::optional<Signature_help_name> find_instantiate_module_and_struct_name(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        lsp::Position const position
    )
    {
        iris::Source_position const source_position = to_source_position(position);
        iris::parser::Parse_node const root_node = iris::parser::get_root_node(parse_tree);
        iris::parser::Parse_node const smallest_node = iris::parser::get_smallest_node_that_contains_position(
            root_node,
            source_position
        );

        std::optional<Signature_help_name> semantic_name = std::nullopt;
        std::optional<iris::Source_range> semantic_instantiate_range = std::nullopt;

        auto const is_more_nested_range = [](iris::Source_range const& lhs, iris::Source_range const& rhs) -> bool
        {
            bool const lhs_starts_later =
                lhs.start.line > rhs.start.line ||
                (lhs.start.line == rhs.start.line && lhs.start.column >= rhs.start.column);
            bool const lhs_ends_earlier =
                lhs.end.line < rhs.end.line ||
                (lhs.end.line == rhs.end.line && lhs.end.column <= rhs.end.column);

            return lhs_starts_later && lhs_ends_earlier;
        };

        visit_expressions_that_contain_position(
            declaration_database,
            core_module,
            source_position,
            [&](iris::Function_declaration const* function_declaration, iris::compiler::Scope const& scope, iris::Statement const& statement, iris::Expression const& expression) -> bool
            {
                (void)function_declaration;
                (void)scope;

                if (!std::holds_alternative<iris::Instantiate_expression>(expression.data) ||
                    !expression.source_range.has_value())
                {
                    return false;
                }

                iris::Struct_declaration const* const instantiate_struct = find_instantiated_struct_declaration(
                    declaration_database,
                    core_module,
                    function_declaration,
                    scope,
                    statement,
                    expression
                );
                if (instantiate_struct == nullptr)
                    return false;

                iris::Struct_declaration const* const refined_struct = refine_struct_declaration_from_parse_tree_context(
                    declaration_database,
                    core_module,
                    parse_tree,
                    smallest_node,
                    source_position,
                    instantiate_struct
                );
                if (refined_struct == nullptr)
                    return false;

                bool const should_update =
                    !semantic_instantiate_range.has_value() ||
                    is_more_nested_range(expression.source_range.value(), semantic_instantiate_range.value());
                if (!should_update)
                    return false;

                std::optional<Signature_help_name> const current_name = create_struct_signature_help_name(
                    declaration_database,
                    refined_struct,
                    core_module
                );
                if (!current_name.has_value())
                    return false;

                semantic_instantiate_range = expression.source_range;
                semantic_name = current_name;
                return false;
            }
        );

        if (semantic_name.has_value())
            return semantic_name;

        std::optional<iris::parser::Parse_node> const instantiate_node = find_enclosing_instantiate_node(
            smallest_node
        );
        if (!instantiate_node.has_value())
            return std::nullopt;

        std::optional<iris::parser::Parse_node> const instantiate_target_node =
            iris::parser::get_child_node(parse_tree, instantiate_node.value(), 0);
        iris::Struct_declaration const* refine_start_struct = nullptr;

        if (instantiate_target_node.has_value())
        {
            iris::Statement instantiate_target_statement;
            iris::parser::node_to_expression(
                instantiate_target_statement,
                iris::parser::create_module_info(core_module),
                parse_tree,
                instantiate_target_node.value(),
                {},
                {}
            );

            if (!instantiate_target_statement.expressions.empty())
            {
                iris::Expression const& instantiate_target_expression = instantiate_target_statement.expressions.front();
                std::optional<Declaration> const declaration = find_value_declaration_using_expression(
                    declaration_database,
                    core_module,
                    instantiate_target_statement,
                    instantiate_target_expression
                );

                if (declaration.has_value() &&
                    std::holds_alternative<iris::Struct_declaration const*>(declaration->data))
                {
                    refine_start_struct = std::get<iris::Struct_declaration const*>(declaration->data);
                }
            }
        }

        iris::Struct_declaration const* const refined_struct = refine_struct_declaration_from_parse_tree_context(
            declaration_database,
            core_module,
            parse_tree,
            smallest_node,
            source_position,
            refine_start_struct
        );

        return create_struct_signature_help_name(
            declaration_database,
            refined_struct,
            core_module
        );
    }

    std::uint32_t find_best_member_name_match(
        std::span<std::pmr::string const> const member_names,
        std::string_view const value
    )
    {
        if (member_names.empty())
            return 0;

        std::string_view trimmed_value = value;
        while (!trimmed_value.empty() &&
               (trimmed_value.front() == ' ' || trimmed_value.front() == '\t'))
        {
            trimmed_value.remove_prefix(1);
        }
        while (!trimmed_value.empty() &&
               (trimmed_value.back() == ' ' || trimmed_value.back() == '\t'))
        {
            trimmed_value.remove_suffix(1);
        }

        if (trimmed_value.empty())
            return 0;

        for (std::size_t i = 0; i < member_names.size(); ++i)
        {
            if (std::string_view(member_names[i]) == trimmed_value)
                return static_cast<std::uint32_t>(i);
        }

        for (std::size_t i = 0; i < member_names.size(); ++i)
        {
            if (std::string_view(member_names[i]).starts_with(trimmed_value))
                return static_cast<std::uint32_t>(i);
        }

        return 0;
    }

    static std::optional<std::uint32_t> find_best_unwritten_member_name_match(
        std::span<std::pmr::string const> const member_names,
        std::vector<bool> const& written_member_mask,
        std::string_view const value
    )
    {
        if (member_names.empty() || member_names.size() != written_member_mask.size())
            return std::nullopt;

        std::string_view trimmed_value = value;
        while (!trimmed_value.empty() &&
               (trimmed_value.front() == ' ' || trimmed_value.front() == '\t'))
        {
            trimmed_value.remove_prefix(1);
        }
        while (!trimmed_value.empty() &&
               (trimmed_value.back() == ' ' || trimmed_value.back() == '\t'))
        {
            trimmed_value.remove_suffix(1);
        }

        if (trimmed_value.empty())
            return std::nullopt;

        for (std::size_t i = 0; i < member_names.size(); ++i)
        {
            if (written_member_mask[i])
                continue;

            if (std::string_view(member_names[i]) == trimmed_value)
                return static_cast<std::uint32_t>(i);
        }

        for (std::size_t i = 0; i < member_names.size(); ++i)
        {
            if (written_member_mask[i])
                continue;

            if (std::string_view(member_names[i]).starts_with(trimmed_value))
                return static_cast<std::uint32_t>(i);
        }

        return std::nullopt;
    }

    std::optional<std::uint32_t> find_instantiate_active_member(
        Declaration_database const& declaration_database,
        iris::Module const& core_module,
        Signature_help_name const& struct_name,
        iris::parser::Parse_tree const& parse_tree,
        lsp::Position const position
    )
    {
        std::optional<Declaration> const declaration = find_underlying_declaration(
            declaration_database,
            struct_name.module_name,
            struct_name.declaration_name
        );
        if (!declaration.has_value() ||
            !std::holds_alternative<iris::Struct_declaration const*>(declaration->data))
        {
            return std::nullopt;
        }

        iris::Struct_declaration const& struct_declaration =
            *std::get<iris::Struct_declaration const*>(declaration->data);
        if (struct_declaration.member_names.empty())
            return static_cast<std::uint32_t>(0);

        iris::Source_position const source_position = to_source_position(position);
        iris::parser::Parse_node const root_node = iris::parser::get_root_node(parse_tree);
        iris::parser::Parse_node const smallest_node = iris::parser::get_smallest_node_that_contains_position(
            root_node,
            source_position
        );

        std::optional<iris::Instantiate_expression> instantiate_expression = std::nullopt;
        std::optional<iris::Source_range> instantiate_source_range = std::nullopt;

        auto const is_more_nested_range = [](iris::Source_range const& lhs, iris::Source_range const& rhs) -> bool
        {
            bool const lhs_starts_later =
                lhs.start.line > rhs.start.line ||
                (lhs.start.line == rhs.start.line && lhs.start.column >= rhs.start.column);
            bool const lhs_ends_earlier =
                lhs.end.line < rhs.end.line ||
                (lhs.end.line == rhs.end.line && lhs.end.column <= rhs.end.column);

            return lhs_starts_later && lhs_ends_earlier;
        };

        visit_expressions_that_contain_position(
            declaration_database,
            core_module,
            source_position,
            [&](iris::Function_declaration const* function_declaration, iris::compiler::Scope const& scope, iris::Statement const& statement, iris::Expression const& expression) -> bool
            {
                if (!std::holds_alternative<iris::Instantiate_expression>(expression.data) ||
                    !expression.source_range.has_value())
                {
                    return false;
                }

                iris::Struct_declaration const* const instantiated_struct =
                    find_instantiated_struct_declaration(
                        declaration_database,
                        core_module,
                        function_declaration,
                        scope,
                        statement,
                        expression
                    );
                if (instantiated_struct != &struct_declaration)
                    return false;

                bool const should_update =
                    !instantiate_source_range.has_value() ||
                    is_more_nested_range(expression.source_range.value(), instantiate_source_range.value());
                if (!should_update)
                    return false;

                instantiate_expression = std::get<iris::Instantiate_expression>(expression.data);
                instantiate_source_range = expression.source_range;
                return false;
            }
        );

        if (!instantiate_expression.has_value())
        {
            std::optional<iris::parser::Parse_node> const instantiate_node =
                find_enclosing_instantiate_node(smallest_node);
            if (instantiate_node.has_value())
            {
                iris::Statement instantiate_statement;
                iris::parser::node_to_expression(
                    instantiate_statement,
                    iris::parser::create_module_info(core_module),
                    parse_tree,
                    instantiate_node.value(),
                    {},
                    {}
                );

                for (iris::Expression const& expression : instantiate_statement.expressions)
                {
                    if (std::holds_alternative<iris::Instantiate_expression>(expression.data))
                    {
                        instantiate_expression = std::get<iris::Instantiate_expression>(expression.data);
                        break;
                    }
                }
            }
        }

        if (!instantiate_expression.has_value())
            return std::nullopt;

        struct Written_member
        {
            std::size_t declaration_index;
            std::optional<iris::Source_range> source_range;
        };

        std::vector<Written_member> written_members;
        std::vector<bool> written_member_mask(struct_declaration.member_names.size(), false);

        for (iris::Instantiate_member_value_pair const& member_pair : instantiate_expression->members)
        {
            for (std::size_t i = 0; i < struct_declaration.member_names.size(); ++i)
            {
                if (std::string_view(struct_declaration.member_names[i]) !=
                    std::string_view(member_pair.member_name))
                {
                    continue;
                }

                written_members.push_back(
                    Written_member{
                        .declaration_index = i,
                        .source_range = member_pair.source_range,
                    }
                );
                written_member_mask[i] = true;
                break;
            }
        }

        if (written_members.empty())
            return static_cast<std::uint32_t>(0);

        for (Written_member const& written_member : written_members)
        {
            if (written_member.source_range.has_value() &&
                iris::range_contains_position_inclusive(written_member.source_range.value(), source_position))
            {
                return static_cast<std::uint32_t>(written_member.declaration_index);
            }
        }

        auto const is_identifier_character = [](char8_t const character) -> bool
        {
            return (character >= u8'a' && character <= u8'z') ||
                   (character >= u8'A' && character <= u8'Z') ||
                   (character >= u8'0' && character <= u8'9') ||
                   character == u8'_';
        };

        std::span<std::pmr::string const> const all_member_names{
            struct_declaration.member_names.data(),
            struct_declaration.member_names.size()
        };

        std::uint32_t const cursor_byte =
            iris::parser::calculate_byte(parse_tree, root_node, source_position);

        if (cursor_byte > 0)
        {
            std::optional<std::uint32_t> colon_byte = std::nullopt;
            for (std::uint32_t byte_index = cursor_byte; byte_index > 0;)
            {
                --byte_index;
                char8_t const character = parse_tree.text[byte_index];

                if (character == u8':')
                {
                    colon_byte = byte_index;
                    break;
                }

                if (character == u8',' || character == u8'{' || character == u8'}')
                    break;
            }

            if (colon_byte.has_value())
            {
                std::uint32_t member_name_end = colon_byte.value();
                while (member_name_end > 0)
                {
                    char8_t const previous = parse_tree.text[member_name_end - 1];
                    if (previous == u8' ' || previous == u8'\t' || previous == u8'\n' || previous == u8'\r')
                        --member_name_end;
                    else
                        break;
                }

                std::uint32_t member_name_start = member_name_end;
                while (member_name_start > 0)
                {
                    char8_t const previous = parse_tree.text[member_name_start - 1];
                    if (is_identifier_character(previous))
                        --member_name_start;
                    else
                        break;
                }

                if (member_name_start < member_name_end)
                {
                    std::string_view const member_name_text{
                        reinterpret_cast<char const*>(parse_tree.text.data() + member_name_start),
                        static_cast<std::size_t>(member_name_end - member_name_start)
                    };

                    std::uint32_t const member_index =
                        find_best_member_name_match(all_member_names, member_name_text);
                    if (member_index < struct_declaration.member_names.size())
                        return member_index;
                }
            }

            std::uint32_t token_end = cursor_byte;
            while (token_end > 0)
            {
                char8_t const previous = parse_tree.text[token_end - 1];
                if (previous == u8' ' || previous == u8'\t' || previous == u8'\n' || previous == u8'\r')
                    --token_end;
                else
                    break;
            }

            std::uint32_t token_start = token_end;
            while (token_start > 0)
            {
                char8_t const previous = parse_tree.text[token_start - 1];
                if (is_identifier_character(previous))
                    --token_start;
                else
                    break;
            }

            if (token_start < token_end)
            {
                std::string_view const partial_text{
                    reinterpret_cast<char const*>(parse_tree.text.data() + token_start),
                    static_cast<std::size_t>(token_end - token_start)
                };

                std::optional<std::uint32_t> const member_index =
                    find_best_unwritten_member_name_match(
                        all_member_names,
                        written_member_mask,
                        partial_text
                    );
                if (member_index.has_value())
                    return member_index.value();
            }
        }

        auto const position_is_before = [](iris::Source_position const& lhs, iris::Source_position const& rhs) -> bool
        {
            if (lhs.line != rhs.line)
                return lhs.line < rhs.line;

            return lhs.column < rhs.column;
        };

        for (Written_member const& written_member : written_members)
        {
            if (!written_member.source_range.has_value())
                continue;

            if (position_is_before(source_position, written_member.source_range->start))
                return static_cast<std::uint32_t>(written_member.declaration_index);
        }

        std::size_t const last_written_index = written_members.back().declaration_index;
        for (std::size_t i = last_written_index + 1; i < struct_declaration.member_names.size(); ++i)
        {
            if (!written_member_mask[i])
                return static_cast<std::uint32_t>(i);
        }

        for (std::size_t i = 0; i < struct_declaration.member_names.size(); ++i)
        {
            if (!written_member_mask[i])
                return static_cast<std::uint32_t>(i);
        }

        return static_cast<std::uint32_t>(0);
    }

    lsp::TextDocument_SignatureHelpResult compute_struct_signature_help(
        iris::Module const& core_module,
        iris::Struct_declaration const& struct_declaration,
        std::uint32_t const active_member
    )
    {
        std::pmr::polymorphic_allocator<> const output_allocator;
        std::pmr::polymorphic_allocator<> const temporaries_allocator;

        std::vector<lsp::ParameterInformation> parameters;
        std::string const label = build_struct_signature_label(
            core_module,
            struct_declaration,
            parameters,
            output_allocator,
            temporaries_allocator
        );

        lsp::SignatureInformation signature_info;
        signature_info.label = label;
        signature_info.parameters = std::move(parameters);

        if (struct_declaration.comment.has_value())
        {
            std::optional<std::string> const doc = extract_function_doc(struct_declaration.comment.value());
            if (doc.has_value())
                signature_info.documentation = doc.value();
        }

        lsp::SignatureHelp signature_help;
        signature_help.signatures.push_back(std::move(signature_info));
        signature_help.activeSignature = 0;
        signature_help.activeParameter = active_member;
        return signature_help;
    }

    lsp::TextDocument_SignatureHelpResult compute_signature_help(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        lsp::Position const position,
        std::function<void(lsp::LogMessageParams&&)> const& window_log_message
    )
    {
        (void)window_log_message;

        if (core_module.name.empty())
            return nullptr;

        Signature_help_kind const kind = decide_signature_help_kind(
            declaration_database,
            parse_tree,
            core_module,
            position
        );

        if (kind == Signature_help_kind::Function)
        {
            std::optional<Signature_help_name> const name = find_function_call_module_and_function_name(
                declaration_database,
                parse_tree,
                core_module,
                position
            );
            std::optional<std::uint32_t> const active_parameter = find_function_call_active_parameter(
                parse_tree,
                position
            );

            if (!name.has_value() || !active_parameter.has_value())
                return nullptr;

            std::optional<Declaration> const declaration = find_underlying_declaration(
                declaration_database,
                name->module_name,
                name->declaration_name
            );
            if (!declaration.has_value() ||
                !std::holds_alternative<iris::Function_declaration const*>(declaration->data))
            {
                return nullptr;
            }

            iris::Function_declaration const& function_declaration =
                *std::get<iris::Function_declaration const*>(declaration->data);
            return compute_function_signature_help(
                core_module,
                function_declaration,
                active_parameter.value()
            );
        }

        if (kind == Signature_help_kind::Struct)
        {
            std::optional<Signature_help_name> const name = find_instantiate_module_and_struct_name(
                declaration_database,
                parse_tree,
                core_module,
                position
            );
            if (!name.has_value())
                return nullptr;

            std::optional<std::uint32_t> const active_member = find_instantiate_active_member(
                declaration_database,
                core_module,
                name.value(),
                parse_tree,
                position
            );

            if (!active_member.has_value())
                return nullptr;

            std::optional<Declaration> const declaration = find_underlying_declaration(
                declaration_database,
                name->module_name,
                name->declaration_name
            );
            if (!declaration.has_value() ||
                !std::holds_alternative<iris::Struct_declaration const*>(declaration->data))
            {
                return nullptr;
            }

            iris::Struct_declaration const& struct_declaration =
                *std::get<iris::Struct_declaration const*>(declaration->data);
            return compute_struct_signature_help(
                core_module,
                struct_declaration,
                active_member.value()
            );
        }

        return nullptr;
    }
}
