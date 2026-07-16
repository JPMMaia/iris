module;

#include <cstdio>
#include <span>
#include <string_view>

#include <lsp/io/socket.h>
#include <lsp/messagehandler.h>
#include <lsp/types.h>

export module iris.language_server.message_handler;

import iris.language_server.server;

namespace iris::language_server
{
    export struct Message_handler
    {
    };

    export Message_handler create_message_handler();

    export void process_messages(
        Message_handler& message_handler
    );

    void run_message_handler(
        lsp::io::Socket socket
    );

    void request_workspace_configurations(
        lsp::MessageHandler& message_handler,
        Server& server,
        std::span<lsp::WorkspaceFolder const> const workspace_folders,
        bool const has_configuration_capability,
        bool const has_workspace_diagnostic_refresh_capability,
        bool const has_workspace_inlay_hint_refresh_capability
    );

    struct Workspace_initialized
    {
        static constexpr auto Method = std::string_view("iris/workspaceInitialized");
        static constexpr auto Direction = lsp::MessageDirection::ServerToClient;
        static constexpr auto Type = lsp::Message::Notification;
    };

    struct Recompute_diagnostics
    {
        static constexpr auto Method = std::string_view("iris/recomputeDiagnostics");
        static constexpr auto Direction = lsp::MessageDirection::ClientToServer;
        static constexpr auto Type = lsp::Message::Notification;
    };

    // Announces that a watched file event rebuilt the workspace. Unlike Workspace_initialized,
    // which only ever fires once, this may fire any number of times, which lets a client (or a
    // test) wait for a specific rebuild to finish.
    struct Workspace_rebuilt
    {
        static constexpr auto Method = std::string_view("iris/workspaceRebuilt");
        static constexpr auto Direction = lsp::MessageDirection::ServerToClient;
        static constexpr auto Type = lsp::Message::Notification;
    };

    void register_did_change_watched_files(
        lsp::MessageHandler& message_handler
    );
}
