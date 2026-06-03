module;

#include <nlohmann/json.hpp>

module iris.compiler.project;

import std;

import iris.common;

namespace iris::compiler
{
    static std::pmr::vector<std::pmr::string> parse_string_array(
        nlohmann::json const& json,
        std::string_view const key
    )
    {
        if (!json.contains(key))
            return {};

        nlohmann::json const& value = json.at(key);
        if (!value.is_array())
            throw std::runtime_error(std::format("'{}' must be an array.", key));

        std::pmr::vector<std::pmr::string> result;
        result.reserve(value.size());

        for (nlohmann::json const& element : value)
        {
            if (!element.is_string())
                throw std::runtime_error(std::format("'{}' must contain only strings.", key));
            result.push_back(element.get<std::pmr::string>());
        }

        return result;
    }

    static Project_dependency parse_dependency(
        nlohmann::json const& json
    )
    {
        Project_dependency dependency;

        if (!json.contains("name") || !json.at("name").is_string())
            throw std::runtime_error("Project_dependency must contain 'name' string.");
        dependency.name = json.at("name").get<std::pmr::string>();

        if (!json.contains("version") || !json.at("version").is_string())
            throw std::runtime_error("Project_dependency must contain 'version' string.");
        dependency.version = json.at("version").get<std::pmr::string>();

        if (!json.contains("source_url") || !json.at("source_url").is_string())
            throw std::runtime_error("Project_dependency must contain 'source_url' string.");
        dependency.source_url = json.at("source_url").get<std::pmr::string>();

        dependency.build_commands = parse_string_array(json, "build_commands");

        if (json.contains("install_path"))
        {
            if (!json.at("install_path").is_string())
                throw std::runtime_error("'install_path' must be a string.");
            dependency.install_path = json.at("install_path").get<std::pmr::string>();
        }
        else
        {
            dependency.install_path = "install";
        }

        return dependency;
    }

    std::optional<Iris_project> try_get_iris_project(std::filesystem::path const& project_file_path)
    {
        if (!std::filesystem::exists(project_file_path))
            return std::nullopt;

        std::optional<std::pmr::string> const json_data = iris::common::get_file_contents(project_file_path);
        if (!json_data.has_value())
            throw std::runtime_error(std::format("Failed to read contents of {}", project_file_path.generic_string()));

        nlohmann::json const root = nlohmann::json::parse(json_data.value());
        if (!root.is_object())
            throw std::runtime_error("iris_project.json root must be a JSON object.");

        std::filesystem::path const project_directory = project_file_path.parent_path();

        Iris_project project;
        project.file_path = project_file_path;

        if (!root.contains("name") || !root.at("name").is_string())
            throw std::runtime_error("iris_project.json must contain 'name' string.");
        project.name = root.at("name").get<std::pmr::string>();

        if (!root.contains("version") || !root.at("version").is_string())
            throw std::runtime_error("iris_project.json must contain 'version' string.");
        project.version = root.at("version").get<std::pmr::string>();

        if (!root.contains("dependencies_storage_path") || !root.at("dependencies_storage_path").is_string())
            throw std::runtime_error("iris_project.json must contain 'dependencies_storage_path' string.");
        project.dependencies_storage_path = root.at("dependencies_storage_path").get<std::pmr::string>();

        if (!root.contains("dependencies_build_path") || !root.at("dependencies_build_path").is_string())
            throw std::runtime_error("iris_project.json must contain 'dependencies_build_path' string.");
        project.dependencies_build_path = root.at("dependencies_build_path").get<std::pmr::string>();

        if (root.contains("dependencies"))
        {
            nlohmann::json const& dependencies_json = root.at("dependencies");
            if (!dependencies_json.is_array())
                throw std::runtime_error("'dependencies' must be an array.");

            for (nlohmann::json const& dependency_json : dependencies_json)
            {
                project.dependencies.push_back(parse_dependency(dependency_json));
            }
        }

        return project;
    }

    Iris_project get_iris_project(std::filesystem::path const& project_file_path)
    {
        auto const result = try_get_iris_project(project_file_path);
        if (!result.has_value())
            iris::common::print_message_and_exit(std::format("Failed to find or read {}", project_file_path.generic_string()));
        return result.value();
    }
}
