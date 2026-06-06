module;

#include <cstdio>
#include <filesystem>
#include <memory_resource>
#include <span>
#include <sstream>
#include <vector>

#include <lsp/types.h>

module iris.language_server.server;

import iris.common;
import iris.common.filesystem;
import iris.common.filesystem_common;
import iris.compiler;
import iris.compiler.artifact;
import iris.compiler.builder;
import iris.compiler.diagnostic;
import iris.compiler.presets;
import iris.compiler.target;
import iris.core;
import iris.core.declarations;
import iris.language_server.code_action;
import iris.language_server.completion;
import iris.language_server.core;
import iris.language_server.diagnostics;
import iris.language_server.go_to_location;
import iris.language_server.inlay_hints;
import iris.language_server.signature_help;
import iris.parser.convertor;
import iris.parser.parse_tree;
import iris.parser.parser;

namespace iris::language_server
{
    static constexpr bool g_debug = true;

    Server create_server(
        Server_logger logger
    )
    {
        iris::parser::Parser parser = iris::parser::create_parser();

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
        iris::parser::destroy_parser(std::move(server.parser));
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

        lsp::SignatureHelpOptions const signature_help_options
        {
            .triggerCharacters = lsp::Array<lsp::String>{"(", ","},
        };

        lsp::InitializeResult result
        {
            .capabilities =
            {
                .textDocumentSync = text_document_sync_server_capabilities,
                .completionProvider = completion_options,
                .signatureHelpProvider = signature_help_options,
                .definitionProvider = definition_options,
                .inlayHintProvider = inlay_hint_options,
                .diagnosticProvider = diagnostic_options,
                .workspace = workspace_server_capabilities,
            },
            .serverInfo = lsp::InitializeResultServerInfo
            {
                .name = "Iris Language Server",
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
                iris::parser::destroy_tree(std::move(workspace_data.core_module_parse_trees[index]));
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

    static std::pmr::vector<std::filesystem::path> merge_paths_with_dedup(
        std::span<std::filesystem::path const> const first,
        std::span<std::filesystem::path const> const second
    )
    {
        std::pmr::vector<std::filesystem::path> merged_paths;
        std::pmr::set<std::pmr::string> seen_paths;

        auto append_unique = [&](std::span<std::filesystem::path const> const source)
        {
            for (std::filesystem::path const& path : source)
            {
                std::filesystem::path const normalized = path.lexically_normal();
                std::pmr::string const key{ normalized.generic_string() };
                if (seen_paths.contains(key))
                    continue;

                seen_paths.insert(key);
                merged_paths.push_back(normalized);
            }
        };

        append_unique(first);
        append_unique(second);
        return merged_paths;
    }

    static std::optional<iris::compiler::Presets> try_get_workspace_presets(
        Server& server,
        std::filesystem::path const& workspace_folder_path
    )
    {
        std::filesystem::path const presets_file_path = workspace_folder_path / "iris_presets.json";
        try
        {
            return iris::compiler::try_get_presets(presets_file_path);
        }
        catch (std::exception const& error)
        {
            server.logger.window_show_message(
                lsp::ShowMessageParams{
                    .type = lsp::MessageType::Error,
                    .message = std::format(
                        "Failed to parse '{}': {}",
                        presets_file_path.generic_string(),
                        error.what()
                    ),
                }
            );
            return std::nullopt;
        }
    }

    static std::filesystem::path get_build_directory_path(
        std::filesystem::path const& workspace_folder_path,
        std::optional<iris::compiler::Presets> const& presets
    )
    {
        if (presets.has_value() && presets->build_directory_path.has_value())
            return presets->build_directory_path.value();

        return workspace_folder_path / "build";
    }

    static std::pmr::vector<std::filesystem::path> get_header_search_paths(
        std::optional<iris::compiler::Presets> const& presets
    )
    {
        std::pmr::vector<std::filesystem::path> const default_header_search_paths = iris::common::get_default_header_search_directories();
        if (!presets.has_value())
            return default_header_search_paths;

        return merge_paths_with_dedup(default_header_search_paths, presets->header_search_paths);
    }

    static std::pmr::vector<std::filesystem::path> get_repository_paths(
        std::optional<iris::compiler::Presets> const& presets
    )
    {
        if (!presets.has_value())
            return {};

        return presets->repository_paths;
    }

    static iris::compiler::Compilation_options get_compilation_options(
        std::optional<iris::compiler::Presets> const& presets
    )
    {
        return
        {
            .target_triple = std::nullopt,
            .is_optimized = false,
            .debug = true,
            .contract_options =
                presets.has_value() && presets->function_contract_options.has_value()
                ? presets->function_contract_options.value()
                : iris::compiler::Contract_options::Log_error_and_abort,
        };
    }

    static iris::compiler::Builder_options get_builder_options(
        std::optional<iris::compiler::Presets> const& presets
    )
    {
        return
        {
            .output_llvm_ir = presets.has_value() && presets->output_llvm_ir.value_or(false),
            .is_test_mode = false,
            .environment_variables = presets.has_value() ? presets->environment_variables : iris::compiler::Environment_variables{},
        };
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

    static std::pmr::vector<iris::parser::Parse_tree> parse_source_files(
        iris::parser::Parser const& parser,
        std::span<std::filesystem::path const> const source_files_paths,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<iris::parser::Parse_tree> trees{output_allocator};
        trees.resize(source_files_paths.size());

        for (std::size_t index = 0; index < source_files_paths.size(); ++index)
        {
            std::filesystem::path const& source_file_path = source_files_paths[index];

            std::optional<std::pmr::string> const source_content = iris::common::get_file_contents(source_file_path);
            if (!source_content.has_value())
                continue;

            std::pmr::u8string const utf_8_source_content{reinterpret_cast<char8_t const*>(source_content->data()), source_content->size(), output_allocator};
            iris::parser::Parse_tree parse_tree = iris::parser::parse(parser, std::move(utf_8_source_content));

            trees[index] = std::move(parse_tree);
        }

        return trees;
    }

    static std::optional<iris::Module> convert_to_core_module(
        std::filesystem::path const& source_file_path,
        iris::parser::Parse_tree const& parse_tree,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        iris::parser::Parse_node const& root_node = iris::parser::get_root_node(parse_tree);

        std::optional<iris::Module> core_module = iris::parser::parse_node_to_module(
            parse_tree,
            root_node,
            source_file_path,
            output_allocator,
            temporaries_allocator
        );

        return core_module;
    }

    static std::pmr::vector<iris::Module> convert_to_core_modules(
        std::span<std::filesystem::path const> const source_file_paths,
        std::span<iris::parser::Parse_tree const> const parse_trees,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<iris::Module> core_modules{output_allocator};
        core_modules.resize(parse_trees.size(), iris::Module{});

        for (std::size_t index = 0; index < parse_trees.size(); ++index)
        {
            std::filesystem::path const& source_file_path = source_file_paths[index];
            iris::parser::Parse_tree const& parse_tree = parse_trees[index];

            std::optional<iris::Module> core_module = convert_to_core_module(
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

        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        for (std::size_t index = 0; index < configurations.size(); ++index)
        {
            lsp::WorkspaceFolder const& workspace_folder = server.workspace_folders[index];

            std::filesystem::path const workspace_folder_path = to_filesystem_path(
                target,
                workspace_folder.uri
            );

            std::optional<iris::compiler::Presets> const presets = try_get_workspace_presets(server, workspace_folder_path);
            std::filesystem::path const build_directory_path = get_build_directory_path(workspace_folder_path, presets);
            std::pmr::vector<std::filesystem::path> const header_search_paths = get_header_search_paths(presets);
            std::pmr::vector<std::filesystem::path> const repository_paths = get_repository_paths(presets);

            if (!validate_paths(server, header_search_paths))
                return;

            if (!validate_paths(server, repository_paths))
                return;

            iris::compiler::Compilation_options const compilation_options = get_compilation_options(presets);
            iris::compiler::Builder_options const builder_options = get_builder_options(presets);

            iris::compiler::Builder builder = iris::compiler::create_builder(
                target,
                build_directory_path,
                header_search_paths,
                repository_paths,
                compilation_options,
                builder_options,
                output_allocator
            );

            std::pmr::vector<std::filesystem::path> const artifact_file_paths = iris::common::search_files(
                workspace_folder_path,
                "iris_artifact.json",
                temporaries_allocator,
                temporaries_allocator
            );

            bool const is_test_mode = true;

            std::pmr::vector<iris::compiler::Artifact> artifacts = iris::compiler::get_sorted_artifacts(
                artifact_file_paths,
                builder.repositories,
                is_test_mode,
                builder.environment_variables,
                output_allocator,
                temporaries_allocator
            );

            std::pmr::vector<std::filesystem::path> core_module_source_file_paths = get_artifacts_source_files(
                artifacts,
                is_test_mode,
                output_allocator,
                temporaries_allocator
            );

            std::pmr::vector<std::optional<int>> core_module_versions{output_allocator};
            core_module_versions.resize(core_module_source_file_paths.size(), std::nullopt);

            std::pmr::vector<std::pmr::vector<iris::compiler::Diagnostic>> core_module_diagnostics{output_allocator};
            core_module_diagnostics.resize(core_module_source_file_paths.size());

            std::pmr::vector<std::pmr::string> core_module_diagnostic_result_ids{output_allocator};
            core_module_diagnostic_result_ids.resize(core_module_source_file_paths.size(), "0");

            std::pmr::vector<bool> core_module_diagnostic_dirty_flags{output_allocator};
            core_module_diagnostic_dirty_flags.resize(core_module_source_file_paths.size(), true);

            std::pmr::vector<iris::parser::Parse_tree> core_module_parse_trees = parse_source_files(
                server.parser,
                core_module_source_file_paths,
                output_allocator,
                temporaries_allocator
            );

            std::pmr::vector<iris::Module> core_modules = convert_to_core_modules(
                core_module_source_file_paths,
                core_module_parse_trees,
                output_allocator,
                temporaries_allocator
            );

            iris::compiler::Modules_and_declaration_database modules_and_declaration_database = import_and_export_c_headers(
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
                iris::parser::Parse_tree parse_tree = iris::parser::parse(server.parser, std::move(text));

                iris::parser::destroy_tree(std::move(workspace_data.core_module_parse_trees[core_module_index]));
                workspace_data.core_module_parse_trees[core_module_index] = std::move(parse_tree);
            }
            else if (std::holds_alternative<lsp::TextDocumentContentChangeEvent_Range_Text>(event))
            {
                lsp::TextDocumentContentChangeEvent_Range_Text const& range_content_event = std::get<lsp::TextDocumentContentChangeEvent_Range_Text>(event);

                iris::Source_range const range = utf_16_lsp_range_to_utf_8_source_range(range_content_event.range);
                std::pmr::u8string const new_text = convert_to_utf_8_string(range_content_event.text, {});

                iris::parser::Parse_tree new_parse_tree = iris::parser::edit_tree(
                    server.parser,
                    std::move(workspace_data.core_module_parse_trees[core_module_index]),
                    range,
                    new_text
                );

                workspace_data.core_module_parse_trees[core_module_index] = std::move(new_parse_tree);
            }

            std::optional<iris::Module> core_module = convert_to_core_module(
                workspace_data.core_module_source_file_paths[core_module_index],
                workspace_data.core_module_parse_trees[core_module_index],
                output_allocator,
                temporaries_allocator
            );
            if (core_module.has_value())
            {
                workspace_data.core_modules[core_module_index] = core_module.value();

                iris::compiler::Declaration_database_and_sorted_modules result =
                    iris::compiler::create_declaration_database_and_sorted_modules(
                        workspace_data.header_modules,
                        workspace_data.core_modules,
                        temporaries_allocator,
                        temporaries_allocator
                    );

                workspace_data.declaration_database = std::move(result.declaration_database);
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
        iris::Module const& core_module = workspace_data.core_modules[core_module_index];
        if (core_module.name.empty())
            return nullptr;

        std::vector<lsp::InlayHint> inlay_hints;

        auto const process_function = [&](iris::Function_declaration const& function_declaration) -> void {
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

        for (iris::Function_declaration const& function_declaration : core_module.export_declarations.function_declarations)
            process_function(function_declaration);
        for (iris::Function_declaration const& function_declaration : core_module.internal_declarations.function_declarations)
            process_function(function_declaration);

        return inlay_hints;
    }

    std::filesystem::path to_filesystem_path(
        iris::compiler::Target const& target,
        lsp::Uri const& uri
    )
    {
        std::filesystem::path file_path = 
            target.operating_system == "windows" ?
            uri.path().substr(1) :
            uri.path();

        return file_path;
    }

    iris::Source_range utf_16_lsp_range_to_utf_8_source_range(
        lsp::Range const& utf_16_range
    )
    {
        // TODO do conversion
        lsp::Range const utf_8_range = utf_16_range;

        return to_source_range(utf_8_range);
    }

    lsp::TextDocument_SignatureHelpResult compute_text_document_signature_help(
        Server& server,
        lsp::SignatureHelpParams const& parameters
    )
    {
        std::optional<std::pair<Workspace_data&, std::size_t>> const workspace_core_module_pair =
            find_workspace_core_module_index(server, parameters.textDocument.uri);
        if (!workspace_core_module_pair.has_value())
            return nullptr;

        Workspace_data const& workspace_data = workspace_core_module_pair->first;
        std::size_t const core_module_index = workspace_core_module_pair->second;

        return compute_signature_help(
            workspace_data.declaration_database,
            workspace_data.core_module_parse_trees[core_module_index],
            workspace_data.core_modules[core_module_index],
            parameters.position,
            server.logger.window_log_message
        );
    }
}
