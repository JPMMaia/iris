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
import iris.compiler.artifact;
import iris.core;
import iris.core.declarations;
import iris.language_server.completion;
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

        static bool find_completion_item(
            lsp::TextDocument_CompletionResult const& result,
            std::string_view const label
        )
        {
            REQUIRE(!result.isNull());
            lsp::CompletionList const& completion_list = result.get<lsp::CompletionList>();

            return std::any_of(
                completion_list.items.begin(),
                completion_list.items.end(),
                [label](lsp::CompletionItem const& item)
                {
                    return item.label == label;
                }
            );
        }

        static lsp::TextDocument_CompletionResult run_completion_test(
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

            std::vector<iris::compiler::Artifact> const artifacts{};
            std::vector<iris::Module> const header_modules{};

            return compute_completion(
                artifacts,
                header_modules,
                session.modules,
                declaration_database,
                session.trees[index.value()],
                session.modules[index.value()],
                source_with_cursor.position
            );
        }
    }

    TEST_CASE("Extracts cursor marker position and removes marker", "[Language_server][Completion]")
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
    // Lambda Completion Tests
    // ============================================================================

    TEST_CASE("Completes 'lambda' keyword in module body", "[Language_server][Completion][Lambda]")
    {
        // Verifies that 'lambda' is offered as a keyword completion at module body scope.
        std::string_view const source =
            R"(module A;

$CURSOR_POSITION
)";

        lsp::TextDocument_CompletionResult const result = run_completion_test(source);
        CHECK(find_completion_item(result, "lambda"));
    }

    TEST_CASE("Completes lambda type name as a type", "[Language_server][Completion][Lambda]")
    {
        // Verifies that a named lambda type is offered as a type completion in a variable declaration.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: $CURSOR_POSITION
}
)";

        lsp::TextDocument_CompletionResult const result = run_completion_test(source);
        CHECK(find_completion_item(result, "Comparator"));
    }

    TEST_CASE("Completes lambda type name from another module", "[Language_server][Completion][Lambda]")
    {
        // Verifies that a lambda type defined in an imported module is offered after a dot-accessor.
        std::string_view const other_module_source =
            R"(module other;

export lambda Comparator(a: Int32, b: Int32) -> (result: Int32);
)";

        std::string_view const source =
            R"(module A;

import other as other;

export function main() -> ()
{
    var cmp: other.$CURSOR_POSITION
}
)";

        lsp::TextDocument_CompletionResult const result = run_completion_test(source, {&other_module_source, 1});
        CHECK(find_completion_item(result, "Comparator"));
    }

    TEST_CASE("Completes lambda type name in struct member", "[Language_server][Completion][Lambda]")
    {
        // Verifies that a named lambda type is offered as a struct member type completion.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

struct Data
{
    cmp: $CURSOR_POSITION
}
)";

        lsp::TextDocument_CompletionResult const result = run_completion_test(source);
        CHECK(find_completion_item(result, "Comparator"));
    }

    TEST_CASE("Completes lambda type name in function parameter", "[Language_server][Completion][Lambda]")
    {
        // Verifies that a named lambda type is offered as a function parameter type completion.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function apply(cmp: $CURSOR_POSITION, x: Int32, y: Int32) -> (result: Int32)
{
    return cmp(x, y);
}
)";

        lsp::TextDocument_CompletionResult const result = run_completion_test(source);
        CHECK(find_completion_item(result, "Comparator"));
    }

    TEST_CASE("Completes lambda type name in function return type", "[Language_server][Completion][Lambda]")
    {
        // Verifies that a named lambda type is offered as a function return type completion.
        std::string_view const source =
            R"(module A;

lambda Mapper(value: Int32) -> (result: Int32);

export function create_mapper() -> (mapper: $CURSOR_POSITION)
{
    return (x) => x * 2;
}
)";

        lsp::TextDocument_CompletionResult const result = run_completion_test(source);
        CHECK(find_completion_item(result, "Mapper"));
    }

    TEST_CASE("Completes lambda type name in variable declaration", "[Language_server][Completion][Lambda]")
    {
        // Verifies that a named lambda type is offered as a variable type completion inside a function body.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: $CURSOR_POSITION = (a, b) => a - b;
}
)";

        lsp::TextDocument_CompletionResult const result = run_completion_test(source);
        CHECK(find_completion_item(result, "Comparator"));
    }

    TEST_CASE("Completes lambda type name with partial input", "[Language_server][Completion][Lambda]")
    {
        // Verifies that only lambda types matching the typed prefix are offered.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);
lambda Mapper(value: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: Com$CURSOR_POSITION
}
)";

        lsp::TextDocument_CompletionResult const result = run_completion_test(source);
        CHECK(find_completion_item(result, "Comparator"));
        CHECK(!find_completion_item(result, "Mapper"));
    }

    TEST_CASE("Completes lambda type name in nested lambda context", "[Language_server][Completion][Lambda]")
    {
        // Verifies that a named lambda type is offered inside a nested lambda expression body.
        std::string_view const source =
            R"(module A;

lambda Inner(x: Int32) -> (result: Int32);
lambda Outer(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var outer: Outer = (a, b) => {
        var inner: $CURSOR_POSITION = (x) => x + a;
        return inner(b);
    };
}
)";

        lsp::TextDocument_CompletionResult const result = run_completion_test(source);
        CHECK(find_completion_item(result, "Inner"));
    }
}
