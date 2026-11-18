export module iris.compiler.lambda_pass;

import std;

import iris.compiler.lambda_database;
import iris.core;
import iris.core.declarations;

namespace iris::compiler
{
    // Lowers every lambda literal in a module to an internal function plus, when the lambda
    // captures anything, a capture environment struct. The name of the function each body
    // became is recorded in the lambda database, and code generation only has to build the
    // two-word { function_pointer, user_data } value at the use site.
    export void run_lambda_pass_on_module(
        iris::Module& core_module,
        iris::Declaration_database& declaration_database,
        Lambda_database& lambda_database,
        bool const is_test_mode
    );

    export void run_lambda_pass_on_function(
        iris::Module& core_module,
        iris::Declaration_database& declaration_database,
        Lambda_database& lambda_database,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition& function_definition
    );
}
