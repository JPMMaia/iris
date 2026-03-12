export module h.compiler.compile_time_pass;

import std;

import h.core;

namespace h::compiler
{
    export struct Compile_time_value_and_type
    {
        h::Statement statement;
        std::optional<h::Type_reference> type;
    };

    export struct Compile_time_parameters
    {
        h::Module const& core_module;
        std::pmr::polymorphic_allocator<> const& output_allocator;
    };

    std::optional<Compile_time_value_and_type> evaluate_compile_time_expression(
        h::Statement const& statement,
        h::Expression const& expression,
        Compile_time_parameters const& parameters
    );

    std::optional<Compile_time_value_and_type> evaluate_compile_time_statement(
        h::Statement const& statement,
        Compile_time_parameters const& parameters
    );

    export void run_compile_time_pass_on_function(
        h::Module const& core_module,
        h::Function_declaration const& function_declaration,
        h::Function_definition& function_definition,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );
}
