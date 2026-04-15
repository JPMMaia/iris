export module iris.compiler.clang_code_generation;

import std;
import llvm;
import clang;

import iris.core;
import iris.core.declarations;
import iris.compiler.clang_data;
import iris.compiler.debug_info;
import iris.compiler.instructions;
import iris.compiler.types;

namespace iris::compiler
{
    export void add_clang_struct_declaration(
        Clang_declaration_database& clang_declaration_database,
        clang::ASTContext& clang_ast_context,
        iris::Module const& core_module,
        iris::Struct_declaration const& struct_declaration,
        Declaration_database const& declaration_database
    );

    export void add_clang_union_declaration(
        Clang_declaration_database& clang_declaration_database,
        clang::ASTContext& clang_ast_context,
        iris::Module const& core_module,
        iris::Union_declaration const& union_declaration,
        Declaration_database const& declaration_database
    );

    export void add_clang_function_declaration(
        Clang_declaration_database& clang_declaration_database,
        clang::ASTContext& clang_ast_context,
        std::string_view const module_name,
        iris::Function_declaration const& function_declaration,
        Declaration_database const& declaration_database
    );

    export void add_clang_function_declarations(
        Clang_declaration_database& clang_declaration_database,
        clang::ASTContext& clang_ast_context,
        iris::Module const& core_module,
        Declaration_database const& declaration_database
    );

    export llvm::FunctionType* create_llvm_function_type(
        Clang_module_data const& clang_module_data,
        std::string_view const module_name,
        std::string_view const function_name
    );

    export llvm::FunctionType* convert_to_llvm_function_type(
        Clang_module_data const& clang_module_data,
        Declaration_database const& declaration_database,
        iris::Function_type const& function_type
    );

    export void set_llvm_function_argument_names(
        llvm::LLVMContext& llvm_context,
        llvm::DataLayout const& llvm_data_layout,
        Clang_module_data const& clang_module_data,
        iris::Function_declaration const& function_declaration,
        llvm::Function& llvm_function,
        Declaration_database const& declaration_database,
        Type_database const& type_database
    );

    struct Transformed_arguments
    {
        std::pmr::vector<llvm::Value*> values;
        std::pmr::vector<std::pmr::vector<llvm::Attribute>> attributes;
        bool is_return_value_passed_as_first_argument = false;
    };

    Transformed_arguments transform_arguments(
        std::pmr::vector<bool> const& is_expression_address_of,
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        Clang_module_data const& clang_module_data,
        llvm::Module& llvm_module,
        llvm::Function& llvm_function,
        iris::Module const& core_module,
        iris::Function_type const& function_type,
        std::span<llvm::Value* const> const original_arguments,
        Declaration_database const& declaration_database,
        Type_database const& type_database
    );

    llvm::Value* read_function_return_instruction(
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Function& llvm_parent_function,
        iris::Module const& core_module,
        iris::Function_type const& function_type,
        clang::CodeGen::CGFunctionInfo const& function_info,
        Type_database const& type_database,
        llvm::Value* const call_instruction
    );

    export llvm::Value* generate_function_call(
        std::pmr::vector<bool> const& is_expression_address_of,
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Module& llvm_module,
        llvm::Function& llvm_parent_function,
        Clang_module_data const& clang_module_data,
        iris::Module const& core_module,
        iris::Function_type const& function_type,
        llvm::FunctionType& llvm_function_type,
        llvm::Value& llvm_function_callee,
        std::span<llvm::Value* const> const llvm_arguments,
        Declaration_database const& declaration_database,
        Type_database const& type_database
    );

    export std::pmr::vector<Value_and_type> generate_function_arguments(
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        Clang_module_data const& clang_module_data,
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        llvm::Function& llvm_function,
        llvm::BasicBlock& llvm_block,
        Declaration_database const& declaration_database,
        Type_database const& type_database,
        Debug_info* debug_info
    );

    export llvm::Value* generate_function_return_instruction(
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Module& llvm_module,
        Clang_module_data const& clang_module_data,
        iris::Module const& core_module,
        iris::Function_type const& function_type,
        llvm::Function& llvm_function,
        Declaration_database const& declaration_database,
        Type_database const& type_database,
        Value_and_type const& value_to_return,
        bool const is_taking_address_of_llvm_value
    );

    export void set_function_definition_attributes(
        llvm::LLVMContext& llvm_context,
        Clang_module_data const& clang_module_data,
        llvm::Function& llvm_function
    );

    export llvm::Type* convert_type(
        Clang_module_data const& clang_module_data,
        std::string_view const module_name,
        std::string_view const declaration_name
    );

    export llvm::Type* convert_type_on_demand(
        Clang_context const& clang_context,
        Declaration_database const& declaration_database,
        std::string_view const module_name,
        std::string_view const declaration_name
    );

    export llvm::Type* convert_type(
        Clang_module_data const& clang_module_data,
        clang::RecordDecl* const record_declaration
    );

    export llvm::FunctionType* convert_function_type(
        Clang_module_data const& clang_module_data,
        clang::FunctionDecl* const function_declaration
    );

    export struct Clang_struct_member_bit_field_info
    {
        std::uint32_t bit_field_offset_in_bits;
        std::uint32_t bit_field_size_in_bits;
    };

    export struct Clang_struct_member_info
    {
        std::uint32_t llvm_struct_member_index;
        std::optional<Clang_struct_member_bit_field_info> bit_field_info;
    };

    export std::pmr::vector<Clang_struct_member_info> get_clang_struct_member_infos(
        Clang_module_data const& clang_module_data,
        std::string_view const module_name,
        Struct_declaration const& struct_declaration,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export Value_and_type generate_load_struct_member_instructions(
        Clang_module_data const& clang_module_data,
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Value* const struct_alloca,
        std::string_view const access_member_name,
        std::string_view const module_name,
        Struct_declaration const& struct_declaration,
        Type_database const& type_database
    );

    export Value_and_type generate_store_struct_member_instructions(
        Clang_module_data const& clang_module_data,
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Value* const struct_alloca,
        std::string_view const access_member_name,
        std::string_view const module_name,
        Struct_declaration const& struct_declaration,
        Value_and_type const& value_to_store,
        Type_database const& type_database
    );

    std::optional<clang::QualType> create_type(
        clang::ASTContext& clang_ast_context,
        std::span<iris::Type_reference const> const type_reference,
        bool const alloca_type,
        Declaration_database const& declaration_database,
        Clang_declaration_database const& clang_declaration_database
    );

    std::optional<clang::QualType> create_type(
        clang::ASTContext& clang_ast_context,
        iris::Type_reference const& type_reference,
        bool const alloca_type,
        Declaration_database const& declaration_database,
        Clang_declaration_database const& clang_declaration_database
    );

    enum class Convertion_type
    {
        From_original_to_abi,
        From_abi_to_original
    };

    llvm::Value* read_from_type(
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Function& llvm_parent_function,
        llvm::Value* const source_llvm_value,
        llvm::Type* const source_llvm_type,
        bool const is_taking_address_of_source_llvm_value,
        llvm::Type* const destination_llvm_type,
        std::optional<std::string_view> const alloca_name,
        clang::CodeGen::ABIArgInfo const& abi_argument_info,
        Convertion_type const convertion_type
    );

    export Clang_data create_clang_data(
        llvm::LLVMContext& llvm_context,
        llvm::Triple const& llvm_triple,
        unsigned int const optimization_level
    );

    export Clang_context create_clang_context(
        llvm::LLVMContext& llvm_context,
        Clang_data const& clang_data,
        std::string_view const module_name
    );

    export Clang_declaration_database create_clang_declaration_database(
        llvm::LLVMContext& llvm_context,
        Clang_data const& clang_data,
        std::span<iris::Module const* const> const core_modules,
        Declaration_database const& declaration_database
    );

    export Clang_module_data create_clang_module_data(
        Clang_context&& clang_context,
        std::span<iris::Module const* const> const sorted_modules,
        Declaration_database const& declaration_database
    );

    export Clang_module_data create_clang_module_data(
        llvm::LLVMContext& llvm_context,
        Clang_data const& clang_data,
        std::string_view const module_name,
        std::span<iris::Module const* const> const sorted_modules,
        Declaration_database const& declaration_database
    );
}
