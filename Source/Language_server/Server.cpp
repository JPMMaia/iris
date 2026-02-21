module;

#include <cstdio>
#include <filesystem>
#include <memory_resource>
#include <span>
#include <sstream>
#include <vector>

#include <lsp/types.h>

module h.language_server.server;

import h.common;
import h.common.filesystem;
import h.common.filesystem_common;
import h.compiler;
import h.compiler.artifact;
import h.compiler.builder;
import h.compiler.diagnostic;
import h.compiler.target;
import h.core;
import h.core.declarations;
import h.language_server.code_action;
import h.language_server.completion;
import h.language_server.core;
import h.language_server.diagnostics;
import h.language_server.go_to_location;
import h.language_server.inlay_hints;
import h.parser.convertor;
import h.parser.parse_tree;
import h.parser.parser;

namespace h::language_server
{
    static constexpr bool g_debug = true;

    Server create_server(
        Server_logger logger
    )
    {
        h::parser::Parser parser = h::parser::create_parser();

        return
        {
            .workspace_folders = {},
            .workspaces_data = {},
            .parser = std::move(parser),
            .logger = std::move(logger),
        };
    }

    void destroy_server(
        Server& server
    )
    {
        destroy_workspaces_data(server);
        h::parser::destroy_parser(std::move(server.parser));
    }

    lsp::InitializeResult initialize(
        Server& server,
        lsp::InitializeParams const& parameters
    )
    {
        if constexpr (g_debug)
            std::printf("Language Server initialize\n");

        lsp::ClientCapabilities const& client_capabilities = parameters.capabilities;

        bool has_code_action_literal_support = false;

        if (client_capabilities.textDocument.has_value())
        {
            lsp::TextDocumentClientCapabilities const& text_document_client_capabilities = client_capabilities.textDocument.value();
            if (text_document_client_capabilities.codeAction.has_value())
            {
                lsp::CodeActionClientCapabilities const& code_action_client_capabilities = text_document_client_capabilities.codeAction.value();
                if (code_action_client_capabilities.codeActionLiteralSupport.has_value())
                {
                    has_code_action_literal_support = true;
                }
            }
        }

        lsp::DiagnosticOptions const diagnostic_options
        {
            .workDoneProgress = false,
            .interFileDependencies = true,
            .workspaceDiagnostics = true,
            .identifier = std::nullopt,
        };

        lsp::CodeActionOptions const code_action_options
        {
            .workDoneProgress = false,
            .codeActionKinds = {},
            .resolveProvider = false,
        };

        lsp::CompletionOptions const completion_options
        {
            .triggerCharacters = lsp::Array<lsp::String>{
                ".",
                " ",
                ">"
            },
        };

        lsp::DefinitionOptions const definition_options
        {
        };

        lsp::TextDocumentSyncOptions const text_document_sync_server_capabilities
        {
            .openClose = true,
            .change = lsp::TextDocumentSyncKind::Incremental,
            .willSave = false,
            .willSaveWaitUntil = false,
            .save = false,
        };

        lsp::ServerCapabilitiesWorkspace const workspace_server_capabilities
        {
            .workspaceFolders = lsp::WorkspaceFoldersServerCapabilities
            {
                .supported = true,
                .changeNotifications = true,
            }
        };

        lsp::InlayHintOptions const inlay_hint_options
        {
            .resolveProvider = false,
        };

        lsp::InitializeResult result
        {
            .capabilities =
            {
                .textDocumentSync = text_document_sync_server_capabilities,
                .completionProvider = completion_options,
                .definitionProvider = definition_options,
                .inlayHintProvider = inlay_hint_options,
                .diagnosticProvider = diagnostic_options,
                .workspace = workspace_server_capabilities,
            },
            .serverInfo = lsp::InitializeResultServerInfo
            {
                .name = "Hlang Language Server",
                .version = "0.1.0"
            }
        };

        if (has_code_action_literal_support)
        {
            result.capabilities.codeActionProvider = code_action_options;
        }
        else
        {
            result.capabilities.codeActionProvider = true;
        }

        std::span<lsp::WorkspaceFolder const> const workspace_folders =
            parameters.workspaceFolders && !parameters.workspaceFolders->isNull() ?
            parameters.workspaceFolders->value() :
            std::span<lsp::WorkspaceFolder const>{};

        set_workspace_folders(server, workspace_folders);

        return result;
    }

    lsp::ShutdownResult shutdown(
        Server& server
    )
    {
        if constexpr (g_debug)
            std::printf("Language Server shutdown\n");

        return {};
    }
    
    void exit(
        Server& server
    )
    {
        if constexpr (g_debug)
            std::printf("Language Server exit\n");
    }

    void destroy_workspaces_data(
        Server& server
    )
    {
        for (Workspace_data& workspace_data : server.workspaces_data)
        {
            for (std::size_t index = 0; index < workspace_data.core_module_parse_trees.size(); ++index)
            {
                h::parser::destroy_tree(std::move(workspace_data.core_module_parse_trees[index]));
            }
        }

        server.workspaces_data.clear();
    }

    void set_workspace_folders(
        Server& server,
        std::span<lsp::WorkspaceFolder const> const workspace_folders
    )
    {
        server.workspace_folders.clear();
        destroy_workspaces_data(server);

        server.workspace_folders.assign(workspace_folders.begin(), workspace_folders.end());
    }

    std::optional<std::pmr::string> resolve_vscode_variable_value(
        std::filesystem::path const& workspace_folder_path,
        std::string_view const variable_name
    )
    {
        if (variable_name == "workspaceFolder")
            return std::pmr::string{workspace_folder_path.generic_string()};

        return std::nullopt;
    }

    std::pmr::string resolve_vscode_variables(
        std::filesystem::path const& workspace_folder_path,
        std::string_view const value
    )
    {
        using String_stream = std::basic_stringstream<char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>>;
        String_stream stream;

        std::size_t index = 0;
        while (index < value.size())
        {
            using namespace std::literals;
            std::size_t const begin = value.find("${"sv, index);
            if (begin == value.npos)
                break;

            std::size_t const end = value.find("}"sv, begin);
            if (end == value.npos)
                break;

            std::size_t const count = end - begin;
            std::string_view const variable_name = value.substr(begin + 2, count - 2);

            std::optional<std::pmr::string> const variable_value = resolve_vscode_variable_value(workspace_folder_path, variable_name);
            if (variable_value.has_value())
            {
                stream << variable_value.value();
            }
            else
            {
                stream << value.substr(index, end + 1 - index);
            }

            index = end + 1;
        }

        if (index >= value.size())
            return stream.str();

        std::string_view const rest = value.substr(index, value.size() - index);
        stream << rest;
        return stream.str();
    }

    static std::pmr::vector<std::filesystem::path> get_header_search_paths_from_configuration(
        lsp::json::Any const& configuration
    )
    {
        std::pmr::vector<std::filesystem::path> header_search_paths = h::common::get_default_header_search_directories();

        return header_search_paths;
    }

    static std::pmr::vector<std::filesystem::path> get_repository_paths_from_configuration(
        std::filesystem::path const& workspace_folder_path,
        lsp::json::Any const& configuration
    )
    {
        if (!configuration.isObject())
            return {};

        lsp::json::Any const& extension_settings = configuration.object().get("hlang_language_server");
        if (!extension_settings.isObject())
            return {};

        lsp::json::Any const& repositories_json = extension_settings.object().get("repositories");
        if (!repositories_json.isArray())
            return {};

        lsp::json::Array const& repositories = repositories_json.array();

        std::pmr::vector<std::filesystem::path> repository_paths;
        repository_paths.reserve(repositories.size());

        for (lsp::json::Any const repository_json : repositories)
        {
            if (!repository_json.isString())
                continue;

            std::pmr::string const resolved_repository_path_string = resolve_vscode_variables(
                workspace_folder_path,
                repository_json.string()
            );

            std::filesystem::path repository_path = resolved_repository_path_string;

            if (repository_path.is_absolute())
            {
                repository_paths.push_back(std::move(repository_path));
            }
            else
            {
                std::filesystem::path repository_absolute_path = workspace_folder_path / ".vscode" / repository_path;
                repository_paths.push_back(std::move(repository_absolute_path));
            }
        }

        return repository_paths;
    }

    static bool validate_paths(
        Server& server,
        std::span<std::filesystem::path const> const paths
    )
    {
        for (std::filesystem::path const& path : paths)
        {
            if (!std::filesystem::exists(path))
            {
                lsp::ShowMessageParams parameters
                {
                    .type = lsp::MessageType::Error,
                    .message = std::format("Could not find path: '{}'.", path.generic_string()),
                };

                server.logger.window_show_message(std::move(parameters));
                return false;
            }
        }

        return true;
    }

    static std::pmr::vector<h::parser::Parse_tree> parse_source_files(
        h::parser::Parser const& parser,
        std::span<std::filesystem::path const> const source_files_paths,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<h::parser::Parse_tree> trees{output_allocator};
        trees.resize(source_files_paths.size());

        for (std::size_t index = 0; index < source_files_paths.size(); ++index)
        {
            std::filesystem::path const& source_file_path = source_files_paths[index];

            std::optional<std::pmr::string> const source_content = h::common::get_file_contents(source_file_path);
            if (!source_content.has_value())
                continue;

            std::pmr::u8string const utf_8_source_content{reinterpret_cast<char8_t const*>(source_content->data()), source_content->size(), output_allocator};
            h::parser::Parse_tree parse_tree = h::parser::parse(parser, std::move(utf_8_source_content));

            trees[index] = std::move(parse_tree);
        }

        return trees;
    }

    static std::optional<h::Module> convert_to_core_module(
        std::filesystem::path const& source_file_path,
        h::parser::Parse_tree const& parse_tree,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::parser::Parse_node const& root_node = h::parser::get_root_node(parse_tree);

        std::optional<h::Module> core_module = h::parser::parse_node_to_module(
            parse_tree,
            root_node,
            source_file_path,
            output_allocator,
            temporaries_allocator
        );

        return core_module;
    }

    static std::pmr::vector<h::Module> convert_to_core_modules(
        std::span<std::filesystem::path const> const source_file_paths,
        std::span<h::parser::Parse_tree const> const parse_trees,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<h::Module> core_modules{output_allocator};
        core_modules.resize(parse_trees.size(), h::Module{});

        for (std::size_t index = 0; index < parse_trees.size(); ++index)
        {
            std::filesystem::path const& source_file_path = source_file_paths[index];
            h::parser::Parse_tree const& parse_tree = parse_trees[index];

            std::optional<h::Module> core_module = convert_to_core_module(
                source_file_path,
                parse_tree,
                output_allocator,
                temporaries_allocator
            );

            if (core_module.has_value())
                core_modules[index] = std::move(core_module.value());
        }

        return core_modules;
    }

    void set_workspace_folder_configurations(
        Server& server,
        lsp::Workspace_ConfigurationResult const& configurations
    )
    {
        if (configurations.size() != server.workspace_folders.size())
            return;

        destroy_workspaces_data(server);

        h::compiler::Target const target = h::compiler::get_default_target();

        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        for (std::size_t index = 0; index < configurations.size(); ++index)
        {
            lsp::WorkspaceFolder const& workspace_folder = server.workspace_folders[index];
            lsp::json::Any const& workspace_configuration = configurations[index];

            std::filesystem::path const workspace_folder_path = to_filesystem_path(
                target,
                workspace_folder.uri
            );

            std::filesystem::path const build_directory_path = workspace_folder_path / "build";

            std::pmr::vector<std::filesystem::path> const header_search_paths = get_header_search_paths_from_configuration(workspace_configuration);
            std::pmr::vector<std::filesystem::path> const repository_paths = get_repository_paths_from_configuration(workspace_folder_path, workspace_configuration);

            if (!validate_paths(server, header_search_paths))
                return;

            if (!validate_paths(server, repository_paths))
                return;

            h::compiler::Compilation_options const compilation_options
            {
                .target_triple = std::nullopt,
                .is_optimized = false,
                .debug = true,
                .contract_options = h::compiler::Contract_options::Log_error_and_abort,
            };

            h::compiler::Builder_options const builder_options
            {
            };

            h::compiler::Builder builder = h::compiler::create_builder(
                target,
                build_directory_path,
                header_search_paths,
                repository_paths,
                compilation_options,
                builder_options,
                output_allocator
            );

            std::pmr::vector<std::filesystem::path> const artifact_file_paths = h::common::search_files(
                workspace_folder_path,
                "hlang_artifact.json",
                temporaries_allocator,
                temporaries_allocator
            );

            std::pmr::vector<h::compiler::Artifact> artifacts = h::compiler::get_sorted_artifacts(
                artifact_file_paths,
                builder.repositories,
                false,
                output_allocator,
                temporaries_allocator
            );

            std::pmr::vector<std::filesystem::path> core_module_source_file_paths = get_artifacts_source_files(
                artifacts,
                output_allocator,
                temporaries_allocator
            );

            std::pmr::vector<std::optional<int>> core_module_versions{output_allocator};
            core_module_versions.resize(core_module_source_file_paths.size(), std::nullopt);

            std::pmr::vector<std::pmr::vector<h::compiler::Diagnostic>> core_module_diagnostics{output_allocator};
            core_module_diagnostics.resize(core_module_source_file_paths.size());

            std::pmr::vector<std::pmr::string> core_module_diagnostic_result_ids{output_allocator};
            core_module_diagnostic_result_ids.resize(core_module_source_file_paths.size(), "0");

            std::pmr::vector<bool> core_module_diagnostic_dirty_flags{output_allocator};
            core_module_diagnostic_dirty_flags.resize(core_module_source_file_paths.size(), true);

            std::pmr::vector<h::parser::Parse_tree> core_module_parse_trees = parse_source_files(
                server.parser,
                core_module_source_file_paths,
                output_allocator,
                temporaries_allocator
            );

            std::pmr::vector<h::Module> core_modules = convert_to_core_modules(
                core_module_source_file_paths,
                core_module_parse_trees,
                output_allocator,
                temporaries_allocator
            );

            h::compiler::Modules_and_declaration_database modules_and_declaration_database = import_and_export_c_headers(
                builder,
                artifacts,
                core_modules,
                true,
                output_allocator,
                temporaries_allocator
            );

            Workspace_data workspace_data
            {
                .builder = std::move(builder),
                .artifacts = std::move(artifacts),
                .header_modules = std::move(modules_and_declaration_database.header_modules),
                .core_module_source_file_paths = std::move(core_module_source_file_paths),
                .core_module_versions = std::move(core_module_versions),
                .core_module_diagnostics = std::move(core_module_diagnostics),
                .core_module_diagnostic_result_ids = std::move(core_module_diagnostic_result_ids),
                .core_module_diagnostic_dirty_flags = std::move(core_module_diagnostic_dirty_flags),
                .core_module_parse_trees = std::move(core_module_parse_trees),
                .core_modules = std::move(core_modules),
                .declaration_database = std::move(modules_and_declaration_database.declaration_database),
            };

            server.workspaces_data.push_back(std::move(workspace_data));
        }
    }

    static std::optional<std::pair<Workspace_data&, std::size_t>> find_workspace_core_module_index(
        Server& server,
        lsp::Uri const& uri
    )
    {
        for (Workspace_data& workspace_data : server.workspaces_data)
        {
            std::filesystem::path const file_path = to_filesystem_path(workspace_data.builder.target, uri);

            auto const location = std::find(
                workspace_data.core_module_source_file_paths.begin(),
                workspace_data.core_module_source_file_paths.end(),
                file_path
            );
            if (location == workspace_data.core_module_source_file_paths.end())
                continue;

            auto const index = std::distance(workspace_data.core_module_source_file_paths.begin(), location);
            
            return std::pair<Workspace_data&, std::size_t>{ workspace_data, index };
        }

        return std::nullopt;
    }

    void text_document_did_open(
        Server& server,
        lsp::DidOpenTextDocumentParams const& parameters
    )
    {
        std::optional<std::pair<Workspace_data&, std::size_t>> const result = find_workspace_core_module_index(
            server,
            parameters.textDocument.uri
        );
        if (!result.has_value())
            return;

        result->first.core_module_versions[result->second] = parameters.textDocument.version;
    }

    void text_document_did_close(
        Server& server,
        lsp::DidCloseTextDocumentParams const& parameters
    )
    {
        std::optional<std::pair<Workspace_data&, std::size_t>> const result = find_workspace_core_module_index(
            server,
            parameters.textDocument.uri
        );
        if (!result.has_value())
            return;

        result->first.core_module_versions[result->second] = std::nullopt;
    }

    void text_document_did_change(
        Server& server,
        lsp::DidChangeTextDocumentParams const& parameters
    )
    {
        std::optional<std::pair<Workspace_data&, std::size_t>> const result = find_workspace_core_module_index(
            server,
            parameters.textDocument.uri
        );
        if (!result.has_value())
            return;

        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        Workspace_data& workspace_data = result->first;
        std::size_t const core_module_index = result->second;
        
        workspace_data.core_module_versions[core_module_index] = parameters.textDocument.version;

        for (lsp::TextDocumentContentChangeEvent const& event : parameters.contentChanges)
        {
            if (std::holds_alternative<lsp::TextDocumentContentChangeEvent_Text>(event))
            {
                lsp::TextDocumentContentChangeEvent_Text const& full_content_event = std::get<lsp::TextDocumentContentChangeEvent_Text>(event);

                std::pmr::u8string text = convert_to_utf_8_string(full_content_event.text, {});
                h::parser::Parse_tree parse_tree = h::parser::parse(server.parser, std::move(text));

                h::parser::destroy_tree(std::move(workspace_data.core_module_parse_trees[core_module_index]));
                workspace_data.core_module_parse_trees[core_module_index] = std::move(parse_tree);
            }
            else if (std::holds_alternative<lsp::TextDocumentContentChangeEvent_Range_Text>(event))
            {
                lsp::TextDocumentContentChangeEvent_Range_Text const& range_content_event = std::get<lsp::TextDocumentContentChangeEvent_Range_Text>(event);

                h::Source_range const range = utf_16_lsp_range_to_utf_8_source_range(range_content_event.range);
                std::pmr::u8string const new_text = convert_to_utf_8_string(range_content_event.text, {});

                h::parser::Parse_tree new_parse_tree = h::parser::edit_tree(
                    server.parser,
                    std::move(workspace_data.core_module_parse_trees[core_module_index]),
                    range,
                    new_text
                );

                workspace_data.core_module_parse_trees[core_module_index] = std::move(new_parse_tree);
            }

            std::optional<h::Module> core_module = convert_to_core_module(
                workspace_data.core_module_source_file_paths[core_module_index],
                workspace_data.core_module_parse_trees[core_module_index],
                output_allocator,
                temporaries_allocator
            );
            if (core_module.has_value())
            {
                workspace_data.core_modules[core_module_index] = core_module.value();

                std::pmr::vector<h::Module const*> const sorted_core_modules = h::compiler::sort_core_modules(
                    workspace_data.core_modules,
                    temporaries_allocator,
                    temporaries_allocator
                );

                workspace_data.declaration_database = h::compiler::create_declaration_database_and_add_modules(
                    workspace_data.header_modules,
                    sorted_core_modules
                );
            }

            workspace_data.core_module_diagnostic_dirty_flags[core_module_index] = true;
        }
    }

    lsp::TextDocument_CodeActionResult compute_text_document_code_actions(
        Server& server,
        lsp::CodeActionParams const& parameters
    )
    {
        std::optional<std::pair<Workspace_data&, std::size_t>> const workspace_core_module_pair = find_workspace_core_module_index(
            server,
            parameters.textDocument.uri
        );
        if (!workspace_core_module_pair.has_value())
            return nullptr;

        Workspace_data const& workspace_data = workspace_core_module_pair->first;
        std::size_t const core_module_index = workspace_core_module_pair->second;

        return compute_code_actions(
            workspace_data.declaration_database,
            workspace_data.core_module_parse_trees[core_module_index],
            workspace_data.core_modules[core_module_index],
            workspace_data.core_module_diagnostics[core_module_index],
            parameters.range,
            parameters.context
        );
    }

    lsp::TextDocument_CompletionResult compute_text_document_completion(
        Server& server,
        lsp::CompletionParams const& parameters
    )
    {
        std::optional<std::pair<Workspace_data&, std::size_t>> const workspace_core_module_pair = find_workspace_core_module_index(
            server,
            parameters.textDocument.uri
        );
        if (!workspace_core_module_pair.has_value())
            return nullptr;

        Workspace_data const& workspace_data = workspace_core_module_pair->first;
        std::size_t const core_module_index = workspace_core_module_pair->second;

        lsp::Position const& position = parameters.position;

        return compute_completion(
            workspace_data.artifacts,
            workspace_data.header_modules,
            workspace_data.core_modules,
            workspace_data.declaration_database,
            workspace_data.core_module_parse_trees[core_module_index],
            workspace_data.core_modules[core_module_index],
            position
        );
    }

    lsp::TextDocument_DefinitionResult compute_text_document_definition(
        Server& server,
        lsp::DefinitionParams const& parameters,
        bool const client_supports_definition_link
    )
    {
        std::optional<std::pair<Workspace_data&, std::size_t>> const workspace_core_module_pair = find_workspace_core_module_index(
            server,
            parameters.textDocument.uri
        );
        if (!workspace_core_module_pair.has_value())
            return nullptr;

        Workspace_data const& workspace_data = workspace_core_module_pair->first;
        std::size_t const core_module_index = workspace_core_module_pair->second;

        return compute_go_to_definition(
            workspace_data.declaration_database,
            workspace_data.core_module_parse_trees[core_module_index],
            workspace_data.core_modules[core_module_index],
            parameters.position,
            client_supports_definition_link
        );
    }

    lsp::WorkspaceDiagnosticReport compute_workspace_diagnostics(
        Server& server,
        lsp::WorkspaceDiagnosticParams const& parameters
    )
    {
        if (server.workspace_folders.size() != server.workspaces_data.size())
            return lsp::WorkspaceDiagnosticReport{};

        // TODO use workDoneToken
        // TODO use partialResultToken

        lsp::WorkspaceDiagnosticReport report = {};

        std::pmr::polymorphic_allocator<> temporaries_allocator;

        for (Workspace_data& workspace_data : server.workspaces_data)
        {
            std::pmr::vector<lsp::WorkspaceDocumentDiagnosticReport> const items = create_all_diagnostics(
                workspace_data.core_module_source_file_paths,
                workspace_data.core_module_versions,
                workspace_data.core_module_diagnostics,
                parameters.previousResultIds,
                workspace_data.core_module_diagnostic_result_ids,
                workspace_data.core_module_diagnostic_dirty_flags,
                workspace_data.core_module_parse_trees,
                workspace_data.header_modules,
                workspace_data.core_modules,
                temporaries_allocator,
                temporaries_allocator
            );

            report.items.reserve(report.items.capacity() + items.size());
            for (lsp::WorkspaceDocumentDiagnosticReport const& item : items)
                report.items.push_back(item);

            std::fill(
                workspace_data.core_module_diagnostic_dirty_flags.begin(),
                workspace_data.core_module_diagnostic_dirty_flags.end(),
                false
            );
        }

        return report;
    }

    lsp::DocumentDiagnosticReport compute_document_diagnostics(
        Server& server,
        lsp::DocumentDiagnosticParams const& parameters
    )
    {
        if (parameters.previousResultId.has_value())
        {
            lsp::RelatedUnchangedDocumentDiagnosticReport output = {};
            output.resultId = parameters.previousResultId.value();
            return output;
        }
        else
        {
            lsp::RelatedUnchangedDocumentDiagnosticReport output = {};
            output.resultId = "1";
            return output;
        }
    }

    lsp::TextDocument_InlayHintResult compute_document_inlay_hints(
        Server& server,
        lsp::InlayHintParams const& parameters
    )
    {
        std::optional<std::pair<Workspace_data&, std::size_t>> const workspace_core_module_pair = find_workspace_core_module_index(
            server,
            parameters.textDocument.uri
        );
        if (!workspace_core_module_pair.has_value())
            return nullptr;
        
        std::pmr::polymorphic_allocator<> const temporaries_allocator;

        Workspace_data const& workspace_data = workspace_core_module_pair->first;
        std::size_t const core_module_index = workspace_core_module_pair->second;

        Declaration_database const& declaration_database = workspace_data.declaration_database;
        h::Module const& core_module = workspace_data.core_modules[core_module_index];
        if (core_module.name.empty())
            return nullptr;

        std::vector<lsp::InlayHint> inlay_hints;

        auto const process_function = [&](h::Function_declaration const& function_declaration) -> void {
            std::optional<Function_definition const*> const function_definition = find_function_definition(core_module, function_declaration.name);
            if (function_definition.has_value())
            {
                std::pmr::vector<lsp::InlayHint> const function_inlay_hints = create_function_inlay_hints(
                    core_module,
                    function_declaration,
                    *function_definition.value(),
                    declaration_database,
                    temporaries_allocator,
                    temporaries_allocator
                );

                if (!function_inlay_hints.empty())
                    inlay_hints.insert(inlay_hints.end(), function_inlay_hints.begin(), function_inlay_hints.end());
            }
        };

        for (h::Function_declaration const& function_declaration : core_module.export_declarations.function_declarations)
            process_function(function_declaration);
        for (h::Function_declaration const& function_declaration : core_module.internal_declarations.function_declarations)
            process_function(function_declaration);

        return inlay_hints;
    }

    std::filesystem::path to_filesystem_path(
        h::compiler::Target const& target,
        lsp::Uri const& uri
    )
    {
        std::filesystem::path file_path = 
            target.operating_system == "windows" ?
            uri.path().substr(1) :
            uri.path();

        return file_path;
    }

    h::Source_range utf_16_lsp_range_to_utf_8_source_range(
        lsp::Range const& utf_16_range
    )
    {
        // TODO do conversion
        lsp::Range const utf_8_range = utf_16_range;

        return to_source_range(utf_8_range);
    }
}
