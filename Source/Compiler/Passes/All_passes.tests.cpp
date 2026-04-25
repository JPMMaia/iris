import iris.compiler;
import iris.compiler.all_passes;
import iris.compiler.clang_code_generation;
import iris.compiler.clang_data;
import iris.compiler.pass_test_helpers;
import iris.core;
import iris.core.declarations;
import iris.core.formatter;
import iris.parser.convertor;

#include <catch2/catch_test_macros.hpp>

import std;

namespace iris::compiler
{
    struct All_passes_runtime_context
    {
        iris::compiler::LLVM_data llvm_data;
        iris::compiler::Clang_context_pointer clang_context;
    };

    static All_passes_runtime_context create_all_passes_runtime_context(
        iris::Module const& core_module,
        std::span<iris::Module const> const dependency_core_modules,
        iris::Declaration_database const& declaration_database
    )
    {
        iris::compiler::Compilation_options const options =
        {
            .is_optimized = false,
            .debug = true,
        };

        iris::compiler::LLVM_data llvm_data = iris::compiler::initialize_llvm(options);

        std::pmr::vector<iris::Module const*> sorted_modules;
        sorted_modules.reserve(dependency_core_modules.size() + 1);

        for (iris::Module const& dependency_module : dependency_core_modules)
            sorted_modules.push_back(&dependency_module);

        sorted_modules.push_back(&core_module);

        iris::compiler::Clang_context_pointer clang_context = iris::compiler::create_clang_context(
            *llvm_data.context,
            *llvm_data.clang_data,
            "Iris_clang_module"
        );

        return All_passes_runtime_context
        {
            .llvm_data = std::move(llvm_data),
            .clang_context = std::move(clang_context),
        };
    }

    static std::pmr::string run_all_passes_and_format(
        std::string_view const input_text,
        std::span<std::string_view const> const input_dependencies_text,
        std::string_view const function_name
    )
    {
        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input_text, input_dependencies_text);
        iris::Module& core_module = context.core_module();

        iris::Function_declaration* function_declaration = iris::compiler::tests::find_mutable_function_declaration(core_module, function_name);
        REQUIRE(function_declaration != nullptr);

        iris::Function_definition* function_definition = iris::compiler::tests::find_mutable_function_definition(core_module, function_name);
        REQUIRE(function_definition != nullptr);

        All_passes_runtime_context runtime_context = create_all_passes_runtime_context(
            core_module,
            context.dependencies(),
            context.declaration_database
        );

        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        All_passes_parameters const parameters =
        {
            .llvm_context = *runtime_context.llvm_data.context,
            .llvm_data_layout = runtime_context.llvm_data.data_layout,
            .declaration_database = context.declaration_database,
            .clang_context = *runtime_context.clang_context,
            .dependencies = core_module.dependencies,
            .instanced_declarations = core_module.instanced_declarations,
            .definitions = core_module.definitions,
            .output_allocator = output_allocator,
            .temporaries_allocator = temporaries_allocator,
        };

        run_all_passes_on_function(
            core_module.name,
            *function_declaration,
            *function_definition,
            parameters
        );

        return iris::compiler::tests::format_core_module_to_text(core_module);
    }

    TEST_CASE("Combines compile_time_pass and instantiate_pass", "[All_passes][Passes]")
    {
        std::string_view const input = R"(module All_passes_test_1;

var g_use_int32 = true;

export function_constructor add(value_type: Type)
{
    return function (first: value_type, second: value_type) -> (result: value_type)
    {
        return first + second;
    };
}

function run() -> ()
{
    compile_time if g_use_int32
    {
        var a = add::<Int32>(1, 2);
    }
    else
    {
        var a = add::<Float32>(1.0f32, 2.0f32);
    }
}
)";

        std::string_view const expected = R"(module All_passes_test_1;

var g_use_int32 = true;

export function_constructor add(value_type: Type)
{
    return function (first: value_type, second: value_type) -> (result: value_type)
    {
        return first + second;
    };
}

@unique_name("All_passes_test_1@add@3510542370392782654")
function All_passes_test_1@add@3510542370392782654(first: Int32, second: Int32) -> (result: Int32)
{
    return first + second;
}

function run() -> ()
{
    {
        var a = All_passes_test_1@add@3510542370392782654(1, 2);
    }
}
)";

        std::pmr::string const actual = run_all_passes_and_format(input, {}, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("Combines instantiate_pass and implicit_function_pass", "[All_passes][Passes]")
    {
        std::string_view const input = R"(module All_passes_test_2;

export type_constructor Box(element_type: Type)
{
    return struct
    {
        value: element_type;
    };
}

export function get_value(instance: *Box::<element_type>, element_type: Type) -> (result: element_type)
{
    return instance->value;
}

function run() -> ()
{
    mutable b: Box::<Int32> = {};
    var v = b.get_value();
}
)";

        std::string_view const expected = R"(module All_passes_test_2;

export type_constructor Box(element_type: Type)
{
    return struct
    {
        value: element_type = ;
    };
}

@unique_name("All_passes_test_2@Box@8186224852659227827")
struct All_passes_test_2@Box@8186224852659227827
{
    value: Int32 = ;
}

export function get_value(instance: *Box::<element_type>, element_type: Type) -> (result: element_type)
{
    return instance->value;
}

function run() -> ()
{
    mutable b: All_passes_test_2@Box@8186224852659227827 = {};
    var v = get_value(&b);
}
)";

        std::pmr::string const actual = run_all_passes_and_format(input, {}, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("Combines compile_time_pass, instantiate_pass, and implicit_function_pass", "[All_passes][Passes]")
    {
        std::string_view const input = R"(module All_passes_test_3;

var g_enabled = true;

export type_constructor Box(element_type: Type)
{
    return struct
    {
        value: element_type;
    };
}

export function get_value(instance: *Box::<element_type>, element_type: Type) -> (result: element_type)
{
    return instance->value;
}

function run() -> ()
{
    compile_time if g_enabled
    {
        mutable b: Box::<Int32> = {};
        var v = b.get_value();
    }
    else
    {
        mutable b: Box::<Float32> = {};
        var v = b.get_value();
    }
}
)";

        std::string_view const expected = R"(module All_passes_test_3;

var g_enabled = true;

export type_constructor Box(element_type: Type)
{
    return struct
    {
        value: element_type = ;
    };
}

@unique_name("All_passes_test_3@Box@16743479164112415117")
struct All_passes_test_3@Box@16743479164112415117
{
    value: Int32 = ;
}

export function get_value(instance: *Box::<element_type>, element_type: Type) -> (result: element_type)
{
    return instance->value;
}

function run() -> ()
{
    {
        mutable b: All_passes_test_3@Box@16743479164112415117 = {};
        var v = get_value(&b);
    }
}
)";

        std::pmr::string const actual = run_all_passes_and_format(input, {}, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("Transform dynamic_array", "[All_passes][Passes]")
    {
        std::string_view const dependency_text = R"(module containers.dynamic_array;

export struct Allocator
{
    allocate: function<(size_in_bytes: Uint64, alignment_in_bytes: Uint64) -> (pointer: *mutable Byte)> = null;
    deallocate: function<(pointer: *mutable Byte) -> ()> = null;
}

export type_constructor Dynamic_array(element_type: Type)
{
    return struct
    {
        data: *mutable element_type = null;
        length: Uint64 = 0u64;
        capacity: Uint64 = 0u64;
        allocator: Allocator = {};
    };
}

export function_constructor create(element_type: Type)
{
    return function (allocator: Allocator) -> (instance: Dynamic_array::<element_type>)
        precondition "allocator.allocate != null" { allocator.allocate != null }
        precondition "allocator.deallocate != null" { allocator.deallocate != null }
    {
        return {
            data: null,
            length: 0u64,
            capacity: 0u64,
            allocator: allocator
        };
    };
}

export function_constructor push_back(element_type: Type)
{
    return function (instance: *mutable Dynamic_array::<element_type>, element: element_type) -> ()
        precondition "instance != null" { instance != null }
    {
        if instance->length == instance->capacity
        {
            var new_capacity = 2u64 * (instance->capacity + 1u64);

            var allocation_size_in_bytes = new_capacity * @size_of::<element_type>();
            var allocation = instance->allocator.allocate(allocation_size_in_bytes, @alignment_of::<element_type>());
            assert "Allocation did not fail" { allocation != null };

            instance->data = allocation as *mutable element_type;
            instance->capacity = new_capacity;
        }

        var index = instance->length;
        instance->data[index] = element;
        instance->length += 1u64;
    };
}

export function_constructor get(element_type: Type)
{
    return function (instance: *mutable Dynamic_array::<element_type>, index: Uint64) -> (result: element_type)
        precondition "instance != null" { instance != null }
        precondition "index < instance->length" { index < instance->length }
    {
        return instance->data[index];
    };
}
)";

        std::string_view const input = R"(module dynamic_array_usage;

import containers.dynamic_array as da;

function run() -> ()
{
    var allocator: da.Allocator = {};
    var instance = da.create::<Int32>(allocator);

    instance.push_back(1);
    var element = instance.get(0u64);
}
)";

        std::string_view const expected = R"(module dynamic_array_usage;

import containers.dynamic_array as da;

function run() -> ()
{
    var allocator: da.Allocator = {};
    var instance = containers.dynamic_array@create@2530642789161636205(allocator);

    containers.dynamic_array@push_back@11054321879898878598(&instance, 1);
    var element = containers.dynamic_array@get@9219431704710098038(&instance, 0u64);
}

@unique_name("containers.dynamic_array@Dynamic_array@5865945007316310718")
struct containers.dynamic_array@Dynamic_array@5865945007316310718
{
    data: *mutable Int32 = null;
    length: Uint64 = 0u64;
    capacity: Uint64 = 0u64;
    allocator: da.Allocator = {};
}

@unique_name("containers.dynamic_array@create@2530642789161636205")
function containers.dynamic_array@create@2530642789161636205(allocator: da.Allocator) -> (instance: da.containers.dynamic_array@Dynamic_array@5865945007316310718)
    precondition "allocator.allocate != null" { allocator.allocate != null }
    precondition "allocator.deallocate != null" { allocator.deallocate != null }
{
    return {
        data: null,
        length: 0u64,
        capacity: 0u64,
        allocator: allocator
    };
}

@unique_name("containers.dynamic_array@push_back@11054321879898878598")
function containers.dynamic_array@push_back@11054321879898878598(instance: *mutable da.containers.dynamic_array@Dynamic_array@5865945007316310718, element: Int32) -> ()
    precondition "instance != null" { instance != null }
{
    if instance->length == instance->capacity
    {
        var new_capacity = 2u64 * (instance->capacity + 1u64);

        var allocation_size_in_bytes = new_capacity * 4u64;
        var allocation = instance->allocator.allocate(allocation_size_in_bytes, 4u64);
        assert "Allocation did not fail" { allocation != null };

        instance->data = allocation as *mutable Int32;
        instance->capacity = new_capacity;
    }

    var index = instance->length;
    instance->data[index] = element;
    instance->length += 1u64;
}

@unique_name("containers.dynamic_array@get@9219431704710098038")
function containers.dynamic_array@get@9219431704710098038(instance: *mutable da.containers.dynamic_array@Dynamic_array@5865945007316310718, index: Uint64) -> (result: Int32)
    precondition "instance != null" { instance != null }
    precondition "index < instance->length" { index < instance->length }
{
    return instance->data[index];
}
)";

        std::pmr::vector<std::string_view> const dependencies = { dependency_text };
        std::pmr::string const actual = run_all_passes_and_format(input, dependencies, "run");

        CHECK(expected == actual);
    }
}
