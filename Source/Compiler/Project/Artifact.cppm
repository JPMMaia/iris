export module iris.compiler.artifact;

import std;

import iris.compiler.target;
import iris.compiler.presets;
import iris.core;

namespace iris::compiler
{
    export struct Version
    {
        std::uint32_t major;
        std::uint32_t minor;
        std::uint32_t patch;
    };

    export enum Artifact_type
    {
        Executable,
        Library
    };

    export struct Dependency
    {
        std::pmr::string artifact_name;
    };

    export struct C_header
    {
        std::pmr::string module_name;
        std::pmr::string header;
        std::pmr::vector<std::pmr::string> dependencies;
        std::optional<bool> allow_errors;
    };

    export struct Export_c_header_source_group
    {
        std::optional<std::filesystem::path> output_directory;
    };

    export struct Import_c_header_source_group
    {
        std::pmr::vector<C_header> c_headers;
        std::pmr::vector<std::filesystem::path> search_paths;
        std::pmr::vector<std::pmr::string> public_prefixes;
        std::pmr::vector<std::pmr::string> remove_prefixes;
    };


    export struct Cpp_source_group
    {
    };

    export struct Iris_source_group
    {
    };

    export struct Source_group
    {
        using Data_type = std::variant<
            Export_c_header_source_group,
            Import_c_header_source_group,
            Cpp_source_group,
            Iris_source_group
        >;
        
        std::optional<Data_type> data;
        std::pmr::vector<std::pmr::string> include;
        std::pmr::vector<std::pmr::string> additional_flags;
    };

    export struct Executable_info
    {
        std::filesystem::path source;
        std::pmr::string entry_point;
    };

    export struct Library_info
    {
        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> external_libraries;
    };

    export struct Copy_entry
    {
        std::filesystem::path source;
        std::filesystem::path destination;
    };

    export struct Artifact
    {
        std::filesystem::path file_path;
        std::pmr::string name;
        Version version;
        Artifact_type type;
        std::pmr::vector<Dependency> dependencies;
        std::pmr::vector<Source_group> sources;
        std::pmr::vector<std::filesystem::path> public_include_directories;
        std::optional<std::variant<Executable_info, Library_info>> info;
        std::pmr::vector<Copy_entry> copy_entries;
    };

    export Artifact get_artifact(std::filesystem::path const& artifact_file_path);

    export Artifact get_artifact(
        std::filesystem::path const& artifact_file_path,
        Environment_variables const& environment_variables
    );

    export void write_artifact_to_file(Artifact const& artifact, std::filesystem::path const& artifact_file_path);

    export std::pmr::vector<std::filesystem::path> get_public_include_directories(Artifact const& artifact, std::span<Artifact const> const artifacts, std::pmr::polymorphic_allocator<> const& output_allocator, std::pmr::polymorphic_allocator<> const& temporaries_allocator);
    
    export bool contains_any_compilable_source(Artifact const& artifact);

    export std::pmr::vector<Source_group const*> get_export_c_header_source_groups(Artifact const& artifact, std::pmr::polymorphic_allocator<> const& output_allocator);
    
    export std::pmr::vector<Source_group const*> get_c_header_source_groups(Artifact const& artifact, std::pmr::polymorphic_allocator<> const& output_allocator);

    export std::pmr::vector<C_header> get_c_headers(Artifact const& artifact, std::pmr::polymorphic_allocator<> const& output_allocator);

    export C_header const* find_c_header(Artifact const& artifact, std::string_view const module_name);

    export std::optional<std::filesystem::path> find_c_header_path(
        std::string_view c_header,
        std::span<std::filesystem::path const> search_paths
    );

    export bool visit_included_files(
        std::filesystem::path const& root_path,
        std::string_view const regular_expression,
        std::function<bool(std::filesystem::path)> const& predicate
    );

    export bool visit_included_files(
        Artifact const& artifact,
        std::function<bool(std::filesystem::path)> const& predicate
    );

    export std::pmr::vector<std::filesystem::path> get_artifact_iris_source_files(
        Artifact const& artifact,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<std::filesystem::path> get_artifact_cpp_source_files(
        Artifact const& artifact,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<std::filesystem::path> find_included_files(
        std::filesystem::path const& root_path,
        std::string_view const regular_expression,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export std::pmr::vector<std::filesystem::path> find_included_files(
        std::filesystem::path const& root_path,
        std::span<std::pmr::string const> const regular_expressions,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<std::filesystem::path> find_root_include_directories(
        Artifact const& artifact,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::optional<std::size_t> find_artifact_index_that_includes_source_file(
        std::span<Artifact const> const artifacts,
        std::filesystem::path const& source_file_path,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<Artifact const*> get_artifact_dependencies(
        Artifact const& artifact,
        std::span<Artifact const> const all_artifacts,
        bool const recursive,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<iris::Module const*> get_artifact_modules_and_dependencies(
        Artifact const& artifact,
        std::span<Artifact const> const all_artifacts,
        std::span<iris::Module const> const header_modules,
        std::span<iris::Module const> const core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export struct External_library_info
    {
        std::pmr::string key;
        std::pmr::vector<std::pmr::string> names;
        bool is_debug;
        bool is_dynamic;
    };

    export std::optional<External_library_info> get_external_library(
        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const& external_libraries,
        Target const& target,
        bool prefer_debug,
        bool prefer_dynamic
    );

    export std::pmr::vector<std::string_view> get_external_library_dlls(
        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const& external_libraries,
        std::string_view const key
    );
}
