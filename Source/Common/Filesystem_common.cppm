export module iris.common.filesystem_common;

import std;

namespace iris::common
{
    export std::optional<std::filesystem::path> search_file(
        std::string_view const filename,
        std::span<std::filesystem::path const> const search_paths
    );

    export std::pmr::vector<std::filesystem::path> search_files(
        std::filesystem::path const& root_directory,
        std::string_view const filename,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export std::filesystem::path get_builtin_include_directory();

    export std::filesystem::path get_builtin_module_file_path();

    export std::filesystem::path get_json_module_file_path();

    export std::filesystem::path get_tests_main_file_path();

    export std::filesystem::path get_standard_repository_file_path();

    export std::filesystem::path get_visualizers_file_path();

    export std::optional<std::pmr::string> download_file(
        std::string_view const url,
        std::filesystem::path const& destination_path
    );

    export bool extract_zip(
        std::filesystem::path const& archive_path,
        std::filesystem::path const& destination_directory
    );

    export int execute_command(
        std::filesystem::path const& working_directory,
        std::string_view const command
    );
}
