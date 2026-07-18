module iris.graph;

import std;

namespace iris::graph
{
    bool has_node(
        Graph const& graph,
        std::string_view const id
    )
    {
        auto const is_match = [id](Graph_node const& node) -> bool
        {
            return node.id == id;
        };

        return std::ranges::any_of(graph.nodes, is_match);
    }

    bool add_node(
        Graph& graph,
        std::string_view const id,
        std::string_view const label,
        std::optional<std::string_view> const file_path,
        bool const external
    )
    {
        if (has_node(graph, id))
            return false;

        std::pmr::polymorphic_allocator<> const allocator{ graph.nodes.get_allocator().resource() };

        Graph_node node
        {
            .id = std::pmr::string{ id, allocator },
            .label = std::pmr::string{ label, allocator },
            .file_path = file_path.has_value() ? std::optional<std::pmr::string>{ std::pmr::string{ *file_path, allocator } } : std::nullopt,
            .external = external,
        };

        graph.nodes.push_back(std::move(node));
        return true;
    }

    bool add_edge(
        Graph& graph,
        std::string_view const from,
        std::string_view const to
    )
    {
        auto const is_match = [from, to](Graph_edge const& edge) -> bool
        {
            return edge.from == from && edge.to == to;
        };

        if (std::ranges::any_of(graph.edges, is_match))
            return false;

        std::pmr::polymorphic_allocator<> const allocator{ graph.edges.get_allocator().resource() };

        Graph_edge edge
        {
            .from = std::pmr::string{ from, allocator },
            .to = std::pmr::string{ to, allocator },
        };

        graph.edges.push_back(std::move(edge));
        return true;
    }
}
