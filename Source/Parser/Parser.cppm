module;

#include <tree_sitter/api.h>

export module iris.parser.parser;

import std;

import iris.core;
import iris.parser.parse_tree;

namespace iris::parser
{
    struct Parser_data
    {
        TSLanguage const* language = nullptr;
        TSParser* parser = nullptr;
        
        std::filesystem::path parser_javascript_path;
    };

    export using Parser = Parser_data;

    export Parser create_parser();
    export void destroy_parser(Parser&& parser);

    export Parse_tree parse(Parser const& parser, std::pmr::u8string text);
    export void destroy_tree(Parse_tree&& tree);

    export Parse_tree edit_tree(
        Parser const& parser,
        Parse_tree&& previous_parse_tree,
        iris::Source_range const range,
        std::u8string_view const new_text
    );

    export std::optional<std::pmr::string> read_module_name(std::filesystem::path const& unparsed_file_path);
}
