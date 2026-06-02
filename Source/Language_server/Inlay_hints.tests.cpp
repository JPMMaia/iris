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
import iris.language_server.inlay_hints;
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

        static iris::Function_declaration const* find_first_function_declaration(iris::Module const& module)
        {
            if (!module.export_declarations.function_declarations.empty())
                return &module.export_declarations.function_declarations.front();
            if (!module.internal_declarations.function_declarations.empty())
                return &module.internal_declarations.function_declarations.front();
            return nullptr;
        }

        static std::pmr::vector<lsp::InlayHint> run_inlay_hints_test(
            std::string_view const source,
            std::span<std::string_view const> const dependencies = {}
        )
        {
            Parse_session session;
            for (std::string_view const dependency : dependencies)
                session.add_module(dependency);
            std::optional<std::size_t> const index = session.add_module(source);
            REQUIRE(index.has_value());

            iris::Function_declaration const* const function_declaration =
                find_first_function_declaration(session.modules[index.value()]);
            REQUIRE(function_declaration != nullptr);

            std::optional<iris::Function_definition> const function_definition = [&]() -> std::optional<iris::Function_definition>
            {
                for (auto const& def : session.modules[index.value()].definitions.function_definitions)
                {
                    if (def.name == function_declaration->name)
                        return def;
                }
                return std::nullopt;
            }();
            REQUIRE(function_definition.has_value());

            iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

            std::pmr::polymorphic_allocator<> const temporaries_allocator{};
            std::pmr::polymorphic_allocator<> const output_allocator{};

            return create_function_inlay_hints(
                session.modules[index.value()],
                *function_declaration,
                function_definition.value(),
                declaration_database,
                temporaries_allocator,
                output_allocator
            );
        }

        static std::uint32_t find_lambda_arrow_line(std::string_view const source)
        {
            std::size_t const arrow_index = source.find("=>");
            REQUIRE(arrow_index != std::string_view::npos);

            std::uint32_t line = 0;
            for (std::size_t i = 0; i < arrow_index; ++i)
            {
                if (source[i] == '\n')
                    ++line;
            }
            return line;
        }

        static lsp::Position find_parameter_position(
            std::string_view const source,
            std::string_view const parameter_name,
            std::uint32_t start_line = 0
        )
        {
            std::size_t const param_index = source.find(parameter_name);
            REQUIRE(param_index != std::string_view::npos);

            std::uint32_t line = 0;
            std::uint32_t column = 0;
            for (std::size_t i = 0; i < param_index; ++i)
            {
                if (source[i] == '\n')
                {
                    ++line;
                    column = 0;
                }
                else
                {
                    ++column;
                }
            }
            return lsp::Position{.line = line, .character = column};
        }

        static bool has_inlay_hint(
            std::span<std::pmr::vector<lsp::InlayHint> const> const hints,
            lsp::Position const& position
        )
        {
            return std::any_of(
                hints->begin(),
                hints->end(),
                [&position](lsp::InlayHint const& hint)
                {
                    return hint.position.line == position.line && hint.position.character == position.character;
                }
            );
        }

        static std::optional<std::string_view> get_inlay_hint_label(
            lsp::InlayHint const& hint
        )
        {
            if (hint.label.has_str())
                return hint.label.get_str();
            return std::nullopt;
        }

        static std::optional<lsp::InlayHintKind> get_inlay_hint_kind(
            lsp::InlayHint const& hint
        )
        {
            if (hint.kind.has_value())
                return hint.kind.value();
            return std::nullopt;
        }
    }

    TEST_CASE("Extracts cursor marker position and removes marker", "[Language_server][Inlay_hints]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;
function foo() -> ()
{
    var x = (1 + $CURSOR_POSITION2);
}
)"
        );

        CHECK(source_with_cursor.position.line == 3);
        CHECK(source_with_cursor.position.character == 17);
        CHECK(source_with_cursor.source.find(std::string{g_cursor_marker}) == std::string::npos);
    }

    TEST_CASE("Computes inlay hints for variable declaration with inferred type", "[Language_server][Inlay_hints]")
    {
        std::string_view const source =
            R"(module A;

export function foo() -> ()
{
    var x = 42;
}
)";

        std::pmr::vector<lsp::InlayHint> const inlay_hints = run_inlay_hints_test(source);
        CHECK(inlay_hints.size() >= 1);
    }

    TEST_CASE("Lambda inlay hints for parameter types with inferred types", "[Language_server][Inlay_hints][Lambda]")
    {
        // Verifies that inlay hints showing parameter types are created after each
        // parameter name in a lambda literal with inferred types.
        // Source: (a, b) => a - b  where Comparator(a: Int32, b: Int32) -> (result: Int32)
        // Expected hints: (a: Int32, b: Int32) => a - b
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: Comparator = lambda(a, b) => a - b;
}
)";

        std::pmr::vector<lsp::InlayHint> const inlay_hints = run_inlay_hints_test(source);

        // Find the lambda line (line 6: "    var cmp: Comparator = lambda(a, b) => a - b;")
        std::uint32_t const lambda_line = 6;

        // Find parameter positions in the source (after the "=" and "(")
        // 'a' is at column 30, 'b' is at column 33
        lsp::Position const a_position = find_parameter_position(source, "a");
        lsp::Position const b_position = find_parameter_position(source, "b");

        // Verify two hints exist for parameters 'a' and 'b'
        REQUIRE(inlay_hints.size() >= 2);

        // Verify hint for 'a' shows ": Int32"
        bool found_a = false;
        bool found_b = false;
        for (lsp::InlayHint const& hint : inlay_hints)
        {
            if (hint.position.line == a_position.line && hint.position.character == a_position.character + 1)
            {
                found_a = true;
                std::optional<std::string_view> const label = get_inlay_hint_label(hint);
                CHECK(label.has_value());
                CHECK(*label == ": Int32");
                std::optional<lsp::InlayHintKind> const kind = get_inlay_hint_kind(hint);
                CHECK(kind.has_value());
                CHECK(kind.value() == lsp::InlayHintKind::Type);
            }
            else if (hint.position.line == b_position.line && hint.position.character == b_position.character + 1)
            {
                found_b = true;
                std::optional<std::string_view> const label = get_inlay_hint_label(hint);
                CHECK(label.has_value());
                CHECK(*label == ": Int32");
                std::optional<lsp::InlayHintKind> const kind = get_inlay_hint_kind(hint);
                CHECK(kind.has_value());
                CHECK(kind.value() == lsp::InlayHintKind::Type);
            }
        }
        CHECK(found_a);
        CHECK(found_b);
    }

    TEST_CASE("Lambda inlay hints for return type with inferred return type", "[Language_server][Inlay_hints][Lambda]")
    {
        // Verifies that an inlay hint showing the inferred return type is created
        // before the "=>" token in a lambda literal.
        // Source: (x) => x * 2  where Mapper(value: Int32) -> (result: Int32)
        // Expected hint: (x) -> Int32 => x * 2
        std::string_view const source =
            R"(module A;

lambda Mapper(value: Int32) -> (result: Int32);

export function main() -> ()
{
    var mapper = lambda(x) => x * 2;
}
)";

        std::pmr::vector<lsp::InlayHint> const inlay_hints = run_inlay_hints_test(source);

        // Find the "=>" token position
        std::size_t const arrow_index = source.find("=>");
        REQUIRE(arrow_index != std::string_view::npos);

        lsp::Position const arrow_position = find_parameter_position(source, "=>");

        // Verify an inlay hint exists before "=>" showing "-> Int32"
        bool found_return_hint = false;
        for (lsp::InlayHint const& hint : inlay_hints)
        {
            // The return type hint should be on the same line as the arrow, positioned before it
            if (hint.position.line == arrow_position.line && hint.position.character < arrow_position.character)
            {
                found_return_hint = true;
                std::optional<std::string_view> const label = get_inlay_hint_label(hint);
                CHECK(label.has_value());
                CHECK(*label == "-> Int32");
                std::optional<lsp::InlayHintKind> const kind = get_inlay_hint_kind(hint);
                CHECK(kind.has_value());
                CHECK(kind.value() == lsp::InlayHintKind::Type);
            }
        }
        CHECK(found_return_hint);
    }

    TEST_CASE("Lambda inlay hints skip parameters with explicit types", "[Language_server][Inlay_hints][Lambda]")
    {
        // Verifies that no inlay hint is created for a parameter that already has
        // an explicit type annotation.
        // Source: (a: Int32, b) => a + b  where Comparator(a: Int32, b: Int32) -> (result: Int32)
        // Expected hint: (a: Int32, b: Int32) => a + b  (only 'b' gets a hint)
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: Comparator = lambda(a: Int32, b) => a + b;
}
)";

        std::pmr::vector<lsp::InlayHint> const inlay_hints = run_inlay_hints_test(source);

        // Find parameter positions in the source
        lsp::Position const a_position = find_parameter_position(source, "a");
        lsp::Position const b_position = find_parameter_position(source, "b");

        // Verify only one hint exists for parameter 'b' (not 'a' which has explicit type)
        bool found_a_hint = false;
        bool found_b_hint = false;
        for (lsp::InlayHint const& hint : inlay_hints)
        {
            if (hint.position.line == a_position.line && hint.position.character == a_position.character + 1)
            {
                found_a_hint = true;
            }
            else if (hint.position.line == b_position.line && hint.position.character == b_position.character + 1)
            {
                found_b_hint = true;
                std::optional<std::string_view> const label = get_inlay_hint_label(hint);
                CHECK(label.has_value());
                CHECK(*label == ": Int32");
            }
        }
        CHECK(!found_a_hint);  // 'a' has explicit type, no hint expected
        CHECK(found_b_hint);   // 'b' has inferred type, hint expected
    }

    TEST_CASE("Lambda inlay hints skip return type with explicit return type", "[Language_server][Inlay_hints][Lambda]")
    {
        // Verifies that no inlay hint is created for a return type that is
        // explicitly written in the lambda literal.
        // Source: (x) -> Int32 => x * 2
        std::string_view const source =
            R"(module A;

lambda Mapper(value: Int32) -> (result: Int32);

export function main() -> ()
{
    var mapper = lambda(x) -> Int32 => x * 2;
}
)";

        std::pmr::vector<lsp::InlayHint> const inlay_hints = run_inlay_hints_test(source);

        // Find the "=>" token position
        std::size_t const arrow_index = source.find("=>");
        REQUIRE(arrow_index != std::string_view::npos);

        lsp::Position const arrow_position = find_parameter_position(source, "=>");

        // Verify no return type hint exists before "=>" since return type is explicit
        bool found_return_hint = false;
        for (lsp::InlayHint const& hint : inlay_hints)
        {
            if (hint.position.line == arrow_position.line && hint.position.character < arrow_position.character)
            {
                found_return_hint = true;
            }
        }
        CHECK(!found_return_hint);  // Return type is explicit, no hint expected
    }

    TEST_CASE("Lambda inlay hints for lambda with block body", "[Language_server][Inlay_hints][Lambda]")
    {
        // Verifies that inlay hints are created for lambda parameters even when
        // the lambda has a block body instead of an expression body.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: Comparator = lambda(a, b) => { return a - b; };
}
)";

        std::pmr::vector<lsp::InlayHint> const inlay_hints = run_inlay_hints_test(source);

        // Find parameter positions in the source
        lsp::Position const a_position = find_parameter_position(source, "a");
        lsp::Position const b_position = find_parameter_position(source, "b");

        // Verify two hints exist for parameters 'a' and 'b'
        bool found_a = false;
        bool found_b = false;
        for (lsp::InlayHint const& hint : inlay_hints)
        {
            if (hint.position.line == a_position.line && hint.position.character == a_position.character + 1)
            {
                found_a = true;
                std::optional<std::string_view> const label = get_inlay_hint_label(hint);
                CHECK(label.has_value());
                CHECK(*label == ": Int32");
            }
            else if (hint.position.line == b_position.line && hint.position.character == b_position.character + 1)
            {
                found_b = true;
                std::optional<std::string_view> const label = get_inlay_hint_label(hint);
                CHECK(label.has_value());
                CHECK(*label == ": Int32");
            }
        }
        CHECK(found_a);
        CHECK(found_b);
    }

    TEST_CASE("Lambda inlay hints for nested lambda", "[Language_server][Inlay_hints][Lambda]")
    {
        // Verifies that inlay hints are created for both outer and inner lambda
        // parameters in nested lambda expressions.
        std::string_view const source =
            R"(module A;

lambda Inner(x: Int32) -> (result: Int32);
lambda Outer(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var outer: Outer = lambda(a, b) => {
        var inner: Inner = lambda(x) => x + a;
        return inner(b);
    };
}
)";

        std::pmr::vector<lsp::InlayHint> const inlay_hints = run_inlay_hints_test(source);

        // Find parameter positions in the source
        // Outer lambda 'a' and 'b' are on line 7
        // Inner lambda 'x' is on line 8
        lsp::Position const outer_a_position = find_parameter_position(source, "a");
        lsp::Position const outer_b_position = find_parameter_position(source, "b");
        lsp::Position const inner_x_position = find_parameter_position(source, "x");

        // Verify three hints exist: two for outer ('a', 'b') and one for inner ('x')
        bool found_outer_a = false;
        bool found_outer_b = false;
        bool found_inner_x = false;
        for (lsp::InlayHint const& hint : inlay_hints)
        {
            if (hint.position.line == outer_a_position.line && hint.position.character == outer_a_position.character + 1)
            {
                found_outer_a = true;
                std::optional<std::string_view> const label = get_inlay_hint_label(hint);
                CHECK(label.has_value());
                CHECK(*label == ": Int32");
            }
            else if (hint.position.line == outer_b_position.line && hint.position.character == outer_b_position.character + 1)
            {
                found_outer_b = true;
                std::optional<std::string_view> const label = get_inlay_hint_label(hint);
                CHECK(label.has_value());
                CHECK(*label == ": Int32");
            }
            else if (hint.position.line == inner_x_position.line && hint.position.character == inner_x_position.character + 1)
            {
                found_inner_x = true;
                std::optional<std::string_view> const label = get_inlay_hint_label(hint);
                CHECK(label.has_value());
                CHECK(*label == ": Int32");
            }
        }
        CHECK(found_outer_a);
        CHECK(found_outer_b);
        CHECK(found_inner_x);
    }

    TEST_CASE("Lambda inlay hints for lambda with captures", "[Language_server][Inlay_hints][Lambda]")
    {
        // Verifies that inlay hints are created for lambda parameters even when
        // the lambda captures variables from the enclosing scope.
        std::string_view const source =
            R"(module A;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var offset: Int32 = 10;
    var cmp: Comparator = lambda(a, b) => a - b + offset;
}
)";

        std::pmr::vector<lsp::InlayHint> const inlay_hints = run_inlay_hints_test(source);

        // Find parameter positions in the source
        lsp::Position const a_position = find_parameter_position(source, "a");
        lsp::Position const b_position = find_parameter_position(source, "b");

        // Verify two hints exist for parameters 'a' and 'b'
        bool found_a = false;
        bool found_b = false;
        for (lsp::InlayHint const& hint : inlay_hints)
        {
            if (hint.position.line == a_position.line && hint.position.character == a_position.character + 1)
            {
                found_a = true;
                std::optional<std::string_view> const label = get_inlay_hint_label(hint);
                CHECK(label.has_value());
                CHECK(*label == ": Int32");
            }
            else if (hint.position.line == b_position.line && hint.position.character == b_position.character + 1)
            {
                found_b = true;
                std::optional<std::string_view> const label = get_inlay_hint_label(hint);
                CHECK(label.has_value());
                CHECK(*label == ": Int32");
            }
        }
        CHECK(found_a);
        CHECK(found_b);
    }
}
