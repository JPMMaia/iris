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
import iris.language_server.hover;
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

        static lsp::TextDocument_HoverResult run_hover_test(
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

            return compute_hover(
                declaration_database,
                session.trees[index.value()],
                session.modules[index.value()],
                source_with_cursor.position
            );
        }

        static bool has_hover_with_content(
            lsp::TextDocument_HoverResult const& result,
            std::string_view const expected_content
        )
        {
            if (result.isNull())
                return false;

            std::vector<lsp::MarkedString>& contents = result.get<std::vector<lsp::MarkedString>>();
            return std::any_of(
                contents.begin(),
                contents.end(),
                [expected_content](lsp::MarkedString const& ms)
                {
                    if (ms.get_str().has_value())
                    {
                        return ms.get_str()->find(expected_content) != std::string_view::npos;
                    }
                    return false;
                }
            );
        }

        static bool has_hover_with_lambda_signature(
            lsp::TextDocument_HoverResult const& result,
            std::string_view const lambda_name
        )
        {
            if (result.isNull())
                return false;

            std::vector<lsp::MarkedString>& contents = result.get<std::vector<lsp::MarkedString>>();
            std::string_view const expected_signature = "lambda " + std::string{lambda_name};
            return std::any_of(
                contents.begin(),
                contents.end(),
                [expected_signature](lsp::MarkedString const& ms)
                {
                    if (ms.get_str().has_value())
                    {
                        return ms.get_str()->find(expected_signature) != std::string_view::npos;
                    }
                    return false;
                }
            );
        }

        static bool has_hover_with_capture_info(
            lsp::TextDocument_HoverResult const& result,
            std::string_view const variable_name
        )
        {
            if (result.isNull())
                return false;

            std::vector<lsp::MarkedString>& contents = result.get<std::vector<lsp::MarkedString>>();
            return std::any_of(
                contents.begin(),
                contents.end(),
                [variable_name](lsp::MarkedString const& ms)
                {
                    if (ms.get_str().has_value())
                    {
                        return ms.get_str()->find(variable_name) != std::string_view::npos;
                    }
                    return false;
                }
            );
        }
    }

    TEST_CASE("Extracts cursor marker position and removes marker", "[Language_server][Hover]")
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
    // Lambda Hover Tests
    // ============================================================================

    TEST_CASE("Hover on lambda type reference shows full signature", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over a lambda type name in a variable declaration
        // shows the full lambda signature.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: Comparator$CURSOR_POSITION = lambda(a, b) => a - b;
}
)";

        lsp::TextDocument_HoverResult const result = run_hover_test(source);
        CHECK(has_hover_with_lambda_signature(result, "Comparator"));
        CHECK(has_hover_with_content(result, "a: Int32"));
        CHECK(has_hover_with_content(result, "b: Int32"));
        CHECK(has_hover_with_content(result, "result: Int32"));
    }

    TEST_CASE("Hover on lambda type reference in function parameter shows full signature", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over a lambda type name in a function parameter
        // shows the full lambda signature.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function apply(cmp: Comparator$CURSOR_POSITION, x: Int32, y: Int32) -> (result: Int32)
{
    return cmp(x, y);
}
)";

        lsp::TextDocument_HoverResult const result = run_hover_test(source);
        CHECK(has_hover_with_lambda_signature(result, "Comparator"));
        CHECK(has_hover_with_content(result, "a: Int32"));
        CHECK(has_hover_with_content(result, "b: Int32"));
    }

    TEST_CASE("Hover on lambda type reference in struct member shows full signature", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over a lambda type name in a struct member
        // shows the full lambda signature.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

struct Comparator_wrapper
{
    cmp: Comparator$CURSOR_POSITION;
}
)";

        lsp::TextDocument_HoverResult const result = run_hover_test(source);
        CHECK(has_hover_with_lambda_signature(result, "Comparator"));
        CHECK(has_hover_with_content(result, "a: Int32"));
        CHECK(has_hover_with_content(result, "b: Int32"));
    }

    TEST_CASE("Hover on lambda literal shows resolved signature with inferred types", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over a lambda literal shows the resolved signature
        // with inferred types from the expected type.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: Comparator = $CURSOR_POSITIONlambda(a, b) => a - b;
}
)";

        lsp::TextDocument_HoverResult const result = run_hover_test(source);
        // Should show the resolved signature with inferred types
        CHECK(has_hover_with_content(result, "a: Int32"));
        CHECK(has_hover_with_content(result, "b: Int32"));
        CHECK(has_hover_with_content(result, "Int32"));
    }

    TEST_CASE("Hover on lambda literal with explicit return type shows resolved signature", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over a lambda literal with explicit return type
        // shows the resolved signature including the explicit return type.
        std::string_view const source =
            R"(module A;

export function main() -> ()
{
    var mapper = lambda(x: Int32) -> Int32 => x * 2;
    var result = mapper$CURSOR_POSITION(5);
}
)";

        lsp::TextDocument_HoverResult const result = run_hover_test(source);
        // Should show the resolved signature with inferred types
        CHECK(has_hover_with_content(result, "x: Int32"));
        CHECK(has_hover_with_content(result, "Int32"));
    }

    TEST_CASE("Hover on captured variable shows capture info", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over a captured variable inside a lambda body
        // shows information about the capture.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var offset: Int32 = 10;
    var cmp: Comparator = lambda(a, b) => a - b + offset$CURSOR_POSITION;
}
)";

        lsp::TextDocument_HoverResult const result = run_hover_test(source);
        CHECK(has_hover_with_content(result, "offset"));
        CHECK(has_hover_with_content(result, "Int32"));
    }

    TEST_CASE("Hover on lambda type from another module shows full signature", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over an imported lambda type shows the full signature
        // from the defining module.
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

        lsp::TextDocument_HoverResult const result = run_hover_test(source, {&other_module_source, 1});
        CHECK(has_hover_with_lambda_signature(result, "Comparator"));
        CHECK(has_hover_with_content(result, "a: Int32"));
        CHECK(has_hover_with_content(result, "b: Int32"));
    }

    TEST_CASE("Hover on nested lambda type shows correct signature", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over a lambda type name inside a nested lambda
        // expression navigates to the correct lambda declaration.
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

        lsp::TextDocument_HoverResult const result = run_hover_test(source);
        CHECK(has_hover_with_lambda_signature(result, "Inner"));
        CHECK(has_hover_with_content(result, "x: Int32"));
    }

    TEST_CASE("Hover on lambda type in function return type shows full signature", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over a lambda type name in a function return type
        // shows the full lambda signature.
        std::string_view const source =
            R"(module A;

lambda Mapper(value: Int32) -> (result: Int32);

export function create_mapper() -> (mapper: Mapper$CURSOR_POSITION)
{
    return (x) => x * 2;
}
)";

        lsp::TextDocument_HoverResult const result = run_hover_test(source);
        CHECK(has_hover_with_lambda_signature(result, "Mapper"));
        CHECK(has_hover_with_content(result, "value: Int32"));
        CHECK(has_hover_with_content(result, "result: Int32"));
    }

    TEST_CASE("Hover on lambda type in struct with multiple lambdas shows correct signature", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over a lambda type name in a struct member with
        // multiple lambda types navigates to the correct declaration.
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

        lsp::TextDocument_HoverResult const result = run_hover_test(source);
        CHECK(has_hover_with_lambda_signature(result, "Comparator"));
        CHECK(has_hover_with_content(result, "a: Int32"));
        CHECK(has_hover_with_content(result, "b: Int32"));
    }

    TEST_CASE("Hover on lambda declaration itself shows full signature", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over a lambda name in its own declaration
        // shows the full lambda signature.
        std::string_view const source =
            R"(module A;

lambda Comparator$CURSOR_POSITION(a: Int32, b: Int32) -> (result: Int32);
)";

        lsp::TextDocument_HoverResult const result = run_hover_test(source);
        CHECK(has_hover_with_lambda_signature(result, "Comparator"));
        CHECK(has_hover_with_content(result, "a: Int32"));
        CHECK(has_hover_with_content(result, "b: Int32"));
        CHECK(has_hover_with_content(result, "result: Int32"));
    }

    TEST_CASE("Hover on lambda literal with captures shows captured variables", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over a lambda literal with captures shows
        // information about the captured variables.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var offset: Int32 = 10;
    var cmp: Comparator = lambda$CURSOR_POSITION(a, b) => a - b + offset;
}
)";

        lsp::TextDocument_HoverResult const result = run_hover_test(source);
        CHECK(has_hover_with_content(result, "a: Int32"));
        CHECK(has_hover_with_content(result, "b: Int32"));
        CHECK(has_hover_with_content(result, "Int32"));
        // Should mention captured variable
        CHECK(has_hover_with_capture_info(result, "offset"));
    }

    TEST_CASE("Hover on lambda with no parameters shows empty parameter list", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over a lambda type with no parameters shows
        // the correct empty parameter list.
        std::string_view const source =
            R"(module A;

lambda Action() -> ();

export function main() -> ()
{
    var action: Action$CURSOR_POSITION = lambda() => {};
}
)";

        lsp::TextDocument_HoverResult const result = run_hover_test(source);
        CHECK(has_hover_with_lambda_signature(result, "Action"));
        CHECK(has_hover_with_content(result, "()"));
    }

    TEST_CASE("Hover on lambda with multiple return values shows tuple return", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over a lambda type with multiple return values
        // shows the tuple return type.
        std::string_view const source =
            R"(module A;

lambda Pair(a: Int32, b: Int32) -> (first: Int32, second: Int32);

export function main() -> ()
{
    var p: Pair$CURSOR_POSITION = lambda(a, b) => (a, b);
}
)";

        lsp::TextDocument_HoverResult const result = run_hover_test(source);
        CHECK(has_hover_with_lambda_signature(result, "Pair"));
        CHECK(has_hover_with_content(result, "first: Int32"));
        CHECK(has_hover_with_content(result, "second: Int32"));
    }

    TEST_CASE("Hover on lambda call shows lambda signature", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over a lambda-typed variable used in a call
        // expression shows the lambda signature.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function apply(cmp: Comparator, x: Int32, y: Int32) -> (result: Int32)
{
    return cmp$CURSOR_POSITION(x, y);
}
)";

        lsp::TextDocument_HoverResult const result = run_hover_test(source);
        CHECK(has_hover_with_content(result, "Comparator"));
        CHECK(has_hover_with_content(result, "a: Int32"));
        CHECK(has_hover_with_content(result, "b: Int32"));
    }

    TEST_CASE("Hover on lambda with captured variable from enclosing function shows capture info", "[Language_server][Hover][Lambda]")
    {
        // Verifies that hovering over a captured variable from an enclosing function
        // shows information about the capture.
        std::string_view const source =
            R"(module A;

lambda Action(x: Int32) -> ();

export function create_action() -> (result: Action)
{
    var base: Int32 = 100;
    return (x) => {};
}

export function main() -> ()
{
    var base: Int32 = 100;
    var action: Action = lambda(x) => {
        var temp = base$CURSOR_POSITION;
    };
}
)";

        lsp::TextDocument_HoverResult const result = run_hover_test(source);
        CHECK(has_hover_with_content(result, "base"));
        CHECK(has_hover_with_content(result, "Int32"));
    }
}
