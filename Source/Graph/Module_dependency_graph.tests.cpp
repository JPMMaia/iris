#include <memory_resource>
#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_all.hpp>

import iris.core;
import iris.graph;
import iris.graph.module_dependency;

namespace iris::graph
{
    namespace
    {
        iris::Module create_module(
            std::string_view const name,
            std::vector<std::string_view> const& imported_module_names
        )
        {
            iris::Module module;
            module.name = std::pmr::string{ name };

            for (std::string_view const imported_name : imported_module_names)
            {
                iris::Import_module_with_alias import;
                import.module_name = std::pmr::string{ imported_name };
                module.dependencies.alias_imports.push_back(std::move(import));
            }

            return module;
        }

        Graph_node const* find_node(Graph const& graph, std::string_view const id)
        {
            for (Graph_node const& node : graph.nodes)
                if (node.id == id)
                    return &node;
            return nullptr;
        }

        bool has_edge(Graph const& graph, std::string_view const from, std::string_view const to)
        {
            for (Graph_edge const& edge : graph.edges)
                if (edge.from == from && edge.to == to)
                    return true;
            return false;
        }
    }

    TEST_CASE("create_module_dependency_graph builds nodes and edges from imports", "[graph]")
    {
        std::pmr::polymorphic_allocator<> const allocator{ std::pmr::get_default_resource() };

        std::pmr::vector<iris::Module> modules;
        modules.push_back(create_module("A", { "B" }));
        modules.push_back(create_module("B", {}));

        Graph const graph = create_module_dependency_graph(std::span<iris::Module const>{ modules }, allocator);

        CHECK(graph.nodes.size() == 2);
        CHECK(graph.edges.size() == 1);
        CHECK(find_node(graph, "A") != nullptr);
        CHECK(find_node(graph, "B") != nullptr);
        CHECK(has_edge(graph, "A", "B"));
    }

    TEST_CASE("imports not present in the input are flagged external", "[graph]")
    {
        std::pmr::polymorphic_allocator<> const allocator{ std::pmr::get_default_resource() };

        std::pmr::vector<iris::Module> modules;
        modules.push_back(create_module("A", { "External" }));

        Graph const graph = create_module_dependency_graph(std::span<iris::Module const>{ modules }, allocator);

        Graph_node const* const external_node = find_node(graph, "External");
        REQUIRE(external_node != nullptr);
        CHECK(external_node->external);
        CHECK(has_edge(graph, "A", "External"));
    }

    TEST_CASE("collect_module_and_dependencies gathers transitive deps and handles cycles", "[graph]")
    {
        std::pmr::polymorphic_allocator<> const allocator{ std::pmr::get_default_resource() };

        std::pmr::vector<iris::Module> modules;
        modules.push_back(create_module("A", { "B" }));
        modules.push_back(create_module("B", { "C" }));
        modules.push_back(create_module("C", { "A" })); // cycle back to A
        modules.push_back(create_module("Unrelated", {}));

        std::pmr::vector<iris::Module const*> const subset =
            collect_module_and_dependencies(std::span<iris::Module const>{ modules }, "A", allocator);

        CHECK(subset.size() == 3); // A, B, C but not Unrelated

        Graph const graph = create_module_dependency_graph(std::span<iris::Module const* const>{ subset }, allocator);
        CHECK(find_node(graph, "A") != nullptr);
        CHECK(find_node(graph, "B") != nullptr);
        CHECK(find_node(graph, "C") != nullptr);
        CHECK(find_node(graph, "Unrelated") == nullptr);
    }
}
