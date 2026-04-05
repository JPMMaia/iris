#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclBase.h>
#include <clang/AST/Type.h>
#include <clang/Basic/Builtins.h>
#include <clang/Basic/CodeGenOptions.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/IdentifierTable.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/CodeGen/CodeGenABITypes.h>
#include <clang/CodeGen/CGFunctionInfo.h>
#include <clang/CodeGen/ModuleBuilder.h>
#include "clang/Frontend/CompilerInstance.h"
#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/VirtualFileSystem.h>

import h.compiler;
import h.compiler.all_passes;
import h.compiler.clang_code_generation;
import h.compiler.clang_data;
import h.compiler.pass_test_helpers;
import h.core;
import h.core.declarations;
import h.core.formatter;
import h.parser.convertor;

#include <catch2/catch_test_macros.hpp>

import std;

namespace h::compiler
{
    struct All_passes_runtime_context
    {
        h::compiler::LLVM_data llvm_data;
        h::compiler::Clang_context clang_context;
    };

    static All_passes_runtime_context create_all_passes_runtime_context(
        h::Module const& core_module,
        std::span<h::Module const> const dependency_core_modules,
        h::Declaration_database const& declaration_database
    )
    {
        h::compiler::Compilation_options const options =
        {
            .is_optimized = false,
            .debug = true,
        };

        h::compiler::LLVM_data llvm_data = h::compiler::initialize_llvm(options);

        std::pmr::vector<h::Module const*> sorted_modules;
        sorted_modules.reserve(dependency_core_modules.size() + 1);

        for (h::Module const& dependency_module : dependency_core_modules)
            sorted_modules.push_back(&dependency_module);

        sorted_modules.push_back(&core_module);

        h::compiler::Clang_context clang_context = h::compiler::create_clang_context(
            *llvm_data.context,
            llvm_data.clang_data,
            "Hl_clang_module"
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
        h::compiler::tests::Parsed_module_context context = h::compiler::tests::parse_module_context(input_text, input_dependencies_text);

        h::Function_declaration* function_declaration = h::compiler::tests::find_mutable_function_declaration(context.core_module, function_name);
        REQUIRE(function_declaration != nullptr);

        h::Function_definition* function_definition = h::compiler::tests::find_mutable_function_definition(context.core_module, function_name);
        REQUIRE(function_definition != nullptr);

        All_passes_runtime_context runtime_context = create_all_passes_runtime_context(
            context.core_module,
            context.dependency_core_modules,
            context.declaration_database
        );

        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        All_passes_parameters const parameters =
        {
            .llvm_context = *runtime_context.llvm_data.context,
            .llvm_data_layout = runtime_context.llvm_data.data_layout,
            .declaration_database = context.declaration_database,
            .clang_context = runtime_context.clang_context,
            .output_allocator = output_allocator,
            .temporaries_allocator = temporaries_allocator,
        };

        run_all_passes_on_function(
            context.core_module,
            *function_declaration,
            *function_definition,
            parameters
        );

        return h::compiler::tests::format_core_module_to_text(context.core_module);
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
)";

        std::pmr::vector<std::string_view> const dependencies = { dependency_text };
        std::pmr::string const actual = run_all_passes_and_format(input, dependencies, "run");

        CHECK(expected == actual);
    }
}
