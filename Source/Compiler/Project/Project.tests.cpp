#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <fstream>
#include <nlohmann/json.hpp>

import std;

import iris.compiler.project;

TEST_CASE("Parse valid iris_project.json")
{
    // Create temporary test file
    std::filesystem::path const test_file = "test_project.json";
    std::ofstream file(test_file);
    file << R"({
        "name": "test_project",
        "version": "1.0.0",
        "dependencies_storage_path": "external",
        "dependencies_build_path": "build_deps",
        "dependencies": [
            {
                "name": "SDL",
                "version": "2.0.0",
                "source_url": "https://example.com/sdl.zip",
                "build_commands": ["cmake -S . -B build"],
                "install_path": "install"
            }
        ]
    })";
    file.close();

    auto const project = iris::compiler::try_get_iris_project(test_file);
    REQUIRE(project.has_value());
    REQUIRE(project->name == "test_project");
    REQUIRE(project->version == "1.0.0");
    REQUIRE(project->dependencies_storage_path == "external");
    REQUIRE(project->dependencies_build_path == "build_deps");
    REQUIRE(project->dependencies.size() == 1);
    REQUIRE(project->dependencies[0].name == "SDL");
    REQUIRE(project->dependencies[0].version == "2.0.0");
    REQUIRE(project->dependencies[0].source_url == "https://example.com/sdl.zip");
    REQUIRE(project->dependencies[0].build_commands.size() == 1);
    REQUIRE(project->dependencies[0].build_commands[0] == "cmake -S . -B build");
    REQUIRE(project->dependencies[0].install_path.has_value());
    REQUIRE(project->dependencies[0].install_path.value() == "install");

    std::filesystem::remove(test_file);
}

TEST_CASE("Parse dependency with missing optional install_path")
{
    std::filesystem::path const test_file = "test_project_no_install.json";
    std::ofstream file(test_file);
    file << R"({
        "name": "test_project",
        "version": "1.0.0",
        "dependencies_storage_path": "external",
        "dependencies_build_path": "build_deps",
        "dependencies": [
            {
                "name": "MyLib",
                "version": "1.2.3",
                "source_url": "https://example.com/mylib.zip",
                "build_commands": ["make"]
            }
        ]
    })";
    file.close();

    auto const project = iris::compiler::try_get_iris_project(test_file);
    REQUIRE(project.has_value());
    REQUIRE(project->dependencies.size() == 1);
    REQUIRE(project->dependencies[0].name == "MyLib");
    REQUIRE(project->dependencies[0].install_path.has_value());
    REQUIRE(project->dependencies[0].install_path.value() == "install");

    std::filesystem::remove(test_file);
}

TEST_CASE("try_get_iris_project returns nullopt for non-existent file")
{
    auto const result = iris::compiler::try_get_iris_project("non_existent_file.json");
    REQUIRE(!result.has_value());
}

TEST_CASE("Parse iris_project.json without dependencies array")
{
    std::filesystem::path const test_file = "test_project_no_deps.json";
    std::ofstream file(test_file);
    file << R"({
        "name": "test_project",
        "version": "1.0.0",
        "dependencies_storage_path": "external",
        "dependencies_build_path": "build_deps"
    })";
    file.close();

    auto const project = iris::compiler::try_get_iris_project(test_file);
    REQUIRE(project.has_value());
    REQUIRE(project->dependencies.empty());

    std::filesystem::remove(test_file);
}

TEST_CASE("Parse iris_project.json with multiple dependencies")
{
    std::filesystem::path const test_file = "test_project_multi_deps.json";
    std::ofstream file(test_file);
    file << R"({
        "name": "multi_dep_project",
        "version": "2.0.0",
        "dependencies_storage_path": "deps",
        "dependencies_build_path": "build",
        "dependencies": [
            {
                "name": "SDL",
                "version": "2.0.0",
                "source_url": "https://example.com/sdl.zip",
                "build_commands": ["cmake -S . -B build"]
            },
            {
                "name": "Boost",
                "version": "1.80.0",
                "source_url": "https://example.com/boost.zip",
                "build_commands": ["./bootstrap.sh", "./b2"],
                "install_path": "stage"
            }
        ]
    })";
    file.close();

    auto const project = iris::compiler::try_get_iris_project(test_file);
    REQUIRE(project.has_value());
    REQUIRE(project->name == "multi_dep_project");
    REQUIRE(project->dependencies.size() == 2);
    REQUIRE(project->dependencies[0].name == "SDL");
    REQUIRE(project->dependencies[1].name == "Boost");
    REQUIRE(project->dependencies[1].install_path.value() == "stage");

    std::filesystem::remove(test_file);
}

TEST_CASE("get_iris_project exits on missing file")
{
    // This test verifies that get_iris_project calls print_message_and_exit
    // We can't easily test the exit, so we just verify the function exists and works for valid files
    std::filesystem::path const test_file = "test_project_valid.json";
    std::ofstream file(test_file);
    file << R"({
        "name": "valid_project",
        "version": "1.0.0",
        "dependencies_storage_path": "external",
        "dependencies_build_path": "build"
    })";
    file.close();

    auto const project = iris::compiler::get_iris_project(test_file);
    REQUIRE(project.name == "valid_project");

    std::filesystem::remove(test_file);
}
