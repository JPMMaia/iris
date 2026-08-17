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
        iris::Module& core_module = context.core_module();

        std::optional<Function_declaration const*> const function_declaration = find_function_declaration(core_module, function_name);
        REQUIRE(function_declaration.has_value());

        iris::Function_definition* function_definition = find_mutable_function_definition(core_module, function_name);
        REQUIRE(function_definition != nullptr);

        run_implicit_function_pass_on_function(
            core_module.name,
            core_module.dependencies,
            context.declaration_database,
            *function_declaration.value(),
            *function_definition
        );

        std::pmr::string const actual = iris::compiler::tests::format_core_module_to_text(core_module);

        CHECK(expected == actual);
    }

    TEST_CASE("Replaces implicit dot call with explicit function call", "[Implicit_function_pass][Passes]")
    {
        std::string_view const input = R"(module Implicit_arguments;

export struct My_struct
{
    v0: Int32 = 1;
    v1: Int32 = 2;
}

export function get_v0(instance: *My_struct) -> (result: Int32)
{
    return instance->v0;
}

function run() -> ()
{
    mutable instance: My_struct = {};
    var a = instance.get_v0();
}
)";

        std::string_view const expected = R"(module Implicit_arguments;

export struct My_struct
{
    v0: Int32 = 1;
    v1: Int32 = 2;
}

export function get_v0(instance: *My_struct) -> (result: Int32)
{
    return instance->v0;
}

function run() -> ()
{
    mutable instance: My_struct = {};
    var a = get_v0(&instance);
}
)";

        test_implicit_function_pass_on_function(input, {}, "run", expected);
    }

    TEST_CASE("Replaces implicit pointer call with explicit function call", "[Implicit_function_pass][Passes]")
    {
        std::string_view const input = R"(module Implicit_arguments;

export struct My_struct
{
    v0: Int32 = 1;
    v1: Int32 = 2;
}

export function get_v0(instance: *My_struct) -> (result: Int32)
{
    return instance->v0;
}

function run() -> ()
{
    mutable instance: My_struct = {};
    var instance_pointer = &instance;
    var b = instance_pointer->get_v0();
}
)";

        std::string_view const expected = R"(module Implicit_arguments;

export struct My_struct
{
    v0: Int32 = 1;
    v1: Int32 = 2;
}

export function get_v0(instance: *My_struct) -> (result: Int32)
{
    return instance->v0;
}

function run() -> ()
{
    mutable instance: My_struct = {};
    var instance_pointer = &instance;
    var b = get_v0(instance_pointer);
}
)";

        test_implicit_function_pass_on_function(input, {}, "run", expected);
    }

    TEST_CASE("Replaces implicit dot call with explicit function call for imported module", "[Implicit_function_pass][Passes]")
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

    TEST_CASE("Does not replace implicit call when the module is only reachable transitively", "[Implicit_function_pass][Passes]")
    {
        std::string_view const module_a_text = R"(module Implicit_arguments;

export struct My_struct
{
    v0: Int32 = 1;
}

export function get_v0(instance: *My_struct) -> (result: Int32)
{
    return instance->v0;
}
)";

        std::string_view const module_b_text = R"(module Implicit_arguments_holder;

import Implicit_arguments as em;

export struct Holder
{
    instance: em.My_struct = {};
}

export function get_instance(holder: *mutable Holder) -> (result: *mutable em.My_struct)
{
    return &holder->instance;
}
)";

        std::string_view const input = R"(module Implicit_arguments_external;

import Implicit_arguments_holder as hm;

function run() -> ()
{
    mutable holder: hm.Holder = {};
    var instance = hm.get_instance(&holder);
    var a = instance->get_v0();
}
)";

        std::pmr::vector<std::string_view> const dependencies = { module_a_text, module_b_text };

        // The rewrite would have to name 'Implicit_arguments', which is not imported here, so the
        // statement is left alone; validation rejects the call before code generation sees it.
        test_implicit_function_pass_on_function(input, dependencies, "run", input);
    }

    TEST_CASE("Replaces implicit pointer call with explicit function call for imported module", "[Implicit_function_pass][Passes]")
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