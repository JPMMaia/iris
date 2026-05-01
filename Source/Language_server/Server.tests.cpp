#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_all.hpp>
#include <lsp/types.h>

import iris.common;
import iris.compiler;
import iris.language_server.server;

namespace iris::language_server
{
    namespace
    {
        static std::filesystem::path create_clean_temporary_directory(std::string_view const name)
        {
            std::filesystem::path const path = std::filesystem::temp_directory_path() / name;
            std::filesystem::remove_all(path);
            std::filesystem::create_directories(path);
            return path;
        }
    }

    TEST_CASE("Workspace initialization reads repositories and environment variables from presets", "[Language_server][Server]")
    {
        std::filesystem::path const workspace_directory = create_clean_temporary_directory("iris_language_server_presets_workspace");
        std::filesystem::path const dependency_directory = workspace_directory / "dependency";
        std::filesystem::path const build_directory = workspace_directory / "preset_build";
        std::filesystem::create_directories(dependency_directory);

        iris::common::write_to_file(
            workspace_directory / "iris_repository.json",
            R"({
    "name": "Test_repository",
    "artifacts": [
        {
            "name": "dependency_library",
            "location": "dependency/iris_artifact.json"
        }
    ]
})"
        );

        iris::common::write_to_file(
            dependency_directory / "iris_artifact.json",
            R"({
    "name": "dependency_library",
    "version": "0.1.0",
    "type": "library",
    "sources": [
        {
            "type": "iris",
            "include": [
                "./dep.iris"
            ]
        }
    ]
})"
        );

        iris::common::write_to_file(
            dependency_directory / "dep.iris",
            R"(module dependency_library;

export function dep() -> ()
{
}
)"
        );

        iris::common::write_to_file(
            workspace_directory / "iris_artifact.json",
            R"({
    "name": "app",
    "version": "0.1.0",
    "type": "executable",
    "dependencies": [
        {
            "name": "dependency_library"
        }
    ],
    "sources": [
        {
            "type": "iris",
            "include": [
                "./main.iris"
            ]
        }
    ],
    "executable": {
        "source": "${PROJECT_ROOT}/main.iris"
    }
})"
        );

        iris::common::write_to_file(
            workspace_directory / "main.iris",
            R"(module app;

import dependency_library;

@unique_name("main")
export function main() -> (result: Int32)
{
    dependency_library.dep();
    return 0;
}
)"
        );

        iris::common::write_to_file(
            workspace_directory / "iris_presets.json",
            std::format(
                R"({{
    "build_directory": "preset_build",
    "repository_paths": [
        "iris_repository.json"
    ],
    "function_contracts": "disabled",
    "output_llvm_ir": true,
    "environment_variables": {{
        "PROJECT_ROOT": "{}"
    }}
}})",
                workspace_directory.generic_string()
            )
        );

        std::vector<std::string> shown_messages;
        Server server = create_server(
            Server_logger{
                .window_log_message = [](lsp::LogMessageParams&&) {},
                .window_show_message = [&](lsp::ShowMessageParams&& parameters)
                {
                    shown_messages.push_back(parameters.message);
                },
            }
        );

        lsp::WorkspaceFolder const workspace_folder{
            .uri = lsp::DocumentUri::fromPath(workspace_directory.generic_string()),
            .name = "workspace",
        };
        set_workspace_folders(server, {&workspace_folder, 1});

        lsp::Workspace_ConfigurationResult configurations;
        configurations.push_back(lsp::json::Object{});
        set_workspace_folder_configurations(server, configurations);

        REQUIRE(shown_messages.empty());
        REQUIRE(server.workspaces_data.size() == 1);

        auto const& workspace_data = server.workspaces_data.front();
        CHECK(workspace_data.builder.build_directory_path == build_directory.lexically_normal());
        CHECK(workspace_data.builder.compilation_options.contract_options == iris::compiler::Contract_options::Disabled);
        CHECK(workspace_data.builder.output_llvm_ir);
        REQUIRE(workspace_data.builder.environment_variables.contains("PROJECT_ROOT"));
        std::string const actual_project_root = workspace_data.builder.environment_variables.at("PROJECT_ROOT").c_str();
        std::string const expected_project_root = workspace_directory.generic_string();
        CHECK(actual_project_root == expected_project_root);
        CHECK(workspace_data.builder.repositories.size() == 1);
        CHECK(
            std::ranges::find(
                workspace_data.core_module_source_file_paths,
                (workspace_directory / "main.iris").lexically_normal()
            ) != workspace_data.core_module_source_file_paths.end()
        );
        CHECK(
            std::ranges::find(
                workspace_data.core_module_source_file_paths,
                (dependency_directory / "dep.iris").lexically_normal()
            ) != workspace_data.core_module_source_file_paths.end()
        );

        destroy_server(server);
    }
}
