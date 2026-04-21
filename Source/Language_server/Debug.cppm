module;

#include <cstdio>
#include <filesystem>
#include <string_view>

export module iris.language_server.debug;

namespace iris::language_server
{
    export void write_debug_message(
        std::string_view message
    )
    {
        static constexpr char const* path_string = "build/logs/debug-language-server.log";
        std::filesystem::path const path = path_string;
        if (!std::filesystem::exists(path.parent_path()))
        {
            std::filesystem::create_directories(path.parent_path());
        }

        std::FILE* file = fopen(path_string, "a");
        if (!file) {
            std::perror("Failed to open file");
            return;
        }

        std::fputs(message.data(), file);
        std::fclose(file);
    }
}
