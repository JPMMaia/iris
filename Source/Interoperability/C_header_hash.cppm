export module iris.c_header_hash;

import std;

namespace iris::c
{
    export std::optional<std::uint64_t> calculate_header_file_hash(
        std::filesystem::path const& header_path,
        std::optional<std::string_view> target_triple,
        std::span<std::filesystem::path const> include_directories
    );
}
