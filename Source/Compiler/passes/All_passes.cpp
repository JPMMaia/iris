module iris.compiler.all_passes;

import std;

import iris.compiler.compile_time_pass;
import iris.compiler.implicit_function_pass;
import iris.compiler.instantiate_pass;

namespace iris::compiler
{
    void run_all_passes_on_module(
        iris::Module& core_module,
        All_passes_parameters const& parameters
    )
    {
        Compile_time_parameters const compile_time_parameters
        {
            .core_module = core_module,
            .output_allocator = parameters.output_allocator,
            .temporaries_allocator = parameters.temporaries_allocator,
            .llvm_context = parameters.llvm_context,
            .llvm_data_layout = parameters.llvm_data_layout,
            .declaration_database = parameters.declaration_database,
            .clang_context = parameters.clang_context,
        };
        run_compile_time_pass_on_module(core_module, compile_time_parameters);

        run_instantiate_pass_on_module(core_module, parameters);
        run_implicit_function_pass_on_module(core_module, parameters.declaration_database);
    }

    void run_all_passes_on_function(
        iris::Module& core_module,
        iris::Function_declaration& function_declaration,
        iris::Function_definition& function_definition,
        All_passes_parameters const& parameters
    )
    {
        Compile_time_parameters const compile_time_parameters
        {
            .core_module = core_module,
            .output_allocator = parameters.output_allocator,
            .temporaries_allocator = parameters.temporaries_allocator,
            .llvm_context = parameters.llvm_context,
            .llvm_data_layout = parameters.llvm_data_layout,
            .declaration_database = parameters.declaration_database,
            .clang_context = parameters.clang_context,
        };
        run_compile_time_pass_on_function(function_declaration, function_definition, compile_time_parameters);
        
        run_instantiate_pass_on_function(core_module, function_declaration, function_definition, parameters);
        run_implicit_function_pass_on_function(core_module, parameters.declaration_database, function_declaration, function_definition);
    }
}
