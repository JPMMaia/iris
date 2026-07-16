module;

#include <filesystem>
#include <functional>
#include <span>
#include <vector>

#include <lsp/messagehandler.h>
#include <lsp/types.h>

export module iris.language_server.server;

import iris.compiler.artifact;
import iris.compiler.builder;
import iris.compiler.diagnostic;
import iris.compiler.target;
import iris.core;
import iris.core.declarations;
import iris.language_server.diagnostics;
import iris.parser.parse_tree;
import iris.parser.parser;

namespace iris::language_server
{
    struct Workspace_data
    {
        iris::compiler::Builder builder;
        std::pmr::vector<iris::compiler::Artifact> artifacts;
        std::pmr::vector<iris::Module> header_modules;
        std::pmr::vector<std::filesystem::path> core_module_source_file_paths;
        std::pmr::vector<std::optional<int>> core_module_versions;
        std::pmr::vector<iris::parser::Parse_tree> core_module_parse_trees;
        std::pmr::vector<iris::Module> core_modules;
        std::pmr::vector<Diagnostics_state> diagnostics_states;
        iris::Declaration_database declaration_database;
    };

    export struct Server_logger
    {
        std::function<void(lsp::LogMessageParams&&)> window_log_message;
        std::function<void(lsp::ShowMessageParams&&)> window_show_message;
    };
    
    export struct Server
    {
        std::pmr::vector<lsp::WorkspaceFolder> workspace_folders;
        // Cached so that a rebuild triggered by a file event does not need another
        // workspace/configuration round-trip with the client.
        lsp::Workspace_ConfigurationResult workspace_configurations;
        // Aligned with workspace_folders. Recorded even for workspaces that fail validation, so
        // that watched file events can be filtered against the build directory regardless.
        std::pmr::vector<std::filesystem::path> workspace_build_directory_paths;
        // Documents the client currently has open. Their in-memory parse trees hold unsaved
        // edits and must survive a rebuild.
        std::pmr::vector<std::filesystem::path> open_document_paths;
        std::pmr::vector<Workspace_data> workspaces_data;
        iris::parser::Parser parser;
        Server_logger logger;
    };

    export Server create_server(
        Server_logger logger
    );

    export void destroy_server(
        Server& server
    );

    export lsp::InitializeResult initialize(
        Server& server,
        lsp::InitializeParams const& parameters
    );

    export lsp::ShutdownResult shutdown(
        Server& server
    );

    export void exit(
        Server& server
    );

    void destroy_workspaces_data(
        Server& server
    );

    export void set_workspace_folders(
        Server& server,
        std::span<lsp::WorkspaceFolder const> const workspace_folders
    );

    export void set_workspace_folder_configurations(
        Server& server,
        lsp::Workspace_ConfigurationResult const& configurations
    );

    // Rediscovers every workspace's source files and re-creates its Declaration_database from
    // the cached configurations. Parse trees and versions of open documents are carried over so
    // that unsaved editor state is not replaced by the contents on disk.
    export void rebuild_workspaces_data(
        Server& server
    );

    // Returns true if the events warranted a rebuild, so that the caller can skip asking the
    // client to refresh when nothing changed.
    export bool workspace_did_change_watched_files(
        Server& server,
        lsp::DidChangeWatchedFilesParams const& parameters
    );

    export void text_document_did_open(
        Server& server,
        lsp::DidOpenTextDocumentParams const& parameters
    );

    export void text_document_did_close(
        Server& server,
        lsp::DidCloseTextDocumentParams const& parameters
    );

    export void text_document_did_change(
        Server& server,
        lsp::DidChangeTextDocumentParams const& parameters
    );

    export lsp::TextDocument_CodeActionResult compute_text_document_code_actions(
        Server& server,
        lsp::CodeActionParams const& parameters
    );

    export lsp::TextDocument_CompletionResult compute_text_document_completion(
        Server& server,
        lsp::CompletionParams const& parameters
    );

    export lsp::TextDocument_DefinitionResult compute_text_document_definition(
        Server& server,
        lsp::DefinitionParams const& parameters,
        bool const client_supports_definition_link
    );

    export lsp::WorkspaceDiagnosticReport compute_workspace_diagnostics(
        Server& server,
        lsp::WorkspaceDiagnosticParams const& parameters
    );

    export lsp::DocumentDiagnosticReport compute_document_diagnostics(
        Server& server,
        lsp::DocumentDiagnosticParams const& parameters
    );

    export void invalidate_all_diagnostics(
        Server& server
    );

    export lsp::TextDocument_InlayHintResult compute_document_inlay_hints(
        Server& server,
        lsp::InlayHintParams const& parameters
    );

    export lsp::TextDocument_SignatureHelpResult compute_text_document_signature_help(
        Server& server,
        lsp::SignatureHelpParams const& parameters
    );

    std::filesystem::path to_filesystem_path(
        iris::compiler::Target const& target,
        lsp::Uri const& uri
    );

    iris::Source_range utf_16_lsp_range_to_utf_8_source_range(
        lsp::Range const& range
    );
}
