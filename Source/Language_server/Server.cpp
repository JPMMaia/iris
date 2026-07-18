module;

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory_resource>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <lsp/messagehandler.h>
#include <lsp/messages.h>
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
import iris.compiler.validation;
import iris.core;
import iris.core.declarations;
import iris.graph;
import iris.graph.module_dependency;
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

    static void destroy_workspaces_data_vector(
        std::pmr::vector<Workspace_data>& workspaces_data
    )
    {
        for (Workspace_data& workspace_data : workspaces_data)
        {
            for (std::size_t index = 0; index < workspace_data.core_module_parse_trees.size(); ++index)
            {
                iris::parser::destroy_tree(std::move(workspace_data.core_module_parse_trees[index]));
            }
        }

        workspaces_data.clear();
    }

    void destroy_workspaces_data(
        Server& server
    )
    {
        destroy_workspaces_data_vector(server.workspaces_data);
    }

    void set_workspace_folders(
        Server& server,
        std::span<lsp::WorkspaceFolder const> const workspace_folders
    )
    {
        server.workspace_folders.clear();
        destroy_workspaces_data(server);

        // The cached configurations are indexed by workspace folder, so they no longer apply.
        server.workspace_configurations.clear();
        server.workspace_build_directory_paths.clear();

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
        std::filesystem::path const standard_repository_path = iris::common::get_standard_repository_file_path();
        if (!presets.has_value())
            return {standard_repository_path};

        return merge_paths_with_dedup(
            std::span<std::filesystem::path const>{ &standard_repository_path, 1 },
            presets->repository_paths
        );
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

    // Paths that reach the server from different places do not agree on case or separators: a
    // client may send 'c:/foo' where an artifact glob produced 'C:\foo'. Comparing them as
    // std::filesystem::path would treat those as different files, so they are compared the same
    // way document uris are.
    static bool are_file_paths_equal(
        std::filesystem::path const& left,
        std::filesystem::path const& right
    )
    {
        return compare_document_uris(
            lsp::DocumentUri::fromPath(left.generic_string()),
            lsp::DocumentUri::fromPath(right.generic_string())
        );
    }

    static bool is_document_open(
        Server const& server,
        std::filesystem::path const& file_path
    )
    {
        auto const location = std::ranges::find_if(
            server.open_document_paths,
            [&](std::filesystem::path const& open_document_path) -> bool { return are_file_paths_equal(open_document_path, file_path); }
        );

        return location != server.open_document_paths.end();
    }

    // Finds a source file in workspace data that is about to be discarded, so that state which
    // only exists in memory can be carried over to the rebuilt workspace.
    static std::optional<std::pair<std::size_t, std::size_t>> find_previous_core_module_index(
        std::pmr::vector<Workspace_data> const& previous_workspaces_data,
        std::filesystem::path const& file_path
    )
    {
        for (std::size_t workspace_index = 0; workspace_index < previous_workspaces_data.size(); ++workspace_index)
        {
            std::pmr::vector<std::filesystem::path> const& source_file_paths =
                previous_workspaces_data[workspace_index].core_module_source_file_paths;

            auto const location = std::ranges::find_if(
                source_file_paths,
                [&](std::filesystem::path const& source_file_path) -> bool { return are_file_paths_equal(source_file_path, file_path); }
            );
            if (location == source_file_paths.end())
                continue;

            return std::pair<std::size_t, std::size_t>{
                workspace_index,
                static_cast<std::size_t>(std::distance(source_file_paths.begin(), location))
            };
        }

        return std::nullopt;
    }

    // Hands the in-memory parse tree and client version of every open document over to the
    // freshly parsed workspace. A rebuild re-reads sources from disk, which would otherwise
    // discard unsaved edits; worse, text_document_did_change applies incremental edits against
    // the stored tree, so replacing it with the on-disk text would make every subsequent edit
    // range apply at the wrong offset.
    static void preserve_open_document_state(
        Server const& server,
        std::pmr::vector<Workspace_data>& previous_workspaces_data,
        std::span<std::filesystem::path const> const core_module_source_file_paths,
        std::pmr::vector<iris::parser::Parse_tree>& core_module_parse_trees,
        std::pmr::vector<std::optional<int>>& core_module_versions
    )
    {
        for (std::size_t index = 0; index < core_module_source_file_paths.size(); ++index)
        {
            std::filesystem::path const& source_file_path = core_module_source_file_paths[index];

            if (!is_document_open(server, source_file_path))
                continue;

            std::optional<std::pair<std::size_t, std::size_t>> const previous_location =
                find_previous_core_module_index(previous_workspaces_data, source_file_path);
            if (!previous_location.has_value())
                continue;

            Workspace_data& previous_workspace_data = previous_workspaces_data[previous_location->first];
            std::size_t const previous_index = previous_location->second;

            iris::parser::Parse_tree& previous_parse_tree = previous_workspace_data.core_module_parse_trees[previous_index];
            if (previous_parse_tree.ts_tree == nullptr)
                continue;

            iris::parser::destroy_tree(std::move(core_module_parse_trees[index]));

            core_module_parse_trees[index] = std::move(previous_parse_tree);
            // Parse_tree holds a raw TSTree* and moving it does not clear the source, so the
            // previous entry must be nulled or destroy_workspaces_data_vector would free the
            // tree that was just handed over.
            previous_parse_tree.ts_tree = nullptr;

            core_module_versions[index] = previous_workspace_data.core_module_versions[previous_index];
        }
    }

    void set_workspace_folder_configurations(
        Server& server,
        lsp::Workspace_ConfigurationResult const& configurations
    )
    {
        if (configurations.size() != server.workspace_folders.size())
            return;

        server.workspace_configurations = configurations;

        rebuild_workspaces_data(server);
    }

    void rebuild_workspaces_data(
        Server& server
    )
    {
        if (server.workspace_configurations.size() != server.workspace_folders.size())
            return;

        // Keep the old data alive until the new data is built, so that open documents can hand
        // their unsaved parse trees over to it.
        std::pmr::vector<Workspace_data> previous_workspaces_data = std::move(server.workspaces_data);
        server.workspaces_data.clear();
        server.workspace_build_directory_paths.clear();

        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        for (std::size_t index = 0; index < server.workspace_configurations.size(); ++index)
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

            // Recorded before validation so that watched file events under the build directory
            // can still be filtered out for a workspace that failed to load.
            server.workspace_build_directory_paths.push_back(build_directory_path);

            // A workspace that fails validation still occupies its slot: leaving workspaces_data
            // shorter than workspace_folders would make compute_workspace_diagnostics report
            // nothing at all until the next configuration round-trip. An empty placeholder is
            // inert because every lookup goes through core_module_source_file_paths.
            if (!validate_paths(server, header_search_paths) || !validate_paths(server, repository_paths))
            {
                server.workspaces_data.push_back(Workspace_data{});
                continue;
            }

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
            core_module_versions.resize(core_module_source_file_paths.size(), std::optional<int>{1});

            std::pmr::vector<Diagnostics_state> diagnostics_states{output_allocator};
            diagnostics_states.resize(core_module_source_file_paths.size());

            std::pmr::vector<iris::parser::Parse_tree> core_module_parse_trees = parse_source_files(
                server.parser,
                core_module_source_file_paths,
                output_allocator,
                temporaries_allocator
            );

            // Must happen before the core modules are converted, so that they and the
            // declaration database describe the text the editor actually shows.
            preserve_open_document_state(
                server,
                previous_workspaces_data,
                core_module_source_file_paths,
                core_module_parse_trees,
                core_module_versions
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
                .core_module_parse_trees = std::move(core_module_parse_trees),
                .core_modules = std::move(core_modules),
                .diagnostics_states = std::move(diagnostics_states),
                .declaration_database = std::move(modules_and_declaration_database.declaration_database),
            };

            server.workspaces_data.push_back(std::move(workspace_data));
        }

        // Any tree that was handed over to the new workspace data was nulled out, so this only
        // frees the trees that were genuinely replaced.
        destroy_workspaces_data_vector(previous_workspaces_data);
    }

    static std::optional<std::pair<Workspace_data&, std::size_t>> find_workspace_core_module_index(
        Server& server,
        lsp::Uri const& uri
    )
    {
        for (Workspace_data& workspace_data : server.workspaces_data)
        {
            std::filesystem::path const file_path = to_filesystem_path(workspace_data.builder.target, uri);

            // The uri is spelled by the client while these paths were produced by expanding the
            // artifact globs, so the two need not agree on case or separators.
            auto const location = std::ranges::find_if(
                workspace_data.core_module_source_file_paths,
                [&](std::filesystem::path const& source_file_path) -> bool { return are_file_paths_equal(source_file_path, file_path); }
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
        // Tracked before the lookup below, which fails for a file that the workspace does not
        // know about yet. Such a file becomes known once a rebuild picks it up, and by then it
        // must already count as open.
        std::filesystem::path const file_path = to_filesystem_path(
            iris::compiler::get_default_target(),
            parameters.textDocument.uri
        ).lexically_normal();
        if (!is_document_open(server, file_path))
            server.open_document_paths.push_back(file_path);

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
        std::filesystem::path const file_path = to_filesystem_path(
            iris::compiler::get_default_target(),
            parameters.textDocument.uri
        ).lexically_normal();

        auto const location = std::ranges::find(server.open_document_paths, file_path);
        if (location != server.open_document_paths.end())
            server.open_document_paths.erase(location);

        std::optional<std::pair<Workspace_data&, std::size_t>> const result = find_workspace_core_module_index(
            server,
            parameters.textDocument.uri
        );
        if (!result.has_value())
            return;
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

            std::filesystem::path const& source_file_path = workspace_data.core_module_source_file_paths[core_module_index];
            iris::parser::Parse_tree const& parse_tree = workspace_data.core_module_parse_trees[core_module_index];

            std::vector<lsp::Diagnostic> parser_diagnostics = create_document_parser_diagnostics(
                source_file_path,
                parse_tree,
                temporaries_allocator
            );

            if (parser_diagnostics.empty())
            {
                std::optional<iris::Module> core_module = convert_to_core_module(
                    source_file_path,
                    parse_tree,
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
            }
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
            workspace_data.diagnostics_states[core_module_index].diagnostics,
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
                workspace_data.core_modules,
                workspace_data.core_module_versions,
                workspace_data.core_module_parse_trees,
                workspace_data.diagnostics_states,
                parameters.previousResultIds,
                workspace_data.header_modules,
                workspace_data.declaration_database,
                temporaries_allocator,
                temporaries_allocator
            );

            report.items.reserve(report.items.capacity() + items.size());
            for (lsp::WorkspaceDocumentDiagnosticReport const& item : items)
                report.items.push_back(item);
        }

        return report;
    }

    lsp::DocumentDiagnosticReport compute_document_diagnostics(
        Server& server,
        lsp::DocumentDiagnosticParams const& parameters
    )
    {
        std::optional<std::pair<Workspace_data&, std::size_t>> const workspace_core_module_pair = find_workspace_core_module_index(
            server,
            parameters.textDocument.uri
        );
        if (!workspace_core_module_pair.has_value())
        {
            lsp::RelatedUnchangedDocumentDiagnosticReport output = {};
            output.resultId = parameters.previousResultId.has_value() ? parameters.previousResultId.value() : "1";
            return output;
        }

        Workspace_data& workspace_data = workspace_core_module_pair->first;
        std::size_t const core_module_index = workspace_core_module_pair->second;
        std::optional<int> const version = workspace_data.core_module_versions[core_module_index];

        Diagnostics_state& diagnostics_state = workspace_data.diagnostics_states[core_module_index];

        // Recompute when this module or any of its transitive dependencies changed since the
        // diagnostics were last produced (or when a forced recompute was requested), rather
        // than only when this document's own version changed. Otherwise editing a dependency
        // would leave this document with stale diagnostics.
        std::pmr::polymorphic_allocator<> temporaries_allocator;
        std::pmr::vector<std::pair<std::pmr::string, std::optional<int>>> current_dependency_versions =
            collect_transitive_dependency_versions(
                core_module_index,
                workspace_data.core_modules,
                workspace_data.core_module_versions,
                {},
                temporaries_allocator
            );

        bool const are_diagnostics_dirty =
            diagnostics_state.force_recompute
            || current_dependency_versions != diagnostics_state.validated_dependency_versions;

        if (!are_diagnostics_dirty)
        {
            if (parameters.previousResultId.has_value() && parameters.previousResultId.value() == diagnostics_state.result_id)
            {
                lsp::RelatedUnchangedDocumentDiagnosticReport output = {};
                output.resultId = parameters.previousResultId.value();
                return output;
            }
        }

        lsp::DocumentDiagnosticReport report = create_document_diagnostics(
            parameters.textDocument.uri,
            workspace_data.core_modules[core_module_index],
            version,
            workspace_data.core_module_parse_trees[core_module_index],
            workspace_data.diagnostics_states[core_module_index],
            workspace_data.declaration_database,
            {},
            {}
        );

        diagnostics_state.force_recompute = false;
        diagnostics_state.validated_dependency_versions = std::move(current_dependency_versions);

        return report;
    }

    static bool is_path_under_directory(
        std::filesystem::path const& path,
        std::filesystem::path const& directory
    )
    {
        // Walks the ancestors rather than using lexically_relative, so that the comparison
        // tolerates the same case and separator differences as everywhere else.
        std::filesystem::path current = path.lexically_normal();

        while (true)
        {
            std::filesystem::path parent = current.parent_path();
            if (parent == current)
                return false;

            if (are_file_paths_equal(parent, directory))
                return true;

            current = std::move(parent);
        }
    }

    static bool is_project_file_name(
        std::filesystem::path const& file_name
    )
    {
        return file_name == "iris_artifact.json"
            || file_name == "iris_presets.json"
            || file_name == "iris_repository.json";
    }

    static bool does_watched_file_event_need_rebuild(
        Server const& server,
        std::filesystem::path const& file_path,
        lsp::FileChangeType const change_type
    )
    {
        // Sources generated by a build would otherwise feed the watcher back into itself:
        // search_files does not exclude the build directory, so they are discovered as if they
        // were workspace sources.
        for (std::filesystem::path const& build_directory_path : server.workspace_build_directory_paths)
        {
            if (is_path_under_directory(file_path, build_directory_path))
                return false;
        }

        // A project file describes which sources exist and how they are compiled, so any change
        // to one can change the whole workspace.
        if (is_project_file_name(file_path.filename()))
            return true;

        if (file_path.extension() != ".iris")
            return false;

        // Creating or deleting a source changes the set of core modules.
        if (change_type != lsp::FileChangeType::Changed)
            return true;

        // An open document's edits already arrive through textDocument/didChange, and rebuilding
        // on every save of the file being edited would be pure waste. A closed document has no
        // other path back into the server, so it does need a rebuild.
        return !is_document_open(server, file_path);
    }

    bool workspace_did_change_watched_files(
        Server& server,
        lsp::DidChangeWatchedFilesParams const& parameters
    )
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        bool needs_rebuild = false;

        for (lsp::FileEvent const& change : parameters.changes)
        {
            std::filesystem::path const file_path = to_filesystem_path(target, change.uri).lexically_normal();

            if (does_watched_file_event_need_rebuild(server, file_path, change.type))
            {
                // Clients batch file events, so a single rebuild covers the whole notification.
                needs_rebuild = true;
                break;
            }
        }

        if (!needs_rebuild)
            return false;

        rebuild_workspaces_data(server);
        invalidate_all_diagnostics(server);

        return true;
    }

    void invalidate_all_diagnostics(
        Server& server
    )
    {
        for (Workspace_data& workspace_data : server.workspaces_data)
        {
            for (Diagnostics_state& diagnostics_state : workspace_data.diagnostics_states)
                diagnostics_state.force_recompute = true;
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

    iris::graph::Graph compute_module_dependency_graph(
        Server& server,
        std::string_view const scope,
        std::optional<lsp::Uri> const& text_document_uri
    )
    {
        std::pmr::polymorphic_allocator<> const output_allocator{ std::pmr::get_default_resource() };

        if (scope == "currentModule")
        {
            if (!text_document_uri.has_value())
                return iris::graph::Graph{ .nodes = std::pmr::vector<iris::graph::Graph_node>{ output_allocator }, .edges = std::pmr::vector<iris::graph::Graph_edge>{ output_allocator } };

            std::optional<std::pair<Workspace_data&, std::size_t>> const workspace_core_module_pair =
                find_workspace_core_module_index(server, *text_document_uri);
            if (!workspace_core_module_pair.has_value())
                return iris::graph::Graph{ .nodes = std::pmr::vector<iris::graph::Graph_node>{ output_allocator }, .edges = std::pmr::vector<iris::graph::Graph_edge>{ output_allocator } };

            Workspace_data const& workspace_data = workspace_core_module_pair->first;
            std::size_t const core_module_index = workspace_core_module_pair->second;
            std::string_view const root_module_name = workspace_data.core_modules[core_module_index].name;

            std::pmr::vector<iris::Module const*> const subset = iris::graph::collect_module_and_dependencies(
                workspace_data.core_modules,
                root_module_name,
                output_allocator
            );

            return iris::graph::create_module_dependency_graph(
                std::span<iris::Module const* const>{ subset },
                output_allocator
            );
        }

        // "workspace" scope (default): every module across all workspaces.
        std::pmr::vector<iris::Module const*> all_modules{ output_allocator };
        for (Workspace_data const& workspace_data : server.workspaces_data)
        {
            for (iris::Module const& module : workspace_data.core_modules)
                all_modules.push_back(&module);
        }

        return iris::graph::create_module_dependency_graph(
            std::span<iris::Module const* const>{ all_modules },
            output_allocator
        );
    }
}
