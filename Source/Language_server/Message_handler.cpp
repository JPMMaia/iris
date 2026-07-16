module;

#include <cstdio>
#include <thread>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <lsp/messages.h>
#include <lsp/connection.h>
#include <lsp/io/socket.h>
#include <lsp/messagehandler.h>

#include <nlohmann/json.hpp>

module iris.language_server.message_handler;

import iris.language_server.server;

namespace iris::language_server
{
    Message_handler create_message_handler()
    {
        return
        {
        };
    }

    void process_messages(
        Message_handler& message_handler_c
    )
    {
        int const port = 12345;
        lsp::io::SocketListener socket_listener = lsp::io::SocketListener(port);

        std::fprintf(stdout, "Listening...\n");
        std::fflush(stdout);

        while (socket_listener.isReady())
        {
            lsp::io::Socket socket = socket_listener.listen();

            if (!socket.isOpen())
                continue;

            run_message_handler(std::move(socket));
        } 
    }

    void run_message_handler(lsp::io::Socket socket)
    {
        bool running = true;

        lsp::Connection connection{socket};
        lsp::MessageHandler message_handler{connection};

        auto const window_log_message = [&message_handler](lsp::LogMessageParams&& parameters)
        {
            message_handler.sendNotification<lsp::notifications::Window_LogMessage>(std::move(parameters));
        };

        auto const window_show_message = [&message_handler](lsp::ShowMessageParams&& parameters)
        {
            message_handler.sendNotification<lsp::notifications::Window_ShowMessage>(std::move(parameters));
        };

        Server_logger const server_logger =
        {
            .window_log_message = window_log_message,
            .window_show_message = window_show_message,
        };
        Server server = create_server(server_logger);

        bool has_configuration_capability = false;
        bool has_workspace_diagnostic_refresh_capability = false;
        bool has_workspace_inlay_hint_refresh_capability = false;
        bool has_workspace_folder_capability = false;
        bool has_definition_link_support = false;
        bool has_did_change_watched_files_dynamic_registration_capability = false;
        
        message_handler.add<lsp::requests::Initialize>(
            [&](lsp::requests::Initialize::Params&& parameters) -> lsp::requests::Initialize::Result
            {
                lsp::ClientCapabilities const& client_capabilities = parameters.capabilities;

                if (client_capabilities.workspace)
                {
                    has_configuration_capability = client_capabilities.workspace->configuration.value_or(false);
                    has_workspace_folder_capability = client_capabilities.workspace->workspaceFolders.value_or(false);

                    if (client_capabilities.workspace->diagnostics)
                    {
                        has_workspace_diagnostic_refresh_capability = client_capabilities.workspace->diagnostics->refreshSupport.value_or(false);
                    }

                    if (client_capabilities.workspace->inlayHint)
                    {
                        has_workspace_inlay_hint_refresh_capability = client_capabilities.workspace->inlayHint->refreshSupport.value_or(false);
                    }

                    if (client_capabilities.workspace->didChangeWatchedFiles)
                    {
                        has_did_change_watched_files_dynamic_registration_capability = client_capabilities.workspace->didChangeWatchedFiles->dynamicRegistration.value_or(false);
                    }
                }

                if (client_capabilities.textDocument)
                {
                    if (client_capabilities.textDocument->definition)
                    {
                        has_definition_link_support = client_capabilities.textDocument->definition->linkSupport.value_or(false);
                    }
                }

                return initialize(server, parameters);
            }
        );

        message_handler.add<lsp::notifications::Initialized>(
            [&](lsp::notifications::Initialized::Params&& parameters) -> void
            {
                // Registered before the configurations are requested so that files created
                // while the initial workspace build is still in flight are not missed.
                if (has_did_change_watched_files_dynamic_registration_capability)
                {
                    register_did_change_watched_files(message_handler);
                }

                if (has_workspace_folder_capability)
                {
                    message_handler.add<lsp::notifications::Workspace_DidChangeWorkspaceFolders>(
                        [&](lsp::notifications::Workspace_DidChangeWorkspaceFolders::Params&& parameters) -> void
                        {
                        }
                    );

                    request_workspace_configurations(
                        message_handler,
                        server,
                        server.workspace_folders,
                        has_configuration_capability,
                        has_workspace_diagnostic_refresh_capability,
                        has_workspace_inlay_hint_refresh_capability
                    );
                }
            }
        );

        message_handler.add<lsp::requests::Shutdown>(
            [&]() -> lsp::requests::Shutdown::Result
            {
                return shutdown(server);
            }
        );

        message_handler.add<lsp::notifications::Exit>(
            [&]() -> void
            {
                exit(server);
                running = false;
            }
        );

        message_handler.add<lsp::notifications::TextDocument_DidOpen>(
            [&](lsp::notifications::TextDocument_DidOpen::Params&& parameters) -> void
            {
                text_document_did_open(server, parameters);
            }
        );

        message_handler.add<lsp::notifications::TextDocument_DidClose>(
            [&](lsp::notifications::TextDocument_DidClose::Params&& parameters) -> void
            {
                text_document_did_close(server, parameters);
            }
        );

        message_handler.add<lsp::notifications::TextDocument_DidChange>(
            [&](lsp::notifications::TextDocument_DidChange::Params&& parameters) -> void
            {
                text_document_did_change(server, parameters);
            }
        );

        message_handler.add<lsp::requests::TextDocument_CodeAction>(
            [&](lsp::requests::TextDocument_CodeAction::Params&& parameters) -> lsp::requests::TextDocument_CodeAction::Result
            {
                return compute_text_document_code_actions(server, parameters);
            }
        );

        message_handler.add<lsp::requests::TextDocument_Completion>(
            [&](lsp::requests::TextDocument_Completion::Params&& parameters) -> lsp::requests::TextDocument_Completion::Result
            {
                return compute_text_document_completion(server, parameters);
            }
        );

        message_handler.add<lsp::requests::TextDocument_Definition>(
            [&](lsp::requests::TextDocument_Definition::Params&& parameters) -> lsp::requests::TextDocument_Definition::Result
            {
                return compute_text_document_definition(server, parameters, has_definition_link_support);
            }
        );

        message_handler.add<lsp::requests::TextDocument_Diagnostic>(
            [&](lsp::requests::TextDocument_Diagnostic::Params&& parameters) -> lsp::requests::TextDocument_Diagnostic::Result
            {
                return compute_document_diagnostics(server, parameters);
            }
        );

        message_handler.add<lsp::requests::TextDocument_InlayHint>(
            [&](lsp::requests::TextDocument_InlayHint::Params&& parameters) -> lsp::requests::TextDocument_InlayHint::Result
            {
                return compute_document_inlay_hints(server, parameters);
            }
        );

        message_handler.add<lsp::requests::TextDocument_SignatureHelp>(
            [&](lsp::requests::TextDocument_SignatureHelp::Params&& parameters) -> lsp::requests::TextDocument_SignatureHelp::Result
            {
                return compute_text_document_signature_help(server, parameters);
            }
        );

        

        message_handler.add<lsp::requests::Workspace_Diagnostic>(
            [&](lsp::requests::Workspace_Diagnostic::Params&& parameters) -> lsp::requests::Workspace_Diagnostic::Result
            {
                return compute_workspace_diagnostics(server, parameters);
            }
        );

        message_handler.add<Recompute_diagnostics>(
            [&]() -> void
            {
                invalidate_all_diagnostics(server);

                if (has_workspace_diagnostic_refresh_capability)
                {
                    message_handler.sendRequest<lsp::requests::Workspace_Diagnostic_Refresh>(
                        [](lsp::Workspace_Diagnostic_RefreshResult&&)
                        {
                        }
                    );
                }
            }
        );

        message_handler.add<lsp::notifications::Workspace_DidChangeWatchedFiles>(
            [&](lsp::notifications::Workspace_DidChangeWatchedFiles::Params&& parameters) -> void
            {
                // TODO debounce rebuilds across back-to-back notifications. Events within a
                // single notification are already coalesced into one rebuild, but the message
                // loop has no idle hook to defer work to.
                bool const rebuilt = workspace_did_change_watched_files(server, parameters);
                if (!rebuilt)
                    return;

                message_handler.sendNotification<Workspace_rebuilt>();

                if (has_workspace_diagnostic_refresh_capability)
                {
                    message_handler.sendRequest<lsp::requests::Workspace_Diagnostic_Refresh>(
                        [](lsp::Workspace_Diagnostic_RefreshResult&&)
                        {
                        }
                    );
                }

                if (has_workspace_inlay_hint_refresh_capability)
                {
                    message_handler.sendRequest<lsp::requests::Workspace_InlayHint_Refresh>(
                        [](lsp::Workspace_InlayHint_RefreshResult&&)
                        {
                        }
                    );
                }
            }
        );

         while(running)
         {
            try
            {
                message_handler.processIncomingMessages();
            }
            catch (const std::exception& e)
            {
                std::fprintf(stderr, "Error processing message: %s\n", e.what());
            }
         }
    }

    void request_workspace_configurations(
        lsp::MessageHandler& message_handler,
        Server& server,
        std::span<lsp::WorkspaceFolder const> const workspace_folders,
        bool const has_configuration_capability,
        bool const has_workspace_diagnostic_refresh_capability,
        bool const has_workspace_inlay_hint_refresh_capability
    )
    {
        if (!has_configuration_capability)
        {
            // TODO set global configuration
        }

        lsp::Array<lsp::ConfigurationItem> configuration_items;
        configuration_items.reserve(workspace_folders.size());

        for (lsp::WorkspaceFolder const& workspace_folder : workspace_folders)
            configuration_items.push_back(lsp::ConfigurationItem{ .scopeUri = workspace_folder.uri });

        lsp::requests::Workspace_Configuration::Params workspace_configuration_parameters
        {
            .items = std::move(configuration_items),
        };

        message_handler.sendRequest<lsp::requests::Workspace_Configuration>(
            std::move(workspace_configuration_parameters),
            [=, &message_handler, &server](lsp::requests::Workspace_Configuration::Result&& result)
            {
                set_workspace_folder_configurations(server, result);

                message_handler.sendNotification<Workspace_initialized>();

                if (has_workspace_diagnostic_refresh_capability)
                {
                    message_handler.sendRequest<lsp::requests::Workspace_Diagnostic_Refresh>(
                        [](lsp::Workspace_Diagnostic_RefreshResult&& result)
                        {
                        }
                    );
                }

                if (has_workspace_inlay_hint_refresh_capability)
                {
                    message_handler.sendRequest<lsp::requests::Workspace_InlayHint_Refresh>(
                        [](lsp::Workspace_InlayHint_RefreshResult&& result)
                        {
                        }
                    );
                }
            },
            [](const lsp::Error& error)
            {
                std::fprintf(stderr, error.message());
            }
        );
    }

    void register_did_change_watched_files(
        lsp::MessageHandler& message_handler
    )
    {
        // The protocol has no static server-side declaration for file watching, so the watchers
        // have to be registered dynamically. The patterns are kept here rather than in a client
        // so that they stay next to the workspace discovery logic they must agree with.
        lsp::DidChangeWatchedFilesRegistrationOptions registration_options
        {
            .watchers =
            {
                lsp::FileSystemWatcher{ .globPattern = lsp::Pattern{"**/*.iris"} },
                lsp::FileSystemWatcher{ .globPattern = lsp::Pattern{"**/iris_artifact.json"} },
                lsp::FileSystemWatcher{ .globPattern = lsp::Pattern{"**/iris_presets.json"} },
                lsp::FileSystemWatcher{ .globPattern = lsp::Pattern{"**/iris_repository.json"} },
            },
        };

        lsp::requests::Client_RegisterCapability::Params registration_parameters
        {
            .registrations =
            {
                lsp::Registration
                {
                    .id = "iris-did-change-watched-files",
                    .method = std::string{lsp::notifications::Workspace_DidChangeWatchedFiles::Method},
                    .registerOptions = lsp::toJson(std::move(registration_options)),
                },
            },
        };

        message_handler.sendRequest<lsp::requests::Client_RegisterCapability>(
            std::move(registration_parameters),
            [](lsp::requests::Client_RegisterCapability::Result&&)
            {
            },
            [](const lsp::Error& error)
            {
                std::fprintf(stderr, error.message());
            }
        );
    }
}
