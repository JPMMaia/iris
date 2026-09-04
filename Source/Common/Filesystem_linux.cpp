module;

#include <filesystem>
#include <memory_resource>
#include <string>
#include <string_view>

module iris.common.filesystem;

import iris.common;

namespace iris::common
{
    std::filesystem::path get_executable_path()
    {
        std::error_code error_code;
        std::filesystem::path const executable_path = std::filesystem::read_symlink("/proc/self/exe", error_code);
        if (error_code)
            return std::filesystem::path{};

        return executable_path;
    }

    std::filesystem::path get_executable_directory()
    {
        return get_executable_path().parent_path();
    }

    std::pmr::vector<std::filesystem::path> get_default_header_search_directories()
    {
        std::pmr::vector<std::filesystem::path> include_directories
        {
            "/usr/include"
        };

        return include_directories;
    }

    std::pmr::vector<std::filesystem::path> get_default_library_directories()
    {
        std::pmr::vector<std::filesystem::path> library_directories
        {
            "/usr/lib"
        };

        return library_directories;
    }
}
