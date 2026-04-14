module;

#include <wtr/watcher.hpp>

export module h.compiler.file_watcher;

import std;

import h.compiler.artifact;
import h.compiler.repository;

using namespace wtr::watcher;

namespace h::compiler
{
    using Wtr_watcher_deleter = void(void*);

    export struct File_watcher
    {
        wtr::event::callback callback;

        // TODO using void* because MSVC can't compile this:
        // std::pmr::vector<std::unique_ptr<wtd::watch::watcher>> wtr_watchers;
        std::pmr::vector<void*> wtr_watchers;

        ~File_watcher();
    };

    export std::unique_ptr<File_watcher> create_file_watcher(
        std::function<void(wtr::watcher::event const&)> callback
    );

    export void watch_artifact_directories(
        File_watcher& file_watcher,
        Artifact const& artifact
    );

    export void watch_repository_directories(
        File_watcher& file_watcher,
        std::span<std::filesystem::path const> repositories_file_paths
    );
}
