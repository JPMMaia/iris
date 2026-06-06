export module iris.c_macro_parser;

import std;

import iris.core;

namespace iris::c
{
    export struct Macro_replacement_text_entry
    {
        std::uint32_t line = 0;
        std::pmr::string name;
        std::optional<std::pmr::string> replacement_text;
    };

    export std::optional<std::pmr::vector<Macro_replacement_text_entry>> get_macro_replacement_text_entries(
        std::filesystem::path const& file_path
    );

    export std::optional<std::pmr::string> get_macro_replacement_text(
        std::string_view macro_name,
        iris::Source_range_location const& source_location
    );

    export std::optional<iris::Statement> parse_macro_replacement_text_to_statement(
        std::string_view replacement_text
    );
}
