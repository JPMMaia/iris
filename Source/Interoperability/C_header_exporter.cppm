export module h.c_header_exporter;

import std;

import h.core;
import h.core.declarations;

namespace h::c
{
    export struct Exported_c_header
    {
        std::pmr::string content;
    };

    export Exported_c_header export_module_as_c_header(
        h::Module const& core_module,
        h::Declaration_database const& declaration_database,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& dependencies_c_file_paths,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export struct Exported_cpp_header
    {
        std::pmr::string content;
    };

    export Exported_cpp_header export_module_as_cpp_header(
        h::Module const& core_module,
        std::filesystem::path const& c_header_file_path,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );
}
