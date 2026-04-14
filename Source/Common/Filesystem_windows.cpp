module;

#include <windows.h>

module h.common.filesystem;

import std;

import h.common;

namespace h::common
{
    std::filesystem::path get_executable_directory()
    {
        std::pmr::string buffer;
        buffer.resize(1024);

        int const bytes = GetModuleFileName(nullptr, buffer.data(), buffer.size());

        std::filesystem::path const executable_path = std::string_view{ buffer.data(), buffer.data() + bytes + 1 };
        return executable_path.parent_path();
    }

    static std::filesystem::path find_default_windows_kit_subdirectory_path(
        std::string_view const subdirectory
    )
    {
        std::filesystem::path const windows_kit_root = "C:/Program Files (x86)/Windows Kits/10";
        std::filesystem::path const library_path = windows_kit_root / subdirectory;

        if (!std::filesystem::exists(library_path))
            h::common::print_message_and_exit(std::format("{} does not exist! Is Windows 10 Kit installed?", library_path.generic_string()));

        std::optional<std::filesystem::path> best_match = std::nullopt;
        for (std::filesystem::directory_entry const& entry : std::filesystem::directory_iterator{ library_path })
        {
            if (!best_match)
            {
                best_match = entry.path();
            }
            else
            {
                if (entry.path() > best_match)
                    best_match = entry.path();
            }
        }

        if (!best_match)
            h::common::print_message_and_exit(std::format("Could not find an Windows 10 Kit version in {}! Is Windows 10 Kit installed?", library_path.generic_string()));

        return *best_match / "ucrt";
    }

    std::pmr::vector<std::filesystem::path> get_default_header_search_directories()
    {
        std::filesystem::path const windows_kit_library_path = find_default_windows_kit_subdirectory_path("Include");

        std::pmr::vector<std::filesystem::path> include_directories
        {
            windows_kit_library_path
        };

        return include_directories;
    }

    std::pmr::vector<std::filesystem::path> get_default_library_directories()
    {
        std::pmr::vector<std::filesystem::path> library_directories
        {
            find_default_windows_kit_subdirectory_path("Lib") / "x64"
        };

        return library_directories;
    }
}
