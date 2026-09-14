#include <span>
#include <string_view>

#include <catch2/catch_all.hpp>

import iris.compiler.lambda_database;
import iris.compiler.lambda_pass;
import iris.compiler.pass_test_helpers;
import iris.core;
import iris.core.declarations;
import iris.core.formatter;
import iris.parser.convertor;

namespace iris::compiler
{
    static std::pmr::string run_pass_on_function_and_format(
        iris::compiler::tests::Parsed_module_context& context,
        Lambda_database& lambda_database,
        std::string_view const function_name
    )
    {
        iris::Module& core_module = context.core_module();

        std::optional<Function_declaration const*> const function_declaration = find_function_declaration(core_module, function_name);
        REQUIRE(function_declaration.has_value());

        iris::Function_definition* const function_definition = iris::compiler::tests::find_mutable_function_definition(core_module, function_name);
        REQUIRE(function_definition != nullptr);

        // run_lambda_pass_on_function appends to the module's declaration and definition
        // vectors, which invalidates both pointers above, so it gets copies.
        iris::Function_declaration const declaration_copy = *function_declaration.value();
        iris::Function_definition definition_copy = *function_definition;

        run_lambda_pass_on_function(
            core_module,
            context.declaration_database,
            lambda_database,
            declaration_copy,
            definition_copy
        );

        iris::Function_definition* const updated_definition = iris::compiler::tests::find_mutable_function_definition(core_module, function_name);
        REQUIRE(updated_definition != nullptr);
        *updated_definition = std::move(definition_copy);

        return iris::compiler::tests::format_core_module_to_text(core_module);
    }

    static void test_lambda_pass_on_function(
        std::string_view const input_text,
        std::string_view const function_name,
        std::string_view const expected
    )
    {
        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input_text, {});

        Lambda_database lambda_database;
        std::pmr::string const actual = run_pass_on_function_and_format(context, lambda_database, function_name);

        CHECK(expected == actual);
    }

    static void test_lambda_pass_on_module(
        std::string_view const input_text,
        std::string_view const expected,
        bool const is_test_mode = false
    )
    {
        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input_text, {});

        Lambda_database lambda_database;
        run_lambda_pass_on_module(context.core_module(), context.declaration_database, lambda_database, is_test_mode);

        std::pmr::string const actual = iris::compiler::tests::format_core_module_to_text(context.core_module());

        CHECK(expected == actual);
    }

    static iris::Lambda_expression const* find_first_lambda_expression(
        std::span<iris::Statement const> const statements
    )
    {
        for (iris::Statement const& statement : statements)
        {
            for (iris::Expression const& expression : statement.expressions)
            {
                if (std::holds_alternative<iris::Lambda_expression>(expression.data))
                    return &std::get<iris::Lambda_expression>(expression.data);
            }
        }

        return nullptr;
    }

    static bool has_function_declaration(
        iris::Module const& core_module,
        std::string_view const name
    )
    {
        std::optional<Function_declaration const*> const declaration = find_function_declaration(core_module, name);
        return declaration.has_value();
    }

    static std::size_t count_internal_struct_declarations(iris::Module const& core_module)
    {
        return core_module.internal_declarations.struct_declarations.size();
    }

    TEST_CASE("Lambda pass lowers a lambda without captures to an internal function", "[Lambda_pass][Passes][Lambda]")
    {
        std::string_view const input = R"(module Lambda_pass_test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function run() -> ()
{
    var cmp: Comparator = lambda(a, b) => a - b;
}
)";

        std::string_view const expected = R"(module Lambda_pass_test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function run() -> ()
{
    var cmp: Comparator = lambda(a, b) => a - b;
}

function run__lambda0(a: Int32, b: Int32, user_data: *mutable Void) -> (result_0: Int32)
{
    return a - b;
}
)";

        test_lambda_pass_on_function(input, "run", expected);
    }

    TEST_CASE("Lambda pass creates an environment struct for a captured variable", "[Lambda_pass][Passes][Lambda]")
    {
        std::string_view const input = R"(module Lambda_pass_test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function run() -> ()
{
    var offset: Int32 = 10;
    var cmp: Comparator = lambda(a, b) => a - b + offset;
}
)";

        std::string_view const expected = R"(module Lambda_pass_test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function run() -> ()
{
    var offset: Int32 = 10;
    var cmp: Comparator = lambda(a, b) => a - b + offset;
}

struct run__lambda0__environment
{
    offset: Int32 = ;
}

function run__lambda0(a: Int32, b: Int32, user_data: *mutable run__lambda0__environment) -> (result_0: Int32)
{
    var offset = user_data->offset;
    return a - b + offset;
}
)";

        test_lambda_pass_on_function(input, "run", expected);
    }

    TEST_CASE("Lambda pass keeps the statements of a block body", "[Lambda_pass][Passes][Lambda]")
    {
        std::string_view const input = R"(module Lambda_pass_test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function run() -> ()
{
    var cmp: Comparator = lambda(a, b) => {
        var difference = a - b;
        return difference;
    };
}
)";

        std::string_view const expected = R"(module Lambda_pass_test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function run() -> ()
{
    var cmp: Comparator = lambda(a, b) => {
        var difference = a - b;
        return difference;
    };
}

function run__lambda0(a: Int32, b: Int32, user_data: *mutable Void) -> (result_0: Int32)
{
    var difference = a - b;
    return difference;
}
)";

        test_lambda_pass_on_function(input, "run", expected);
    }

    TEST_CASE("Lambda pass numbers multiple lambdas of the same function", "[Lambda_pass][Passes][Lambda]")
    {
        std::string_view const input = R"(module Lambda_pass_test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

lambda Mapper(value: Int32) -> (result: Int32);

export function run() -> ()
{
    var cmp: Comparator = lambda(a, b) => a - b;
    var mapper: Mapper = lambda(x) => x * 2;
}
)";

        std::string_view const expected = R"(module Lambda_pass_test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

lambda Mapper(value: Int32) -> (result: Int32);

export function run() -> ()
{
    var cmp: Comparator = lambda(a, b) => a - b;
    var mapper: Mapper = lambda(x) => x * 2;
}

function run__lambda0(a: Int32, b: Int32, user_data: *mutable Void) -> (result_0: Int32)
{
    return a - b;
}

function run__lambda1(x: Int32, user_data: *mutable Void) -> (result_0: Int32)
{
    return x * 2;
}
)";

        test_lambda_pass_on_function(input, "run", expected);
    }

    TEST_CASE("Lambda pass uses the explicit parameter and return types of a lambda literal", "[Lambda_pass][Passes][Lambda]")
    {
        std::string_view const input = R"(module Lambda_pass_test;

export function run() -> ()
{
    var mapper = lambda(x: Int32) -> Int32 => x * 2;
}
)";

        std::string_view const expected = R"(module Lambda_pass_test;

export function run() -> ()
{
    var mapper = lambda(x: Int32) -> Int32 => x * 2;
}

function run__lambda0(x: Int32, user_data: *mutable Void) -> (result_0: Int32)
{
    return x * 2;
}
)";

        test_lambda_pass_on_function(input, "run", expected);
    }

    TEST_CASE("Lambda pass lowers a nested lambda when run on the module", "[Lambda_pass][Passes][Lambda]")
    {
        std::string_view const input = R"(module Lambda_pass_test;

lambda Inner(x: Int32) -> (result: Int32);

lambda Outer(a: Int32, b: Int32) -> (result: Int32);

export function run() -> ()
{
    var outer: Outer = lambda(a, b) => {
        var inner: Inner = lambda(x) => x + a;
        return inner(b);
    };
}
)";

        std::string_view const expected = R"(module Lambda_pass_test;

lambda Inner(x: Int32) -> (result: Int32);

lambda Outer(a: Int32, b: Int32) -> (result: Int32);

export function run() -> ()
{
    var outer: Outer = lambda(a, b) => {
        var inner: Inner = lambda(x) => x + a;
        return inner(b);
    };
}

struct run__lambda0__lambda0__environment
{
    a: Int32 = ;
}

function run__lambda0(a: Int32, b: Int32, user_data: *mutable Void) -> (result_0: Int32)
{
    var inner: Inner = lambda(x) => x + a;
    return inner(b);
}

function run__lambda0__lambda0(x: Int32, user_data: *mutable run__lambda0__lambda0__environment) -> (result_0: Int32)
{
    var a = user_data->a;
    return x + a;
}
)";

        test_lambda_pass_on_module(input, expected);
    }

    TEST_CASE("Lambda pass lowers lambdas of every function of the module", "[Lambda_pass][Passes][Lambda]")
    {
        std::string_view const input = R"(module Lambda_pass_test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function first() -> ()
{
    var cmp: Comparator = lambda(a, b) => a - b;
}

export function second() -> ()
{
    var cmp: Comparator = lambda(a, b) => a + b;
}
)";

        std::string_view const expected = R"(module Lambda_pass_test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function first() -> ()
{
    var cmp: Comparator = lambda(a, b) => a - b;
}

export function second() -> ()
{
    var cmp: Comparator = lambda(a, b) => a + b;
}

function first__lambda0(a: Int32, b: Int32, user_data: *mutable Void) -> (result_0: Int32)
{
    return a - b;
}

function second__lambda0(a: Int32, b: Int32, user_data: *mutable Void) -> (result_0: Int32)
{
    return a + b;
}
)";

        test_lambda_pass_on_module(input, expected);
    }

    TEST_CASE("Lambda pass records the generated function name in the lambda database", "[Lambda_pass][Passes][Lambda]")
    {
        std::string_view const input = R"(module Lambda_pass_test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function run() -> ()
{
    var cmp: Comparator = lambda(a, b) => a - b;
}
)";

        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input, {});

        Lambda_database lambda_database;
        run_lambda_pass_on_module(context.core_module(), context.declaration_database, lambda_database, false);

        iris::Function_definition const* const definition = iris::compiler::tests::find_mutable_function_definition(context.core_module(), "run");
        REQUIRE(definition != nullptr);

        iris::Lambda_expression const* const lambda_expression = find_first_lambda_expression(definition->statements);
        REQUIRE(lambda_expression != nullptr);

        std::pmr::string const lambda_key = create_lambda_key(context.core_module().name, "run", *lambda_expression);

        std::optional<std::string_view> const generated_function_name = find_generated_lambda_function_name(lambda_database, lambda_key);
        REQUIRE(generated_function_name.has_value());
        CHECK(generated_function_name.value() == "run__lambda0");

        CHECK(has_function_declaration(context.core_module(), "run__lambda0"));
    }

    TEST_CASE("Lambda pass records the captured variables of a lambda expression", "[Lambda_pass][Passes][Lambda]")
    {
        std::string_view const input = R"(module Lambda_pass_test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function run() -> ()
{
    var offset: Int32 = 10;
    var cmp: Comparator = lambda(a, b) => a - b + offset;
}
)";

        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input, {});

        Lambda_database lambda_database;
        run_lambda_pass_on_module(context.core_module(), context.declaration_database, lambda_database, false);

        iris::Function_definition const* const definition = iris::compiler::tests::find_mutable_function_definition(context.core_module(), "run");
        REQUIRE(definition != nullptr);

        iris::Lambda_expression const* const lambda_expression = find_first_lambda_expression(definition->statements);
        REQUIRE(lambda_expression != nullptr);

        REQUIRE(lambda_expression->captured_variables.has_value());
        REQUIRE(lambda_expression->captured_variables.value().size() == 1);
        CHECK(lambda_expression->captured_variables.value()[0] == "offset");
    }

    TEST_CASE("Lambda pass is idempotent", "[Lambda_pass][Passes][Lambda]")
    {
        std::string_view const input = R"(module Lambda_pass_test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function run() -> ()
{
    var offset: Int32 = 10;
    var cmp: Comparator = lambda(a, b) => a - b + offset;
}
)";

        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input, {});

        Lambda_database lambda_database;
        run_lambda_pass_on_module(context.core_module(), context.declaration_database, lambda_database, false);
        std::pmr::string const after_first_run = iris::compiler::tests::format_core_module_to_text(context.core_module());

        run_lambda_pass_on_module(context.core_module(), context.declaration_database, lambda_database, false);
        std::pmr::string const after_second_run = iris::compiler::tests::format_core_module_to_text(context.core_module());

        CHECK(after_first_run == after_second_run);
    }

    TEST_CASE("Lambda pass leaves a module without lambdas unchanged", "[Lambda_pass][Passes][Lambda]")
    {
        std::string_view const input = R"(module Lambda_pass_test;

export function add(a: Int32, b: Int32) -> (result: Int32)
{
    return a + b;
}
)";

        test_lambda_pass_on_module(input, input);
    }

    TEST_CASE("Lambda pass does not lower a lambda whose type cannot be resolved", "[Lambda_pass][Passes][Lambda]")
    {
        // Neither an expected type nor explicit parameter types, so the signature is unknown.
        // Validation reports this; the pass must leave the module alone rather than synthesize
        // a function with made-up parameter types.
        std::string_view const input = R"(module Lambda_pass_test;

export function run() -> ()
{
    var cmp = lambda(a, b) => a - b;
}
)";

        test_lambda_pass_on_module(input, input);
    }

    TEST_CASE("Lambda pass skips test functions when not in test mode", "[Lambda_pass][Passes][Lambda]")
    {
        std::string_view const input = R"(module Lambda_pass_test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

@test
function run() -> ()
{
    var cmp: Comparator = lambda(a, b) => a - b;
}
)";

        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input, {});

        Lambda_database lambda_database;
        run_lambda_pass_on_module(context.core_module(), context.declaration_database, lambda_database, false);

        CHECK(!has_function_declaration(context.core_module(), "run__lambda0"));

        run_lambda_pass_on_module(context.core_module(), context.declaration_database, lambda_database, true);

        CHECK(has_function_declaration(context.core_module(), "run__lambda0"));
    }

    TEST_CASE("Lambda pass creates no environment struct when there are no captures", "[Lambda_pass][Passes][Lambda]")
    {
        std::string_view const input = R"(module Lambda_pass_test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function run() -> ()
{
    var offset: Int32 = 10;
    var cmp: Comparator = lambda(a, b) => a - b;
}
)";

        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input, {});

        std::size_t const struct_count_before = count_internal_struct_declarations(context.core_module());

        Lambda_database lambda_database;
        run_lambda_pass_on_module(context.core_module(), context.declaration_database, lambda_database, false);

        CHECK(has_function_declaration(context.core_module(), "run__lambda0"));
        CHECK(count_internal_struct_declarations(context.core_module()) == struct_count_before);
    }
}
