#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_all.hpp>
#include <lsp/types.h>

import iris.compiler;
import iris.core;
import iris.core.declarations;
import iris.language_server.go_to_location;
import iris.parser.convertor;
import iris.parser.parse_tree;
import iris.parser.parser;

namespace iris::language_server
{
    namespace
    {
        constexpr std::string_view g_cursor_marker = "$CURSOR_POSITION";

        struct Source_with_cursor
        {
            std::string source;
            lsp::Position position;
        };

        struct Parse_session
        {
            iris::parser::Parser parser = iris::parser::create_parser();
            std::vector<iris::Module> modules;
            std::vector<iris::parser::Parse_tree> trees;

            ~Parse_session()
            {
                for (iris::parser::Parse_tree& tree : trees)
                    iris::parser::destroy_tree(std::move(tree));

                iris::parser::destroy_parser(std::move(parser));
            }

            std::optional<std::size_t> add_module(
                std::string_view const source,
                std::optional<std::filesystem::path> const& source_file_path = std::nullopt
            )
            {
                std::pmr::u8string utf_8_source{
                    reinterpret_cast<char8_t const*>(source.data()),
                    source.size()
                };

                iris::parser::Parse_tree tree = iris::parser::parse(parser, std::move(utf_8_source));
                iris::parser::Parse_node const root = iris::parser::get_root_node(tree);

                std::optional<iris::Module> const module = iris::parser::parse_node_to_module(
                    tree,
                    root,
                    source_file_path,
                    {},
                    {}
                );
                if (!module.has_value())
                {
                    iris::parser::destroy_tree(std::move(tree));
                    return std::nullopt;
                }

                modules.push_back(module.value());
                trees.push_back(std::move(tree));
                return modules.size() - 1;
            }
        };

        static Source_with_cursor extract_cursor_position(std::string source_with_marker)
        {
            std::size_t const marker_index = source_with_marker.find(g_cursor_marker);
            REQUIRE(marker_index != std::string::npos);
            REQUIRE(source_with_marker.find(g_cursor_marker, marker_index + 1) == std::string::npos);

            std::uint32_t line = 0;
            std::uint32_t column = 0;
            for (std::size_t i = 0; i < marker_index; ++i)
            {
                if (source_with_marker[i] == '\n')
                {
                    ++line;
                    column = 0;
                }
                else
                {
                    ++column;
                }
            }

            source_with_marker.erase(marker_index, g_cursor_marker.size());

            return Source_with_cursor{
                .source = std::move(source_with_marker),
                .position = lsp::Position{.line = line, .character = column},
            };
        }

        static iris::Declaration_database create_declaration_database(std::span<iris::Module const> const modules)
        {
            std::pmr::vector<iris::Module const*> const sorted_modules = iris::compiler::sort_core_modules(
                modules,
                {},
                {}
            );

            return iris::compiler::create_declaration_database_and_add_modules({}, sorted_modules);
        }

        static lsp::TextDocument_DefinitionResult run_go_to_definition_test(
            std::string_view const source,
            std::span<std::string_view const> const dependencies = {}
        )
        {
            Source_with_cursor const source_with_cursor = extract_cursor_position(source);

            Parse_session session;
            for (std::string_view const dependency : dependencies)
                session.add_module(dependency);
            std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
            REQUIRE(index.has_value());

            iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

            return compute_go_to_definition(
                declaration_database,
                session.trees[index.value()],
                session.modules[index.value()],
                source_with_cursor.position,
                false
            );
        }

        static bool find_location_in_result(
            lsp::TextDocument_DefinitionResult const& result,
            std::uint32_t const expected_line
        )
        {
            REQUIRE(!result.isNull());
            std::vector<lsp::Location>& locations = result.get<std::vector<lsp::Location>>();
            REQUIRE(locations.size() > 0);

            return std::any_of(
                locations.begin(),
                locations.end(),
                [expected_line](lsp::Location const& location)
                {
                    return location.range.start.line == expected_line;
                }
            );
        }

        static std::uint32_t find_lambda_declaration_line(std::string_view const source)
        {
            std::size_t const lambda_index = source.find("lambda ");
            REQUIRE(lambda_index != std::string_view::npos);

            std::uint32_t line = 0;
            for (std::size_t i = 0; i < lambda_index; ++i)
            {
                if (source[i] == '\n')
                    ++line;
            }
            return line;
        }
    }

    TEST_CASE("Extracts cursor marker position and removes marker", "[Language_server][Go_to_location]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;
function foo() -> ()
{
    var x = $CURSOR_POSITION42;
}
)"
        );

        CHECK(source_with_cursor.position.line == 3);
        CHECK(source_with_cursor.position.character == 12);
        CHECK(source_with_cursor.source.find(std::string{g_cursor_marker}) == std::string::npos);
    }

    // ============================================================================
    // Lambda Go-to-Definition Tests
    // ============================================================================

    TEST_CASE("Go to definition navigates to lambda declaration from type reference", "[Language_server][Go_to_location][Lambda]")
    {
        // Verifies that clicking on a lambda type name in a variable declaration navigates to the lambda declaration.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: $CURSOR_POSITIONComparator = lambda(a, b) => a - b;
}
)";

        lsp::TextDocument_DefinitionResult const result = run_go_to_definition_test(source);
        std::uint32_t const expected_line = find_lambda_declaration_line(source);
        CHECK(find_location_in_result(result, expected_line));
    }

    TEST_CASE("Go to definition navigates to lambda declaration from function parameter type", "[Language_server][Go_to_location][Lambda]")
    {
        // Verifies that clicking on a lambda type name in a function parameter navigates to the lambda declaration.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function apply(cmp: $CURSOR_POSITIONComparator, x: Int32, y: Int32) -> (result: Int32)
{
    return cmp(x, y);
}
)";

        lsp::TextDocument_DefinitionResult const result = run_go_to_definition_test(source);
        std::uint32_t const expected_line = find_lambda_declaration_line(source);
        CHECK(find_location_in_result(result, expected_line));
    }

    TEST_CASE("Go to definition navigates to lambda declaration from lambda literal assignment", "[Language_server][Go_to_location][Lambda]")
    {
        // Verifies that clicking on a lambda type name followed by a lambda literal navigates to the lambda declaration.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: Comparator$CURSOR_POSITION = lambda(a, b) => a - b;
}
)";

        lsp::TextDocument_DefinitionResult const result = run_go_to_definition_test(source);
        std::uint32_t const expected_line = find_lambda_declaration_line(source);
        CHECK(find_location_in_result(result, expected_line));
    }

    TEST_CASE("Go to definition for lambda type used in struct member", "[Language_server][Go_to_location][Lambda]")
    {
        // Verifies that clicking on a lambda type name in a struct member navigates to the lambda declaration.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

struct Comparator_wrapper
{
    cmp: Comparator$CURSOR_POSITION;
}
)";

        lsp::TextDocument_DefinitionResult const result = run_go_to_definition_test(source);
        std::uint32_t const expected_line = find_lambda_declaration_line(source);
        CHECK(find_location_in_result(result, expected_line));
    }

    TEST_CASE("Go to definition for lambda type from another module", "[Language_server][Go_to_location][Lambda]")
    {
        // Verifies that clicking on an imported lambda type navigates to the lambda declaration in the other module.
        std::string_view const other_module_source =
            R"(module other;

export lambda Comparator(a: Int32, b: Int32) -> (result: Int32);
)";

        std::string_view const source =
            R"(module A;

import other as other;

export function main() -> ()
{
    var cmp: other.Comparator$CURSOR_POSITION = lambda(a, b) => a - b;
}
)";

        lsp::TextDocument_DefinitionResult const result = run_go_to_definition_test(source, {&other_module_source, 1});
        std::uint32_t const expected_line = find_lambda_declaration_line(other_module_source);
        CHECK(find_location_in_result(result, expected_line));
    }

    TEST_CASE("Go to definition for lambda literal with no named type", "[Language_server][Go_to_location][Lambda]")
    {
        // Verifies that clicking on a lambda literal navigates to the matching lambda declaration by type.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: Comparator = $CURSOR_POSITION(a, b) => a - b;
}
)";

        lsp::TextDocument_DefinitionResult const result = run_go_to_definition_test(source);
        std::uint32_t const expected_line = find_lambda_declaration_line(source);
        CHECK(find_location_in_result(result, expected_line));
    }

    TEST_CASE("Go to definition for lambda call expression", "[Language_server][Go_to_location][Lambda]")
    {
        // Verifies that clicking on a lambda-typed variable used in a call expression navigates to the lambda declaration.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function apply(cmp: Comparator, x: Int32, y: Int32) -> (result: Int32)
{
    return $CURSOR_POSITIONcmp(x, y);
}
)";

        lsp::TextDocument_DefinitionResult const result = run_go_to_definition_test(source);
        std::uint32_t const expected_line = find_lambda_declaration_line(source);
        CHECK(find_location_in_result(result, expected_line));
    }

    TEST_CASE("Go to definition for nested lambda type", "[Language_server][Go_to_location][Lambda]")
    {
        // Verifies that clicking on a lambda type name inside a nested lambda expression navigates to the correct lambda declaration.
        std::string_view const source =
            R"(module A;

lambda Inner(x: Int32) -> (result: Int32);
lambda Outer(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var outer: Outer = lambda(a, b) => {
        var inner: Inner$CURSOR_POSITION = lambda(x) => x + a;
        return inner(b);
    };
}
)";

        lsp::TextDocument_DefinitionResult const result = run_go_to_definition_test(source);
        std::uint32_t const expected_line = find_lambda_declaration_line(source);
        CHECK(find_location_in_result(result, expected_line));
    }

    TEST_CASE("Go to definition for lambda type in function return type", "[Language_server][Go_to_location][Lambda]")
    {
        // Verifies that clicking on a lambda type name in a function return type navigates to the lambda declaration.
        std::string_view const source =
            R"(module A;

lambda Mapper(value: Int32) -> (result: Int32);

export function create_mapper() -> (mapper: Mapper$CURSOR_POSITION)
{
    return (x) => x * 2;
}
)";

        lsp::TextDocument_DefinitionResult const result = run_go_to_definition_test(source);
        std::uint32_t const expected_line = find_lambda_declaration_line(source);
        CHECK(find_location_in_result(result, expected_line));
    }

    TEST_CASE("Go to definition for lambda type in struct with multiple lambdas", "[Language_server][Go_to_location][Lambda]")
    {
        // Verifies that clicking on a lambda type name in a struct member with multiple lambda types navigates to the correct declaration.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);
lambda Mapper(value: Int32) -> (result: Int32);

struct Data
{
    cmp: Comparator$CURSOR_POSITION;
    mapper: Mapper;
}
)";

        lsp::TextDocument_DefinitionResult const result = run_go_to_definition_test(source);
        std::uint32_t const expected_line = find_lambda_declaration_line(source);
        CHECK(find_location_in_result(result, expected_line));
    }

    TEST_CASE("Go to definition for lambda declaration itself", "[Language_server][Go_to_location][Lambda]")
    {
        // Verifies that clicking on a lambda name in its own declaration navigates to the declaration.
        std::string_view const source =
            R"(module A;

lambda Comparator$CURSOR_POSITION(a: Int32, b: Int32) -> (result: Int32);
)";

        lsp::TextDocument_DefinitionResult const result = run_go_to_definition_test(source);
        std::uint32_t const expected_line = find_lambda_declaration_line(source);
        CHECK(find_location_in_result(result, expected_line));
    }
}
