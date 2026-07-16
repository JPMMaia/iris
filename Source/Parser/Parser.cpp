module;

#include <assert.h>

#include <tree_sitter/api.h>

module iris.parser.parser;

import std;

import iris.common;
import iris.common.filesystem;
import iris.core;

extern "C"
{
    TSLanguage const* tree_sitter_iris(void);
}

namespace iris::parser
{
    Parser create_parser()
    {
        TSLanguage const* language = tree_sitter_iris();

        TSParser* parser = ts_parser_new();
        bool const success = ts_parser_set_language(parser, language);
        if (!success)
            iris::common::print_message_and_exit("Failed to set tree sitter language!");

        return Parser
        {
            .language = language,
            .parser = parser
        };
    }

    void destroy_parser(Parser&& parser)
    {
        if (parser.parser != nullptr)
        {
            ts_parser_delete(parser.parser);
            parser.parser = nullptr;
        }

        if (parser.language != nullptr)
        {
            ts_language_delete(parser.language);
            parser.language = nullptr;
        }
    }

    Parse_tree parse(Parser const& parser, std::pmr::u8string text)
    {
        TSTree* tree = ts_parser_parse_string(
            parser.parser,
            nullptr,
            reinterpret_cast<char const*>(text.data()),
            text.size()
        );

        return Parse_tree
        { 
            .text = std::move(text),
            .ts_tree = tree
        };
    }

    void destroy_tree(Parse_tree&& tree)
    {
        if (tree.ts_tree == nullptr)
            return;

        ts_tree_delete(tree.ts_tree);
        tree.ts_tree = nullptr;
    }

    static std::uint32_t calculate_new_end_byte(
        std::uint32_t const start_byte,
        std::uint32_t const old_end_byte,
        std::uint32_t const new_text_size_in_bytes
    )
    {
        std::uint32_t const bytes_to_remove = old_end_byte - start_byte;
        std::uint32_t const new_end_byte = start_byte + new_text_size_in_bytes;
        return new_end_byte;    
    }

    static TSPoint calculate_byte_point(
        std::u8string_view const text,
        std::uint32_t const target_byte
    )
    {
        assert(target_byte <= text.size());

        TSPoint point{};

        for (std::uint32_t current_byte = 0; current_byte < target_byte; ++current_byte)
        {
            char8_t const character = text[current_byte];
            if (character == '\n')
            {
                point.row += 1;
                point.column = 0;
            }
            else
            {
                point.column += 1;
            }
        }

        return point;
    }

    static void edit_text(
        std::pmr::u8string& text_to_edit,
        std::uint32_t const start_byte,
        std::uint32_t const end_byte,
        std::u8string_view const new_text
    )
    {
        std::uint32_t const count = end_byte - start_byte;
        text_to_edit.replace(
            start_byte,
            count,
            new_text
        );
    }

    Parse_tree edit_tree(
        Parser const& parser,
        Parse_tree&& previous_parse_tree,
        iris::Source_range const range,
        std::u8string_view const new_text
    )
    {
        TSPoint const start_code_point{range.start.line - 1, range.start.column - 1};
        TSPoint const end_code_point{range.end.line - 1, range.end.column - 1};

        std::uint32_t const start_byte = calculate_byte(
            previous_parse_tree.text,
            TSPoint{},
            0,
            start_code_point
        );
        std::uint32_t const old_end_byte = calculate_byte(
            previous_parse_tree.text,
            start_code_point,
            start_byte,
            end_code_point
        );

        TSPoint const start_point = calculate_byte_point(previous_parse_tree.text, start_byte);
        TSPoint const old_end_point = calculate_byte_point(previous_parse_tree.text, old_end_byte);

        edit_text(previous_parse_tree.text, start_byte, old_end_byte, new_text);
        std::pmr::u8string& text_after_edit = previous_parse_tree.text;

        std::uint32_t const new_end_byte = calculate_new_end_byte(start_byte, old_end_byte, new_text.size());
        TSPoint const new_end_point = calculate_byte_point(text_after_edit, new_end_byte);

        TSInputEdit const edit
        {
            .start_byte = start_byte,
            .old_end_byte = old_end_byte,
            .new_end_byte = new_end_byte,
            .start_point = start_point,
            .old_end_point = old_end_point,
            .new_end_point = new_end_point,
        };

        ts_tree_edit(previous_parse_tree.ts_tree, &edit);

        TSTree* new_tree = ts_parser_parse_string(
            parser.parser,
            previous_parse_tree.ts_tree,
            reinterpret_cast<char const*>(text_after_edit.data()),
            text_after_edit.size()
        );

        destroy_tree(std::move(previous_parse_tree));

        return Parse_tree
        { 
            .text = std::move(text_after_edit),
            .ts_tree = new_tree
        };
    }

    std::optional<std::pmr::string> read_module_name(std::filesystem::path const& unparsed_file_path)
    {
        std::string const path_string = unparsed_file_path.generic_string();
        std::FILE* file_stream = std::fopen(path_string.c_str(), "r");
        if (file_stream == nullptr)
            return std::nullopt;

        std::optional<std::pmr::string> module_name = std::nullopt;

        constexpr int line_size = 1000;
        char line[line_size];
        while (true)
        {
            if (std::fgets(line, line_size, file_stream) == nullptr)
                break;

            char const* const end = std::find(line, line + line_size, '\0');

            std::string_view const line_view{ line, end };

            std::string_view::size_type const line_without_spaces_begin = line_view.find_first_not_of(' ');
            if (line_without_spaces_begin == std::string_view::npos)
                continue;

            std::string_view const line_without_spaces{ line + line_without_spaces_begin, end - 1 };

            if (line_without_spaces.starts_with("module ") && line_without_spaces.ends_with(';'))
            {
                module_name = line_without_spaces.substr(7, line_without_spaces.size() - 8);
                break;
            }
        }

        std::fclose(file_stream);

        return module_name;
    }
}
