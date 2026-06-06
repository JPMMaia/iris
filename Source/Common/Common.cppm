export module iris.common;

import std;

namespace iris::common
{
    export void print_message_and_exit(std::string const& message);
    export void print_message_and_exit(char const* const message);

    export std::optional<std::pmr::vector<std::byte>> read_binary_file(char const* const path);
    export std::optional<std::pmr::vector<std::byte>> read_binary_file(std::filesystem::path const& path);

    export void write_binary_file(char const* const path, std::span<std::byte const> const content);
    export void write_binary_file(std::filesystem::path const& path, std::span<std::byte const> const content);

    export std::optional<std::pmr::string> get_file_contents(char const* const path);
    export std::optional<std::pmr::string> get_file_contents(std::filesystem::path const& path);

    export std::optional<std::pmr::u8string> get_file_utf8_contents(char const* const path);
    export std::optional<std::pmr::u8string> get_file_utf8_contents(std::filesystem::path const& path);

    export void write_to_file(char const* const path, std::string_view const content);
    export void write_to_file(std::filesystem::path const& path, std::string_view const content);

    export std::pmr::vector<std::pmr::string> convert_path_to_string(std::span<std::filesystem::path const> const values, std::pmr::polymorphic_allocator<> output_allocator);

    export std::pmr::vector<std::string_view> split_string(std::string_view const value, char const seperator, std::pmr::polymorphic_allocator<> const& output_allocator);

    export void install_abort_handlers();
}
