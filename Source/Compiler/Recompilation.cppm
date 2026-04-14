export module h.compiler.recompilation;

import std;

import h.core;
import h.core.hash;

namespace h::compiler
{
    export std::pmr::vector<std::pmr::string> find_modules_to_recompile(
        h::Module const& core_module,
        Symbol_name_to_hash const& previous_symbol_name_to_hash,
        Symbol_name_to_hash const& new_symbol_name_to_hash,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& module_name_to_file_path,
        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const& module_name_to_reverse_dependencies,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );
}
