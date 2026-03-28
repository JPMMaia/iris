export module h.compiler.implicit_function_pass;

import std;

import h.core;
import h.core.declarations;

namespace h::compiler
{
    export void run_implicit_function_pass_on_module(
        h::Module& core_module,
        h::Declaration_database const& declaration_database
    );

    export void run_implicit_function_pass_on_function(
        h::Module& core_module,
        h::Declaration_database const& declaration_database,
        h::Function_declaration const& function_declaration,
        h::Function_definition& function_definition
    );
}
