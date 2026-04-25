module;

export module iris.compiler.instantiate_pass;

import std;

import iris.core;
import iris.core.declarations;

namespace iris::compiler
{
    export struct All_passes_parameters;

    export void run_instantiate_pass_on_module(
        iris::Module& core_module,
        All_passes_parameters const& parameters
    );

    export void run_instantiate_pass_on_function(
        std::string_view const module_name,
        iris::Function_declaration& function_declaration,
        iris::Function_definition& function_definition,
        All_passes_parameters const& parameters
    );
}
