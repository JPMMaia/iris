module;

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LLVMContext.h>

export module iris.compiler.compile_time_pass;

import std;

import iris.compiler.clang_data;
import iris.compiler.types;
import iris.core;
import iris.core.declarations;

namespace iris::compiler
{
    export struct Compile_time_value_and_type
    {
        iris::Statement statement;
        std::optional<iris::Type_reference> type;
    };

    export struct Compile_time_parameters
    {
        iris::Module& core_module;
        std::pmr::polymorphic_allocator<> const& output_allocator;
        std::pmr::polymorphic_allocator<> const& temporaries_allocator;
        llvm::LLVMContext& llvm_context;
        llvm::DataLayout const& llvm_data_layout;
        iris::Declaration_database const& declaration_database;
        iris::compiler::Clang_context const& clang_context;
    };

    std::optional<Compile_time_value_and_type> evaluate_compile_time_expression(
        iris::Statement const& statement,
        iris::Expression const& expression,
        Compile_time_parameters const& parameters
    );

    std::optional<Compile_time_value_and_type> evaluate_compile_time_statement(
        iris::Statement const& statement,
        Compile_time_parameters const& parameters
    );

    export void run_compile_time_pass_on_module(
        iris::Module& core_module,
        Compile_time_parameters const& parameters
    );

    export void run_compile_time_pass_on_function(
        iris::Function_declaration const& function_declaration,
        iris::Function_definition& function_definition,
        Compile_time_parameters const& parameters
    );
}
