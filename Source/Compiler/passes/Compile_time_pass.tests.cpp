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
import h.compiler.clang_code_generation;
import h.compiler.compile_time_pass;
import h.compiler.pass_test_helpers;
import h.core;
import h.core.declarations;
import h.core.formatter;
import h.parser.convertor;

#include <catch2/catch_test_macros.hpp>

import std;

namespace h::compiler
{
    struct Compile_time_runtime_context
    {
        h::compiler::LLVM_data llvm_data;
        h::compiler::Clang_context clang_context;
    };

    static Compile_time_runtime_context create_compile_time_runtime_context(
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

        return Compile_time_runtime_context
        {
            .llvm_data = std::move(llvm_data),
            .clang_context = std::move(clang_context),
        };
    }

    static std::pmr::string run_compile_time_pass_and_format(
        std::string_view const input_text,
        std::string_view const function_name
    )
    {
        h::compiler::tests::Parsed_module_context context = h::compiler::tests::parse_module_context(input_text, {});

        h::Function_declaration* function_declaration = h::compiler::tests::find_mutable_function_declaration(context.core_module, function_name);
        REQUIRE(function_declaration != nullptr);

        h::Function_definition* function_definition = h::compiler::tests::find_mutable_function_definition(context.core_module, function_name);
        REQUIRE(function_definition != nullptr);

        Compile_time_runtime_context runtime_context = create_compile_time_runtime_context(context.core_module, context.declaration_database);

        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        Compile_time_parameters const parameters =
        {
            .core_module = context.core_module,
            .output_allocator = output_allocator,
            .temporaries_allocator = temporaries_allocator,
            .llvm_context = *runtime_context.llvm_data.context,
            .llvm_data_layout = runtime_context.llvm_data.data_layout,
            .declaration_database = context.declaration_database,
            .clang_context = runtime_context.clang_context,
        };

        run_compile_time_pass_on_function(
            *function_declaration,
            *function_definition,
            parameters
        );

        return h::compiler::tests::format_core_module_to_text(context.core_module);
    }

    TEST_CASE("Evaluates compile_time if with true condition", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_if;

var g_debug = true;

export function run_0() -> (result: Int32)
{
    compile_time if g_debug
    {
        return 0;
    }
    else
    {
        return 1;
    }
}
)";

        std::string_view const expected = R"(module compile_time_if;

var g_debug = true;

export function run_0() -> (result: Int32)
{
    {
        return 0;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_0");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time if with false condition via logical not", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_if;

var g_debug = true;

export function run_1() -> (result: Int32)
{
    compile_time if !g_debug
    {
        return 2;
    }
    else
    {
        return 3;
    }
}
)";

        std::string_view const expected = R"(module compile_time_if;

var g_debug = true;

export function run_1() -> (result: Int32)
{
    {
        return 3;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_1");

        CHECK(expected == actual);
    }

    TEST_CASE("Unrolls compile_time for loop", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_for;

function foo(index: Uint64) -> ()
{
}

function run() -> ()
{
    compile_time for index in 0u64 to 3u64
    {
        foo(index);
    }
}
)";

        std::string_view const expected = R"(module compile_time_for;

function foo(index: Uint64) -> ()
{
}

function run() -> ()
{
    {
        {
            foo(0i64);
        }
        {
            foo(1i64);
        }
        {
            foo(2i64);
        }
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time reflection size_of", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_reflection;

export function run_size() -> (result: Int32)
{
    compile_time if @size_of::<Int32>() == 4u64
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
)";

        std::string_view const expected = R"(module compile_time_reflection;

export function run_size() -> (result: Int32)
{
    {
        return 1;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_size");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time reflection alignment_of", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_reflection;

export function run_alignment() -> (result: Int32)
{
    compile_time if @alignment_of::<Int32>() >= 1u64
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
)";

        std::string_view const expected = R"(module compile_time_reflection;

export function run_alignment() -> (result: Int32)
{
    {
        return 1;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_alignment");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time binary equal", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_binary_expression;

export function run_equal() -> (result: Int32)
{
    compile_time if 2i64 == 2i64
    {
        return 11;
    }
    else
    {
        return 12;
    }
}
)";

        std::string_view const expected = R"(module compile_time_binary_expression;

export function run_equal() -> (result: Int32)
{
    {
        return 11;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_equal");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time binary not_equal", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_binary_expression;

export function run_not_equal() -> (result: Int32)
{
    compile_time if 2i64 != 2i64
    {
        return 21;
    }
    else
    {
        return 22;
    }
}
)";

        std::string_view const expected = R"(module compile_time_binary_expression;

export function run_not_equal() -> (result: Int32)
{
    {
        return 22;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_not_equal");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time binary less_than", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_binary_expression;

export function run_less_than() -> (result: Int32)
{
    compile_time if 2i64 < 5i64
    {
        return 31;
    }
    else
    {
        return 32;
    }
}
)";

        std::string_view const expected = R"(module compile_time_binary_expression;

export function run_less_than() -> (result: Int32)
{
    {
        return 31;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_less_than");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time binary less_than_or_equal_to", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_binary_expression;

export function run_less_than_or_equal_to() -> (result: Int32)
{
    compile_time if 8i64 <= 5i64
    {
        return 41;
    }
    else
    {
        return 42;
    }
}
)";

        std::string_view const expected = R"(module compile_time_binary_expression;

export function run_less_than_or_equal_to() -> (result: Int32)
{
    {
        return 42;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_less_than_or_equal_to");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time binary greater_than", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_binary_expression;

export function run_greater_than() -> (result: Int32)
{
    compile_time if 8i64 > 5i64
    {
        return 51;
    }
    else
    {
        return 52;
    }
}
)";

        std::string_view const expected = R"(module compile_time_binary_expression;

export function run_greater_than() -> (result: Int32)
{
    {
        return 51;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_greater_than");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time binary greater_than_or_equal_to", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_binary_expression;

export function run_greater_than_or_equal_to() -> (result: Int32)
{
    compile_time if 3i64 >= 7i64
    {
        return 61;
    }
    else
    {
        return 62;
    }
}
)";

        std::string_view const expected = R"(module compile_time_binary_expression;

export function run_greater_than_or_equal_to() -> (result: Int32)
{
    {
        return 62;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_greater_than_or_equal_to");

        CHECK(expected == actual);
    }
}
