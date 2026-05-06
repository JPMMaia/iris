export module iris.compiler.expressions;

import std;
import llvm;

import iris.core;
import iris.core.declarations;
import iris.compiler.clang_data;
import iris.compiler.debug_info;
import iris.compiler.instructions;
import iris.compiler.types;

namespace iris::compiler
{
    export enum class Block_type
    {
        None,
        For_loop,
        If,
        Switch,
        While_loop
    };

    export struct Block_info
    {
        Block_type block_type = {};
        llvm::BasicBlock* repeat_block = nullptr;
        llvm::BasicBlock* after_block = nullptr;
        llvm::Value* stack_save_pointer = nullptr;
    };

    using Enum_constants = std::pmr::vector<llvm::Constant*>;

    export struct Enum_value_constants
    {
        std::pmr::unordered_map<std::pmr::string, Enum_constants> map;
    };

    export enum class Contract_options
    {
        Disabled,
        Log_error_and_abort,
    };

    export struct Expression_parameters
    {
        llvm::LLVMContext& llvm_context;
        llvm::DataLayout const& llvm_data_layout;
        llvm::IRBuilder<>& llvm_builder;
        llvm::Function* const llvm_parent_function;
        llvm::Module& llvm_module;
        Clang_module_data const& clang_module_data;
        Module const& core_module;
        std::pmr::unordered_map<std::pmr::string, Module const*> const& core_module_dependencies;
        Declaration_database& declaration_database;
        Type_database& type_database;
        Enum_value_constants const& enum_value_constants;
        std::span<Block_info> blocks;
        std::span<std::pmr::vector<Statement>> defer_expressions_per_block;
        std::optional<Function_declaration const*> function_declaration;
        std::span<Value_and_type const> function_arguments;
        std::span<Value_and_type const> local_variables;
        std::optional<Type_reference> expression_type;
        Debug_info* debug_info;
        Contract_options contract_options; 
        std::optional<Source_position> source_position;
        std::pmr::polymorphic_allocator<> const& temporaries_allocator;
    };

    export llvm::Constant* fold_constant(
        llvm::Value* value,
        llvm::DataLayout const& llvm_data_layout
    );

    export llvm::Constant* fold_statement_constant(
        Statement const& statement,
        Expression_parameters const& parameters
    );


    export Value_and_type create_expression_value(
        std::size_t expression_index,
        Statement const& statement,
        Expression_parameters const& parameters
    );

    export Value_and_type load_if_needed(
        Value_and_type const& value,
        std::size_t const expression_index,
        Statement const& statement,
        Expression_parameters const& parameters
    );

    export Value_and_type create_loaded_expression_value(
        std::size_t expression_index,
        Statement const& statement,
        Expression_parameters const& parameters
    );

    export Value_and_type create_statement_value(
        Statement const& statement,
        Expression_parameters const& parameters
    );

    export Value_and_type create_loaded_statement_value(
        Statement const& statement,
        Expression_parameters const& parameters
    );

    export void create_statement_values(
        std::span<Statement const> statements,
        Expression_parameters const& parameters,
        bool const execute_defer_expressions_at_end
    );

    export void create_instructions_at_end_of_block(
        Expression_parameters const& parameters
    );

    export void create_instructions_pop_blocks(
        Expression_parameters const& parameters,
        std::size_t const blocks_to_pop_count
    );

    export void create_instructions_at_return(
        Expression_parameters const& parameters
    );

    enum class Condition_type
    {
        Assert,
        Precondition,
        Postcondition,
    };

    export void create_function_preconditions(
        llvm::LLVMContext& llvm_context,
        llvm::Module& llvm_module,
        llvm::Function& llvm_function,
        llvm::IRBuilder<>& llvm_builder,
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        Expression_parameters const& expression_parameters
    );

    export void create_function_postconditions(
        llvm::LLVMContext& llvm_context,
        llvm::Module& llvm_module,
        llvm::Function& llvm_function,
        llvm::IRBuilder<>& llvm_builder,
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        Expression_parameters const& expression_parameters
    );

    void create_check_condition_instructions(
        llvm::LLVMContext& llvm_context,
        llvm::Module& llvm_module,
        llvm::Function& llvm_function,
        llvm::IRBuilder<>& llvm_builder,
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        iris::Function_condition const& function_condition,
        Condition_type const condition_type,
        Expression_parameters const& expression_parameters
    );

    Value_and_type create_variable_expression_value(
        Variable_expression const& expression,
        Expression_parameters const& parameters
    );

    Value_and_type convert_to_expected_type_if_needed(
        Value_and_type const& value,
        Expression_parameters const& parameters
    );
}
