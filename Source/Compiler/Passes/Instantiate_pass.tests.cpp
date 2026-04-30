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
        iris::Module& core_module = context.core_module();

        iris::Function_declaration* function_declaration = iris::compiler::tests::find_mutable_function_declaration(core_module, function_name);
        REQUIRE(function_declaration != nullptr);

        iris::Function_definition* function_definition = iris::compiler::tests::find_mutable_function_definition(core_module, function_name);
        REQUIRE(function_definition != nullptr);

        Instantiate_runtime_context runtime_context = create_instantiate_runtime_context(core_module, context.declaration_database);

        std::pmr::vector<iris::Module const*> sorted_modules;
        sorted_modules.reserve(context.dependencies().size() + 1);
        for (iris::Module const& dependency_module : context.dependencies())
            sorted_modules.push_back(&dependency_module);
        sorted_modules.push_back(&core_module);

        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        All_passes_parameters const parameters =
        {
            .target_module_name = core_module.name,
            .sorted_core_modules = sorted_modules,
            .llvm_context = *runtime_context.llvm_data.context,
            .llvm_data_layout = runtime_context.llvm_data.data_layout,
            .declaration_database = context.declaration_database,
            .clang_context = *runtime_context.clang_context,
            .dependencies = core_module.dependencies,
            .instanced_declarations = core_module.instanced_declarations,
            .definitions = core_module.definitions,
            .output_allocator = output_allocator,
            .temporaries_allocator = temporaries_allocator,
            .is_test_mode = false,
        };

        run_instantiate_pass_on_function(
            core_module.name,
            *function_declaration,
            *function_definition,
            parameters
        );

        return iris::compiler::tests::format_core_module_to_text(core_module);
    }

    static std::pmr::string run_instantiate_pass_and_format(
        std::string_view const input_text,
        std::span<std::string_view const> const input_dependencies_text
    )
    {
        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input_text, input_dependencies_text);
        iris::Module& core_module = context.core_module();

        Instantiate_runtime_context runtime_context = create_instantiate_runtime_context(core_module, context.declaration_database);

        std::pmr::vector<iris::Module const*> sorted_modules;
        sorted_modules.reserve(context.dependencies().size() + 1);
        for (iris::Module const& dependency_module : context.dependencies())
            sorted_modules.push_back(&dependency_module);
        sorted_modules.push_back(&core_module);

        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        All_passes_parameters const parameters =
        {
            .target_module_name = core_module.name,
            .sorted_core_modules = sorted_modules,
            .llvm_context = *runtime_context.llvm_data.context,
            .llvm_data_layout = runtime_context.llvm_data.data_layout,
            .declaration_database = context.declaration_database,
            .clang_context = *runtime_context.clang_context,
            .dependencies = core_module.dependencies,
            .instanced_declarations = core_module.instanced_declarations,
            .definitions = core_module.definitions,
            .output_allocator = output_allocator,
            .temporaries_allocator = temporaries_allocator,
            .is_test_mode = false,
        };

        run_instantiate_pass_on_module(core_module, parameters);

        return iris::compiler::tests::format_core_module_to_text(core_module);
    }

    static bool has_import_usage(
        iris::Module_dependencies const& dependencies,
        std::string_view const alias,
        std::string_view const usage
    )
    {
        Import_module_with_alias const* const import_alias = find_import_module_with_alias(dependencies, alias);
        if (import_alias == nullptr)
            return false;

        return std::find(import_alias->usages.begin(), import_alias->usages.end(), usage) != import_alias->usages.end();
    }

    static bool has_import_usage_for_module(
        iris::Module_dependencies const& dependencies,
        std::string_view const module_name,
        std::string_view const usage
    )
    {
        Import_module_with_alias const* const import_alias = find_import_module_with_module_name(dependencies, module_name);
        if (import_alias == nullptr)
            return false;

        return std::find(import_alias->usages.begin(), import_alias->usages.end(), usage) != import_alias->usages.end();
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

import another_module as am;

function use_internal() -> ()
{
}

export function_constructor to_json(value_type: Type)
{
    return function(value: *value_type) -> ()
    {
        use_internal();
        am.foo();
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
import another_module as am;

function run(value: *Int32) -> ()
{
    iris.json@print_json@9753731967319569499(value);
}

@unique_name("iris.json.use_internal")
function iris.json.use_internal() -> ()
{
}

@unique_name("iris.json@to_json@3489948734076117284")
function iris.json@to_json@3489948734076117284(value: *Int32) -> ()
{
    iris.json.use_internal();
    am.foo();
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

    TEST_CASE("Rewrites nested constructor calls across modules", "[Instantiate_pass][Passes]")
    {
        std::string_view const dependency = R"(module iris.json_nested;

function use_internal() -> ()
{
}

export function_constructor to_json(value_type: Type)
{
    return function(value: *value_type) -> ()
    {
        if true
        {
            use_internal();
        }
    };
}
)";

        std::string_view const input = R"(module json_nested_usage;

import iris.json_nested as json_nested;

function run(value: *Int32) -> ()
{
    json_nested.to_json::<Int32>(value);
}
)";

        std::string_view const expected = R"(module json_nested_usage;

import iris.json_nested as json_nested;

@unique_name("iris.json_nested.use_internal")
function iris.json_nested.use_internal() -> ()
{
}

function run(value: *Int32) -> ()
{
    iris.json_nested@to_json@217872819520902618(value);
}

@unique_name("iris.json_nested@to_json@217872819520902618")
function iris.json_nested@to_json@217872819520902618(value: *Int32) -> ()
{
    if true
    {
        iris.json_nested.use_internal();
    }
}
)";
        std::array<std::string_view, 1> const dependencies = { dependency };
        std::pmr::string const actual = run_instantiate_pass_and_format(input, dependencies, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("Duplicates private function dependencies recursively across modules", "[Instantiate_pass][Passes]")
    {
        std::string_view const dependency = R"(module iris.json_recursive;

function private_leaf() -> ()
{
}

function private_mid() -> ()
{
    private_leaf();
}

export function_constructor to_json(value_type: Type)
{
    return function(value: *value_type) -> ()
    {
        private_mid();
    };
}
)";

        std::string_view const input = R"(module json_recursive_usage;

import iris.json_recursive as json_recursive;

function run(value: *Int32) -> ()
{
    json_recursive.to_json::<Int32>(value);
}
)";

        std::string_view const expected = R"(module json_recursive_usage;

import iris.json_recursive as json_recursive;

@unique_name("iris.json_recursive.private_leaf")
function iris.json_recursive.private_leaf() -> ()
{
}

function run(value: *Int32) -> ()
{
    iris.json_recursive@to_json@1271315375365545525(value);
}

@unique_name("iris.json_recursive.private_mid")
function iris.json_recursive.private_mid() -> ()
{
    iris.json_recursive.private_leaf();
}

@unique_name("iris.json_recursive@to_json@1271315375365545525")
function iris.json_recursive@to_json@1271315375365545525(value: *Int32) -> ()
{
    iris.json_recursive.private_mid();
}
)";
        std::array<std::string_view, 1> const dependencies = { dependency };
        std::pmr::string const actual = run_instantiate_pass_and_format(input, dependencies, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("Rewrites nested constructor calls across modules 2", "[Instantiate_pass][Passes]")
    {
        std::string_view const dependency = R"(module iris.json_nested;

export struct Write_stream
{
    write: function<(value: *C_char) -> ()> = null;
}

function print_to_stdout(value: *C_char) -> ()
{
}

export function_constructor print_json(value_type: Type)
{
    return function(value: *value_type) -> ()
    {
        var stream: Write_stream = explicit {
            write: print_to_stdout
        };
    };
}
)";

        std::string_view const input = R"(module json_nested_usage;

import iris.json_nested as json_nested;

function run(value: *Int32) -> ()
{
    json_nested.print_json::<Int32>(value);
}
)";

        std::string_view const expected = R"(module json_nested_usage;

import iris.json_nested as json_nested;

function run(value: *Int32) -> ()
{
    iris.json_nested@print_json@11451538589209302994(value);
}

@unique_name("iris.json_nested.print_to_stdout")
function iris.json_nested.print_to_stdout(value: *C_char) -> ()
{
}

@unique_name("iris.json_nested@print_json@11451538589209302994")
function iris.json_nested@print_json@11451538589209302994(value: *Int32) -> ()
{
    var stream: json_nested.Write_stream = explicit {
        write: iris.json_nested.print_to_stdout
    };
}
)";
        std::array<std::string_view, 1> const dependencies = { dependency };
        std::pmr::string const actual = run_instantiate_pass_and_format(input, dependencies, "run");

        CHECK(expected == actual);
    }

    TEST_CASE("Adds import usages for rewritten constructor accesses", "[Instantiate_pass][Passes]")
    {
        std::string_view const dependency = R"(module iris.json_usage_test;

import another_module as am;

function use_internal() -> ()
{
}

export function_constructor to_json(value_type: Type)
{
    return function(value: *value_type) -> ()
    {
        use_internal();
        am.foo();
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

        std::string_view const input = R"(module json_usage_test;

import iris.json_usage_test as iris_json;

function run(value: *Int32) -> ()
{
    iris_json.print_json::<Int32>(value);
}
)";

        std::array<std::string_view, 1> const dependencies = { dependency };
        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input, dependencies);
        iris::Module& core_module = context.core_module();

        iris::Function_declaration* function_declaration = iris::compiler::tests::find_mutable_function_declaration(core_module, "run");
        REQUIRE(function_declaration != nullptr);

        iris::Function_definition* function_definition = iris::compiler::tests::find_mutable_function_definition(core_module, "run");
        REQUIRE(function_definition != nullptr);

        Instantiate_runtime_context runtime_context = create_instantiate_runtime_context(core_module, context.declaration_database);

        std::pmr::vector<iris::Module const*> sorted_modules;
        sorted_modules.reserve(context.dependencies().size() + 1);
        for (iris::Module const& dependency_module : context.dependencies())
            sorted_modules.push_back(&dependency_module);
        sorted_modules.push_back(&core_module);

        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        All_passes_parameters const parameters =
        {
            .target_module_name = core_module.name,
            .sorted_core_modules = sorted_modules,
            .llvm_context = *runtime_context.llvm_data.context,
            .llvm_data_layout = runtime_context.llvm_data.data_layout,
            .declaration_database = context.declaration_database,
            .clang_context = *runtime_context.clang_context,
            .dependencies = core_module.dependencies,
            .instanced_declarations = core_module.instanced_declarations,
            .definitions = core_module.definitions,
            .output_allocator = output_allocator,
            .temporaries_allocator = temporaries_allocator,
            .is_test_mode = false,
        };

        run_instantiate_pass_on_function(
            core_module.name,
            *function_declaration,
            *function_definition,
            parameters
        );

        CHECK(!has_import_usage(core_module.dependencies, "iris_json", "use_internal"));
        CHECK(has_import_usage_for_module(core_module.dependencies, "another_module", "foo"));
    }
}