export module iris.compiler.project;

import std;

import iris.common;

namespace iris::compiler
{
    export struct Project_dependency
    {
        std::pmr::string name;
        std::pmr::string version;
        std::pmr::string source_url;
        std::pmr::vector<std::pmr::string> build_commands;
        std::optional<std::pmr::string> install_path;
    };

    export struct Iris_project
    {
        std::filesystem::path file_path;
        std::pmr::string name;
        std::pmr::string version;
        std::pmr::vector<Project_dependency> dependencies;
        std::pmr::string dependencies_storage_path;
        std::pmr::string dependencies_build_path;
    };

    export Iris_project get_iris_project(std::filesystem::path const& project_file_path);
    export std::optional<Iris_project> try_get_iris_project(std::filesystem::path const& project_file_path);
}
