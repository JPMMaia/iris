module;

#include <assert.h>

#include <tree_sitter/api.h>

module iris.parser.parse_tree;

import std;

import iris.core;

namespace iris::parser
{
    std::string_view get_node_value(
        Parse_tree const& tree,
        Parse_node const& node
    )
    {
        std::uint32_t const start_byte = ts_node_start_byte(node.ts_node);
        std::uint32_t const end_byte = ts_node_end_byte(node.ts_node);
        std::uint32_t const count = end_byte - start_byte;
        return std::string_view{reinterpret_cast<char const*>(tree.text.data()) + start_byte, count};
    }

    std::string_view get_node_symbol(
        Parse_node const& node
    )
    {
        return ts_node_grammar_type(node.ts_node);
    }

    Parse_node get_root_node(Parse_tree const& tree)
    {
        TSNode const root_node = ts_tree_root_node(tree.ts_tree);
        return Parse_node{ .ts_node = root_node };
    }

    std::optional<Parse_node> get_parent_node(
        Parse_node const& node
    )
    {
        TSNode const parent_node = ts_node_parent(node.ts_node);
        if (ts_node_is_null(parent_node))
            return std::nullopt;

        return Parse_node{ .ts_node = parent_node };
    }

    std::optional<Parse_node> get_ancestor_node(
        Parse_node const& node,
        std::uint32_t const degree
    )
    {
        TSNode current_node = node.ts_node;

        for (std::uint32_t index = 0; index < degree; ++index)
        {
            current_node = ts_node_parent(current_node);
            if (ts_node_is_null(current_node))
                return std::nullopt;
        }

        return Parse_node{current_node};
    }

    std::optional<Parse_node> get_child_node(
        Parse_tree const& tree,
        Parse_node const& node,
        std::uint32_t const child_index
    )
    {
        TSNode const child_node = ts_node_child(node.ts_node, child_index);
        if (ts_node_is_null(child_node))
            return std::nullopt;

        return Parse_node{ .ts_node = child_node };
    }

    std::optional<Parse_node> get_child_node(
        Parse_tree const& tree,
        Parse_node const& node,
        std::string_view const child_key
    )
    {
        std::uint32_t const child_count = ts_node_child_count(node.ts_node);

        for (std::uint32_t child_index = 0; child_index < child_count; ++child_index)
        {
            TSNode const child_node = ts_node_child(node.ts_node, child_index);

            std::string_view const child_value = ts_node_grammar_type(child_node);
            if (child_value == child_key)
                return Parse_node{ .ts_node = child_node };
        }

        return std::nullopt;
    }

    std::optional<Parse_node> get_last_child_node(
        Parse_tree const& tree,
        Parse_node const& node
    )
    {
        std::uint32_t const child_count = ts_node_child_count(node.ts_node);
        if (child_count == 0)
            return std::nullopt;

        TSNode const child_node = ts_node_child(node.ts_node, child_count - 1);
        return Parse_node{ .ts_node = child_node };
    }

    std::pmr::vector<Parse_node> get_child_nodes(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<Parse_node> output{output_allocator};
        
        std::uint32_t const child_count = ts_node_child_count(node.ts_node);
        output.resize(child_count);

        for (std::uint32_t child_index = 0; child_index < child_count; ++child_index)
        {
            TSNode const child_node = ts_node_child(node.ts_node, child_index);
            output[child_index] = {.ts_node = child_node};
        }
        
        return output;
    }

    std::pmr::vector<Parse_node> get_child_nodes(
        Parse_tree const& tree,
        Parse_node const& node,
        std::string_view const child_key,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<Parse_node> output{output_allocator};
        
        std::uint32_t const named_child_count = ts_node_named_child_count(node.ts_node);
        output.reserve(named_child_count);
        
        std::uint32_t const child_count = ts_node_child_count(node.ts_node);

        for (std::uint32_t child_index = 0; child_index < child_count; ++child_index)
        {
            TSNode const child_node = ts_node_child(node.ts_node, child_index);

            std::string_view const child_value = ts_node_grammar_type(child_node);
            if (child_value == child_key)
                output.push_back({.ts_node = child_node});
        }
        
        return output;
    }

    std::pmr::vector<Parse_node> get_named_child_nodes(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<Parse_node> output{output_allocator};
        
        std::uint32_t const named_child_count = ts_node_named_child_count(node.ts_node);
        output.resize(named_child_count);
        
        for (std::uint32_t child_index = 0; child_index < named_child_count; ++child_index)
        {
            TSNode const child_node = ts_node_named_child(node.ts_node, child_index);
            output[child_index] = {.ts_node = child_node};
        }
        
        return output;
    }

    std::pmr::vector<Parse_node> get_child_nodes_of_parent(
        Parse_tree const& tree,
        Parse_node const& node,
        std::string_view const parent_key,
        std::string_view const child_key,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::optional<Parse_node> const parent_node = get_child_node(tree, node, parent_key);
        if (!parent_node.has_value())
            return std::pmr::vector<Parse_node>{output_allocator};

        return get_child_nodes(tree, parent_node.value(), child_key, output_allocator);
    }

    std::optional<std::uint32_t> get_child_node_index(
        Parse_node const& node
    )
    {
        std::optional<Parse_node> const parent = get_parent_node(
            node
        );
        if (!parent.has_value())
            return std::nullopt;

        std::uint32_t const child_count = ts_node_child_count(parent->ts_node);

        for (std::uint32_t child_index = 0; child_index < child_count; ++child_index)
        {
            TSNode const child_node = ts_node_child(parent->ts_node, child_index);

            if (child_node.id == node.ts_node.id)
                return child_index;
        }

        return std::nullopt;
    }

    std::optional<Parse_node> get_node_next_sibling(
        Parse_node const& node
    )
    {
        TSNode const sibling = ts_node_next_sibling(node.ts_node);
        if (ts_node_is_null(sibling))
            return std::nullopt;

        return Parse_node{ .ts_node = sibling };
    }

    std::optional<Parse_node> get_node_previous_sibling(
        Parse_node const& node
    )
    {
        TSNode const sibling = ts_node_prev_sibling(node.ts_node);
        if (ts_node_is_null(sibling))
            return std::nullopt;

        return Parse_node{ .ts_node = sibling };
    }

    std::optional<Parse_node> get_node_previous_sibling(
        Parse_node const& node,
        std::uint32_t const degree
    )
    {
        TSNode current = node.ts_node;

        for (std::uint32_t index = 0; index < degree; ++index)
        {
            TSNode const sibling = ts_node_prev_sibling(current);
            if (ts_node_is_null(sibling))
                return std::nullopt;

            current = sibling;
        }

        return Parse_node{ .ts_node = current };
    }

    Source_position get_node_start_source_position(
        Parse_node const& node
    )
    {
        TSPoint const point = ts_node_start_point(node.ts_node);
        return Source_position
        {
            .line = point.row + 1,
            .column = point.column + 1,
        };
    }

    Source_range get_node_source_range(
        Parse_node const& node
    )
    {
        TSPoint const start_point = ts_node_start_point(node.ts_node);
        TSPoint const end_point = ts_node_end_point(node.ts_node);

        return Source_range
        {
            .start = Source_position
            {
                .line = start_point.row + 1,
                .column = start_point.column + 1,
            },
            .end = Source_position
            {
                .line = end_point.row + 1,
                .column = end_point.column + 1,
            }
        };
    }

    bool has_errors(
        Parse_node const& node
    )
    {
        return ts_node_has_error(node.ts_node);
    }

    bool is_error_node(
        Parse_node const& node
    )
    {
        return ts_node_is_error(node.ts_node);
    }

    static bool is_error_or_missing_node(
        TSNode const node
    )
    {
        return ts_node_is_error(node) || ts_node_is_missing(node);
    }

    template<class FunctionT>
    void depth_first_search(
        TSTreeCursor* cursor,
        FunctionT&& visitor
    )
    {
        bool const search_children = visitor(cursor);
        if (!search_children)
            return;

        if (ts_tree_cursor_goto_first_child(cursor))
        {
            do
            {
                depth_first_search(cursor, visitor);
            } while (ts_tree_cursor_goto_next_sibling(cursor));

            ts_tree_cursor_goto_parent(cursor);
        }
    }

    std::pmr::vector<Parse_node> get_error_or_missing_nodes(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        if (!ts_node_has_error(node.ts_node))
            return std::pmr::vector<Parse_node>{output_allocator};

        std::pmr::vector<Parse_node> output{temporaries_allocator};

        auto const visitor = [&](TSTreeCursor* cursor) -> bool
        {
            TSNode const current_node = ts_tree_cursor_current_node(cursor);

            if (!ts_node_has_error(current_node))
                return false;

            if (is_error_or_missing_node(current_node))
            {
                output.push_back({.ts_node = current_node});
                return false;
            }

            return true;
        };

        TSTreeCursor cursor = ts_tree_cursor_new(node.ts_node);

        depth_first_search(&cursor, visitor);

        ts_tree_cursor_delete(&cursor);

        return std::pmr::vector<Parse_node>{std::move(output), output_allocator};
    }

    Parse_node get_smallest_node_that_contains_position(
        Parse_node const& node,
        iris::Source_position const& position
    )
    {
        TSPoint const start_point
        {
            .row = position.line - 1,
            .column = position.column - 1,
        };

        TSPoint const end_point
        {
            .row = position.line - 1,
            .column = position.column - 1,
        };

        TSNode descendant_node = ts_node_descendant_for_point_range(node.ts_node, start_point, end_point);

        return { .ts_node = descendant_node };
    }

    std::uint32_t calculate_byte(
        std::u8string_view const text,
        TSPoint const start_point,
        std::uint32_t const start_byte,
        TSPoint const target_point
    )
    {
        TSPoint current_point = start_point;
        std::uint32_t current_byte = start_byte;

        while (current_point.row < target_point.row && current_byte < text.size())
        {
            char8_t const character = text[current_byte];
            
            if (character == '\n')
            {
                current_point.row += 1;
                current_point.column = 0;
            }

            current_byte += 1;
        }
        assert(current_point.row == target_point.row);
        
        while (current_point.column < target_point.column && current_byte < text.size())
        {
            char8_t const character = text[current_byte];

            if (is_utf_8_code_point(character))
                current_point.column += 1;
            
            current_byte += 1;
        }
        assert(current_point.column == target_point.column);
        
        return current_byte;
    }

    std::uint32_t calculate_byte(
        Parse_tree const& tree,
        Parse_node const& hint_node,
        iris::Source_position const& source_position
    )
    {
        TSPoint const hint_start_point = ts_node_start_point(hint_node.ts_node);
        std::uint32_t const hint_start_byte = ts_node_start_byte(hint_node.ts_node);
        
        TSPoint const target_point{source_position.line - 1, source_position.column - 1};

        std::uint32_t const target_byte = calculate_byte(tree.text, hint_start_point, hint_start_byte, target_point);

        return target_byte;
    }

    std::optional<Parse_node> find_node_before_source_position(
        Parse_tree const& tree,
        Parse_node const& hint_node,
        iris::Source_position const& source_position
    )
    {
        Parse_node const root_node = get_root_node(tree);

        std::uint32_t const end_byte = calculate_byte(
            tree,
            hint_node,
            source_position
        );

        std::uint32_t current_byte = end_byte;
        while (current_byte > 0)
        {
            current_byte -= 1;

            char8_t const character = tree.text[current_byte];
            if (character == ' ' || character == '\n' || character == '\r')
                continue;

            TSNode const found_node = ts_node_descendant_for_byte_range(root_node.ts_node, current_byte, current_byte);
            if (ts_node_is_null(found_node))
                return std::nullopt;

            return Parse_node{found_node};
        }

        return std::nullopt;
    }

    bool is_utf_8_code_point(
        char8_t const character
    )
    {
        return (character & 0b11000000) != 0b10000000;
    }
}
