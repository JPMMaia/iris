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
import iris.language_server.signature_help;
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

        static iris::Struct_declaration const* find_first_struct_declaration(iris::Module const& module)
        {
            if (!module.export_declarations.struct_declarations.empty())
                return &module.export_declarations.struct_declarations.front();
            if (!module.internal_declarations.struct_declarations.empty())
                return &module.internal_declarations.struct_declarations.front();
            return nullptr;
        }
    }

    TEST_CASE("Extracts cursor marker position and removes marker", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;
function foo() -> ()
{
    bar(1, $CURSOR_POSITION2);
}
)"
        );

        CHECK(source_with_cursor.position.line == 3);
        CHECK(source_with_cursor.position.character == 11);
        CHECK(source_with_cursor.source.find(std::string{g_cursor_marker}) == std::string::npos);
    }

    TEST_CASE("Computes signature help for alias-imported call", "[Language_server][Signature_help]")
    {
        std::string_view const other_module_source =
            R"(module other;

export function bar(lhs: Int32, rhs: Int32, value: Int32) -> ()
{
}
)";

        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

import other as other;

function foo() -> ()
{
    other.bar(1, 2, $CURSOR_POSITION);
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const other_index = session.add_module(other_module_source);
        REQUIRE(other_index.has_value());

        std::optional<std::size_t> const main_index = session.add_module(source_with_cursor.source);
        REQUIRE(main_index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        lsp::TextDocument_SignatureHelpResult const result = compute_signature_help(
            declaration_database,
            session.trees[main_index.value()],
            session.modules[main_index.value()],
            source_with_cursor.position,
            [](lsp::LogMessageParams&&){}
        );

        REQUIRE(!result.isNull());
        REQUIRE(result->signatures.size() == 1);
        CHECK(result->signatures[0].label == "function bar(lhs: Int32, rhs: Int32, value: Int32) -> ()");
        REQUIRE(result->activeSignature.has_value());
        CHECK(result->activeSignature.value() == 0);
        REQUIRE(result->activeParameter.has_value());
        CHECK(result->activeParameter.value() == 2);
    }

    TEST_CASE("Computes signature help for incomplete call after comma", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

function add(lhs: Int32, rhs: Int32) -> ()
{
}

function foo() -> ()
{
    add(1, $CURSOR_POSITION);
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const main_index = session.add_module(source_with_cursor.source);
        REQUIRE(main_index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        lsp::TextDocument_SignatureHelpResult const result = compute_signature_help(
            declaration_database,
            session.trees[main_index.value()],
            session.modules[main_index.value()],
            source_with_cursor.position,
            [](lsp::LogMessageParams&&){}
        );

        REQUIRE(!result.isNull());
        REQUIRE(result->signatures.size() == 1);
        CHECK(result->signatures[0].label == "function add(lhs: Int32, rhs: Int32) -> ()");
        REQUIRE(result->activeSignature.has_value());
        CHECK(result->activeSignature.value() == 0);
        REQUIRE(result->activeParameter.has_value());
        CHECK(result->activeParameter.value() == 1);
    }

    TEST_CASE("Decides signature help kind for function call", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

function foo(lhs: Int32, rhs: Int32) -> ()
{
}

function bar() -> ()
{
    foo(1, $CURSOR_POSITION);
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        Signature_help_kind const kind = decide_signature_help_kind(
            declaration_database,
            session.trees[index.value()],
            session.modules[index.value()],
            source_with_cursor.position
        );

        CHECK(kind == Signature_help_kind::Function);
    }

    TEST_CASE("Decides signature help kind for alias-imported function call", "[Language_server][Signature_help]")
    {
        std::string_view const other_module_source =
            R"(module other;

export function bar(lhs: Int32, rhs: Int32) -> ()
{
}
)";

        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

import other as other;

function foo() -> ()
{
    other.bar(1, $CURSOR_POSITION);
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const other_index = session.add_module(other_module_source);
        REQUIRE(other_index.has_value());

        std::optional<std::size_t> const main_index = session.add_module(source_with_cursor.source);
        REQUIRE(main_index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        Signature_help_kind const kind = decide_signature_help_kind(
            declaration_database,
            session.trees[main_index.value()],
            session.modules[main_index.value()],
            source_with_cursor.position
        );

        CHECK(kind == Signature_help_kind::Function);
    }

    TEST_CASE("Decides signature help kind for struct instantiation", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

struct Foo
{
    value: Int32 = 0;
}

function bar() -> ()
{
    var x: Foo = { value: $CURSOR_POSITION };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        Signature_help_kind const kind = decide_signature_help_kind(
            declaration_database,
            session.trees[index.value()],
            session.modules[index.value()],
            source_with_cursor.position
        );

        CHECK(kind == Signature_help_kind::Struct);
    }

    TEST_CASE("Decides signature help kind for nested struct instantiation", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

struct Inner
{
    value: Int32 = 0;
}

struct Outer
{
    inner: Inner = {};
}

function bar() -> ()
{
    var x: Outer = { inner: { value: $CURSOR_POSITION } };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        Signature_help_kind const kind = decide_signature_help_kind(
            declaration_database,
            session.trees[index.value()],
            session.modules[index.value()],
            source_with_cursor.position
        );

        CHECK(kind == Signature_help_kind::Struct);
    }

    TEST_CASE("Decides signature help kind for struct member default instantiation", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

struct Complex
{
    real: Float32 = 0.0f32;
    imaginary: Float32 = 0.0f32;
}

struct My_data
{
    v0: Complex = {
        $CURSOR_POSITION
    };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        Signature_help_kind const kind = decide_signature_help_kind(
            declaration_database,
            session.trees[index.value()],
            session.modules[index.value()],
            source_with_cursor.position
        );

        CHECK(kind == Signature_help_kind::Struct);
    }

    TEST_CASE("Decides signature help kind as none for non-call expressions", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

function bar() -> ()
{
    var x = (1 + $CURSOR_POSITION2);
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        Signature_help_kind const kind = decide_signature_help_kind(
            declaration_database,
            session.trees[index.value()],
            session.modules[index.value()],
            source_with_cursor.position
        );

        CHECK(kind == Signature_help_kind::None);
    }

    TEST_CASE("Finds function call module and function name for local call", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

function foo(lhs: Int32, rhs: Int32) -> ()
{
    foo($CURSOR_POSITION);
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<Signature_help_name> const result = find_function_call_module_and_function_name(
            declaration_database,
            session.trees[index.value()],
            session.modules[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result->module_name == "A");
        CHECK(result->declaration_name == "foo");
    }

    TEST_CASE("Finds function call module and function name for alias-imported call", "[Language_server][Signature_help]")
    {
        std::string_view const other_module_source =
            R"(module other;

export function bar(lhs: Int32, rhs: Int32) -> ()
{
}
)";

        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

import other as other;

function foo() -> ()
{
    other.bar(1, $CURSOR_POSITION);
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const other_index = session.add_module(other_module_source);
        REQUIRE(other_index.has_value());

        std::optional<std::size_t> const main_index = session.add_module(source_with_cursor.source);
        REQUIRE(main_index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<Signature_help_name> const result = find_function_call_module_and_function_name(
            declaration_database,
            session.trees[main_index.value()],
            session.modules[main_index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result->module_name == "other");
        CHECK(result->declaration_name == "bar");
    }

    TEST_CASE("Finds function call module and function name for incomplete call", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

function add(lhs: Int32, rhs: Int32) -> ()
{
}

function foo() -> ()
{
    add(1, $CURSOR_POSITION);
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<Signature_help_name> const result = find_function_call_module_and_function_name(
            declaration_database,
            session.trees[index.value()],
            session.modules[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result->module_name == "A");
        CHECK(result->declaration_name == "add");
    }

    TEST_CASE("Finds active function parameter for second argument", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

function foo(lhs: Int32, rhs: Int32) -> ()
{
    foo(1, $CURSOR_POSITION);
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        std::optional<std::uint32_t> const result = find_function_call_active_parameter(
            session.trees[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result.value() == 1);
    }

    TEST_CASE("Finds active function parameter for first argument", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

function foo(lhs: Int32, rhs: Int32) -> ()
{
    foo($CURSOR_POSITION);
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        std::optional<std::uint32_t> const result = find_function_call_active_parameter(
            session.trees[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result.value() == 0);
    }

    TEST_CASE("Finds active function parameter for third argument", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

function foo(lhs: Int32, rhs: Int32, value: Int32) -> ()
{
    foo(1, 2, $CURSOR_POSITION);
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        std::optional<std::uint32_t> const result = find_function_call_active_parameter(
            session.trees[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result.value() == 2);
    }

    TEST_CASE("Finds active function parameter for innermost nested call", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

function inner(lhs: Int32, rhs: Int32) -> Int32
{
    return lhs + rhs;
}

function outer(value: Int32, other: Int32) -> ()
{
}

function foo() -> ()
{
    outer(inner(1, $CURSOR_POSITION), 3);
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        std::optional<std::uint32_t> const result = find_function_call_active_parameter(
            session.trees[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result.value() == 1);
    }

    TEST_CASE("Finds active function parameter for malformed call", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

function foo(lhs: Int32, rhs: Int32) -> ()
{
    foo(1, $CURSOR_POSITION
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        std::optional<std::uint32_t> const result = find_function_call_active_parameter(
            session.trees[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result.value() == 1);
    }

    TEST_CASE("Does not find active function parameter outside call context", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

function foo() -> ()
{
    var x = (1 + $CURSOR_POSITION2);
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        std::optional<std::uint32_t> const result = find_function_call_active_parameter(
            session.trees[index.value()],
            source_with_cursor.position
        );

        CHECK(!result.has_value());
    }

    TEST_CASE("Computes function signature with documentation", "[Language_server][Signature_help]")
    {
        std::string_view const source =
            R"(module A;

// Add two integers
//
// Add two 32-bit integers.
// It returns the result of adding lhs and rhs.
//
// @input_parameter lhs: Left hand side of add expression
// @input_parameter rhs: Right hand side of add expression
function foo(lhs: Int32, rhs: Int32) -> (result: Int32)
{
}
)";

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source);
        REQUIRE(index.has_value());

        iris::Function_declaration const* const function_declaration =
            find_first_function_declaration(session.modules[index.value()]);
        REQUIRE(function_declaration != nullptr);

        lsp::TextDocument_SignatureHelpResult const result = compute_function_signature_help(
            session.modules[index.value()],
            *function_declaration,
            1
        );

        REQUIRE(!result.isNull());
        REQUIRE(result->signatures.size() == 1);

        lsp::SignatureInformation const& signature = result->signatures[0];
        CHECK(signature.label == "function foo(lhs: Int32, rhs: Int32) -> (result: Int32)");

        REQUIRE(signature.parameters.has_value());
        REQUIRE(signature.parameters->size() == 2);

        lsp::ParameterInformation const& lhs_parameter = (*signature.parameters)[0];
        REQUIRE(std::holds_alternative<lsp::Tuple<lsp::uint, lsp::uint>>(lhs_parameter.label));
        CHECK(std::get<lsp::Tuple<lsp::uint, lsp::uint>>(lhs_parameter.label) == lsp::Tuple<lsp::uint, lsp::uint>{13, 23});
        REQUIRE(lhs_parameter.documentation.has_value());
        REQUIRE(std::holds_alternative<lsp::String>(lhs_parameter.documentation.value()));
        CHECK(std::get<lsp::String>(lhs_parameter.documentation.value()) == "Left hand side of add expression");

        lsp::ParameterInformation const& rhs_parameter = (*signature.parameters)[1];
        REQUIRE(std::holds_alternative<lsp::Tuple<lsp::uint, lsp::uint>>(rhs_parameter.label));
        CHECK(std::get<lsp::Tuple<lsp::uint, lsp::uint>>(rhs_parameter.label) == lsp::Tuple<lsp::uint, lsp::uint>{25, 35});
        REQUIRE(rhs_parameter.documentation.has_value());
        REQUIRE(std::holds_alternative<lsp::String>(rhs_parameter.documentation.value()));
        CHECK(std::get<lsp::String>(rhs_parameter.documentation.value()) == "Right hand side of add expression");

        REQUIRE(signature.documentation.has_value());
        REQUIRE(std::holds_alternative<lsp::String>(signature.documentation.value()));
        CHECK(
            std::get<lsp::String>(signature.documentation.value()) ==
            "Add two integers\n\nAdd two 32-bit integers.\nIt returns the result of adding lhs and rhs."
        );

        REQUIRE(result->activeSignature.has_value());
        CHECK(result->activeSignature.value() == 0);
        REQUIRE(result->activeParameter.has_value());
        CHECK(result->activeParameter.value() == 1);
    }

    TEST_CASE("Computes function signature without documentation when comment is missing", "[Language_server][Signature_help]")
    {
        std::string_view const source =
            R"(module A;

function foo(lhs: Int32, rhs: Int32) -> ()
{
}
)";

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source);
        REQUIRE(index.has_value());

        iris::Function_declaration const* const function_declaration =
            find_first_function_declaration(session.modules[index.value()]);
        REQUIRE(function_declaration != nullptr);

        lsp::TextDocument_SignatureHelpResult const result = compute_function_signature_help(
            session.modules[index.value()],
            *function_declaration,
            0
        );

        REQUIRE(!result.isNull());
        REQUIRE(result->signatures.size() == 1);

        lsp::SignatureInformation const& signature = result->signatures[0];
        CHECK(!signature.documentation.has_value());
        REQUIRE(signature.parameters.has_value());
        REQUIRE(signature.parameters->size() == 2);
        CHECK(!(*signature.parameters)[0].documentation.has_value());
        CHECK(!(*signature.parameters)[1].documentation.has_value());
    }

    TEST_CASE("Finds instantiate module and struct name for local instantiate", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

struct Foo
{
    value: Int32 = 0;
}

function bar() -> ()
{
    var x: Foo = { value: $CURSOR_POSITION };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<Signature_help_name> const result = find_instantiate_module_and_struct_name(
            declaration_database,
            session.trees[index.value()],
            session.modules[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result->module_name == "A");
        CHECK(result->declaration_name == "Foo");
    }

    TEST_CASE("Finds instantiate module and struct name for alias-imported instantiate", "[Language_server][Signature_help]")
    {
        std::string_view const other_module_source =
            R"(module other;

export struct Foo
{
    value: Int32 = 0;
}
)";

        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

import other as other;

function bar() -> ()
{
    var x: other.Foo = { value: $CURSOR_POSITION };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const other_index = session.add_module(other_module_source);
        REQUIRE(other_index.has_value());

        std::optional<std::size_t> const main_index = session.add_module(source_with_cursor.source);
        REQUIRE(main_index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<Signature_help_name> const result = find_instantiate_module_and_struct_name(
            declaration_database,
            session.trees[main_index.value()],
            session.modules[main_index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result->module_name == "other");
        CHECK(result->declaration_name == "Foo");
    }

    TEST_CASE("Finds instantiate module and struct name for nested instantiate", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

struct Inner
{
    value: Int32 = 0;
}

struct Outer
{
    inner: Inner = {};
}

function bar() -> ()
{
    var x: Outer = { inner: { value: $CURSOR_POSITION } };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<Signature_help_name> const result = find_instantiate_module_and_struct_name(
            declaration_database,
            session.trees[index.value()],
            session.modules[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result->module_name == "A");
        CHECK(result->declaration_name == "Inner");
    }

    TEST_CASE("Finds instantiate module and struct name for alias-imported nested instantiate", "[Language_server][Signature_help]")
    {
        std::string_view const other_module_source =
            R"(module other;

export struct Inner
{
    value: Int32 = 0;
}

export struct Outer
{
    inner: Inner = {};
}
)";

        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

import other as other;

function bar() -> ()
{
    var x: other.Outer = { inner: { value: $CURSOR_POSITION } };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const other_index = session.add_module(other_module_source);
        REQUIRE(other_index.has_value());

        std::optional<std::size_t> const main_index = session.add_module(source_with_cursor.source);
        REQUIRE(main_index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<Signature_help_name> const result = find_instantiate_module_and_struct_name(
            declaration_database,
            session.trees[main_index.value()],
            session.modules[main_index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result->module_name == "other");
        CHECK(result->declaration_name == "Inner");
    }

    TEST_CASE("Finds instantiate module and struct name for local struct member default instantiate", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

struct Complex
{
    real: Float32 = 0.0f32;
    imaginary: Float32 = 0.0f32;
}

struct My_data
{
    v0: Complex = {
        real: 1.0f32,
        $CURSOR_POSITION
    };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<Signature_help_name> const result = find_instantiate_module_and_struct_name(
            declaration_database,
            session.trees[index.value()],
            session.modules[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result->module_name == "A");
        CHECK(result->declaration_name == "Complex");
    }

    TEST_CASE("Finds instantiate module and struct name for alias-imported struct member default instantiate", "[Language_server][Signature_help]")
    {
        std::string_view const other_module_source =
            R"(module other;

export struct Complex
{
    real: Float32 = 0.0f32;
    imaginary: Float32 = 0.0f32;
}
)";

        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

import other as other;

struct My_data
{
    value: other.Complex = {
        real: 1.0f32,
        $CURSOR_POSITION
    };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const other_index = session.add_module(other_module_source);
        REQUIRE(other_index.has_value());

        std::optional<std::size_t> const main_index = session.add_module(source_with_cursor.source);
        REQUIRE(main_index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<Signature_help_name> const result = find_instantiate_module_and_struct_name(
            declaration_database,
            session.trees[main_index.value()],
            session.modules[main_index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result->module_name == "other");
        CHECK(result->declaration_name == "Complex");
    }

    TEST_CASE("Finds instantiate active member for first member", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

struct Foo
{
    first: Int32 = 0;
    second: Int32 = 0;
}

function bar() -> ()
{
    var x: Foo = { first: $CURSOR_POSITION };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        Signature_help_name const struct_name{
            .module_name = "A",
            .declaration_name = "Foo",
        };

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<std::uint32_t> const result = find_instantiate_active_member(
            declaration_database,
            session.modules[index.value()],
            struct_name,
            session.trees[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result.value() == 0);
    }

    TEST_CASE("Finds instantiate active member for empty instantiate", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

struct Foo
{
    first: Int32 = 0;
    second: Int32 = 0;
}

function bar() -> ()
{
    var x: Foo = { $CURSOR_POSITION };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        Signature_help_name const struct_name{
            .module_name = "A",
            .declaration_name = "Foo",
        };

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<std::uint32_t> const result = find_instantiate_active_member(
            declaration_database,
            session.modules[index.value()],
            struct_name,
            session.trees[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result.value() == 0); 
    }

    TEST_CASE("Finds instantiate active member while writing second member", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

struct Foo
{
    first: Int32 = 0;
    second: Int32 = 0;
    third: Int32 = 0;
}

function bar() -> ()
{
    var x: Foo = { first: 1, sec$CURSOR_POSITION };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        Signature_help_name const struct_name{
            .module_name = "A",
            .declaration_name = "Foo",
        };

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<std::uint32_t> const result = find_instantiate_active_member(
            declaration_database,
            session.modules[index.value()],
            struct_name,
            session.trees[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result.value() == 1);
    }

    TEST_CASE("Finds instantiate active member for explicit member value", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

struct Foo
{
    first: Int32 = 0;
    second: Int32 = 0;
    third: Int32 = 0;
}

function bar() -> ()
{
    var x: Foo = { first: 1, third: $CURSOR_POSITION };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        Signature_help_name const struct_name{
            .module_name = "A",
            .declaration_name = "Foo",
        };

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<std::uint32_t> const result = find_instantiate_active_member(
            declaration_database,
            session.modules[index.value()],
            struct_name,
            session.trees[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result.value() == 2);
    }

    TEST_CASE("Finds instantiate active member for most likely entry", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

struct Foo
{
    first: Int32 = 0;
    second: Int32 = 0;
    third: Int32 = 0;
}

function bar() -> ()
{
    var x: Foo = { first: 1, thi$CURSOR_POSITION };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        Signature_help_name const struct_name{
            .module_name = "A",
            .declaration_name = "Foo",
        };

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<std::uint32_t> const result = find_instantiate_active_member(
            declaration_database,
            session.modules[index.value()],
            struct_name,
            session.trees[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result.value() == 2);
    }

    TEST_CASE("Finds instantiate active member for most likely entry similar names", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

struct Foo
{
    application: Int32 = 0;
    instance: Int32 = 0;
    command: Int32 = 0;
    include: Int32 = 0;
}

function run() -> ()
{
    var instance: Foo = {
        instance: 0,
        in$CURSOR_POSITION
    };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        Signature_help_name const struct_name{
            .module_name = "A",
            .declaration_name = "Foo",
        };

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<std::uint32_t> const result = find_instantiate_active_member(
            declaration_database,
            session.modules[index.value()],
            struct_name,
            session.trees[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result.value() == 3);
    }

    TEST_CASE("Finds instantiate active member for nested instantiate", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

struct Inner
{
    first: Int32 = 0;
    second: Int32 = 0;
}

struct Outer
{
    inner: Inner = {};
}

function bar() -> ()
{
    var x: Outer = { inner: { second: $CURSOR_POSITION } };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        Signature_help_name const struct_name{
            .module_name = "A",
            .declaration_name = "Inner",
        };

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<std::uint32_t> const result = find_instantiate_active_member(
            declaration_database,
            session.modules[index.value()],
            struct_name,
            session.trees[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result.value() == 1);
    }

    TEST_CASE("Finds instantiate most likely active member for alias-imported incomplete entry", "[Language_server][Signature_help]")
    {
        std::string_view const other_module_source =
            R"(module other;

export struct Foo
{
    first: Int32 = 0;
    second: Int32 = 0;
    third: Int32 = 0;
}
)";

        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

import other as other;

function bar() -> ()
{
    var x: other.Foo = { first: 1, thi$CURSOR_POSITION };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const other_index = session.add_module(other_module_source);
        REQUIRE(other_index.has_value());

        std::optional<std::size_t> const main_index = session.add_module(source_with_cursor.source);
        REQUIRE(main_index.has_value());

        Signature_help_name const struct_name{
            .module_name = "other",
            .declaration_name = "Foo",
        };

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<std::uint32_t> const result = find_instantiate_active_member(
            declaration_database,
            session.modules[main_index.value()],
            struct_name,
            session.trees[main_index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result.value() == 2);
    }

    TEST_CASE("Finds instantiate active member for struct member default instantiate", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

struct Complex
{
    real: Float32 = 0.0f32;
    imaginary: Float32 = 0.0f32;
}

struct My_data
{
    v1: Complex = {
        real: 1.0f32,
        $CURSOR_POSITION
    };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        Signature_help_name const struct_name{
            .module_name = "A",
            .declaration_name = "Complex",
        };

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        std::optional<std::uint32_t> const result = find_instantiate_active_member(
            declaration_database,
            session.modules[index.value()],
            struct_name,
            session.trees[index.value()],
            source_with_cursor.position
        );

        REQUIRE(result.has_value());
        CHECK(result.value() == 1);
    }

    TEST_CASE("Computes struct signature", "[Language_server][Signature_help]")
    {
        std::string_view const source =
            R"(module A;

struct Foo
{
    value: Int32 = 0;
}
)";

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source);
        REQUIRE(index.has_value());

        iris::Struct_declaration const* const struct_declaration =
            find_first_struct_declaration(session.modules[index.value()]);
        REQUIRE(struct_declaration != nullptr);

        lsp::TextDocument_SignatureHelpResult const result = compute_struct_signature_help(
            session.modules[index.value()],
            *struct_declaration,
            0
        );

        REQUIRE(!result.isNull());
        REQUIRE(result->signatures.size() == 1);

        lsp::SignatureInformation const& signature = result->signatures[0];
        CHECK(signature.label == "Foo {\n    value: Int32 = 0\n}");

        REQUIRE(signature.parameters.has_value());
        REQUIRE(signature.parameters->size() == 1);

        lsp::ParameterInformation const& value_parameter = (*signature.parameters)[0];
        REQUIRE(std::holds_alternative<lsp::Tuple<lsp::uint, lsp::uint>>(value_parameter.label));
        CHECK(
            std::get<lsp::Tuple<lsp::uint, lsp::uint>>(value_parameter.label) ==
            lsp::Tuple<lsp::uint, lsp::uint>{10, 26}
        );
        CHECK(!value_parameter.documentation.has_value());

        REQUIRE(result->activeSignature.has_value());
        CHECK(result->activeSignature.value() == 0);
        REQUIRE(result->activeParameter.has_value());
        CHECK(result->activeParameter.value() == 0);
    }

    TEST_CASE("Computes struct signature with documentation", "[Language_server][Signature_help]")
    {
        std::string_view const source =
            R"(module A;

// Represents complex numbers. Uses 32-bit floats.
struct Complex
{
    // The real part.
    real: Float32 = 0.0f32;

    // The imaginary part.
    imaginary: Float32 = 0.0f32;
}
)";

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source);
        REQUIRE(index.has_value());

        iris::Struct_declaration const* const struct_declaration =
            find_first_struct_declaration(session.modules[index.value()]);
        REQUIRE(struct_declaration != nullptr);

        lsp::TextDocument_SignatureHelpResult const result = compute_struct_signature_help(
            session.modules[index.value()],
            *struct_declaration,
            1
        );

        REQUIRE(!result.isNull());
        REQUIRE(result->signatures.size() == 1);

        lsp::SignatureInformation const& signature = result->signatures[0];
        CHECK(signature.label == "Complex {\n    real: Float32 = 0.0f32,\n    imaginary: Float32 = 0.0f32\n}");

        REQUIRE(signature.parameters.has_value());
        REQUIRE(signature.parameters->size() == 2);

        lsp::ParameterInformation const& real_parameter = (*signature.parameters)[0];
        REQUIRE(std::holds_alternative<lsp::Tuple<lsp::uint, lsp::uint>>(real_parameter.label));
        CHECK(
            std::get<lsp::Tuple<lsp::uint, lsp::uint>>(real_parameter.label) ==
            lsp::Tuple<lsp::uint, lsp::uint>{14, 36}
        );
        REQUIRE(real_parameter.documentation.has_value());
        REQUIRE(std::holds_alternative<lsp::String>(real_parameter.documentation.value()));
        CHECK(std::get<lsp::String>(real_parameter.documentation.value()) == "The real part.");

        lsp::ParameterInformation const& imaginary_parameter = (*signature.parameters)[1];
        REQUIRE(std::holds_alternative<lsp::Tuple<lsp::uint, lsp::uint>>(imaginary_parameter.label));
        CHECK(
            std::get<lsp::Tuple<lsp::uint, lsp::uint>>(imaginary_parameter.label) ==
            lsp::Tuple<lsp::uint, lsp::uint>{42, 69}
        );
        REQUIRE(imaginary_parameter.documentation.has_value());
        REQUIRE(std::holds_alternative<lsp::String>(imaginary_parameter.documentation.value()));
        CHECK(std::get<lsp::String>(imaginary_parameter.documentation.value()) == "The imaginary part.");

        REQUIRE(signature.documentation.has_value());
        REQUIRE(std::holds_alternative<lsp::String>(signature.documentation.value()));
        CHECK(
            std::get<lsp::String>(signature.documentation.value()) ==
            "Represents complex numbers. Uses 32-bit floats."
        );

        REQUIRE(result->activeSignature.has_value());
        CHECK(result->activeSignature.value() == 0);
        REQUIRE(result->activeParameter.has_value());
        CHECK(result->activeParameter.value() == 1);
    }

    TEST_CASE("Computes signature help for local struct member default instantiate", "[Language_server][Signature_help]")
    {
        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

struct Complex
{
    real: Float32 = 0.0f32;
    imaginary: Float32 = 0.0f32;
}

struct My_data
{
    v1: Complex = {
        real: 1.0f32,
        $CURSOR_POSITION
    };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source_with_cursor.source);
        REQUIRE(index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        lsp::TextDocument_SignatureHelpResult const result = compute_signature_help(
            declaration_database,
            session.trees[index.value()],
            session.modules[index.value()],
            source_with_cursor.position,
            [](lsp::LogMessageParams&&){}
        );

        REQUIRE(!result.isNull());
        REQUIRE(result->signatures.size() == 1);
        CHECK(result->signatures[0].label == "Complex {\n    real: Float32 = 0.0f32,\n    imaginary: Float32 = 0.0f32\n}");
        REQUIRE(result->activeSignature.has_value());
        CHECK(result->activeSignature.value() == 0);
        REQUIRE(result->activeParameter.has_value());
        CHECK(result->activeParameter.value() == 1);
    }

    TEST_CASE("Computes signature help for alias-imported struct member default instantiate", "[Language_server][Signature_help]")
    {
        std::string_view const other_module_source =
            R"(module other;

export struct Complex
{
    real: Float32 = 0.0f32;
    imaginary: Float32 = 0.0f32;
}
)";

        Source_with_cursor const source_with_cursor = extract_cursor_position(
            R"(module A;

import other as other;

struct My_data
{
    value: other.Complex = {
        real: 1.0f32,
        $CURSOR_POSITION
    };
}
)"
        );

        Parse_session session;
        std::optional<std::size_t> const other_index = session.add_module(other_module_source);
        REQUIRE(other_index.has_value());

        std::optional<std::size_t> const main_index = session.add_module(source_with_cursor.source);
        REQUIRE(main_index.has_value());

        iris::Declaration_database const declaration_database = create_declaration_database(session.modules);

        lsp::TextDocument_SignatureHelpResult const result = compute_signature_help(
            declaration_database,
            session.trees[main_index.value()],
            session.modules[main_index.value()],
            source_with_cursor.position,
            [](lsp::LogMessageParams&&){}
        );

        REQUIRE(!result.isNull());
        REQUIRE(result->signatures.size() == 1);
        CHECK(result->signatures[0].label == "Complex {\n    real: Float32 = 0.0f32,\n    imaginary: Float32 = 0.0f32\n}");
        REQUIRE(result->activeSignature.has_value());
        CHECK(result->activeSignature.value() == 0);
        REQUIRE(result->activeParameter.has_value());
        CHECK(result->activeParameter.value() == 1);
    }

    TEST_CASE("Computes struct signature without documentation when comment is missing", "[Language_server][Signature_help]")
    {
        std::string_view const source =
            R"(module A;

struct Foo
{
    first: Int32 = 0;
    second: Int32 = 0;
}
)";

        Parse_session session;
        std::optional<std::size_t> const index = session.add_module(source);
        REQUIRE(index.has_value());

        iris::Struct_declaration const* const struct_declaration =
            find_first_struct_declaration(session.modules[index.value()]);
        REQUIRE(struct_declaration != nullptr);

        lsp::TextDocument_SignatureHelpResult const result = compute_struct_signature_help(
            session.modules[index.value()],
            *struct_declaration,
            0
        );

        REQUIRE(!result.isNull());
        REQUIRE(result->signatures.size() == 1);

        lsp::SignatureInformation const& signature = result->signatures[0];
        CHECK(signature.label == "Foo {\n    first: Int32 = 0,\n    second: Int32 = 0\n}");
        CHECK(!signature.documentation.has_value());

        REQUIRE(signature.parameters.has_value());
        REQUIRE(signature.parameters->size() == 2);

        lsp::ParameterInformation const& first_parameter = (*signature.parameters)[0];
        REQUIRE(std::holds_alternative<lsp::Tuple<lsp::uint, lsp::uint>>(first_parameter.label));
        CHECK(
            std::get<lsp::Tuple<lsp::uint, lsp::uint>>(first_parameter.label) ==
            lsp::Tuple<lsp::uint, lsp::uint>{10, 26}
        );
        CHECK(!first_parameter.documentation.has_value());

        lsp::ParameterInformation const& second_parameter = (*signature.parameters)[1];
        REQUIRE(std::holds_alternative<lsp::Tuple<lsp::uint, lsp::uint>>(second_parameter.label));
        CHECK(
            std::get<lsp::Tuple<lsp::uint, lsp::uint>>(second_parameter.label) ==
            lsp::Tuple<lsp::uint, lsp::uint>{32, 49}
        );
        CHECK(!second_parameter.documentation.has_value());
    }
}
