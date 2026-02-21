module;

#include <filesystem>
#include <memory_resource>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

export module h.compiler.builder;

import h.core;
import h.core.declarations;
import h.compiler;
import h.compiler.artifact;
import h.compiler.profiler;
import h.compiler.repository;
import h.compiler.target;

namespace h::compiler
{
    export struct Builder_options
    {
        bool output_llvm_ir = false;
        bool is_test_mode = false;
    };

    export struct Builder
    {
        h::compiler::Target target;
        std::filesystem::path build_directory_path;
        std::pmr::vector<std::filesystem::path> header_search_paths;
        std::pmr::vector<h::compiler::Repository> repositories;
        h::compiler::Compilation_options compilation_options;
        Profiler profiler;
        bool use_profiler = true;
        bool output_module_json = false;
        bool output_llvm_ir = false;
        bool is_test_mode = false;
    };

    export Builder create_builder(
        h::compiler::Target const& target,
        std::filesystem::path const& build_directory_path,
        std::span<std::filesystem::path const> header_search_paths,
        std::span<std::filesystem::path const> repository_paths,
        h::compiler::Compilation_options const& compilation_options,
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
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export struct Modules_and_declaration_database
    {
        std::pmr::vector<h::Module> header_modules;
        std::pmr::vector<h::Module const*> sorted_modules;
        Declaration_database declaration_database;
    };

    export Modules_and_declaration_database import_and_export_c_headers(
        Builder& builder,
        std::span<Artifact const> const artifacts,
        std::span<h::Module> const core_modules,
        bool const force_allow_errors,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void validate_modules_and_exit_if_needed(
        std::span<h::Module> const core_modules,
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

    export std::pmr::vector<h::Module> parse_source_files_and_cache(
        Builder& builder,
        std::span<std::filesystem::path const> const source_file_paths,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> create_module_name_to_file_path_map(
        Builder const& builder,
        std::span<h::Module const> const header_modules,
        std::span<h::Module const> const core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void compile_and_write_to_bitcode_files(
        Builder& builder,
        std::span<h::Module const> const core_modules,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& module_name_to_file_path_map,
        LLVM_data& llvm_data,
        Compilation_database& compilation_database,
        Compilation_options const& compilation_options,
        bool const is_test_mode
    );

    std::pmr::vector<h::Module> create_test_artifact_modules(
        Builder& builder,
        std::span<std::filesystem::path const> const artifact_file_paths,
        std::span<Artifact const> const artifacts,
        std::span<h::Module const> const core_modules,
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
        h::compiler::Compilation_options const& compilation_options,
        std::filesystem::path const output_file_path
    );

    std::pmr::vector<h::compiler::Artifact const*> get_artifact_pointers(
        std::span<h::compiler::Artifact const> const artifacts,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    std::pmr::vector<h::compiler::Artifact const*> filter_test_artifacts(
        std::span<std::filesystem::path const> const artifact_file_paths,
        std::span<h::compiler::Artifact const> const artifacts,
        std::span<h::Module const> const core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );
}
