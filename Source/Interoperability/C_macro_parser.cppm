export module iris.c_macro_parser;

import std;

import iris.core;

namespace iris::c
{
    export std::optional<std::pmr::string> get_macro_replacement_text(
        std::string_view macro_name,
        iris::Source_range_location const& source_location
    );

    export std::optional<iris::Statement> parse_macro_replacement_text_to_statement(
        std::string_view replacement_text
    );
}
