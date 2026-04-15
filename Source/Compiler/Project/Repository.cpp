module;

#include <nlohmann/json.hpp>

module iris.compiler.repository;

import std;

import iris.common;
import iris.compiler.common;

namespace iris::compiler
{
    Repository get_repository(std::filesystem::path const& repository_file_path)
    {
        std::optional<std::pmr::string> const json_data = iris::common::get_file_contents(repository_file_path.c_str());
        if (!json_data.has_value())
            iris::common::print_message_and_exit(std::format("Failed to read contents of {}", repository_file_path.generic_string()));

        nlohmann::json const json = nlohmann::json::parse(json_data.value());

        std::pmr::string name = json.at("name").get<std::string>().c_str();

        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> artifact_to_location;

        for (nlohmann::json const& element : json.at("artifacts"))
        {
            std::string const& artifact_name = element.at("name").get<std::string>();

            std::filesystem::path const artifact_relative_location = element.at("location").get<std::string>();
            std::filesystem::path artifact_location = repository_file_path.parent_path() / artifact_relative_location;

            artifact_to_location.insert(std::make_pair(artifact_name, std::move(artifact_location)));
        }

        return Repository
        {
            .file_path = repository_file_path,
            .name = std::move(name),
            .artifact_to_location = std::move(artifact_to_location)
        };
    }

    std::pmr::vector<Repository> get_repositories(std::span<std::filesystem::path const> repository_file_paths)
    {
        std::pmr::vector<Repository> repositories;
        repositories.reserve(repository_file_paths.size());

        for (std::filesystem::path const& path : repository_file_paths)
        {
            Repository repository = get_repository(path);
            repositories.push_back(std::move(repository));
        }

        return repositories;
    }

    std::optional<std::filesystem::path> get_artifact_location(std::span<Repository const> const repositories, std::string_view const artifact_name)
    {
        for (Repository const& repository : repositories)
        {
            auto const location = repository.artifact_to_location.find(artifact_name.data());
            if (location != repository.artifact_to_location.end())
                return location->second;
        }

        return std::nullopt;
    }
}
