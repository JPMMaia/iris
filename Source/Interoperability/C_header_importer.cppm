export module iris.c_header_converter;

import std;

import iris.core;
import iris.core.struct_layout;

namespace iris::c
{
    export struct C_macro_declaration
    {
        std::pmr::string name;
        bool is_function_like = false;
        iris::Source_range_location source_location;
    };

    export struct C_declarations
    {
        std::pmr::string module_name;
        std::pmr::vector<iris::Alias_type_declaration> alias_type_declarations;
        std::pmr::vector<iris::Enum_declaration> enum_declarations;
        std::pmr::vector<iris::Forward_declaration> forward_declarations;
        std::pmr::vector<iris::Global_variable_declaration> global_variable_declarations;
        std::pmr::vector<iris::Struct_declaration> struct_declarations;
        std::pmr::vector<iris::Union_declaration> union_declarations;
        std::pmr::vector<iris::Function_declaration> function_declarations;
        std::pmr::vector<C_macro_declaration> macro_declarations;
        std::uint32_t unnamed_count = 0;
    };

    export struct C_header
    {
        iris::Language_version language_version;
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

    export std::optional<iris::Module> import_header(std::string_view const header_name, std::filesystem::path const& header_path, Options const& options);

    export std::optional<iris::Module> import_header_and_write_to_file(std::string_view const header_name, std::filesystem::path const& header_path, std::filesystem::path const& output_path, Options const& options);

    export std::optional<iris::Struct_layout> calculate_struct_layout(std::filesystem::path const& header_path, std::string_view struct_name, Options const& options);
}
