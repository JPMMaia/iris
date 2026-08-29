module;

#include <stdexcept>

module iris.compiler.lambda_database;

import std;

import iris.core;

namespace iris::compiler
{
    std::pmr::string create_lambda_key(
        std::string_view const module_name,
        std::string_view const enclosing_function_name,
        iris::Lambda_expression const& lambda_expression
    )
    {
        // Without a source range there is nothing to tell two lambdas of the same function
        // apart, and silently reusing a key would make one lambda call another's body.
        if (!lambda_expression.source_range.has_value())
            throw std::runtime_error{ std::format("Lambda expression in '{}.{}' has no source range and cannot be identified!", module_name, enclosing_function_name) };

        iris::Source_position const& start = lambda_expression.source_range.value().start;

        return std::pmr::string{ std::format("{}.{}@{}:{}", module_name, enclosing_function_name, start.line, start.column) };
    }

    void add_generated_lambda_function_name(
        Lambda_database& lambda_database,
        std::pmr::string key,
        std::pmr::string generated_function_name
    )
    {
        lambda_database.lambda_to_generated_function_name.insert_or_assign(std::move(key), std::move(generated_function_name));
    }

    std::optional<std::string_view> find_generated_lambda_function_name(
        Lambda_database const& lambda_database,
        std::string_view const key
    )
    {
        auto const location = lambda_database.lambda_to_generated_function_name.find(std::pmr::string{ key });
        if (location == lambda_database.lambda_to_generated_function_name.end())
            return std::nullopt;

        return std::string_view{ location->second };
    }
}
