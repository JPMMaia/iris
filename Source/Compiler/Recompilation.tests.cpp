#include <memory_resource>
#include <filesystem>
#include <optional>
#include <string_view>
#include <unordered_map>

#include <catch2/catch_all.hpp>

import h.binary_serializer;
import h.common;
import h.compiler;
import h.core.hash;
import h.compiler.recompilation;
import h.core;
import h.json_serializer;
import h.parser.convertor;
import h.parser.parser;

namespace h
{
    std::filesystem::path setup_root_directory(
        std::string_view const directory_name
    )
    {
        std::filesystem::path const root_directory = std::filesystem::temp_directory_path() / "hlang_test" / directory_name;

        if (std::filesystem::exists(root_directory))
            std::filesystem::remove_all(root_directory);

        std::filesystem::create_directories(root_directory);

        return root_directory;
    }

    std::filesystem::path setup_build_directory(
        std::filesystem::path const& root_directory
    )
    {
        std::filesystem::path const build_directory_path = root_directory / "build";
        std::filesystem::create_directories(build_directory_path);
        return build_directory_path;
    }

    std::filesystem::path parse_core_module(
        h::parser::Parser const& parser,
        std::filesystem::path const& build_directory,
        std::filesystem::path const& file_path
    )
    {
        std::filesystem::path const parsed_file_path = build_directory / file_path.filename().replace_extension("hlb");

        std::optional<h::Module> const core_module = h::parser::parse_and_convert_to_module(
            file_path,
            {},
            {}
        );
        REQUIRE(core_module.has_value());
        
        h::binary_serializer::write_module_to_file(parsed_file_path, core_module.value(), {});

        return parsed_file_path;
    }

    h::Module read_core_module(
        std::filesystem::path const& file_path
    )
    {
        std::optional<h::Module> core_module = h::compiler::read_core_module(file_path);
        REQUIRE(core_module.has_value());

        return *core_module;
    }

    TEST_CASE("Recompile modules that depend on changed export interface", "[Recompilation]")
    {
        SKIP();

        std::filesystem::path const root_directory = setup_root_directory("recompilation_0");
        std::filesystem::path const build_directory_path = setup_build_directory(root_directory);
        h::parser::Parser const parser = h::parser::create_parser();

        std::filesystem::path const module_a_code_file_path = root_directory / "A.iris";
        std::string_view const module_a_code = R"(    
            module A;

            import B as B;

            export function main() -> (result: Int32)
            {
                var foo: B.Foo = {};
                return foo.a;
            }
        )";
        h::common::write_to_file(module_a_code_file_path, module_a_code);
        std::filesystem::path const module_a_file_path = parse_core_module(parser, build_directory_path, module_a_code_file_path);

        std::filesystem::path const module_b_code_file_path = root_directory / "B.iris";
        std::string_view const module_b_code = R"(    
            module B;

            export struct Foo
            {
                a: Int32 = 0;
            }
        )";
        h::common::write_to_file(module_b_code_file_path, module_b_code);
        std::filesystem::path const module_b_file_path = parse_core_module(parser, build_directory_path, module_b_code_file_path);

        h::Module const previous_module_b = read_core_module(module_b_file_path);
        h::Symbol_name_to_hash const previous_symbol_name_to_hash = h::hash_module_declarations(previous_module_b, {});

        std::string_view const new_module_b_code = R"(    
            module B;

            export struct Foo
            {
                a: Int32 = 1;
            }
        )";
        h::common::write_to_file(module_b_code_file_path, new_module_b_code);
        parse_core_module(parser, build_directory_path, module_b_code_file_path);

        h::Module const new_module_b = read_core_module(module_b_file_path);
        h::Symbol_name_to_hash const new_symbol_name_to_hash = h::hash_module_declarations(new_module_b, {});


        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path
        {
            std::make_pair("A", module_a_file_path),
            std::make_pair("B", module_b_file_path),
        };

        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const module_name_to_reverse_dependencies
        {
            std::make_pair("B", "A"),
        };

        std::pmr::vector<std::pmr::string> const modules_to_recompile = h::compiler::find_modules_to_recompile(
            new_module_b,
            previous_symbol_name_to_hash,
            new_symbol_name_to_hash,
            module_name_to_file_path,
            module_name_to_reverse_dependencies,
            {},
            {}
        );

        std::pmr::vector<std::pmr::string> const expected_modules_to_recompile
        {
            "A"
        };

        CHECK(modules_to_recompile == expected_modules_to_recompile);
    }

    TEST_CASE("Do not recompile modules that do not depend on changed export interface", "[Recompilation]")
    {
        SKIP();

        std::filesystem::path const root_directory = setup_root_directory("recompilation_1");
        std::filesystem::path const build_directory_path = setup_build_directory(root_directory);
        h::parser::Parser const parser = h::parser::create_parser();

        std::filesystem::path const module_a_code_file_path = root_directory / "A.iris";
        std::string_view const module_a_code = R"(
            module A;

            import B as B;

            export function main() -> (result: Int32)
            {
                var foo: B.Foo = {};
                return foo.a;
            }
        )";
        h::common::write_to_file(module_a_code_file_path, module_a_code);
        std::filesystem::path const module_a_file_path = parse_core_module(parser, build_directory_path, module_a_code_file_path);

        std::filesystem::path const module_b_code_file_path = root_directory / "B.iris";
        std::string_view const module_b_code = R"(
            module B;

            export struct Foo
            {
                a: Int32 = 0;
            }

            export struct Bar
            {
                a: Int32 = 0;
            }
        )";
        h::common::write_to_file(module_b_code_file_path, module_b_code);
        std::filesystem::path const module_b_file_path = parse_core_module(parser, build_directory_path, module_b_code_file_path);

        h::Module const previous_module_b = read_core_module(module_b_file_path);
        h::Symbol_name_to_hash const previous_symbol_name_to_hash = h::hash_module_declarations(previous_module_b, {});

        std::string_view const new_module_b_code = R"(
            module B;

            export struct Foo
            {
                a: Int32 = 0;
            }

            export struct Bar
            {
                a: Int32 = 1;
            }
        )";
        h::common::write_to_file(module_b_file_path, new_module_b_code);
        parse_core_module(parser, build_directory_path, module_b_file_path);

        h::Module const new_module_b = read_core_module(module_b_file_path);
        h::Symbol_name_to_hash const new_symbol_name_to_hash = h::hash_module_declarations(new_module_b, {});


        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path
        {
            std::make_pair("A", module_a_file_path),
            std::make_pair("B", module_b_file_path),
        };

        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const module_name_to_reverse_dependencies
        {
            std::make_pair("B", "A"),
        };

        std::pmr::vector<std::pmr::string> const modules_to_recompile = h::compiler::find_modules_to_recompile(
            new_module_b,
            previous_symbol_name_to_hash,
            new_symbol_name_to_hash,
            module_name_to_file_path,
            module_name_to_reverse_dependencies,
            {},
            {}
        );

        CHECK(modules_to_recompile.empty());
    }

    TEST_CASE("Recompile modules that depend on changed internal interface used by external", "[Recompilation]")
    {
        SKIP();

        std::filesystem::path const root_directory = setup_root_directory("recompilation_2");
        std::filesystem::path const build_directory_path = setup_build_directory(root_directory);
        h::parser::Parser const parser = h::parser::create_parser();

        std::filesystem::path const module_a_code_file_path = root_directory / "A.iris";
        std::string_view const module_a_code = R"(
            module A;

            import B as B;

            export function main() -> (result: Int32)
            {
                var foo: B.Foo = {};
                return foo.bar.a;
            }
        )";
        h::common::write_to_file(module_a_code_file_path, module_a_code);
        std::filesystem::path const module_a_file_path = parse_core_module(parser, build_directory_path, module_a_code_file_path);

        std::filesystem::path const module_b_code_file_path = root_directory / "B.iris";
        std::string_view const module_b_code = R"(
            module B;

            export struct Foo
            {
                bar: Bar = {};
            }

            struct Bar
            {
                a: Int32 = 0;
            }
        )";
        h::common::write_to_file(module_b_code_file_path, module_b_code);
        std::filesystem::path const module_b_file_path = parse_core_module(parser, build_directory_path, module_b_code_file_path);

        h::Module const previous_module_b = read_core_module(module_b_file_path);
        h::Symbol_name_to_hash const previous_symbol_name_to_hash = h::hash_module_declarations(previous_module_b, {});

        std::string_view const new_module_b_code = R"(
            module B;

            export struct Foo
            {
                bar: Bar = {};
            }

            struct Bar
            {
                a: Int32 = 1;
            }
        )";
        h::common::write_to_file(module_b_code_file_path, new_module_b_code);
        parse_core_module(parser, build_directory_path, module_b_code_file_path);

        h::Module const new_module_b = read_core_module(module_b_file_path);
        h::Symbol_name_to_hash const new_symbol_name_to_hash = h::hash_module_declarations(new_module_b, {});


        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path
        {
            std::make_pair("A", module_a_file_path),
            std::make_pair("B", module_b_file_path),
        };

        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const module_name_to_reverse_dependencies
        {
            std::make_pair("B", "A"),
        };

        std::pmr::vector<std::pmr::string> const modules_to_recompile = h::compiler::find_modules_to_recompile(
            new_module_b,
            previous_symbol_name_to_hash,
            new_symbol_name_to_hash,
            module_name_to_file_path,
            module_name_to_reverse_dependencies,
            {},
            {}
        );

        std::pmr::vector<std::pmr::string> const expected_modules_to_recompile
        {
            "A"
        };

        CHECK(modules_to_recompile == expected_modules_to_recompile);
    }

    TEST_CASE("Recompile modules that depend on changed export interface and propagate changes", "[Recompilation]")
    {
        SKIP();

        std::filesystem::path const root_directory = setup_root_directory("recompilation_3");
        std::filesystem::path const build_directory_path = setup_build_directory(root_directory);
        h::parser::Parser const parser = h::parser::create_parser();

        std::filesystem::path const module_a_code_file_path = root_directory / "A.iris";
        std::string_view const module_a_code = R"(
            module A;

            import B as B;

            export function main() -> (result: Int32)
            {
                var foo: B.Foo = {};
                return foo.bar.a;
            }
        )";
        h::common::write_to_file(module_a_code_file_path, module_a_code);
        std::filesystem::path const module_a_file_path = parse_core_module(parser, build_directory_path, module_a_code_file_path);

        std::filesystem::path const module_b_code_file_path = root_directory / "B.iris";
        std::string_view const module_b_code = R"(
            module B;

            import C as C;

            export struct Foo
            {
                foo: *Foo = null;
                bar: C.Bar = 0;
            }
        )";
        h::common::write_to_file(module_b_code_file_path, module_b_code);
        std::filesystem::path const module_b_file_path = parse_core_module(parser, build_directory_path, module_b_code_file_path);

        std::filesystem::path const module_c_code_file_path = root_directory / "C.iris";
        std::string_view const module_c_code = R"(
            module C;

            export struct Bar
            {
                a: Int32 = 0;
            }

            export struct Other_bar
            {
                a: *Other_bar = null;
            }
        )";
        h::common::write_to_file(module_c_code_file_path, module_c_code);
        std::filesystem::path const module_c_file_path = parse_core_module(parser, build_directory_path, module_c_code_file_path);

        h::Module const previous_module_c = read_core_module(module_c_file_path);
        h::Symbol_name_to_hash const previous_symbol_name_to_hash = h::hash_module_declarations(previous_module_c, {});

        std::string_view const new_module_c_code = R"(
            module C;

            export struct Bar
            {
                a: Int32 = 1;
            }

            export struct Other_bar
            {
                a: *Other_bar = null;
            }
        )";
        h::common::write_to_file(module_c_code_file_path, new_module_c_code);
        parse_core_module(parser, build_directory_path, module_c_code_file_path);

        h::Module const new_module_c = read_core_module(module_c_file_path);
        h::Symbol_name_to_hash const new_symbol_name_to_hash = h::hash_module_declarations(new_module_c, {});


        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path
        {
            std::make_pair("A", module_a_file_path),
            std::make_pair("B", module_b_file_path),
            std::make_pair("C", module_c_file_path),
        };

        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const module_name_to_reverse_dependencies
        {
            std::make_pair("B", "A"),
            std::make_pair("C", "B"),
        };

        std::pmr::vector<std::pmr::string> const modules_to_recompile = h::compiler::find_modules_to_recompile(
            new_module_c,
            previous_symbol_name_to_hash,
            new_symbol_name_to_hash,
            module_name_to_file_path,
            module_name_to_reverse_dependencies,
            {},
            {}
        );

        std::pmr::vector<std::pmr::string> const expected_modules_to_recompile
        {
            "B",
            "A",
        };

        CHECK(modules_to_recompile == expected_modules_to_recompile);
    }

    TEST_CASE("Dot not recompile modules that do not depend on changed export interface when propagating changes", "[Recompilation]")
    {
        SKIP();

        std::filesystem::path const root_directory = setup_root_directory("recompilation_4");
        std::filesystem::path const build_directory_path = setup_build_directory(root_directory);
        h::parser::Parser const parser = h::parser::create_parser();

        std::filesystem::path const module_a_code_file_path = root_directory / "A.iris";
        std::string_view const module_a_code = R"(
            module A;

            import B as B;

            export function main() -> (result: Int32)
            {
                var other: B.Other = {};
                return other.a;
            }
        )";
        h::common::write_to_file(module_a_code_file_path, module_a_code);
        std::filesystem::path const module_a_file_path = parse_core_module(parser, build_directory_path, module_a_code_file_path);

        std::filesystem::path const module_b_code_file_path = root_directory / "B.iris";
        std::string_view const module_b_code = R"(
            module B;

            import C as C;

            export struct Other
            {
                a: Int32 = 0;
            }

            export struct Foo
            {
                bar: C.Bar = 0;
            }
        )";
        h::common::write_to_file(module_b_code_file_path, module_b_code);
        std::filesystem::path const module_b_file_path = parse_core_module(parser, build_directory_path, module_b_code_file_path);

        std::filesystem::path const module_c_code_file_path = root_directory / "C.iris";
        std::string_view const module_c_code = R"(
            module C;

            export struct Bar
            {
                a: Int32 = 0;
            }
        )";
        h::common::write_to_file(module_c_code_file_path, module_c_code);
        std::filesystem::path const module_c_file_path = parse_core_module(parser, build_directory_path, module_c_code_file_path);

        h::Module const previous_module_c = read_core_module(module_c_file_path);
        h::Symbol_name_to_hash const previous_symbol_name_to_hash = h::hash_module_declarations(previous_module_c, {});

        std::string_view const new_module_c_code = R"(
            module C;

            export struct Bar
            {
                a: Int32 = 1;
            }
        )";
        h::common::write_to_file(module_c_code_file_path, new_module_c_code);
        parse_core_module(parser, build_directory_path, module_c_code_file_path);

        h::Module const new_module_c = read_core_module(module_c_file_path);
        h::Symbol_name_to_hash const new_symbol_name_to_hash = h::hash_module_declarations(new_module_c, {});


        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path
        {
            std::make_pair("A", module_a_file_path),
            std::make_pair("B", module_b_file_path),
            std::make_pair("C", module_c_file_path),
        };

        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const module_name_to_reverse_dependencies
        {
            std::make_pair("B", "A"),
            std::make_pair("C", "B"),
        };

        std::pmr::vector<std::pmr::string> const modules_to_recompile = h::compiler::find_modules_to_recompile(
            new_module_c,
            previous_symbol_name_to_hash,
            new_symbol_name_to_hash,
            module_name_to_file_path,
            module_name_to_reverse_dependencies,
            {},
            {}
        );

        std::pmr::vector<std::pmr::string> const expected_modules_to_recompile
        {
            "B",
        };

        CHECK(modules_to_recompile == expected_modules_to_recompile);
    }

    TEST_CASE("Recompile modules containing unions that depend on changed export interface and propagate changes", "[Recompilation]")
    {
        SKIP();

        std::filesystem::path const root_directory = setup_root_directory("recompilation_5");
        std::filesystem::path const build_directory_path = setup_build_directory(root_directory);
        h::parser::Parser const parser = h::parser::create_parser();

        std::filesystem::path const module_a_code_file_path = root_directory / "A.iris";
        std::string_view const module_a_code = R"(
            module A;

            import B as B;

            export function main() -> (result: Int32)
            {
                var foo: B.Foo = {
                    b: 1i16
                };
                return foo.b;
            }
        )";
        h::common::write_to_file(module_a_code_file_path, module_a_code);
        std::filesystem::path const module_a_file_path = parse_core_module(parser, build_directory_path, module_a_code_file_path);

        std::filesystem::path const module_b_code_file_path = root_directory / "B.iris";
        std::string_view const module_b_code = R"(
            module B;

            import C as C;

            export union Foo
            {
                bar: C.Bar;
                b: Int16;
            }
        )";
        h::common::write_to_file(module_b_code_file_path, module_b_code);
        std::filesystem::path const module_b_file_path = parse_core_module(parser, build_directory_path, module_b_code_file_path);

        std::filesystem::path const module_c_code_file_path = root_directory / "C.iris";
        std::string_view const module_c_code = R"(
            module C;

            export union Bar
            {
                a: Int32;
                b: Float64;
            }
        )";
        h::common::write_to_file(module_c_code_file_path, module_c_code);
        std::filesystem::path const module_c_file_path = parse_core_module(parser, build_directory_path, module_c_code_file_path);

        h::Module const previous_module_c = read_core_module(module_c_file_path);
        h::Symbol_name_to_hash const previous_symbol_name_to_hash = h::hash_module_declarations(previous_module_c, {});

        std::string_view const new_module_c_code = R"(
            module C;

            export union Bar
            {
                a: Int32;
                b: Int64;
            }
        )";
        h::common::write_to_file(module_c_code_file_path, new_module_c_code);
        parse_core_module(parser, build_directory_path, module_c_code_file_path);

        h::Module const new_module_c = read_core_module(module_c_file_path);
        h::Symbol_name_to_hash const new_symbol_name_to_hash = h::hash_module_declarations(new_module_c, {});


        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path
        {
            std::make_pair("A", module_a_file_path),
            std::make_pair("B", module_b_file_path),
            std::make_pair("C", module_c_file_path),
        };

        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const module_name_to_reverse_dependencies
        {
            std::make_pair("B", "A"),
            std::make_pair("C", "B"),
        };

        std::pmr::vector<std::pmr::string> const modules_to_recompile = h::compiler::find_modules_to_recompile(
            new_module_c,
            previous_symbol_name_to_hash,
            new_symbol_name_to_hash,
            module_name_to_file_path,
            module_name_to_reverse_dependencies,
            {},
            {}
        );

        std::pmr::vector<std::pmr::string> const expected_modules_to_recompile
        {
            "B",
            "A",
        };

        CHECK(modules_to_recompile == expected_modules_to_recompile);
    }

    TEST_CASE("Recompile modules containing alias that depend on changed export interface and propagate changes", "[Recompilation]")
    {
        SKIP();

        std::filesystem::path const root_directory = setup_root_directory("recompilation_6");
        std::filesystem::path const build_directory_path = setup_build_directory(root_directory);
        h::parser::Parser const parser = h::parser::create_parser();

        std::filesystem::path const module_a_code_file_path = root_directory / "A.iris";
        std::string_view const module_a_code = R"(
            module A;

            import B as B;

            export function main() -> (result: Int32)
            {
                var value: B.My_int = 1i64;
                return value;
            }
        )";
        h::common::write_to_file(module_a_code_file_path, module_a_code);
        std::filesystem::path const module_a_file_path = parse_core_module(parser, build_directory_path, module_a_code_file_path);

        std::filesystem::path const module_b_code_file_path = root_directory / "B.iris";
        std::string_view const module_b_code = R"(
            module B;

            import C as C;

            export using My_int = C.My_int;
        )";
        h::common::write_to_file(module_b_code_file_path, module_b_code);
        std::filesystem::path const module_b_file_path = parse_core_module(parser, build_directory_path, module_b_code_file_path);

        std::filesystem::path const module_c_code_file_path = root_directory / "C.iris";
        std::string_view const module_c_code = R"(
            module C;

            export using My_int = Int64;
        )";
        h::common::write_to_file(module_c_code_file_path, module_c_code);
        std::filesystem::path const module_c_file_path = parse_core_module(parser, build_directory_path, module_c_code_file_path);

        h::Module const previous_module_c = read_core_module(module_c_file_path);
        h::Symbol_name_to_hash const previous_symbol_name_to_hash = h::hash_module_declarations(previous_module_c, {});

        std::string_view const new_module_c_code = R"(
            module C;

            export using My_int = Int32;
        )";
        h::common::write_to_file(module_c_code_file_path, new_module_c_code);
        parse_core_module(parser, build_directory_path, module_c_code_file_path);

        h::Module const new_module_c = read_core_module(module_c_file_path);
        h::Symbol_name_to_hash const new_symbol_name_to_hash = h::hash_module_declarations(new_module_c, {});


        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path
        {
            std::make_pair("A", module_a_file_path),
            std::make_pair("B", module_b_file_path),
            std::make_pair("C", module_c_file_path),
        };

        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const module_name_to_reverse_dependencies
        {
            std::make_pair("B", "A"),
            std::make_pair("C", "B"),
        };

        std::pmr::vector<std::pmr::string> const modules_to_recompile = h::compiler::find_modules_to_recompile(
            new_module_c,
            previous_symbol_name_to_hash,
            new_symbol_name_to_hash,
            module_name_to_file_path,
            module_name_to_reverse_dependencies,
            {},
            {}
        );

        std::pmr::vector<std::pmr::string> const expected_modules_to_recompile
        {
            "B",
            "A",
        };

        CHECK(modules_to_recompile == expected_modules_to_recompile);
    }

    TEST_CASE("Recompile modules containing enums that depend on changed export interface and propagate changes", "[Recompilation]")
    {
        SKIP();

        std::filesystem::path const root_directory = setup_root_directory("recompilation_7");
        std::filesystem::path const build_directory_path = setup_build_directory(root_directory);
        h::parser::Parser const parser = h::parser::create_parser();

        std::filesystem::path const module_a_code_file_path = root_directory / "A.iris";
        std::string_view const module_a_code = R"(
            module A;

            import B as B;

            export function main() -> (result: Int32)
            {
                var value: B.My_enum = B.My_enum.A;
                return 0;
            }
        )";
        h::common::write_to_file(module_a_code_file_path, module_a_code);
        std::filesystem::path const module_a_file_path = parse_core_module(parser, build_directory_path, module_a_code_file_path);

        std::filesystem::path const module_b_code_file_path = root_directory / "B.iris";
        std::string_view const module_b_code = R"(
            module B;

            import C as C;

            export using My_enum = C.My_enum;
        )";
        h::common::write_to_file(module_b_code_file_path, module_b_code);
        std::filesystem::path const module_b_file_path = parse_core_module(parser, build_directory_path, module_b_code_file_path);

        std::filesystem::path const module_c_code_file_path = root_directory / "C.iris";
        std::string_view const module_c_code = R"(
            module C;

            export enum My_enum
            {
                A = 0,
                B = 1,
            }
        )";
        h::common::write_to_file(module_c_code_file_path, module_c_code);
        std::filesystem::path const module_c_file_path = parse_core_module(parser, build_directory_path, module_c_code_file_path);

        h::Module const previous_module_c = read_core_module(module_c_file_path);
        h::Symbol_name_to_hash const previous_symbol_name_to_hash = h::hash_module_declarations(previous_module_c, {});

        std::string_view const new_module_c_code = R"(
            module C;

            export enum My_enum
            {
                A = 1,
                B = 2,
            }
        )";
        h::common::write_to_file(module_c_code_file_path, new_module_c_code);
        parse_core_module(parser, build_directory_path, module_c_code_file_path);

        h::Module const new_module_c = read_core_module(module_c_file_path);
        h::Symbol_name_to_hash const new_symbol_name_to_hash = h::hash_module_declarations(new_module_c, {});


        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path
        {
            std::make_pair("A", module_a_file_path),
            std::make_pair("B", module_b_file_path),
            std::make_pair("C", module_c_file_path),
        };

        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const module_name_to_reverse_dependencies
        {
            std::make_pair("B", "A"),
            std::make_pair("C", "B"),
        };

        std::pmr::vector<std::pmr::string> const modules_to_recompile = h::compiler::find_modules_to_recompile(
            new_module_c,
            previous_symbol_name_to_hash,
            new_symbol_name_to_hash,
            module_name_to_file_path,
            module_name_to_reverse_dependencies,
            {},
            {}
        );

        std::pmr::vector<std::pmr::string> const expected_modules_to_recompile
        {
            "B",
            "A",
        };

        CHECK(modules_to_recompile == expected_modules_to_recompile);
    }

    TEST_CASE("Recompile modules containing functions that depend on changed export interface", "[Recompilation]")
    {
        SKIP();

        std::filesystem::path const root_directory = setup_root_directory("recompilation_8");
        std::filesystem::path const build_directory_path = setup_build_directory(root_directory);
        h::parser::Parser const parser = h::parser::create_parser();

        std::filesystem::path const module_a_code_file_path = root_directory / "A.iris";
        std::string_view const module_a_code = R"(
            module A;

            import B as B;

            export function main() -> (result: Int32)
            {
                var value = B.add(1i32, 2i32);
                return 0;
            }
        )";
        h::common::write_to_file(module_a_code_file_path, module_a_code);
        std::filesystem::path const module_a_file_path = parse_core_module(parser, build_directory_path, module_a_code_file_path);

        std::filesystem::path const module_b_code_file_path = root_directory / "B.iris";
        std::string_view const module_b_code = R"(
            module B;

            import C as C;

            export function add(a: Int32, b: Int32) -> (result: Int32)
            {
                return C.add(a, b);
            }
        )";
        h::common::write_to_file(module_b_code_file_path, module_b_code);
        std::filesystem::path const module_b_file_path = parse_core_module(parser, build_directory_path, module_b_code_file_path);

        std::filesystem::path const module_c_code_file_path = root_directory / "C.iris";
        std::string_view const module_c_code = R"(
            module C;

            export function add(a: Int32, b: Int32) -> (result: Int32)
            {
                return a + b;
            }
        )";
        h::common::write_to_file(module_c_code_file_path, module_c_code);
        std::filesystem::path const module_c_file_path = parse_core_module(parser, build_directory_path, module_c_code_file_path);

        h::Module const previous_module_c = read_core_module(module_c_file_path);
        h::Symbol_name_to_hash const previous_symbol_name_to_hash = h::hash_module_declarations(previous_module_c, {});

        std::string_view const new_module_c_code = R"(
            module C;

            export function add(a: Int64, b: Int64) -> (result: Int64)
            {
                return a + b;
            }
        )";
        h::common::write_to_file(module_c_code_file_path, new_module_c_code);
        parse_core_module(parser, build_directory_path, module_c_code_file_path);

        h::Module const new_module_c = read_core_module(module_c_file_path);
        h::Symbol_name_to_hash const new_symbol_name_to_hash = h::hash_module_declarations(new_module_c, {});


        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path
        {
            std::make_pair("A", module_a_file_path),
            std::make_pair("B", module_b_file_path),
            std::make_pair("C", module_c_file_path),
        };

        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const module_name_to_reverse_dependencies
        {
            std::make_pair("B", "A"),
            std::make_pair("C", "B"),
        };

        std::pmr::vector<std::pmr::string> const modules_to_recompile = h::compiler::find_modules_to_recompile(
            new_module_c,
            previous_symbol_name_to_hash,
            new_symbol_name_to_hash,
            module_name_to_file_path,
            module_name_to_reverse_dependencies,
            {},
            {}
        );

        std::pmr::vector<std::pmr::string> const expected_modules_to_recompile
        {
            "B"
        };

        CHECK(modules_to_recompile == expected_modules_to_recompile);
    }

    TEST_CASE("Do not recompile modules when changing function definitions", "[Recompilation]")
    {
        std::filesystem::path const root_directory = setup_root_directory("recompilation_9");
        std::filesystem::path const build_directory_path = setup_build_directory(root_directory);
        h::parser::Parser const parser = h::parser::create_parser();

        std::filesystem::path const module_a_code_file_path = root_directory / "A.iris";
        std::string_view const module_a_code = R"(
            module A;

            import B as B;

            export function main() -> (result: Int32)
            {
                var value = B.add(1i32, 2i32);
                return 0;
            }
        )";
        h::common::write_to_file(module_a_code_file_path, module_a_code);
        std::filesystem::path const module_a_file_path = parse_core_module(parser, build_directory_path, module_a_code_file_path);

        std::filesystem::path const module_b_code_file_path = root_directory / "B.iris";
        std::string_view const module_b_code = R"(
            module B;

            import C as C;

            export function add(a: Int32, b: Int32) -> (result: Int32)
            {
                return C.add(a, b);
            }
        )";
        h::common::write_to_file(module_b_code_file_path, module_b_code);
        std::filesystem::path const module_b_file_path = parse_core_module(parser, build_directory_path, module_b_code_file_path);

        std::filesystem::path const module_c_code_file_path = root_directory / "C.iris";
        std::string_view const module_c_code = R"(
            module C;

            export function add(a: Int32, b: Int32) -> (result: Int32)
            {
                return a + b;
            }
        )";
        h::common::write_to_file(module_c_code_file_path, module_c_code);
        std::filesystem::path const module_c_file_path = parse_core_module(parser, build_directory_path, module_c_code_file_path);

        h::Module const previous_module_c = read_core_module(module_c_file_path);
        h::Symbol_name_to_hash const previous_symbol_name_to_hash = h::hash_module_declarations(previous_module_c, {});

        std::string_view const new_module_c_code = R"(
            module C;

            export function add(other_a: Int32, other_b: Int32) -> (other_result: Int32)
            {
                return a - b;
            }
        )";
        h::common::write_to_file(module_c_code_file_path, new_module_c_code);
        parse_core_module(parser, build_directory_path, module_c_code_file_path);

        h::Module const new_module_c = read_core_module(module_c_file_path);
        h::Symbol_name_to_hash const new_symbol_name_to_hash = h::hash_module_declarations(new_module_c, {});


        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path
        {
            std::make_pair("A", module_a_file_path),
            std::make_pair("B", module_b_file_path),
            std::make_pair("C", module_c_file_path),
        };

        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const module_name_to_reverse_dependencies
        {
            std::make_pair("B", "A"),
            std::make_pair("C", "B"),
        };

        std::pmr::vector<std::pmr::string> const modules_to_recompile = h::compiler::find_modules_to_recompile(
            new_module_c,
            previous_symbol_name_to_hash,
            new_symbol_name_to_hash,
            module_name_to_file_path,
            module_name_to_reverse_dependencies,
            {},
            {}
        );

        std::pmr::vector<std::pmr::string> const expected_modules_to_recompile
        {
        };

        CHECK(modules_to_recompile == expected_modules_to_recompile);
    }
}
