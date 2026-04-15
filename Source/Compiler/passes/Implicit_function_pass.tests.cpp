#include <span>
#include <string_view>

#include <catch2/catch_all.hpp>

import iris.compiler.implicit_function_pass;
import iris.compiler.pass_test_helpers;
import iris.core;
import iris.core.declarations;
import iris.core.formatter;
import iris.parser.convertor;

namespace iris::compiler
{
    static iris::Function_definition* find_mutable_function_definition(
        iris::Module& core_module,
        std::string_view const function_name
    )
    {
        return iris::compiler::tests::find_mutable_function_definition(core_module, function_name);
    }

    static void test_implicit_function_pass_on_function(
        std::string_view const input_text,
        std::span<std::string_view const> const input_dependencies_text,
        std::string_view const function_name,
        std::string_view const expected
    )
    {
        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input_text, input_dependencies_text);

        std::optional<Function_declaration const*> const function_declaration = find_function_declaration(context.core_module, function_name);
        REQUIRE(function_declaration.has_value());

        iris::Function_definition* function_definition = find_mutable_function_definition(context.core_module, function_name);
        REQUIRE(function_definition != nullptr);

        run_implicit_function_pass_on_function(
            context.core_module,
            context.declaration_database,
            *function_declaration.value(),
            *function_definition
        );

        std::pmr::string const actual = iris::compiler::tests::format_core_module_to_text(context.core_module);

        CHECK(expected == actual);
    }

    TEST_CASE("Replaces implicit dot call with explicit function call", "[Implicit_function_pass][Passes]")
    {
        std::string_view const dependency_text = R"(module Implicit_arguments;

export struct My_struct
{
    v0: Int32 = 1;
}

export function get_v0(instance: *My_struct) -> (result: Int32)
{
    return instance->v0;
}
)";

        std::string_view const input = R"(module Implicit_arguments_external;

import Implicit_arguments as em;

function run() -> ()
{
    mutable instance: em.My_struct = {};
    var a = instance.get_v0();
}
)";

        std::string_view const expected = R"(module Implicit_arguments_external;

import Implicit_arguments as em;

function run() -> ()
{
    mutable instance: em.My_struct = {};
    var a = em.get_v0(&instance);
}
)";

        std::pmr::vector<std::string_view> const dependencies = { dependency_text };

        test_implicit_function_pass_on_function(input, dependencies, "run", expected);
    }

    TEST_CASE("Replaces implicit pointer call with explicit function call", "[Implicit_function_pass][Passes]")
    {
        std::string_view const dependency_text = R"(module Implicit_arguments;

export struct My_struct
{
    v0: Int32 = 1;
}

export function get_v0(instance: *My_struct) -> (result: Int32)
{
    return instance->v0;
}
)";

        std::string_view const input = R"(module Implicit_arguments_external;

import Implicit_arguments as em;

function run() -> ()
{
    mutable instance: em.My_struct = {};
    var instance_pointer = &instance;
    var b = instance_pointer->get_v0();
}
)";

        std::string_view const expected = R"(module Implicit_arguments_external;

import Implicit_arguments as em;

function run() -> ()
{
    mutable instance: em.My_struct = {};
    var instance_pointer = &instance;
    var b = em.get_v0(instance_pointer);
}
)";

        std::pmr::vector<std::string_view> const dependencies = { dependency_text };

        test_implicit_function_pass_on_function(input, dependencies, "run", expected);
    }
}