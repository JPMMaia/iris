export module h.c_header_converter;

import std;

import h.core;
import h.core.struct_layout;

namespace h::c
{
    export struct C_macro_declaration
    {
        std::pmr::string name;
        bool is_function_like = false;
        h::Source_range_location source_location;
    };

    export struct C_declarations
    {
        std::pmr::string module_name;
        std::pmr::vector<h::Alias_type_declaration> alias_type_declarations;
        std::pmr::vector<h::Enum_declaration> enum_declarations;
        std::pmr::vector<h::Forward_declaration> forward_declarations;
        std::pmr::vector<h::Global_variable_declaration> global_variable_declarations;
        std::pmr::vector<h::Struct_declaration> struct_declarations;
        std::pmr::vector<h::Union_declaration> union_declarations;
        std::pmr::vector<h::Function_declaration> function_declarations;
        std::pmr::vector<C_macro_declaration> macro_declarations;
        std::uint32_t unnamed_count = 0;
    };

    export struct C_header
    {
        h::Language_version language_version;
        std::filesystem::path path;
        C_declarations declarations;
    };

    export struct Options
    {
        std::optional<std::string_view> target_triple;
        std::span<std::filesystem::path const> include_directories;
        std::span<std::pmr::string const> public_prefixes;
        std::span<std::pmr::string const> remove_prefixes;
        bool allow_errors = true;
    };

    export std::optional<h::Module> import_header(std::string_view const header_name, std::filesystem::path const& header_path, Options const& options);

    export std::optional<h::Module> import_header_and_write_to_file(std::string_view const header_name, std::filesystem::path const& header_path, std::filesystem::path const& output_path, Options const& options);

    export std::optional<std::uint64_t> calculate_header_file_hash(std::filesystem::path const& header_path, Options const& options);

    export std::optional<h::Struct_layout> calculate_struct_layout(std::filesystem::path const& header_path, std::string_view struct_name, Options const& options);
}
