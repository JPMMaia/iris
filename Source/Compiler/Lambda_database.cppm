export module iris.compiler.lambda_database;

import std;

import iris.core;

namespace iris::compiler
{
    // Where the lambda pass records what it generated.
    //
    // Lowering a lambda literal produces an internal function that its body became. That name is
    // a result of compilation, not a property of the source, so it is kept beside the module
    // rather than inside iris::Module, which stays a description of what was written.
    //
    // A lambda is identified by the function that encloses it plus the source range it was
    // written at, which survives the copies and moves a module goes through between the pass and
    // code generation.
    export struct Lambda_database
    {
        std::pmr::unordered_map<std::pmr::string, std::pmr::string> lambda_to_generated_function_name;
    };

    export std::pmr::string create_lambda_key(
        std::string_view const module_name,
        std::string_view const enclosing_function_name,
        iris::Lambda_expression const& lambda_expression
    );

    export void add_generated_lambda_function_name(
        Lambda_database& lambda_database,
        std::pmr::string key,
        std::pmr::string generated_function_name
    );

    export std::optional<std::string_view> find_generated_lambda_function_name(
        Lambda_database const& lambda_database,
        std::string_view const key
    );
}
