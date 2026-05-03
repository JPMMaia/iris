import iris.compiler;
import iris.compiler.clang_code_generation;
import iris.compiler.compile_time_pass;
import iris.compiler.pass_test_helpers;
import iris.core;
import iris.core.declarations;
import iris.core.formatter;
import iris.parser.convertor;

#include <catch2/catch_test_macros.hpp>

import std;

namespace iris::compiler
{
    struct Compile_time_runtime_context
    {
        iris::compiler::LLVM_data llvm_data;
        iris::compiler::Clang_context_pointer clang_context;
    };

    static Compile_time_runtime_context create_compile_time_runtime_context(
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
        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input_text, {});
        iris::Module& core_module = context.core_module();

        iris::Function_declaration* function_declaration = iris::compiler::tests::find_mutable_function_declaration(core_module, function_name);
        REQUIRE(function_declaration != nullptr);

        iris::Function_definition* function_definition = iris::compiler::tests::find_mutable_function_definition(core_module, function_name);
        REQUIRE(function_definition != nullptr);

        Compile_time_runtime_context runtime_context = create_compile_time_runtime_context(core_module, context.declaration_database);

        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        Compile_time_parameters const parameters =
        {
            .dependencies = core_module.dependencies,
            .output_allocator = output_allocator,
            .temporaries_allocator = temporaries_allocator,
            .llvm_context = *runtime_context.llvm_data.context,
            .llvm_data_layout = runtime_context.llvm_data.data_layout,
            .declaration_database = context.declaration_database,
            .clang_context = *runtime_context.clang_context,
            .is_test_mode = false,
        };

        run_compile_time_pass_on_function(
            core_module.name,
            *function_declaration,
            *function_definition,
            parameters
        );

        return iris::compiler::tests::format_core_module_to_text(core_module);
    }

    static void run_compile_time_pass(
        iris::compiler::tests::Parsed_module_context& context,
        std::string_view const function_name
    )
    {
        iris::Module& core_module = context.core_module();

        iris::Function_declaration* function_declaration = iris::compiler::tests::find_mutable_function_declaration(core_module, function_name);
        REQUIRE(function_declaration != nullptr);

        iris::Function_definition* function_definition = iris::compiler::tests::find_mutable_function_definition(core_module, function_name);
        REQUIRE(function_definition != nullptr);

        Compile_time_runtime_context runtime_context = create_compile_time_runtime_context(core_module, context.declaration_database);

        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        Compile_time_parameters const parameters =
        {
            .dependencies = core_module.dependencies,
            .output_allocator = output_allocator,
            .temporaries_allocator = temporaries_allocator,
            .llvm_context = *runtime_context.llvm_data.context,
            .llvm_data_layout = runtime_context.llvm_data.data_layout,
            .declaration_database = context.declaration_database,
            .clang_context = *runtime_context.clang_context,
            .is_test_mode = false,
        };

        run_compile_time_pass_on_function(
            core_module.name,
            *function_declaration,
            *function_definition,
            parameters
        );
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

    TEST_CASE("Evaluates compile_time if with all branch conditions false", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_if;

var g_value = 2;

export function run_0() -> (result: Int32)
{
    compile_time if g_value == 0
    {
        return 0;
    }
    else if g_value == 1
    {
        return 1;
    }

    return 2;
}
)";

        std::string_view const expected = R"(module compile_time_if;

var g_value = 2;

export function run_0() -> (result: Int32)
{
    {
    }
    return 2;
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_0");

        CHECK(expected == actual);
    }

    TEST_CASE("Propagates compile_time var declaration and removes runtime local", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_var;

export function run() -> (result: Int32)
{
    compile_time var is_enabled = true;

    compile_time if is_enabled
    {
        return 7;
    }
    else
    {
        return 8;
    }
}
)";

        std::string_view const expected = R"(module compile_time_var;

export function run() -> (result: Int32)
{
    {
        return 7;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("Propagates compile_time var declaration to expressions", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_var;

function take(a: Int32)

export function run() -> ()
{
    compile_time var v0 = 1;
    take(v0);

    {
        take(v0);
    }

    for index in 0 to v0
    {
        take(v0 + 2);
    }
}
)";

        std::string_view const expected = R"(module compile_time_var;

export function run() -> ()
{
    take(1);

    {
        take(1);
    }

    for index in 0 to 1
    {
        take(1 + 2);
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run");

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

    TEST_CASE("Evaluates compile_time reflection type_name", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_reflection;

export function run_type_name() -> ()
{
    var value = @type_name::<Int32>();
}
)";

        std::string_view const expected = R"(module compile_time_reflection;

export function run_type_name() -> ()
{
    var value = "Int32"c;
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_type_name");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time reflection member_count", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_reflection;

struct Pair
{
    first: Int32 = 0;
    second: Int32 = 0;
}

export function run_member_count() -> (result: Int32)
{
    var value = @member_count::<Pair>();
}
)";

        std::string_view const expected = R"(module compile_time_reflection;

struct Pair
{
    first: Int32 = 0;
    second: Int32 = 0;
}

export function run_member_count() -> (result: Int32)
{
    var value = 2u64;
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_member_count");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time reflection member_type", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_reflection;

struct Pair
{
    first: Int32 = 0;
    second: Int32 = 0;
}

export function run_member_type() -> ()
{
    var value: @member_type::<Pair>(0u64) = 0;
}
)";

        std::string_view const expected = R"(module compile_time_reflection;

struct Pair
{
    first: Int32 = 0;
    second: Int32 = 0;
}

export function run_member_type() -> ()
{
    var value: Int32 = 0;
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_member_type");

        CHECK(expected == actual);
    }

    TEST_CASE("member_type records iris.builtin Type_kind import usage", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_reflection;

import iris.builtin as Builtin;

struct Metadata
{
    kind: Builtin.Type_kind = Builtin.Type_kind.Int;
}

export function run_member_type_usage() -> ()
{
    var value: @member_type::<Metadata>(0u64) = Builtin.Type_kind.Struct;
}
)";

        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input, {});
        run_compile_time_pass(context, "run_member_type_usage");

        Import_module_with_alias const* const builtin_import = find_import_module_with_alias(context.core_module().dependencies, "Builtin");
        REQUIRE(builtin_import != nullptr);

        auto const location = std::find(
            builtin_import->usages.begin(),
            builtin_import->usages.end(),
            "Type_kind"
        );

        CHECK(location != builtin_import->usages.end());
    }

    TEST_CASE("Rewrites check equality to print json difference before checking", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_check;

function run(lhs: Int32, rhs: Int32) -> ()
{
    check(lhs == rhs);
}
)";

        std::string_view const expected = R"(module compile_time_check;

import iris.json as iris_json;

function run(lhs: Int32, rhs: Int32) -> ()
{
    {
        var __condition = lhs == rhs;
        check(__condition);
        if !__condition
        {
            iris_json.print_json_difference::<Int32>(&lhs, &rhs);
        }
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("Rewrites check equality to print json difference before checking 2", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_check;

function run(lhs: Int32, rhs: Int32) -> ()
{
    check((1+2) == (3+4));
}
)";

        std::string_view const expected = R"(module compile_time_check;

import iris.json as iris_json;

function run(lhs: Int32, rhs: Int32) -> ()
{
    {
        var __lhs = (1 + 2);
        var __rhs = (3 + 4);
        var __condition = __lhs == __rhs;
        check(__condition);
        if !__condition
        {
            iris_json.print_json_difference::<Int32>(&__lhs, &__rhs);
        }
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("check equality records iris.json print_json_difference import usage", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_check;

import iris.json as iris_json;

function run(lhs: Int32, rhs: Int32) -> ()
{
    check(lhs == rhs);
}
)";

        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input, {});
        run_compile_time_pass(context, "run");

        Import_module_with_alias const* const json_import = find_import_module_with_alias(context.core_module().dependencies, "iris_json");
        REQUIRE(json_import != nullptr);

        auto const location = std::find(
            json_import->usages.begin(),
            json_import->usages.end(),
            "print_json_difference"
        );

        CHECK(location != json_import->usages.end());
    }

    TEST_CASE("Evaluates compile_time reflection member_offset", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_reflection;

struct Pair
{
    first: Int32 = 0;
    second: Int32 = 0;
}

export function run_member_offset() -> (result: Int32)
{
    var value = @member_offset::<Pair>(1u64);
}
)";

        std::string_view const expected = R"(module compile_time_reflection;

struct Pair
{
    first: Int32 = 0;
    second: Int32 = 0;
}

export function run_member_offset() -> (result: Int32)
{
    var value = 32u64;
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_member_offset");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time reflection member_name", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_reflection;

struct Pair
{
    first: Int32 = 0;
    second: Int32 = 0;
}

export function run_member_name() -> ()
{
    var value = @member_name::<Pair>(1u64);
}
)";

        std::string_view const expected = R"(module compile_time_reflection;

struct Pair
{
    first: Int32 = 0;
    second: Int32 = 0;
}

export function run_member_name() -> ()
{
    var value = "second"c;
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_member_name");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time reflection get_type_kind", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_reflection;

export function run_get_type_kind() -> ()
{
    var kind = @get_type_kind::<Int32>();
}
)";

        std::string_view const expected = R"(module compile_time_reflection;

export function run_get_type_kind() -> ()
{
    var kind = Type_kind.Int;
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_get_type_kind");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time var, if and Type_kind", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_reflection;

export function run() -> ()
{
    compile_time var kind = @get_type_kind::<Int32>();
    compile_time if kind == Type_kind.Int
    {
        return 0;
    }
    else
    {
        return 1;
    }
}
)";

        std::string_view const expected = R"(module compile_time_reflection;

export function run() -> ()
{
    {
        return 0;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time enum access", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_reflection;

enum My_enum
{
    value_0 = 0,
}

export function run() -> ()
{
    compile_time var value = My_enum.value_0;
}
)";

        std::string_view const expected = R"(module compile_time_reflection;

enum My_enum
{
    value_0 = 0,
}

export function run() -> ()
{
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time enum access with implicit values", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_reflection;

enum My_enum
{
    A = 1,
    B,
    C,
}

export function run() -> (result: Int32)
{
    compile_time if My_enum.B == 2
    {
        compile_time if My_enum.C == 3
        {
            return 1;
        }
        else
        {
            return 2;
        }
    }
    else
    {
        return 3;
    }
}
)";

        std::string_view const expected = R"(module compile_time_reflection;

enum My_enum
{
    A = 1,
    B,
    C,
}

export function run() -> (result: Int32)
{
    {
        {
            return 1;
        }
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time enum access after explicit reset", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_reflection;

enum My_enum
{
    A = 3,
    B,
    C,
    D = 20,
    E,
}

export function run() -> (result: Int32)
{
    compile_time if My_enum.E == 21
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

enum My_enum
{
    A = 3,
    B,
    C,
    D = 20,
    E,
}

export function run() -> (result: Int32)
{
    {
        return 1;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run");

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

    TEST_CASE("Evaluates compile_time binary logical_and with both true", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_binary_expression;

export function run_logical_and_true() -> (result: Int32)
{
    compile_time if true && true
    {
        return 71;
    }
    else
    {
        return 72;
    }
}
)";

        std::string_view const expected = R"(module compile_time_binary_expression;

export function run_logical_and_true() -> (result: Int32)
{
    {
        return 71;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_logical_and_true");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time binary logical_and with one false", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_binary_expression;

export function run_logical_and_false() -> (result: Int32)
{
    compile_time if true && false
    {
        return 81;
    }
    else
    {
        return 82;
    }
}
)";

        std::string_view const expected = R"(module compile_time_binary_expression;

export function run_logical_and_false() -> (result: Int32)
{
    {
        return 82;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_logical_and_false");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time binary logical_or with one true", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_binary_expression;

export function run_logical_or_true() -> (result: Int32)
{
    compile_time if false || true
    {
        return 91;
    }
    else
    {
        return 92;
    }
}
)";

        std::string_view const expected = R"(module compile_time_binary_expression;

export function run_logical_or_true() -> (result: Int32)
{
    {
        return 91;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_logical_or_true");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time binary logical_or with both false", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_binary_expression;

export function run_logical_or_false() -> (result: Int32)
{
    compile_time if false || false
    {
        return 101;
    }
    else
    {
        return 102;
    }
}
)";

        std::string_view const expected = R"(module compile_time_binary_expression;

export function run_logical_or_false() -> (result: Int32)
{
    {
        return 102;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_logical_or_false");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time binary chained logical_and with integer comparisons", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_binary_expression;

var g_value = 1;
var g_debug = true;

export function run_chained_and() -> (result: Int32)
{
    compile_time if g_value == 1 && g_debug
    {
        return 111;
    }
    else
    {
        return 112;
    }
}
)";

        std::string_view const expected = R"(module compile_time_binary_expression;

var g_value = 1;

var g_debug = true;

export function run_chained_and() -> (result: Int32)
{
    {
        return 111;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_chained_and");

        CHECK(expected == actual);
    }

    TEST_CASE("Evaluates compile_time binary chained logical_or with integer comparisons", "[Compile_time_pass][Passes]")
    {
        std::string_view const input = R"(module compile_time_binary_expression;

var g_value = 2;

export function run_chained_or() -> (result: Int32)
{
    compile_time if (g_value == 0) || (g_value == 2)
    {
        return 121;
    }
    else
    {
        return 122;
    }
}
)";

        std::string_view const expected = R"(module compile_time_binary_expression;

var g_value = 2;

export function run_chained_or() -> (result: Int32)
{
    {
        return 121;
    }
}
)";

        std::pmr::string const actual = run_compile_time_pass_and_format(input, "run_chained_or");

        CHECK(expected == actual);
    }
}
