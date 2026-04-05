module;

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LLVMContext.h>

export module h.compiler.compile_time_pass;

import std;

import h.compiler.clang_data;
import h.compiler.types;
import h.core;
import h.core.declarations;

namespace h::compiler
{
    export struct Compile_time_value_and_type
    {
        h::Statement statement;
        std::optional<h::Type_reference> type;
    };

    export struct Compile_time_parameters
    {
        h::Module& core_module;
        std::pmr::polymorphic_allocator<> const& output_allocator;
        std::pmr::polymorphic_allocator<> const& temporaries_allocator;
        llvm::LLVMContext& llvm_context;
        llvm::DataLayout const& llvm_data_layout;
        h::Declaration_database const& declaration_database;
        h::compiler::Clang_context const& clang_context;
    };

    std::optional<Compile_time_value_and_type> evaluate_compile_time_expression(
        h::Statement const& statement,
        h::Expression const& expression,
        Compile_time_parameters const& parameters
    );

    std::optional<Compile_time_value_and_type> evaluate_compile_time_statement(
        h::Statement const& statement,
        Compile_time_parameters const& parameters
    );

    export void run_compile_time_pass_on_module(
        h::Module& core_module,
        Compile_time_parameters const& parameters
    );

    export void run_compile_time_pass_on_function(
        h::Function_declaration const& function_declaration,
        h::Function_definition& function_definition,
        Compile_time_parameters const& parameters
    );
}
