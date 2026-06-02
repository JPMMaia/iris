#include <cstdio>

namespace iris::compiler
{
    struct Diagnostic;
    std::ostream& operator<<(
        std::ostream& output_stream,
        Diagnostic const& diagnostic
    );
}

using iris::compiler::operator<<;
#include <catch2/catch_test_macros.hpp>

import std;
import iris.common.filesystem_common;
import iris.compiler;
import iris.compiler.analysis;
import iris.core;
import iris.core.declarations;
import iris.core.types;
import iris.parser.convertor;
import iris.parser.parser;

namespace iris::compiler
{
    void print_diagnostics(std::span<Diagnostic const> const diagnostics)
    {
        std::fprintf(stderr, "[\n");
        for (std::size_t index = 0; index < diagnostics.size(); ++index)
        {
            std::pmr::string const diagnostic_string = diagnostic_to_string(diagnostics[index], {}, {});
            std::fprintf(stderr, "    %s", diagnostic_string.c_str());
            std::fprintf(stderr, index + 1 == diagnostics.size() ? ",\n" : "\n");
        }
        std::fprintf(stderr, "]\n");
    }

    // Helper: Create declaration database with builtin module
    static Declaration_database create_test_declaration_database()
    {
        Declaration_database declaration_database = create_declaration_database();

        std::optional<iris::Module> builtin_module = iris::parser::parse_and_convert_to_module(
            iris::common::get_builtin_module_file_path(),
            {},
            {}
        );
        REQUIRE(builtin_module.has_value());
        add_declarations(declaration_database, builtin_module.value());

        return declaration_database;
    }

    // Helper: Parse and analyze a module, returning the analyzed module and result
    static std::pair<iris::Module, Analysis_result> analyze_module(
        std::string_view const input_text,
        std::span<std::string_view const> const dependencies = {}
    )
    {
        Declaration_database declaration_database = create_test_declaration_database();

        // Process dependencies
        for (std::string_view const dependency_text : dependencies)
        {
            std::optional<iris::Module> dependency_module = iris::parser::parse_and_convert_to_module(
                dependency_text,
                std::nullopt,
                {},
                {}
            );
            REQUIRE(dependency_module.has_value());
            add_declarations(declaration_database, dependency_module.value());

            Analysis_result const dep_result = process_module(
                dependency_module.value(),
                declaration_database,
                { .validate = true },
                {}
            );
            REQUIRE(dep_result.diagnostics.empty());
        }

        // Parse and analyze main module
        std::optional<iris::Module> core_module = iris::parser::parse_and_convert_to_module(
            input_text,
            std::nullopt,
            {},
            {}
        );
        REQUIRE(core_module.has_value());

        add_declarations(declaration_database, core_module.value());

        Analysis_result const result = process_module(
            core_module.value(),
            declaration_database,
            { .validate = true },
            {}
        );

        return { std::move(core_module.value()), std::move(result) };
    }

    // Helper: Test that analysis succeeds with no diagnostics
    static void test_analysis_no_diagnostics(std::string_view const input_text)
    {
        auto const [module, result] = analyze_module(input_text);
        CHECK(result.diagnostics.empty());
        if (!result.diagnostics.empty())
        {
            std::fprintf(stderr, "Unexpected diagnostics:\n");
            print_diagnostics(result.diagnostics);
        }
    }

    // Helper: Test that analysis produces a specific diagnostic message
    static void test_analysis_has_diagnostic(
        std::string_view const input_text,
        std::string_view const expected_message_pattern
    )
    {
        auto const [module, result] = analyze_module(input_text);
        bool found = false;
        for (auto const& diag : result.diagnostics)
        {
            std::pmr::string const diag_str = diagnostic_to_string(diag, {}, {});
            if (diag_str.find(expected_message_pattern) != std::string_view::npos)
            {
                found = true;
                break;
            }
        }
        CHECK(found);
    }

    // Helper: Find lambda expressions in a function's statements
    static std::pmr::vector<iris::Lambda_expression const*> find_lambda_expressions(
        iris::Module const& module,
        std::string_view const function_name
    )
    {
        std::pmr::vector<iris::Lambda_expression const*> lambdas;

        for (auto const& definition : module.definitions.function_definitions)
        {
            if (definition.name != function_name) continue;

            for (auto const& statement : definition.statements)
            {
                for (auto const& expr : statement.expressions)
                {
                    if (std::holds_alternative<iris::Lambda_expression>(expr.data))
                    {
                        lambdas.push_back(&std::get<iris::Lambda_expression>(expr.data));
                    }
                }
            }
        }

        return lambdas;
    }

    // Helper: Find lambda expression assigned to a specific variable
    static std::pmr::vector<iris::Lambda_expression const*> find_lambda_assigned_to(
        iris::Module const& module,
        std::string_view const variable_name
    )
    {
        std::pmr::vector<iris::Lambda_expression const*> lambdas;

        for (auto const& definition : module.definitions.function_definitions)
        {
            for (auto const& statement : definition.statements)
            {
                for (auto const& expr : statement.expressions)
                {
                    if (std::holds_alternative<iris::Variable_declaration_with_type_expression>(expr.data))
                    {
                        auto const& var_expr = std::get<iris::Variable_declaration_with_type_expression>(expr.data);
                        if (var_expr.declaration.name == variable_name &&
                            var_expr.right_hand_side.expression_index < statement.expressions.size())
                        {
                            auto const& rhs = statement.expressions[var_expr.right_hand_side.expression_index];
                            if (std::holds_alternative<iris::Lambda_expression>(rhs.data))
                            {
                                lambdas.push_back(&std::get<iris::Lambda_expression>(rhs.data));
                            }
                        }
                    }
                }
            }
        }

        return lambdas;
    }

    // ============================================================================
    // Lambda Expression Type Analysis Tests
    // ============================================================================

    TEST_CASE("Lambda literal with explicit parameter types and explicit return type resolves to Lambda_type", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: Comparator = lambda(a: Int32, b: Int32) -> (result: Int32) => a - b;
}
)";

        auto const [module, result] = analyze_module(input);
        CHECK(result.diagnostics.empty());

        auto const lambdas = find_lambda_expressions(module, "main");
        REQUIRE(lambdas.size() == 1);

        auto const& lambda_expr = *lambdas[0];
        // Verify parameter names
        CHECK(lambda_expr.parameter_names.size() == 2);
        CHECK(lambda_expr.parameter_names[0] == "a");
        CHECK(lambda_expr.parameter_names[1] == "b");

        // Verify explicit parameter types are preserved
        CHECK(lambda_expr.parameter_types.size() == 2);
        CHECK(lambda_expr.parameter_types[0].has_value());
        CHECK(lambda_expr.parameter_types[1].has_value());

        // Verify explicit return type is preserved
        CHECK(lambda_expr.return_type.has_value());
    }

    TEST_CASE("Lambda literal with explicit return type but inferred parameter types resolves from expected type", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: Comparator = lambda(a, b) -> (result: Int32) => a - b;
}
)";

        auto const [module, result] = analyze_module(input);
        CHECK(result.diagnostics.empty());

        auto const lambdas = find_lambda_expressions(module, "main");
        REQUIRE(lambdas.size() == 1);

        auto const& lambda_expr = *lambdas[0];
        // Verify parameter names
        CHECK(lambda_expr.parameter_names.size() == 2);

        // Verify return type is explicit
        CHECK(lambda_expr.return_type.has_value());
    }

    TEST_CASE("Lambda literal with no explicit types inferred from named lambda type", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: Comparator = lambda(a, b) => a - b;
}
)";

        auto const [module, result] = analyze_module(input);
        CHECK(result.diagnostics.empty());

        auto const lambdas = find_lambda_expressions(module, "main");
        REQUIRE(lambdas.size() == 1);

        auto const& lambda_expr = *lambdas[0];
        // Verify parameter names
        CHECK(lambda_expr.parameter_names.size() == 2);
        CHECK(lambda_expr.parameter_names[0] == "a");
        CHECK(lambda_expr.parameter_names[1] == "b");
    }

    TEST_CASE("Lambda literal with no expected type emits diagnostic", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

export function main() -> ()
{
    var cmp = lambda(a, b) => a - b;
}
)";

        test_analysis_has_diagnostic(input, "Cannot infer");
    }

    TEST_CASE("Lambda literal with partial explicit types resolves correctly", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: Comparator = lambda(a: Int32, b) => a - b;
}
)";

        auto const [module, result] = analyze_module(input);
        CHECK(result.diagnostics.empty());

        auto const lambdas = find_lambda_expressions(module, "main");
        REQUIRE(lambdas.size() == 1);

        auto const& lambda_expr = *lambdas[0];
        // Verify parameter names
        CHECK(lambda_expr.parameter_names.size() == 2);
        CHECK(lambda_expr.parameter_names[0] == "a");
        CHECK(lambda_expr.parameter_names[1] == "b");

        // First param should have explicit type, second should be inferred (nullopt)
        CHECK(lambda_expr.parameter_types.size() == 2);
        CHECK(lambda_expr.parameter_types[0].has_value());   // explicit: Int32
        CHECK(!lambda_expr.parameter_types[1].has_value());   // inferred from expected type
    }

    TEST_CASE("Lambda literal with inferred return type from body", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

export function main() -> ()
{
    var mapper = lambda(x: Int32) => x * 2;
}
)";

        test_analysis_no_diagnostics(input);

        auto const [module, result] = analyze_module(input);
        auto const lambdas = find_lambda_expressions(module, "main");
        REQUIRE(lambdas.size() == 1);

        auto const& lambda_expr = *lambdas[0];
        // Return type should be inferred (nullopt means not explicitly written)
        CHECK(!lambda_expr.return_type.has_value());
    }

    TEST_CASE("Lambda literal with mismatched return type emits diagnostic", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

export function main() -> ()
{
    var mapper = lambda(x: Int32) -> Bool => x * 2;
}
)";

        test_analysis_has_diagnostic(input, "type");
    }

    TEST_CASE("Lambda literal with captures records captured variables", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

export function main() -> ()
{
    var offset: Int32 = 10;
    var cmp = lambda(a, b) => a - b + offset;
}
)";

        auto const [module, result] = analyze_module(input);
        // Should have no diagnostics (captures are valid)
        CHECK(result.diagnostics.empty());

        auto const lambdas = find_lambda_expressions(module, "main");
        REQUIRE(lambdas.size() == 1);

        auto const& lambda_expr = *lambdas[0];
        // Verify captured variables are recorded
        CHECK(lambda_expr.captured_variables.has_value());
        auto const& captured = *lambda_expr.captured_variables;
        CHECK(captured.size() == 1);
        CHECK(captured[0] == "offset");
    }

    TEST_CASE("Named lambda type declaration resolves to Lambda_type", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function take(cmp: Comparator) -> ()
{
}
)";

        test_analysis_no_diagnostics(input);

        auto const [module, result] = analyze_module(input);
        // Verify lambda declaration was created
        CHECK(module.internal_declarations.lambda_declarations.size() == 1);

        auto const& lambda_decl = module.internal_declarations.lambda_declarations[0];
        CHECK(lambda_decl.name == "Comparator");
        CHECK(lambda_decl.input_parameter_names.size() == 2);
        CHECK(lambda_decl.input_parameter_names[0] == "a");
        CHECK(lambda_decl.input_parameter_names[1] == "b");
        CHECK(lambda_decl.output_parameter_names.size() == 1);
        CHECK(lambda_decl.output_parameter_names[0] == "result");
    }

    TEST_CASE("Lambda literal passed as function parameter type", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function apply(cmp: Comparator, x: Int32, y: Int32) -> (result: Int32)
{
    return cmp(x, y);
}

export function main() -> ()
{
    var cmp: Comparator = lambda(a, b) => a - b;
    var result = apply(cmp, 10, 3);
}
)";

        test_analysis_no_diagnostics(input);

        auto const [module, result] = analyze_module(input);
        // Verify lambda declaration exists
        CHECK(module.internal_declarations.lambda_declarations.size() == 1);

        // Verify lambda expression in main
        auto const lambdas = find_lambda_expressions(module, "main");
        REQUIRE(lambdas.size() == 1);

        auto const& lambda_expr = *lambdas[0];
        CHECK(lambda_expr.parameter_names.size() == 2);
    }

    TEST_CASE("Lambda literal with multiple captured variables records all captures", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

export function main() -> ()
{
    var offset: Int32 = 10;
    var scale: Int32 = 2;
    var cmp = lambda(a, b) => (a + offset) * scale - b;
}
)";

        auto const [module, result] = analyze_module(input);
        CHECK(result.diagnostics.empty());

        auto const lambdas = find_lambda_expressions(module, "main");
        REQUIRE(lambdas.size() == 1);

        auto const& lambda_expr = *lambdas[0];
        // Verify both captured variables are recorded
        CHECK(lambda_expr.captured_variables.has_value());
        auto const& captured = *lambda_expr.captured_variables;
        CHECK(captured.size() == 2);
        // Order may vary, but both should be present
        bool has_offset = std::find(captured.begin(), captured.end(), "offset") != captured.end();
        bool has_scale = std::find(captured.begin(), captured.end(), "scale") != captured.end();
        CHECK(has_offset);
        CHECK(has_scale);
    }

    TEST_CASE("Lambda literal with no captures has empty captured variables", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

lambda Adder(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var add: Adder = lambda(a, b) => a + b;
}
)";

        auto const [module, result] = analyze_module(input);
        CHECK(result.diagnostics.empty());

        auto const lambdas = find_lambda_expressions(module, "main");
        REQUIRE(lambdas.size() == 1);

        auto const& lambda_expr = *lambdas[0];
        // No captures expected
        CHECK(!lambda_expr.captured_variables.has_value() || lambda_expr.captured_variables->empty());
    }

    TEST_CASE("Lambda literal with block body and captures records captured variables", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

lambda Adder(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var offset: Int32 = 5;
    var add: Adder = lambda(a, b) => {
        var result = a + b + offset;
        return result;
    };
}
)";

        auto const [module, result] = analyze_module(input);
        CHECK(result.diagnostics.empty());

        auto const lambdas = find_lambda_expressions(module, "main");
        REQUIRE(lambdas.size() == 1);

        auto const& lambda_expr = *lambdas[0];
        // Verify captured variables are recorded
        CHECK(lambda_expr.captured_variables.has_value());
        auto const& captured = *lambda_expr.captured_variables;
        CHECK(captured.size() == 1);
        CHECK(captured[0] == "offset");
    }

    TEST_CASE("Nested lambda literals each record their own captured variables", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

lambda Inner(x: Int32) -> (result: Int32);
lambda Outer(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var base: Int32 = 100;
    var outer: Outer = lambda(a, b) => {
        var inner: Inner = lambda(x) => x + a + base;
        return inner(b);
    };
}
)";

        auto const [module, result] = analyze_module(input);
        CHECK(result.diagnostics.empty());

        auto const lambdas = find_lambda_expressions(module, "main");
        REQUIRE(lambdas.size() == 2);

        // Outer lambda captures 'base' and 'a'
        auto const& outer_lambda = *lambdas[0];
        CHECK(outer_lambda.captured_variables.has_value());
        auto const& outer_captured = *outer_lambda.captured_variables;
        CHECK(outer_captured.size() == 2);

        // Inner lambda captures 'a' and 'base'
        auto const& inner_lambda = *lambdas[1];
        CHECK(inner_lambda.captured_variables.has_value());
        auto const& inner_captured = *inner_lambda.captured_variables;
        CHECK(inner_captured.size() == 2);
    }

    TEST_CASE("Lambda literal with explicit parameter types matching expected type", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

lambda Comparator(a: Int32, b: Int32) -> (result: Bool);

export function main() -> ()
{
    var cmp: Comparator = lambda(a: Int32, b: Int32) => a < b;
}
)";

        test_analysis_no_diagnostics(input);

        auto const [module, result] = analyze_module(input);
        auto const lambdas = find_lambda_expressions(module, "main");
        REQUIRE(lambdas.size() == 1);

        auto const& lambda_expr = *lambdas[0];
        // All parameter types should be explicit
        CHECK(lambda_expr.parameter_types.size() == 2);
        CHECK(lambda_expr.parameter_types[0].has_value());
        CHECK(lambda_expr.parameter_types[1].has_value());
    }

    TEST_CASE("Lambda literal with explicit return type matching body expression", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

export function main() -> ()
{
    var mapper = lambda(x: Int32) -> Int32 => x * 2;
    var result = mapper(5);
}
)";

        test_analysis_no_diagnostics(input);
    }

    TEST_CASE("Lambda literal with mismatched explicit parameter type emits diagnostic", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

lambda Adder(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var add: Adder = lambda(a: Float32, b: Int32) => a + b;
}
)";

        test_analysis_has_diagnostic(input, "type");
    }

    TEST_CASE("Lambda literal with mismatched return type in body emits diagnostic", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

export function main() -> ()
{
    var mapper = lambda(x: Int32) -> Int32 => x + 1.0f32;
}
)";

        test_analysis_has_diagnostic(input, "type");
    }

    TEST_CASE("Lambda literal with no parameters and no return value", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

lambda Action() -> ();

export function main() -> ()
{
    var action: Action = lambda() => {};
}
)";

        test_analysis_no_diagnostics(input);

        auto const [module, result] = analyze_module(input);
        auto const lambdas = find_lambda_expressions(module, "main");
        REQUIRE(lambdas.size() == 1);

        auto const& lambda_expr = *lambdas[0];
        CHECK(lambda_expr.parameter_names.empty());
    }

    TEST_CASE("Lambda literal with multiple return values", "[Analysis][Lambda]")
    {
        std::string_view const input = R"(module Test;

lambda Pair(a: Int32, b: Int32) -> (first: Int32, second: Int32);

export function main() -> ()
{
    var p: Pair = lambda(a, b) => (a, b);
}
)";

        test_analysis_no_diagnostics(input);

        auto const [module, result] = analyze_module(input);
        auto const lambdas = find_lambda_expressions(module, "main");
        REQUIRE(lambdas.size() == 1);

        auto const& lambda_expr = *lambdas[0];
        CHECK(lambda_expr.parameter_names.size() == 2);
    }
}
