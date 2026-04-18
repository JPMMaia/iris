module;

#include <functional>
#include <string>

#include <lsp/types.h>

export module iris.language_server.debug;

namespace iris::language_server
{
    export void send_debug_message(
        std::function<void(lsp::LogMessageParams&&)> const& window_log_message,
        std::string message
    )
    {
        window_log_message({.type = lsp::MessageType::Debug, .message = message});
    }
}
