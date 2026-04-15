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
                if (has_workspace_folder_capability)
                {
                    message_handler.add<lsp::notifications::Workspace_DidChangeWorkspaceFolders>(
                        [&](lsp::notifications::Workspace_DidChangeWorkspaceFolders::Params&& parameters) -> void
                        {
                            connection.writeMessage("Workspace folder change event received.");
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

        

        message_handler.add<lsp::requests::Workspace_Diagnostic>(
            [&](lsp::requests::Workspace_Diagnostic::Params&& parameters) -> lsp::requests::Workspace_Diagnostic::Result
            {
                return compute_workspace_diagnostics(server, parameters);
            }
        );

        // TODO use workspace/didChangeWatchedFiles to watch for changes in artifact and repository files
      
         while(running)
            message_handler.processIncomingMessages();
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
}
