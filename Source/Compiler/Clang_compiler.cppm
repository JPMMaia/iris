export module iris.compiler.clang_compiler;

import std;

import iris.compiler.clang_data;

namespace iris::compiler
{
    export std::filesystem::path find_clang(bool const use_clang_cl);

    export bool compile_cpp(
        Clang_data const& clang_data,
        std::string_view const target_triple,
        std::filesystem::path const& source_file_path,
        std::filesystem::path const& output_file_path,
        std::optional<std::filesystem::path> const output_dependency_file_path,
        std::filesystem::path const& build_artifacts_directory,
        std::span<std::pmr::string const> const include_directories,
        std::span<std::pmr::string const> const additional_flags,
        bool const use_clang_cl,
        bool const debug,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<std::pmr::string> create_compile_cpp_arguments(
        std::filesystem::path const& clang_path,
        std::filesystem::path const& source_file_path,
        std::filesystem::path const& output_file_path,
        std::optional<std::filesystem::path> const output_dependency_file_path,
        std::filesystem::path const& build_artifacts_directory,
        std::span<std::pmr::string const> const include_directories,
        std::span<std::pmr::string const> const additional_flags,
        bool const use_clang_cl,
        bool const debug,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );
}
