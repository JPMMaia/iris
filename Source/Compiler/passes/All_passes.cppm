export module h.compiler.all_passes;

import llvm;
import std;

import h.compiler.clang_data;
import h.compiler.types;
import h.core;
import h.core.declarations;

namespace h::compiler
{
    export struct All_passes_parameters
    {
        llvm::LLVMContext& llvm_context;
        llvm::DataLayout const& llvm_data_layout;
        h::Declaration_database& declaration_database;
        h::compiler::Clang_context& clang_context;
        std::pmr::polymorphic_allocator<> const& output_allocator;
        std::pmr::polymorphic_allocator<> const& temporaries_allocator;
    };

    export void run_all_passes_on_module(
        h::Module& core_module,
        All_passes_parameters const& parameters
    );

    export void run_all_passes_on_function(
        h::Module& core_module,
        h::Function_declaration& function_declaration,
        h::Function_definition& function_definition,
        All_passes_parameters const& parameters
    );
}