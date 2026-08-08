export module iris.common.archive;

import std;

namespace iris::common
{
    // Extracts an archive into destination_directory, stripping the archive's
    // leading directory component. Supports .7z and .zip.
    // Returns std::nullopt on success, an error message otherwise.
    export std::optional<std::pmr::string> extract_archive(
        std::filesystem::path const& archive_path,
        std::filesystem::path const& destination_directory
    );

    // Creates an archive containing source_directory's contents, placed under a
    // single root directory named after output_archive_path's stem. The archive
    // format is chosen from output_archive_path's extension (.7z or .zip).
    // Returns std::nullopt on success, an error message otherwise.
    export std::optional<std::pmr::string> create_archive_from_directory(
        std::filesystem::path const& source_directory,
        std::filesystem::path const& output_archive_path
    );
}
