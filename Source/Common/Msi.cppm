export module iris.common.msi;

import std;

namespace iris::common
{
    export struct Msi_shortcut
    {
        // File the shortcut points at, relative to Msi_package_options::source_directory.
        std::filesystem::path target;
        std::pmr::string name;
    };

    export struct Msi_package_options
    {
        // Everything under this directory is installed into
        // Program Files/<manufacturer>/<product_name>, keeping its layout.
        std::filesystem::path source_directory;
        std::filesystem::path output_path;
        std::pmr::string product_name;
        std::pmr::string manufacturer;
        // Braced, upper-case GUID that stays the same across versions.
        std::pmr::string upgrade_code;
        // "major.minor.build" with major and minor at most 255 and build at most 65535.
        std::pmr::string product_version;
        // Windows Installer platform: "x64" or "Arm64".
        std::pmr::string architecture;
        // Directory relative to source_directory to append to the system PATH.
        std::optional<std::filesystem::path> path_directory;
        // Start menu shortcuts, placed in a folder named after the product.
        std::pmr::vector<Msi_shortcut> shortcuts;
    };

    // Writes a per-machine Windows Installer package that installs source_directory. Installing
    // a newer package replaces any older one with the same upgrade code; installing an older one
    // is refused. Returns std::nullopt on success, an error message otherwise.
    export std::optional<std::pmr::string> create_msi_from_directory(Msi_package_options const& options);

    // Returns the name-based (version 5) GUID of name in the namespace namespace_guid, braced and
    // upper-case. The package uses it for component GUIDs so they stay stable across versions.
    // Returns std::nullopt when namespace_guid is not a braced GUID.
    export std::optional<std::pmr::string> create_name_based_guid(
        std::string_view namespace_guid,
        std::string_view name
    );
}
