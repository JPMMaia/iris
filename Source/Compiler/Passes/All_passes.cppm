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
        std::string_view target_module_name;
        std::span<iris::Module const* const> sorted_core_modules;
        llvm::LLVMContext& llvm_context;
        llvm::DataLayout const& llvm_data_layout;
        iris::Declaration_database& declaration_database;
        iris::compiler::Clang_context& clang_context;
        Module_dependencies& dependencies;
        Module_instanced_declarations& instanced_declarations;
        Module_definitions& definitions;
        std::pmr::polymorphic_allocator<> const& output_allocator;
        std::pmr::polymorphic_allocator<> const& temporaries_allocator;
    };

    export void run_all_passes_on_module(
        iris::Module& core_module,
        All_passes_parameters const& parameters
    );

    export void run_all_passes_on_function(
        std::string_view const module_name,
        iris::Function_declaration& function_declaration,
        iris::Function_definition& function_definition,
        All_passes_parameters const& parameters
    );
}