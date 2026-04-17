export module iris.compiler.all_passes;

import llvm;
import std;

import iris.compiler.clang_data;
import iris.compiler.types;
import iris.core;
import iris.core.declarations;

namespace iris::compiler
{
    export struct All_passes_parameters
    {
        llvm::LLVMContext& llvm_context;
        llvm::DataLayout const& llvm_data_layout;
        iris::Declaration_database& declaration_database;
        iris::compiler::Clang_context& clang_context;
        std::pmr::polymorphic_allocator<> const& output_allocator;
        std::pmr::polymorphic_allocator<> const& temporaries_allocator;
    };

    export void run_all_passes_on_module(
        iris::Module& core_module,
        All_passes_parameters const& parameters
    );

    export void run_all_passes_on_function(
        iris::Module& core_module,
        iris::Function_declaration& function_declaration,
        iris::Function_definition& function_definition,
        All_passes_parameters const& parameters
    );
}