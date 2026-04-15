export module iris.compiler.repository;

import std;

namespace iris::compiler
{
    export struct Repository
    {
        std::filesystem::path file_path;
        std::pmr::string name;
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> artifact_to_location;
    };

    export Repository get_repository(std::filesystem::path const& repository_file_path);
    export std::pmr::vector<Repository> get_repositories(std::span<std::filesystem::path const> repository_file_paths);

    export std::optional<std::filesystem::path> get_artifact_location(std::span<Repository const> repositories, std::string_view artifact_name);
}
