module;

#include <compare>

export module iris.graph;

import std;

namespace iris::graph
{
    // A single node in a generic directed graph. `id` is the unique key used to
    // reference the node from edges; `label` is what should be displayed.
    export struct Graph_node
    {
        std::pmr::string id;
        std::pmr::string label;
        std::optional<std::pmr::string> file_path;
        bool external = false;

        friend bool operator==(Graph_node const&, Graph_node const&) = default;
    };

    // A directed edge from the node with id `from` to the node with id `to`.
    export struct Graph_edge
    {
        std::pmr::string from;
        std::pmr::string to;

        friend bool operator==(Graph_edge const&, Graph_edge const&) = default;
    };

    // A generic, domain-agnostic directed graph.
    export struct Graph
    {
        std::pmr::vector<Graph_node> nodes;
        std::pmr::vector<Graph_edge> edges;

        friend bool operator==(Graph const&, Graph const&) = default;
    };

    // Returns true if a node with the given id already exists in the graph.
    export bool has_node(
        Graph const& graph,
        std::string_view id
    );

    // Adds a node to the graph if one with the same id does not already exist.
    // Returns true if a new node was added, false if it already existed.
    export bool add_node(
        Graph& graph,
        std::string_view id,
        std::string_view label,
        std::optional<std::string_view> file_path = std::nullopt,
        bool external = false
    );

    // Adds a directed edge from `from` to `to`. Duplicate edges are ignored.
    // Returns true if a new edge was added.
    export bool add_edge(
        Graph& graph,
        std::string_view from,
        std::string_view to
    );
}
