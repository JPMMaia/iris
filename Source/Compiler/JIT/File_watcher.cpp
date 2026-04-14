module;

#include <wtr/watcher.hpp>

#include <memory_resource>

module h.compiler.file_watcher;

import std;

import h.compiler.artifact;
import h.compiler.repository;

namespace h::compiler
{
    static std::pmr::vector<std::filesystem::path> find_artifact_directories_to_watch(
        Artifact const& artifact,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<std::filesystem::path> directories_to_watch = find_root_include_directories(artifact, temporaries_allocator, temporaries_allocator);

        // Remove all subdirectories and leave only "root directories"
        for (std::size_t index = directories_to_watch.size(); index > 1; --index)
        {
            std::filesystem::path const& path = directories_to_watch[index - 1];
            std::filesystem::path const canonical_path = std::filesystem::canonical(path);

            for (std::size_t other_index = 0; other_index < directories_to_watch.size(); ++other_index)
            {
                // Don't compare with itself:
                if (other_index == index)
                    continue;

                std::filesystem::path const& other_path = directories_to_watch[other_index];
                std::filesystem::path const canonical_other_path = std::filesystem::canonical(other_path);

                auto const iterator = std::search(path.begin(), path.end(), other_path.begin(), other_path.end());
                bool const path_is_subdirectory = iterator == path.begin();

                if (path_is_subdirectory)
                {
                    directories_to_watch.erase(directories_to_watch.begin() + index - 1);
                    break;
                }
            }
        }

        return directories_to_watch;
    }

    std::unique_ptr<File_watcher> create_file_watcher(
        std::function<void(wtr::watcher::event const&)> callback
    )
    {
        std::unique_ptr<File_watcher> file_watcher = std::make_unique<File_watcher>();
        file_watcher->callback = std::move(callback);
        return file_watcher;
    }

    void watch_artifact_directories(
        File_watcher& file_watcher,
        Artifact const& artifact
    )
    {
        std::pmr::monotonic_buffer_resource temporaries_buffer_resource;
        std::pmr::polymorphic_allocator<> temporaries_allocator{ &temporaries_buffer_resource };

        std::pmr::vector<std::filesystem::path> const directories_to_watch = find_artifact_directories_to_watch(artifact, temporaries_allocator);

        for (std::filesystem::path const& path : directories_to_watch)
        {
            wtr::watcher::watch* wtr_watcher = new wtr::watcher::watch(path, file_watcher.callback);
            file_watcher.wtr_watchers.push_back(wtr_watcher);
        }
    }

    void watch_repository_directories(
        File_watcher& file_watcher,
        std::span<std::filesystem::path const> const repositories_file_paths
    )
    {
        for (std::filesystem::path const& repository_file_path : repositories_file_paths)
        {
            wtr::watcher::watch* wtr_watcher = new wtr::watcher::watch(repository_file_path.parent_path(), file_watcher.callback);
            file_watcher.wtr_watchers.push_back(wtr_watcher);
        }
    }

    File_watcher::~File_watcher()
    {
        for (void* wtr_watcher : this->wtr_watchers)
        {
            delete static_cast<wtr::watcher::watch*>(wtr_watcher);
        }
    }
}
