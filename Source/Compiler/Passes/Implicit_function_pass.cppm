export module iris.compiler.implicit_function_pass;

import std;

import iris.core;
import iris.core.declarations;

namespace iris::compiler
{
    export void run_implicit_function_pass_on_module(
        iris::Module& core_module,
        iris::Declaration_database const& declaration_database,
        bool const is_test_mode
    );

    export void run_implicit_function_pass_on_function(
        std::string_view const module_name,
        iris::Module_dependencies& dependencies,
        iris::Declaration_database const& declaration_database,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition& function_definition
    );
}
