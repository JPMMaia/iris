module;

#include <filesystem>
#include <optional>
#include <span>
#include <string_view>

module h.common.filesystem_common;

import h.common.filesystem;

namespace h::common
{
    std::optional<std::filesystem::path> search_file(
        std::string_view const filename,
        std::span<std::filesystem::path const> const search_paths
    )
    {
        {
            std::filesystem::path const file_path = filename;
            if (file_path.is_absolute())
                return file_path;
        }

        for (std::filesystem::path const& search_path : search_paths)
        {
            for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator{ search_path })
            {
                if (entry.path().filename() == filename)
                {
                    return entry.path();
                }
            }
        }

        return std::nullopt;
    }

    std::pmr::vector<std::filesystem::path> search_files(
        std::filesystem::path const& root_directory,
        std::string_view const filename,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<std::filesystem::path> found_files{temporaries_allocator};

        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator{ root_directory })
        {
            if (entry.is_regular_file())
            {
                std::filesystem::path const& entry_path = entry.path();
                std::filesystem::path const filename_path = entry_path.filename();

                if (filename_path.generic_string() == filename)
                {
                    found_files.push_back(entry_path);
                }
            }
        }

        return std::pmr::vector<std::filesystem::path>{std::move(found_files), output_allocator};
    }

    std::filesystem::path get_share_path(std::filesystem::path const& relative_path)
    {
        std::filesystem::path const current_directory_include_path = std::filesystem::current_path().parent_path() / "share" / "hlang" / relative_path;
        if (std::filesystem::exists(current_directory_include_path))
            return current_directory_include_path;
        std::filesystem::path const executable_directory_include_path = get_executable_directory().parent_path() / "share" / "hlang" / relative_path;
        return executable_directory_include_path;
    }

    std::filesystem::path get_builtin_include_directory()
    {
        return get_share_path("include");
    }

    std::filesystem::path get_tests_main_file_path()
    {
        return get_share_path("source/tests_main.cpp");
    }

    std::filesystem::path get_standard_repository_file_path()
    {
        return get_share_path("libraries/hlang_repository.json");
    }
}
