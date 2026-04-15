module;

#include <tree_sitter/api.h>

export module iris.parser.parse_tree;

import std;

import iris.core;

namespace iris::parser
{
    export struct Parse_node
    {
        TSNode ts_node;
    };

    export struct Parse_tree
    {
        std::pmr::u8string text;
        TSTree* ts_tree;
    };

    export std::string_view get_node_value(
        Parse_tree const& tree,
        Parse_node const& node
    );

    export std::string_view get_node_symbol(
        Parse_node const& node
    );

    export Parse_node get_root_node(
        Parse_tree const& tree
    );

    export std::optional<Parse_node> get_parent_node(
        Parse_node const& node
    );

    export std::optional<Parse_node> get_ancestor_node(
        Parse_node const& node,
        std::uint32_t const degree
    );

    export std::optional<Parse_node> get_child_node(
        Parse_tree const& tree,
        Parse_node const& node,
        std::uint32_t const child_index
    );

    export std::optional<Parse_node> get_child_node(
        Parse_tree const& tree,
        Parse_node const& node,
        std::string_view const child_key
    );

    export std::optional<Parse_node> get_last_child_node(
        Parse_tree const& tree,
        Parse_node const& node
    );

    export std::pmr::vector<Parse_node> get_child_nodes(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export std::pmr::vector<Parse_node> get_child_nodes(
        Parse_tree const& tree,
        Parse_node const& node,
        std::string_view const child_key,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export std::pmr::vector<Parse_node> get_named_child_nodes(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export std::pmr::vector<Parse_node> get_child_nodes_of_parent(
        Parse_tree const& tree,
        Parse_node const& node,
        std::string_view const parent_key,
        std::string_view const child_key,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export std::optional<std::uint32_t> get_child_node_index(
        Parse_node const& node
    );

    export std::optional<Parse_node> get_node_next_sibling(
        Parse_node const& node
    );

    export std::optional<Parse_node> get_node_previous_sibling(
        Parse_node const& node
    );

    export std::optional<Parse_node> get_node_previous_sibling(
        Parse_node const& node,
        std::uint32_t const degree
    );

    export Source_position get_node_start_source_position(
        Parse_node const& node
    );

    export Source_range get_node_source_range(
        Parse_node const& node
    );

    export bool has_errors(
        Parse_node const& node
    );

    export bool is_error_node(
        Parse_node const& node
    );

    export std::pmr::vector<Parse_node> get_error_or_missing_nodes(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export Parse_node get_smallest_node_that_contains_position(
        Parse_node const& node,
        iris::Source_position const& position
    );

    export std::uint32_t calculate_byte(
        std::u8string_view const text,
        TSPoint const start_point,
        std::uint32_t const start_byte,
        TSPoint const target_point
    );

    export std::uint32_t calculate_byte(
        Parse_tree const& tree,
        Parse_node const& hint_node,
        iris::Source_position const& source_position
    );

    export std::optional<Parse_node> find_node_before_source_position(
        Parse_tree const& tree,
        Parse_node const& hint_node,
        iris::Source_position const& source_position
    );

    export bool is_utf_8_code_point(
        char8_t const character
    );
}
