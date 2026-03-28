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
import h.compiler.instantiate_pass;
import h.compiler.pass_test_helpers;
import h.core;
import h.core.declarations;
import h.core.formatter;
import h.core.hash;
import h.core.string_hash;
import h.parser.convertor;

#include <catch2/catch_test_macros.hpp>

import std;

namespace h::compiler
{
    struct Instantiate_runtime_context
    {
        h::compiler::LLVM_data llvm_data;
        h::compiler::Clang_context clang_context;
    };

    static Instantiate_runtime_context create_instantiate_runtime_context(
        h::Module const& core_module,
        h::Declaration_database const& declaration_database
    )
    {
        h::compiler::Compilation_options const options =
        {
            .is_optimized = false,
            .debug = true,
        };

        h::compiler::LLVM_data llvm_data = h::compiler::initialize_llvm(options);

        h::compiler::Clang_context clang_context = h::compiler::create_clang_context(
            *llvm_data.context,
            llvm_data.clang_data,
            "Hl_clang_module"
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
        h::compiler::tests::Parsed_module_context context = h::compiler::tests::parse_module_context(input_text, input_dependencies_text);

        h::Function_declaration* function_declaration = h::compiler::tests::find_mutable_function_declaration(context.core_module, function_name);
        REQUIRE(function_declaration != nullptr);

        h::Function_definition* function_definition = h::compiler::tests::find_mutable_function_definition(context.core_module, function_name);
        REQUIRE(function_definition != nullptr);

        Instantiate_runtime_context runtime_context = create_instantiate_runtime_context(context.core_module, context.declaration_database);

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

        run_instantiate_pass_on_function(
            context.core_module,
            *function_declaration,
            *function_definition,
            parameters
        );

        return h::compiler::tests::format_core_module_to_text(context.core_module);
    }

    static std::pmr::string run_instantiate_pass_and_format(
        std::string_view const input_text,
        std::span<std::string_view const> const input_dependencies_text
    )
    {
        h::compiler::tests::Parsed_module_context context = h::compiler::tests::parse_module_context(input_text, input_dependencies_text);

        Instantiate_runtime_context runtime_context = create_instantiate_runtime_context(context.core_module, context.declaration_database);

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

        run_instantiate_pass_on_module(context.core_module, parameters);

        return h::compiler::tests::format_core_module_to_text(context.core_module);
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

struct My_array
{
    data: Type_constructor@Dynamic_array@12246575587352456780 = {};
}
)";

        std::pmr::string const actual = run_instantiate_pass_and_format(input, {});

        CHECK(expected == actual);
    }
}