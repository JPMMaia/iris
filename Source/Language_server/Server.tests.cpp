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

        struct Glob_workspace
        {
            std::filesystem::path directory;
            std::filesystem::path main_file;
            std::filesystem::path build_directory;
        };

        // Writes a single-artifact workspace whose sources are matched by a glob rather than
        // listed explicitly, so that a file created after initialization is picked up by a
        // rebuild. Mirrors the 'other' fixture used by the extension tests.
        static Glob_workspace write_glob_workspace(
            std::string_view const name
        )
        {
            std::filesystem::path const workspace_directory = create_clean_temporary_directory(name);

            iris::common::write_to_file(
                workspace_directory / "iris_artifact.json",
                R"({
    "name": "app",
    "version": "0.1.0",
    "type": "executable",
    "sources": [
        {
            "type": "iris",
            "include": [
                "./**/*.iris"
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

@unique_name("main")
export function main() -> (result: Int32)
{
    return 0;
}
)"
            );

            iris::common::write_to_file(
                workspace_directory / "iris_presets.json",
                std::format(
                    R"({{
    "build_directory": "preset_build",
    "function_contracts": "disabled",
    "environment_variables": {{
        "PROJECT_ROOT": "{}"
    }}
}})",
                    workspace_directory.generic_string()
                )
            );

            return Glob_workspace{
                .directory = workspace_directory,
                .main_file = workspace_directory / "main.iris",
                .build_directory = workspace_directory / "preset_build",
            };
        }

        static bool notify_watched_file_change(
            Server& server,
            std::filesystem::path const& file,
            lsp::FileChangeType const type
        )
        {
            lsp::DidChangeWatchedFilesParams parameters;
            parameters.changes.push_back(
                lsp::FileEvent{
                    .uri = lsp::DocumentUri::fromPath(file.generic_string()),
                    .type = type,
                }
            );
            return workspace_did_change_watched_files(server, parameters);
        }

        // Clients do not agree with the filesystem on how a Windows path is spelled: a uri may
        // arrive with a lower case drive letter where the artifact globs produced an upper case
        // one. The uri is parsed rather than built with DocumentUri::fromPath, because fromPath
        // runs std::filesystem::canonical on a file that exists and would restore the real case,
        // which a uri arriving over the wire is never subjected to.
        static lsp::DocumentUri to_lower_case_drive_uri(std::filesystem::path const& file)
        {
            std::string generic = file.generic_string();
            if (generic.size() >= 2 && generic[1] == ':')
                generic[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(generic[0])));

            return lsp::DocumentUri{lsp::Uri::parse("file:///" + generic)};
        }

        static void open_document_with_uri(Server& server, lsp::DocumentUri const& uri, int const version, std::string_view const text)
        {
            lsp::DidOpenTextDocumentParams parameters;
            parameters.textDocument.uri = uri;
            parameters.textDocument.languageId = "iris";
            parameters.textDocument.version = version;
            parameters.textDocument.text = std::string{text};
            text_document_did_open(server, parameters);
        }

        static void change_document_full_text_with_uri(Server& server, lsp::DocumentUri const& uri, int const version, std::string_view const new_text)
        {
            lsp::DidChangeTextDocumentParams parameters;
            parameters.textDocument.uri = uri;
            parameters.textDocument.version = version;
            parameters.contentChanges.push_back(lsp::TextDocumentContentChangeEvent_Text{ .text = std::string{new_text} });
            text_document_did_change(server, parameters);
        }

        static bool notify_watched_file_change_with_uri(
            Server& server,
            lsp::DocumentUri const& uri,
            lsp::FileChangeType const type
        )
        {
            lsp::DidChangeWatchedFilesParams parameters;
            parameters.changes.push_back(lsp::FileEvent{ .uri = uri, .type = type });
            return workspace_did_change_watched_files(server, parameters);
        }

        static bool is_core_module_known(
            Server const& server,
            std::filesystem::path const& file
        )
        {
            std::filesystem::path const normalized_file = file.lexically_normal();

            for (auto const& workspace_data : server.workspaces_data)
            {
                if (std::ranges::find(workspace_data.core_module_source_file_paths, normalized_file) != workspace_data.core_module_source_file_paths.end())
                    return true;
            }
            return false;
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

    TEST_CASE("A created source file becomes known to the workspace", "[Language_server][Server][Watched_files]")
    {
        Glob_workspace const workspace = write_glob_workspace("iris_language_server_watched_files_created");

        Server server = create_configured_server(workspace.directory);
        REQUIRE(server.workspaces_data.size() == 1);

        std::filesystem::path const created_file = workspace.directory / "extra.iris";

        // A file that did not exist at initialization is unknown, so every feature no-ops on it.
        CHECK_FALSE(is_core_module_known(server, created_file));
        lsp::DocumentDiagnosticReport const before = pull_document_diagnostics(server, created_file, std::nullopt);
        CHECK(std::holds_alternative<lsp::RelatedUnchangedDocumentDiagnosticReport>(before));

        iris::common::write_to_file(
            created_file,
            R"(module extra;

export function extra_function() -> ()
{
}
)"
        );

        CHECK(notify_watched_file_change(server, created_file, lsp::FileChangeType::Created));

        CHECK(is_core_module_known(server, created_file));
        lsp::DocumentDiagnosticReport const after = pull_document_diagnostics(server, created_file, std::nullopt);
        CHECK(std::holds_alternative<lsp::RelatedFullDocumentDiagnosticReport>(after));

        destroy_server(server);
    }

    TEST_CASE("A deleted source file is dropped from the workspace", "[Language_server][Server][Watched_files]")
    {
        Glob_workspace const workspace = write_glob_workspace("iris_language_server_watched_files_deleted");

        std::filesystem::path const extra_file = workspace.directory / "extra.iris";
        iris::common::write_to_file(
            extra_file,
            R"(module extra;

export function extra_function() -> ()
{
}
)"
        );

        Server server = create_configured_server(workspace.directory);
        REQUIRE(server.workspaces_data.size() == 1);
        REQUIRE(is_core_module_known(server, extra_file));

        std::filesystem::remove(extra_file);

        CHECK(notify_watched_file_change(server, extra_file, lsp::FileChangeType::Deleted));

        CHECK_FALSE(is_core_module_known(server, extra_file));

        destroy_server(server);
    }

    TEST_CASE("Unsaved edits survive a rebuild triggered by an unrelated file", "[Language_server][Server][Watched_files]")
    {
        Glob_workspace const workspace = write_glob_workspace("iris_language_server_watched_files_unsaved_edits");

        Server server = create_configured_server(workspace.directory);
        REQUIRE(server.workspaces_data.size() == 1);

        open_document(server, workspace.main_file, 1, R"(module app;

@unique_name("main")
export function main() -> (result: Int32)
{
    return 0;
}
)");

        // Introduce a syntax error that exists only in the editor, never on disk.
        change_document_full_text(
            server,
            workspace.main_file,
            2,
            R"(module app;

@unique_name("main")
export function main() -> (result: Int32)
{
    return 0
)"
        );

        lsp::DocumentDiagnosticReport const edited = pull_document_diagnostics(server, workspace.main_file, std::nullopt);
        REQUIRE(std::holds_alternative<lsp::RelatedFullDocumentDiagnosticReport>(edited));
        REQUIRE_FALSE(std::get<lsp::RelatedFullDocumentDiagnosticReport>(edited).items.empty());

        // Creating an unrelated file rebuilds the workspace. The rebuild re-reads sources from
        // disk, where main.iris is still valid, so without preserving the open document's parse
        // tree the unsaved error would vanish (and the client's next incremental edit would then
        // be applied against the wrong base text).
        std::filesystem::path const created_file = workspace.directory / "extra.iris";
        iris::common::write_to_file(
            created_file,
            R"(module extra;

export function extra_function() -> ()
{
}
)"
        );
        REQUIRE(notify_watched_file_change(server, created_file, lsp::FileChangeType::Created));

        lsp::DocumentDiagnosticReport const after_rebuild = pull_document_diagnostics(server, workspace.main_file, std::nullopt);
        REQUIRE(std::holds_alternative<lsp::RelatedFullDocumentDiagnosticReport>(after_rebuild));
        CHECK_FALSE(std::get<lsp::RelatedFullDocumentDiagnosticReport>(after_rebuild).items.empty());

        destroy_server(server);
    }

    TEST_CASE("Irrelevant watched file events do not rebuild the workspace", "[Language_server][Server][Watched_files]")
    {
        Glob_workspace const workspace = write_glob_workspace("iris_language_server_watched_files_irrelevant");

        Server server = create_configured_server(workspace.directory);
        REQUIRE(server.workspaces_data.size() == 1);

        // Generated sources under the build directory must never trigger a rebuild, otherwise
        // compiling the workspace would feed the watcher back into itself.
        CHECK_FALSE(notify_watched_file_change(server, workspace.build_directory / "generated.iris", lsp::FileChangeType::Created));

        // Files the server does not care about are ignored.
        CHECK_FALSE(notify_watched_file_change(server, workspace.directory / "README.md", lsp::FileChangeType::Created));

        destroy_server(server);
    }

    TEST_CASE("Watched file events tolerate differently cased paths", "[Language_server][Server][Watched_files]")
    {
        Glob_workspace const workspace = write_glob_workspace("iris_language_server_watched_files_path_case");

        Server server = create_configured_server(workspace.directory);
        REQUIRE(server.workspaces_data.size() == 1);

        lsp::DocumentUri const main_file_uri = to_lower_case_drive_uri(workspace.main_file);
        REQUIRE(main_file_uri.isValid());

        open_document_with_uri(server, main_file_uri, 1, R"(module app;

@unique_name("main")
export function main() -> (result: Int32)
{
    return 0;
}
)");

        // The edit only exists in the editor, and arrives spelled the way the client spells paths.
        change_document_full_text_with_uri(
            server,
            main_file_uri,
            2,
            R"(module app;

@unique_name("main")
export function main() -> (result: Int32)
{
    return 0
)"
        );

        lsp::DocumentDiagnosticReport const edited = pull_document_diagnostics(server, workspace.main_file, std::nullopt);
        REQUIRE(std::holds_alternative<lsp::RelatedFullDocumentDiagnosticReport>(edited));
        REQUIRE_FALSE(std::get<lsp::RelatedFullDocumentDiagnosticReport>(edited).items.empty());

        // A build directory event spelled differently must still be recognized as being under the
        // build directory, otherwise generated sources would trigger a rebuild.
        CHECK_FALSE(
            notify_watched_file_change_with_uri(
                server,
                to_lower_case_drive_uri(workspace.build_directory / "generated.iris"),
                lsp::FileChangeType::Created
            )
        );

        std::filesystem::path const created_file = workspace.directory / "extra.iris";
        iris::common::write_to_file(
            created_file,
            R"(module extra;

export function extra_function() -> ()
{
}
)"
        );
        REQUIRE(
            notify_watched_file_change_with_uri(
                server,
                to_lower_case_drive_uri(created_file),
                lsp::FileChangeType::Created
            )
        );

        // The open document was tracked under the client's spelling while the rebuilt workspace
        // lists it under the filesystem's, so preserving its unsaved tree depends on the two
        // being compared case insensitively.
        lsp::DocumentDiagnosticReport const after_rebuild = pull_document_diagnostics(server, workspace.main_file, std::nullopt);
        REQUIRE(std::holds_alternative<lsp::RelatedFullDocumentDiagnosticReport>(after_rebuild));
        CHECK_FALSE(std::get<lsp::RelatedFullDocumentDiagnosticReport>(after_rebuild).items.empty());

        destroy_server(server);
    }

    TEST_CASE("A workspace that fails validation still occupies its slot", "[Language_server][Server][Watched_files]")
    {
        Glob_workspace const workspace = write_glob_workspace("iris_language_server_watched_files_invalid_presets");

        // Presets that point at a repository which does not exist. Rebuilds make this reachable
        // at any time, for instance while iris_presets.json is being edited.
        iris::common::write_to_file(
            workspace.directory / "iris_presets.json",
            std::format(
                R"({{
    "build_directory": "preset_build",
    "repository_paths": [
        "does_not_exist.json"
    ],
    "environment_variables": {{
        "PROJECT_ROOT": "{}"
    }}
}})",
                workspace.directory.generic_string()
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
            .uri = lsp::DocumentUri::fromPath(workspace.directory.generic_string()),
            .name = "workspace",
        };
        set_workspace_folders(server, {&workspace_folder, 1});

        lsp::Workspace_ConfigurationResult configurations;
        configurations.push_back(lsp::json::Object{});
        set_workspace_folder_configurations(server, configurations);

        CHECK_FALSE(shown_messages.empty());

        // The workspace keeps its slot even though it failed to load. Leaving workspaces_data
        // shorter than workspace_folders would make compute_workspace_diagnostics report nothing
        // at all, for every workspace, until the next configuration round-trip.
        REQUIRE(server.workspaces_data.size() == server.workspace_folders.size());

        lsp::WorkspaceDiagnosticParams parameters;
        parameters.identifier = "iris";
        lsp::WorkspaceDiagnosticReport const report = compute_workspace_diagnostics(server, parameters);
        CHECK(report.items.empty());

        destroy_server(server);
    }

    TEST_CASE("A changed artifact file rebuilds the workspace", "[Language_server][Server][Watched_files]")
    {
        Glob_workspace const workspace = write_glob_workspace("iris_language_server_watched_files_artifact_changed");

        Server server = create_configured_server(workspace.directory);
        REQUIRE(server.workspaces_data.size() == 1);

        CHECK(notify_watched_file_change(server, workspace.directory / "iris_artifact.json", lsp::FileChangeType::Changed));

        destroy_server(server);
    }
}
