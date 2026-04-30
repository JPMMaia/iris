export module iris.compiler.builder;

import std;

import iris.core;
import iris.core.declarations;
import iris.compiler;
import iris.compiler.artifact;
import iris.compiler.profiler;
import iris.compiler.repository;
import iris.compiler.target;

namespace iris::compiler
{
    export struct Builder_options
    {
        bool output_llvm_ir = false;
        bool is_test_mode = false;
    };

    export struct Builder
    {
        iris::compiler::Target target;
        std::filesystem::path build_directory_path;
        std::pmr::vector<std::filesystem::path> header_search_paths;
        std::pmr::vector<iris::compiler::Repository> repositories;
        iris::compiler::Compilation_options compilation_options;
        Profiler profiler;
        bool use_profiler = true;
        bool output_module_json = false;
        bool output_llvm_ir = false;
        bool is_test_mode = false;
    };

    export Builder create_builder(
        iris::compiler::Target const& target,
        std::filesystem::path const& build_directory_path,
        std::span<std::filesystem::path const> header_search_paths,
        std::span<std::filesystem::path const> repository_paths,
        iris::compiler::Compilation_options const& compilation_options,
        Builder_options const& builder_options,
        std::pmr::polymorphic_allocator<> output_allocator
    );

    export void build_artifact(
        Builder& builder,
        std::filesystem::path const& artifact_file_path
    );

    export void build_artifacts(
        Builder& builder,
        std::span<std::filesystem::path const> const artifact_file_paths
    );

    export std::pmr::vector<Artifact> get_sorted_artifacts(
        std::span<std::filesystem::path const> const artifact_file_paths,
        std::span<Repository const> repositories,
        bool const is_test_mode,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<std::filesystem::path> get_artifacts_source_files(
        std::span<Artifact const> const artifacts,
        bool const is_test_mode,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export struct Modules_and_declaration_database
    {
        std::pmr::vector<iris::Module> header_modules;
        std::pmr::vector<iris::Module const*> sorted_modules;
        Declaration_database declaration_database;
    };

    export Modules_and_declaration_database import_and_export_c_headers(
        Builder& builder,
        std::span<Artifact const> const artifacts,
        std::span<iris::Module> const core_modules,
        bool const force_allow_errors,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void validate_modules_and_exit_if_needed(
        std::span<iris::Module> const core_modules,
        Declaration_database& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export bool compile_cpp_and_write_to_bitcode_files(
        Builder& builder,
        std::span<Artifact const> const artifacts,
        LLVM_data& llvm_data,
        Compilation_options const& compilation_options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<iris::Module> parse_source_files_and_cache(
        Builder& builder,
        std::span<std::filesystem::path const> const source_file_paths,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> create_module_name_to_file_path_map(
        Builder const& builder,
        std::span<iris::Module const> const header_modules,
        std::span<iris::Module const> const core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void compile_and_write_to_bitcode_files(
        Builder& builder,
        std::span<iris::Module const> const core_modules,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& module_name_to_file_path_map,
        LLVM_data& llvm_data,
        Declaration_database const& declaration_database,
        Compilation_options const& compilation_options,
        bool const is_test_mode
    );

    std::pmr::vector<iris::Module> create_test_artifact_modules(
        Builder& builder,
        std::span<std::filesystem::path const> const artifact_file_paths,
        std::span<Artifact const> const artifacts,
        std::span<iris::Module const> const core_modules,
        Compilation_options const& compilation_options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void link_artifacts(
        Builder& builder,
        std::span<Artifact const> const artifacts,
        std::span<Artifact const* const> const artifacts_to_link,
        bool const debug,
        bool const is_test_mode,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void copy_dlls(
        Builder const& builder,
        std::span<Artifact const> const artifacts,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    bool is_file_newer_than(
        std::filesystem::path const& first,
        std::filesystem::path const& second
    );

    export void print_struct_layout(
        std::filesystem::path const input_file_path,
        std::string_view const struct_name,
        std::optional<std::string_view> const target_triple
    );

    export void write_compile_commands_json_to_file(
        Builder const& builder,
        std::filesystem::path const& artifact_file_path,
        iris::compiler::Compilation_options const& compilation_options,
        std::filesystem::path const output_file_path
    );

    std::pmr::vector<iris::compiler::Artifact const*> get_artifact_pointers(
        std::span<iris::compiler::Artifact const> const artifacts,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    std::pmr::vector<iris::compiler::Artifact const*> filter_test_artifacts(
        std::span<std::filesystem::path const> const artifact_file_paths,
        std::span<iris::compiler::Artifact const> const artifacts,
        std::span<iris::Module const> const core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export std::pmr::vector<std::filesystem::path> find_artifact_file_paths(
        std::filesystem::path const& path,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );
}
