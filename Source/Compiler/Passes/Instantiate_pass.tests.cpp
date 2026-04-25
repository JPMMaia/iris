import iris.compiler;
import iris.compiler.all_passes;
import iris.compiler.clang_code_generation;
import iris.compiler.clang_data;
import iris.compiler.instantiate_pass;
import iris.compiler.pass_test_helpers;
import iris.core;
import iris.core.declarations;
import iris.core.formatter;
import iris.core.hash;
import iris.core.string_hash;
import iris.parser.convertor;

#include <catch2/catch_test_macros.hpp>

import std;

namespace iris::compiler
{
    struct Instantiate_runtime_context
    {
        iris::compiler::LLVM_data llvm_data;
        iris::compiler::Clang_context_pointer clang_context;
    };

    static Instantiate_runtime_context create_instantiate_runtime_context(
        iris::Module const& core_module,
        iris::Declaration_database const& declaration_database
    )
    {
        iris::compiler::Compilation_options const options =
        {
            .is_optimized = false,
            .debug = true,
        };

        iris::compiler::LLVM_data llvm_data = iris::compiler::initialize_llvm(options);

        iris::compiler::Clang_context_pointer clang_context = iris::compiler::create_clang_context(
            *llvm_data.context,
            *llvm_data.clang_data,
            "Iris_clang_module"
        );

        return Instantiate_runtime_context
        {
            .llvm_data = std::move(llvm_data),
            .clang_context = std::move(clang_context),
        };
    }

    static std::pmr::string run_instantiate_pass_and_format(
        std::string_view const input_text,
        std::span<std::string_view const> const input_dependencies_text,
        std::string_view const function_name
    )
    {
        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input_text, input_dependencies_text);

        iris::Function_declaration* function_declaration = iris::compiler::tests::find_mutable_function_declaration(context.core_module, function_name);
        REQUIRE(function_declaration != nullptr);

        iris::Function_definition* function_definition = iris::compiler::tests::find_mutable_function_definition(context.core_module, function_name);
        REQUIRE(function_definition != nullptr);

        Instantiate_runtime_context runtime_context = create_instantiate_runtime_context(context.core_module, context.declaration_database);

        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        All_passes_parameters const parameters =
        {
            .llvm_context = *runtime_context.llvm_data.context,
            .llvm_data_layout = runtime_context.llvm_data.data_layout,
            .declaration_database = context.declaration_database,
            .clang_context = *runtime_context.clang_context,
            .dependencies = context.core_module.dependencies,
            .instanced_declarations = context.core_module.instanced_declarations,
            .definitions = context.core_module.definitions,
            .output_allocator = output_allocator,
            .temporaries_allocator = temporaries_allocator,
        };

        run_instantiate_pass_on_function(
            context.core_module,
            *function_declaration,
            *function_definition,
            parameters
        );

        return iris::compiler::tests::format_core_module_to_text(context.core_module);
    }

    static std::pmr::string run_instantiate_pass_and_format(
        std::string_view const input_text,
        std::span<std::string_view const> const input_dependencies_text
    )
    {
        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input_text, input_dependencies_text);

        Instantiate_runtime_context runtime_context = create_instantiate_runtime_context(context.core_module, context.declaration_database);

        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        All_passes_parameters const parameters =
        {
            .llvm_context = *runtime_context.llvm_data.context,
            .llvm_data_layout = runtime_context.llvm_data.data_layout,
            .declaration_database = context.declaration_database,
            .clang_context = *runtime_context.clang_context,
            .dependencies = context.core_module.dependencies,
            .instanced_declarations = context.core_module.instanced_declarations,
            .definitions = context.core_module.definitions,
            .output_allocator = output_allocator,
            .temporaries_allocator = temporaries_allocator,
        };

        run_instantiate_pass_on_module(context.core_module, parameters);

        return iris::compiler::tests::format_core_module_to_text(context.core_module);
    }

    TEST_CASE("Replaces function constructor calls after instantiation", "[Instantiate_pass][Passes]")
    {
        std::string_view const input = R"(module Function_constructor;

export function_constructor add(value_type: Type)
{
    return function (first: value_type, second: value_type) -> (result: value_type)
    {
        return first + second;
    };
}

function run() -> ()
{
    var a = add::<Int32>(1, 2);
    var b = add::<Float32>(3.0f32, 4.0f32);
    var c = add(1u32, 2u32);
}
)";

        std::string_view const expected = R"(module Function_constructor;

export function_constructor add(value_type: Type)
{
    return function (first: value_type, second: value_type) -> (result: value_type)
    {
        return first + second;
    };
}

@unique_name("Function_constructor@add@10481941949038830817")
function Function_constructor@add@10481941949038830817(first: Int32, second: Int32) -> (result: Int32)
{
    return first + second;
}

@unique_name("Function_constructor@add@4195550094456234142")
function Function_constructor@add@4195550094456234142(first: Float32, second: Float32) -> (result: Float32)
{
    return first + second;
}

@unique_name("Function_constructor@add@3937835667124936396")
function Function_constructor@add@3937835667124936396(first: Uint32, second: Uint32) -> (result: Uint32)
{
    return first + second;
}

function run() -> ()
{
    var a = Function_constructor@add@10481941949038830817(1, 2);
    var b = Function_constructor@add@4195550094456234142(3.0f32, 4.0f32);
    var c = Function_constructor@add@3937835667124936396(1u32, 2u32);
}
)";

        std::pmr::string const actual = run_instantiate_pass_and_format(input, {}, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("Replaces type constructor usages after instantiation", "[Instantiate_pass][Passes]")
    {
        std::string_view const input = R"(module Type_constructor;

export type_constructor Dynamic_array(element_type: Type)
{
    return struct
    {
        data: *element_type = null;
        length: Uint64 = 0u64;
    };
}

function run(instance_0: Dynamic_array::<Float16>) -> ()
{
    var instance_1: Dynamic_array::<Int32> = {};
}
)";

        std::string_view const expected = R"(module Type_constructor;

export type_constructor Dynamic_array(element_type: Type)
{
    return struct
    {
        data: *element_type = null;
        length: Uint64 = 0u64;
    };
}

@unique_name("Type_constructor@Dynamic_array@9266664480299747837")
struct Type_constructor@Dynamic_array@9266664480299747837
{
    data: *Float16 = null;
    length: Uint64 = 0u64;
}

@unique_name("Type_constructor@Dynamic_array@12246575587352456780")
struct Type_constructor@Dynamic_array@12246575587352456780
{
    data: *Int32 = null;
    length: Uint64 = 0u64;
}

function run(instance_0: Type_constructor@Dynamic_array@9266664480299747837) -> ()
{
    var instance_1: Type_constructor@Dynamic_array@12246575587352456780 = {};
}
)";

        std::pmr::string const actual = run_instantiate_pass_and_format(input, {}, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("Instantiates members of structs", "[Instantiate_pass][Passes]")
    {
        std::string_view const input = R"(module Type_constructor;

export type_constructor Dynamic_array(element_type: Type)
{
    return struct
    {
        data: *element_type = null;
        length: Uint64 = 0u64;
    };
}

struct My_array
{
    data: Dynamic_array::<Int32> = {};
}
)";

        std::string_view const expected = R"(module Type_constructor;

export type_constructor Dynamic_array(element_type: Type)
{
    return struct
    {
        data: *element_type = null;
        length: Uint64 = 0u64;
    };
}

@unique_name("Type_constructor@Dynamic_array@12246575587352456780")
struct Type_constructor@Dynamic_array@12246575587352456780
{
    data: *Int32 = null;
    length: Uint64 = 0u64;
}

struct My_array
{
    data: Type_constructor@Dynamic_array@12246575587352456780 = {};
}
)";

        std::pmr::string const actual = run_instantiate_pass_and_format(input, {});

        CHECK(expected == actual);
    }

    TEST_CASE("Instantiates nested function constructor calls across module boundary", "[Instantiate_pass][Passes]")
    {
        std::string_view const dependency = R"(module iris.json;

export function_constructor to_json(value_type: Type)
{
    return function(value: *value_type) -> ()
    {
    };
}

export function_constructor print_json(value_type: Type)
{
    return function(value: *value_type) -> ()
    {
        to_json::<value_type>(value);
    };
}
)";

        std::string_view const input = R"(module json_usage;

import iris.json as iris_json;

function run(value: *Int32) -> ()
{
    iris_json.print_json::<Int32>(value);
}
)";


        std::string_view const expected = R"(module json_usage;

import iris.json as iris_json;

function run(value: *Int32) -> ()
{
    iris.json@print_json@9753731967319569499(value);
}

@unique_name("iris.json@to_json@3489948734076117284")
function iris.json@to_json@3489948734076117284(value: *Int32) -> ()
{
}

@unique_name("iris.json@print_json@9753731967319569499")
function iris.json@print_json@9753731967319569499(value: *Int32) -> ()
{
    iris.json@to_json@3489948734076117284(value);
}
)";

        std::array<std::string_view, 1> const dependencies = { dependency };
        std::pmr::string const actual = run_instantiate_pass_and_format(input, dependencies, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("Instantiates function constructor calls print_json", "[Instantiate_pass][Passes]")
    {
        std::string_view const input = R"(module json_usage;

function_constructor to_json(value_type: Type)
{
    return function(value: *value_type) -> ()
    {
    };
}

function_constructor print_json(value_type: Type)
{
    return function(value: *value_type) -> ()
    {
        to_json::<value_type>(value);
    };
}

export function run(value: *Int32) -> ()
{
    print_json::<Int32>(value);
}
)";

std::string_view const expected = R"(module json_usage;

function_constructor to_json(value_type: Type)
{
    return function (value: *value_type) -> ()
    {
    };
}

@unique_name("json_usage@to_json@8192659410663046636")
function json_usage@to_json@8192659410663046636(value: *Int32) -> ()
{
}

function_constructor print_json(value_type: Type)
{
    return function (value: *value_type) -> ()
    {
        to_json::<value_type>(value);
    };
}

@unique_name("json_usage@print_json@8253239461601449526")
function json_usage@print_json@8253239461601449526(value: *Int32) -> ()
{
    json_usage@to_json@8192659410663046636(value);
}

export function run(value: *Int32) -> ()
{
    json_usage@print_json@8253239461601449526(value);
}
)";

        std::pmr::string const actual = run_instantiate_pass_and_format(input, {}, "run");

        CHECK(expected == actual);
    }
}