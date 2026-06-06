module;

#include <filesystem>
#include <functional>
#include <span>
#include <vector>

#include <lsp/types.h>

export module iris.language_server.server;

import iris.compiler.artifact;
import iris.compiler.builder;
import iris.compiler.diagnostic;
import iris.compiler.target;
import iris.core;
import iris.core.declarations;
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
        std::pmr::vector<std::pmr::vector<iris::compiler::Diagnostic>> core_module_diagnostics;
        std::pmr::vector<std::pmr::string> core_module_diagnostic_result_ids;
        std::pmr::vector<bool> core_module_diagnostic_dirty_flags;
        std::pmr::vector<iris::parser::Parse_tree> core_module_parse_trees;
        std::pmr::vector<iris::Module> core_modules;
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
