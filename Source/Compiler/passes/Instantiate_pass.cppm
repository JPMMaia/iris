module;

export module h.compiler.instantiate_pass;

import std;

import h.core;
import h.core.declarations;

namespace h::compiler
{
    export struct All_passes_parameters;

    export void run_instantiate_pass_on_module(
        h::Module& core_module,
        All_passes_parameters const& parameters
    );

    export void run_instantiate_pass_on_function(
        h::Module& core_module,
        h::Function_declaration& function_declaration,
        h::Function_definition& function_definition,
        All_passes_parameters const& parameters
    );
}
