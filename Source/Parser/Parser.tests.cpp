#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

import iris.core;
import iris.parser.parse_tree;
import iris.parser.parser;

namespace iris::parser
{
    static bool is_utf_8_start_byte(char8_t const character)
    {
        return (character & 0b11000000) != 0b10000000;
    }

    static Source_position byte_offset_to_source_position(
        std::u8string_view const text,
        std::uint32_t const byte_offset
    )
    {
        REQUIRE(byte_offset <= text.size());

        Source_position position
        {
            .line = 1,
            .column = 1,
        };

        for (std::uint32_t byte_index = 0; byte_index < byte_offset; ++byte_index)
        {
            char8_t const character = text[byte_index];

            if (character == '\n')
            {
                position.line += 1;
                position.column = 1;
                continue;
            }

            if (is_utf_8_start_byte(character))
                position.column += 1;
        }

        return position;
    }

    static std::uint32_t find_byte_offset(
        std::u8string_view const text,
        std::u8string_view const needle
    )
    {
        std::size_t const index = text.find(needle);
        REQUIRE(index != std::u8string_view::npos);
        return static_cast<std::uint32_t>(index);
    }

    static std::string to_bytes_string(std::u8string_view const text)
    {
        return std::string{
            reinterpret_cast<char const*>(text.data()),
            text.size()
        };
    }

    static Parse_tree parse_source(
        Parser const& parser,
        std::u8string_view const source
    )
    {
        return parse(parser, std::pmr::u8string{source.begin(), source.end()});
    }

    static void check_tree_has_no_errors(
        Parse_tree const& tree
    )
    {
        CHECK_FALSE(has_errors(get_root_node(tree)));
    }

    static void check_edited_text_and_tree(
        Parse_tree const& tree,
        std::u8string_view const expected
    )
    {
        CHECK(to_bytes_string(tree.text) == to_bytes_string(expected));
        check_tree_has_no_errors(tree);
    }

    static void run_single_edit_test(
        std::u8string_view const source,
        Source_range const range,
        std::u8string_view const edit_text,
        std::u8string_view const expected
    )
    {
        Parser parser = create_parser();
        Parse_tree tree = parse_source(parser, source);
        Parse_tree edited_tree = edit_tree(parser, std::move(tree), range, edit_text);

        check_edited_text_and_tree(edited_tree, expected);

        destroy_tree(std::move(edited_tree));
        destroy_parser(std::move(parser));
    }

    TEST_CASE("Parser parses UTF-8 string literals", "[Parser][UTF-8]")
    {
        std::u8string const source = u8"module m;\nfunction main() -> () { var text = \"caf\u00E9\"; }\n";

        Parser parser = create_parser();
        Parse_tree tree = parse_source(parser, source);

        check_tree_has_no_errors(tree);

        destroy_tree(std::move(tree));
        destroy_parser(std::move(parser));
    }

    TEST_CASE("Parser edit_tree inserts ASCII after UTF-8 code point", "[Parser][UTF-8]")
    {
        std::u8string const source = u8"module m;\nfunction main() -> () { var text = \"caf\u00E9\"; }\n";

        std::uint32_t const e_acute_byte = find_byte_offset(source, u8"\u00E9\"");
        Source_position const insertion_position = byte_offset_to_source_position(source, e_acute_byte + 2);

        Source_range const range
        {
            .start = insertion_position,
            .end = insertion_position,
        };

        std::u8string const expected = u8"module m;\nfunction main() -> () { var text = \"caf\u00E9!\"; }\n";
        run_single_edit_test(source, range, u8"!", expected);
    }

    TEST_CASE("Parser edit_tree replaces range across UTF-8 boundaries", "[Parser][UTF-8]")
    {
        std::u8string const source = u8"module m;\nfunction main() -> () { var text = \"caf\u00E9\"; }\n";

        std::uint32_t const start_byte = find_byte_offset(source, u8"af\u00E9");
        std::uint32_t const end_byte = start_byte + 4;

        Source_range const range
        {
            .start = byte_offset_to_source_position(source, start_byte),
            .end = byte_offset_to_source_position(source, end_byte),
        };

        std::u8string const expected = u8"module m;\nfunction main() -> () { var text = \"cake\"; }\n";
        run_single_edit_test(source, range, u8"ake", expected);
    }

    TEST_CASE("Parser edit_tree supports sequential UTF-8 edits", "[Parser][UTF-8]")
    {
        std::u8string const source = u8"module m;\nfunction main() -> () { var text = \"caf\u00E9\"; }\n";

        Parser parser = create_parser();
        Parse_tree tree = parse_source(parser, source);

        std::uint32_t const first_insert_byte = find_byte_offset(tree.text, u8"\u00E9\"") + 2;
        Source_position const first_insert_position = byte_offset_to_source_position(tree.text, first_insert_byte);

        Source_range const first_range
        {
            .start = first_insert_position,
            .end = first_insert_position,
        };

        Parse_tree edited_tree = edit_tree(parser, std::move(tree), first_range, u8"!");

        std::uint32_t const second_insert_byte = find_byte_offset(edited_tree.text, u8"text") + 4;
        Source_position const second_insert_position = byte_offset_to_source_position(edited_tree.text, second_insert_byte);

        Source_range const second_range
        {
            .start = second_insert_position,
            .end = second_insert_position,
        };

        Parse_tree twice_edited_tree = edit_tree(parser, std::move(edited_tree), second_range, u8"_new");

        std::u8string const expected = u8"module m;\nfunction main() -> () { var text_new = \"caf\u00E9!\"; }\n";
        check_edited_text_and_tree(twice_edited_tree, expected);

        destroy_tree(std::move(twice_edited_tree));
        destroy_parser(std::move(parser));
    }

    TEST_CASE("Parser rejects non-ASCII identifiers", "[Parser][UTF-8]")
    {
        std::u8string const source = u8"module m;\nfunction main() -> () { var caf\u00E9 = 1; }\n";

        Parser parser = create_parser();
        Parse_tree tree = parse_source(parser, source);

        Parse_node const root = get_root_node(tree);
        CHECK(has_errors(root));

        std::pmr::polymorphic_allocator<> allocator;
        std::pmr::vector<Parse_node> const errors = get_error_or_missing_nodes(root, allocator, allocator);
        CHECK_FALSE(errors.empty());

        destroy_tree(std::move(tree));
        destroy_parser(std::move(parser));
    }
}
