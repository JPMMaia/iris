module iris.graph.module_dependency;

import std;

import iris.core;
import iris.graph;

namespace iris::graph
{
    namespace
    {
        iris::Module const* find_module_by_name(
            std::span<iris::Module const> const modules,
            std::string_view const name
        )
        {
            auto const is_match = [name](iris::Module const& module) -> bool
            {
                return module.name == name;
            };

            auto const location = std::ranges::find_if(modules, is_match);
            return location != modules.end() ? &*location : nullptr;
        }

        void add_module_node(
            Graph& graph,
            iris::Module const& module
        )
        {
            std::string const file_path = module.source_file_path.has_value() ? module.source_file_path->string() : std::string{};
            std::optional<std::string_view> const file_path_view = file_path.empty() ? std::nullopt : std::optional<std::string_view>{ file_path };

            add_node(graph, module.name, module.name, file_path_view, false);
        }

        void add_module_edges(
            Graph& graph,
            iris::Module const& module
        )
        {
            for (iris::Import_module_with_alias const& import : module.dependencies.alias_imports)
            {
                if (!has_node(graph, import.module_name))
                    add_node(graph, import.module_name, import.module_name, std::nullopt, true);

                add_edge(graph, module.name, import.module_name);
            }
        }
    }

    Graph create_module_dependency_graph(
        std::span<iris::Module const> const core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        Graph graph
        {
            .nodes = std::pmr::vector<Graph_node>{ output_allocator },
            .edges = std::pmr::vector<Graph_edge>{ output_allocator },
        };

        // First pass: add every module as a node so imports can be classified as
        // internal or external in the second pass.
        for (iris::Module const& module : core_modules)
            add_module_node(graph, module);

        for (iris::Module const& module : core_modules)
            add_module_edges(graph, module);

        return graph;
    }

    Graph create_module_dependency_graph(
        std::span<iris::Module const* const> const core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        Graph graph
        {
            .nodes = std::pmr::vector<Graph_node>{ output_allocator },
            .edges = std::pmr::vector<Graph_edge>{ output_allocator },
        };

        for (iris::Module const* const module : core_modules)
            add_module_node(graph, *module);

        for (iris::Module const* const module : core_modules)
            add_module_edges(graph, *module);

        return graph;
    }

    std::pmr::vector<iris::Module const*> collect_module_and_dependencies(
        std::span<iris::Module const> const all_modules,
        std::string_view const root_module_name,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<iris::Module const*> result{ output_allocator };
        std::pmr::set<std::string_view> visited{ output_allocator };
        std::pmr::vector<std::string_view> pending{ output_allocator };

        pending.push_back(root_module_name);

        while (!pending.empty())
        {
            std::string_view const current_name = pending.back();
            pending.pop_back();

            if (visited.contains(current_name))
                continue;
            visited.insert(current_name);

            iris::Module const* const module = find_module_by_name(all_modules, current_name);
            if (module == nullptr)
                continue;

            result.push_back(module);

            for (iris::Import_module_with_alias const& import : module->dependencies.alias_imports)
            {
                if (!visited.contains(import.module_name))
                    pending.push_back(import.module_name);
            }
        }

        return result;
    }
}
