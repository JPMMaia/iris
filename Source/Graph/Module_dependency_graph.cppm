module;

export module iris.graph.module_dependency;

import std;

import iris.core;
import iris.graph;

namespace iris::graph
{
    // Builds a dependency graph from a set of modules. Each module becomes a node
    // (id and label are the module name; file_path is the module's source file).
    // Each import becomes a directed edge from the importing module to the imported
    // one. Imported modules that are not present in the input are added as nodes
    // flagged `external`.
    export Graph create_module_dependency_graph(
        std::span<iris::Module const> core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    // Overload taking a non-contiguous subset of modules by pointer. Useful for
    // graphs rooted at a single module (see `collect_module_and_dependencies`).
    export Graph create_module_dependency_graph(
        std::span<iris::Module const* const> core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    // Collects `root_module_name` together with all of its transitive dependencies,
    // following `alias_imports`. Modules that cannot be found in `all_modules` are
    // skipped. Cycles are handled (each module is visited at most once). The result
    // can be passed to `create_module_dependency_graph` to build a graph rooted at a
    // single module.
    export std::pmr::vector<iris::Module const*> collect_module_and_dependencies(
        std::span<iris::Module const> all_modules,
        std::string_view root_module_name,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    // Overload that takes the root module itself. Unlike the overload above, the root is
    // always the first element of the result even when it cannot be found in `all_modules`
    // (for example when its name is empty because the module failed to parse). Prefer this
    // one whenever the root module is already at hand.
    export std::pmr::vector<iris::Module const*> collect_module_and_dependencies(
        std::span<iris::Module const> all_modules,
        iris::Module const& root_module,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );
}
