#include <algorithm>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <catch2/catch_all.hpp>
#include <lsp/types.h>

import iris.common;
import iris.common.filesystem_common;
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

        struct Two_module_workspace
        {
            std::filesystem::path directory;
            std::filesystem::path main_file;
            std::filesystem::path dependency_file;
        };

        // Writes a workspace with an executable module 'app' (main.iris) that imports a
        // library module 'dependency_library' (dep.iris) and references
        // 'dependency_library.dep'. The contents of dep.iris are supplied so tests can
        // edit the dependency and observe whether the dependent's diagnostics refresh.
        static Two_module_workspace write_two_module_workspace(
            std::string_view const name,
            std::string_view const dependency_contents
        )
        {
            std::filesystem::path const workspace_directory = create_clean_temporary_directory(name);
            std::filesystem::path const dependency_directory = workspace_directory / "dependency";
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

            iris::common::write_to_file(dependency_directory / "dep.iris", dependency_contents);

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

import dependency_library as dependency_library;

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

            return Two_module_workspace{
                .directory = workspace_directory,
                .main_file = workspace_directory / "main.iris",
                .dependency_file = dependency_directory / "dep.iris",
            };
        }

        static Server create_configured_server(std::filesystem::path const& workspace_directory)
        {
            Server server = create_server(
                Server_logger{
                    .window_log_message = [](lsp::LogMessageParams&&) {},
                    .window_show_message = [](lsp::ShowMessageParams&&) {},
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

            return server;
        }

        static void open_document(Server& server, std::filesystem::path const& file, int const version, std::string_view const text)
        {
            lsp::DidOpenTextDocumentParams parameters;
            parameters.textDocument.uri = lsp::DocumentUri::fromPath(file.generic_string());
            parameters.textDocument.languageId = "iris";
            parameters.textDocument.version = version;
            parameters.textDocument.text = std::string{text};
            text_document_did_open(server, parameters);
        }

        static void change_document_full_text(Server& server, std::filesystem::path const& file, int const version, std::string_view const new_text)
        {
            lsp::DidChangeTextDocumentParams parameters;
            parameters.textDocument.uri = lsp::DocumentUri::fromPath(file.generic_string());
            parameters.textDocument.version = version;
            parameters.contentChanges.push_back(lsp::TextDocumentContentChangeEvent_Text{ .text = std::string{new_text} });
            text_document_did_change(server, parameters);
        }

        static lsp::DocumentDiagnosticReport pull_document_diagnostics(
            Server& server,
            std::filesystem::path const& file,
            std::optional<std::string> const& previous_result_id
        )
        {
            lsp::DocumentDiagnosticParams parameters;
            parameters.textDocument.uri = lsp::DocumentUri::fromPath(file.generic_string());
            if (previous_result_id.has_value())
                parameters.previousResultId = previous_result_id.value();
            return compute_document_diagnostics(server, parameters);
        }
    }

    static std::string_view const g_dependency_with_dep = R"(module dependency_library;

export function dep() -> ()
{
}
)";

    static std::string_view const g_main_text = R"(module app;

import dependency_library as dependency_library;

@unique_name("main")
export function main() -> (result: Int32)
{
    dependency_library.dep();
    return 0;
}
)";

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
        CHECK(workspace_data.builder.repositories.size() >= 2);
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

    TEST_CASE("Document diagnostics refresh when a dependency changes", "[Language_server][Server][Diagnostics]")
    {
        Two_module_workspace const workspace = write_two_module_workspace(
            "iris_language_server_diagnostics_dependency_change",
            g_dependency_with_dep
        );

        Server server = create_configured_server(workspace.directory);
        REQUIRE(server.workspaces_data.size() == 1);

        open_document(server, workspace.main_file, 1, g_main_text);

        // Baseline: 'app' is valid because 'dependency_library.dep' exists.
        lsp::DocumentDiagnosticReport const baseline = pull_document_diagnostics(server, workspace.main_file, std::nullopt);
        REQUIRE(std::holds_alternative<lsp::RelatedFullDocumentDiagnosticReport>(baseline));
        lsp::RelatedFullDocumentDiagnosticReport const& baseline_full = std::get<lsp::RelatedFullDocumentDiagnosticReport>(baseline);
        CHECK(baseline_full.items.empty());
        REQUIRE(baseline_full.resultId.has_value());
        std::string const baseline_result_id = baseline_full.resultId.value();

        // Edit the dependency so that 'dependency_library.dep' no longer exists.
        change_document_full_text(
            server,
            workspace.dependency_file,
            2,
            R"(module dependency_library;

export function dep_renamed() -> ()
{
}
)"
        );

        // 'app' itself was not edited, but its diagnostics must now report the missing declaration.
        lsp::DocumentDiagnosticReport const refreshed = pull_document_diagnostics(server, workspace.main_file, baseline_result_id);
        REQUIRE(std::holds_alternative<lsp::RelatedFullDocumentDiagnosticReport>(refreshed));
        lsp::RelatedFullDocumentDiagnosticReport const& refreshed_full = std::get<lsp::RelatedFullDocumentDiagnosticReport>(refreshed);
        CHECK_FALSE(refreshed_full.items.empty());

        destroy_server(server);
    }

    TEST_CASE("Workspace diagnostics refresh a dependent when its dependency changes", "[Language_server][Server][Diagnostics]")
    {
        Two_module_workspace const workspace = write_two_module_workspace(
            "iris_language_server_diagnostics_workspace_dependency_change",
            g_dependency_with_dep
        );

        Server server = create_configured_server(workspace.directory);
        REQUIRE(server.workspaces_data.size() == 1);

        open_document(server, workspace.main_file, 1, g_main_text);

        lsp::DocumentUri const main_uri = lsp::DocumentUri::fromPath(workspace.main_file.generic_string());

        auto const find_main_report = [&](lsp::WorkspaceDiagnosticReport const& report)
            -> std::optional<lsp::WorkspaceDocumentDiagnosticReport>
        {
            for (lsp::WorkspaceDocumentDiagnosticReport const& item : report.items)
            {
                lsp::DocumentUri const& uri = std::holds_alternative<lsp::WorkspaceFullDocumentDiagnosticReport>(item)
                    ? std::get<lsp::WorkspaceFullDocumentDiagnosticReport>(item).uri
                    : std::get<lsp::WorkspaceUnchangedDocumentDiagnosticReport>(item).uri;
                if (uri.path() == main_uri.path())
                    return item;
            }
            return std::nullopt;
        };

        // Baseline workspace pull establishes cached result ids.
        lsp::WorkspaceDiagnosticParams baseline_parameters;
        baseline_parameters.identifier = "iris";
        lsp::WorkspaceDiagnosticReport const baseline = compute_workspace_diagnostics(server, baseline_parameters);
        std::optional<lsp::WorkspaceDocumentDiagnosticReport> const baseline_main = find_main_report(baseline);
        REQUIRE(baseline_main.has_value());
        REQUIRE(std::holds_alternative<lsp::WorkspaceFullDocumentDiagnosticReport>(baseline_main.value()));
        lsp::WorkspaceFullDocumentDiagnosticReport const& baseline_main_full = std::get<lsp::WorkspaceFullDocumentDiagnosticReport>(baseline_main.value());
        CHECK(baseline_main_full.items.empty());

        // Provide the previous result ids on the follow-up pull so the server may answer "unchanged".
        lsp::WorkspaceDiagnosticParams refreshed_parameters;
        refreshed_parameters.identifier = "iris";
        for (lsp::WorkspaceDocumentDiagnosticReport const& item : baseline.items)
        {
            if (std::holds_alternative<lsp::WorkspaceFullDocumentDiagnosticReport>(item))
            {
                lsp::WorkspaceFullDocumentDiagnosticReport const& full = std::get<lsp::WorkspaceFullDocumentDiagnosticReport>(item);
                if (full.resultId.has_value())
                    refreshed_parameters.previousResultIds.push_back(lsp::PreviousResultId{ .uri = full.uri, .value = full.resultId.value() });
            }
        }

        change_document_full_text(
            server,
            workspace.dependency_file,
            2,
            R"(module dependency_library;

export function dep_renamed() -> ()
{
}
)"
        );

        lsp::WorkspaceDiagnosticReport const refreshed = compute_workspace_diagnostics(server, refreshed_parameters);
        std::optional<lsp::WorkspaceDocumentDiagnosticReport> const refreshed_main = find_main_report(refreshed);
        REQUIRE(refreshed_main.has_value());
        REQUIRE(std::holds_alternative<lsp::WorkspaceFullDocumentDiagnosticReport>(refreshed_main.value()));
        lsp::WorkspaceFullDocumentDiagnosticReport const& refreshed_main_full = std::get<lsp::WorkspaceFullDocumentDiagnosticReport>(refreshed_main.value());
        CHECK_FALSE(refreshed_main_full.items.empty());

        destroy_server(server);
    }

    TEST_CASE("Invalidate all diagnostics forces a fresh report", "[Language_server][Server][Diagnostics]")
    {
        Two_module_workspace const workspace = write_two_module_workspace(
            "iris_language_server_diagnostics_force_recompute",
            g_dependency_with_dep
        );

        Server server = create_configured_server(workspace.directory);
        REQUIRE(server.workspaces_data.size() == 1);

        open_document(server, workspace.main_file, 1, g_main_text);

        // First pull produces a full report and a result id.
        lsp::DocumentDiagnosticReport const first = pull_document_diagnostics(server, workspace.main_file, std::nullopt);
        REQUIRE(std::holds_alternative<lsp::RelatedFullDocumentDiagnosticReport>(first));
        lsp::RelatedFullDocumentDiagnosticReport const& first_full = std::get<lsp::RelatedFullDocumentDiagnosticReport>(first);
        REQUIRE(first_full.resultId.has_value());
        std::string const first_result_id = first_full.resultId.value();

        // Nothing changed, so a follow-up pull is reported as unchanged.
        lsp::DocumentDiagnosticReport const unchanged = pull_document_diagnostics(server, workspace.main_file, first_result_id);
        REQUIRE(std::holds_alternative<lsp::RelatedUnchangedDocumentDiagnosticReport>(unchanged));

        // Forcing a recompute makes the next pull produce a fresh full report with a new result id.
        invalidate_all_diagnostics(server);

        lsp::DocumentDiagnosticReport const recomputed = pull_document_diagnostics(server, workspace.main_file, first_result_id);
        REQUIRE(std::holds_alternative<lsp::RelatedFullDocumentDiagnosticReport>(recomputed));
        lsp::RelatedFullDocumentDiagnosticReport const& recomputed_full = std::get<lsp::RelatedFullDocumentDiagnosticReport>(recomputed);
        REQUIRE(recomputed_full.resultId.has_value());
        CHECK(recomputed_full.resultId.value() != first_result_id);

        destroy_server(server);
    }
}
