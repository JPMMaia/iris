export module iris.compiler.pipeline_pass;

import std;

import iris.core;
import iris.core.declarations;

namespace iris::compiler
{
    // Expands every pipeline sink inline into the loop one would write by hand, then removes the
    // pipeline declarations. Pipelines are compile-time recipes, so nothing of them survives.
    //
    //   for v in process(list) { ... }
    //
    // becomes
    //
    //   for $pipeline0_i in 0u64 to list.length
    //   {
    //       var input = list[$pipeline0_i];
    //       var v0 = square(input);
    //       if v0.real > threshold
    //       {
    //           var v = v0;
    //           ...
    //       }
    //   }
    export void run_pipeline_pass_on_module(
        iris::Module& core_module,
        iris::Declaration_database const& declaration_database
    );

    export void run_pipeline_pass_on_function(
        std::string_view const module_name,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition& function_definition,
        iris::Declaration_database const& declaration_database
    );
}
