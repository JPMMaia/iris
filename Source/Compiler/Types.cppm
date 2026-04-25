export module iris.compiler.types;

import std;
import llvm;

import iris.compiler.clang_data;
import iris.core.hash;
import iris.core;
import iris.core.declarations;
import iris.core.struct_layout;

namespace iris::compiler
{
    export bool is_enum_type(Type_reference const& type, llvm::Value* value);

    export struct Builtin_types
    {
        llvm::StructType* string;
    };

    export struct Debug_builtin_types
    {
        llvm::DIType* string;
    };

    using LLVM_type_map = std::pmr::unordered_map<std::pmr::string, llvm::Type*>;
    using LLVM_debug_type_map = std::pmr::unordered_map<std::pmr::string, llvm::TypedTrackingMDRef<llvm::DIType>>;
    using Module_name = std::pmr::string;

    export struct Type_database
    {
        Builtin_types builtin;
        std::pmr::unordered_map<Module_name, LLVM_type_map> name_to_llvm_type;
    };

    export struct Debug_type_database
    {
        Debug_builtin_types builtin;
        std::pmr::unordered_map<Module_name, LLVM_debug_type_map> name_to_llvm_debug_type;
        Declaration_database const& declaration_database;
        llvm::DINamespace* builtin_namespace;
    };

    export struct Soa_member_layout
    {
        std::uint64_t block_offset = 0;
        std::uint64_t block_size = 0;
        std::uint64_t element_size = 0;
        std::uint64_t element_alignment = 0;
    };

    export struct Soa_layout
    {
        std::uint64_t size = 0;
        std::uint64_t alignment = 0;
        std::pmr::vector<Soa_member_layout> members;
    };

    export Type_database create_type_database(
        llvm::LLVMContext& llvm_context
    );

    export Debug_type_database create_debug_type_database(
        llvm::DIBuilder& llvm_debug_builder,
        llvm::DIScope& llvm_scope,
        llvm::DataLayout const& llvm_data_layout,
        Declaration_database const& declaration_database
    );

    export void add_module_types(
        Type_database& type_database,
        llvm::LLVMContext& llvm_context,
        llvm::DataLayout const& llvm_data_layout,
        Clang_module_data const& clang_module_data,
        Module const& core_module
    );

    export void add_struct_declaration_to_type_database(
        Type_database& type_database,
        Clang_module_data const& clang_module_data,
        Module const& core_module,
        Struct_declaration const& struct_declaration
    );

    export void add_union_declaration_to_type_database(
        Type_database& type_database,
        Clang_module_data const& clang_module_data,
        Module const& core_module,
        Union_declaration const& union_declaration
    );

    export void add_module_debug_types(
        Debug_type_database& debug_type_database,
        llvm::DIBuilder& llvm_debug_builder,
        llvm::DIScope& llvm_debug_scope,
        llvm::DIFile& llvm_debug_file,
        std::unordered_map<std::filesystem::path, llvm::DIFile*>& llvm_debug_files,
        llvm::DataLayout const& llvm_data_layout,
        Clang_module_data const& clang_module_data,
        Module const& core_module,
        std::pmr::unordered_map<std::pmr::string, std::pmr::vector<llvm::Constant*>> const& enum_value_constants,
        Type_database const& type_database
    );

    export llvm::Type* type_reference_to_llvm_type(
        llvm::LLVMContext& llvm_context,
        llvm::DataLayout const& llvm_data_layout,
        Type_reference const& type_reference,
        Type_database const& type_database
    );

    export llvm::Type* type_reference_to_llvm_type(
        llvm::LLVMContext& llvm_context,
        llvm::DataLayout const& llvm_data_layout,
        std::span<Type_reference const> type_reference,
        Type_database const& type_database
    );

    export llvm::Type* type_reference_to_llvm_type_on_demand(
        llvm::LLVMContext& llvm_context,
        llvm::DataLayout const& llvm_data_layout,
        Type_reference const& type_reference,
        Declaration_database const& declaration_database,
        Clang_context const& clang_context
    );

    export llvm::Type* type_reference_to_llvm_type_on_demand(
        llvm::LLVMContext& llvm_context,
        llvm::DataLayout const& llvm_data_layout,
        std::span<Type_reference const> type_reference,
        Declaration_database const& declaration_database,
        Clang_context const& clang_context
    );

    export std::pmr::vector<llvm::Type*> type_references_to_llvm_types(
        llvm::LLVMContext& llvm_context,
        llvm::DataLayout const& llvm_data_layout,
        std::span<Type_reference const> const type_references,
        Type_database const& type_database,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export llvm::DIType* type_reference_to_llvm_debug_type(
        llvm::DIBuilder& llvm_debug_builder,
        llvm::DIScope& llvm_debug_scope,
        llvm::DataLayout const& llvm_data_layout,
        iris::Module const& core_module,
        Type_reference const& type_reference,
        Debug_type_database const& debug_type_database
    );

    export llvm::DIType* type_reference_to_llvm_debug_type(
        llvm::DIBuilder& llvm_debug_builder,
        llvm::DIScope& llvm_debug_scope,
        llvm::DataLayout const& llvm_data_layout,
        iris::Module const& core_module,
        std::span<Type_reference const> const type_reference,
        Debug_type_database const& debug_type_database
    );

    export std::pmr::vector<llvm::DIType*> type_references_to_llvm_debug_types(
        llvm::DIBuilder& llvm_debug_builder,
        llvm::DIScope& llvm_debug_scope,
        llvm::DataLayout const& llvm_data_layout,
        iris::Module const& core_module,
        std::span<Type_reference const> const type_references,
        Debug_type_database const& debug_type_database,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export Struct_layout calculate_struct_layout(
        llvm::DataLayout const& llvm_data_layout,
        Type_database const& type_database,
        std::string_view const module_name,
        std::string_view const struct_name
    );

    export Soa_layout calculate_soa_layout(
        llvm::DataLayout const& llvm_data_layout,
        Type_database const& type_database,
        std::string_view const module_name,
        std::string_view const struct_name,
        std::uint64_t array_length
    );

    export llvm::FunctionType* create_llvm_function_type(
        llvm::LLVMContext& llvm_context,
        llvm::DataLayout const& llvm_data_layout,
        std::span<Type_reference const> const input_parameter_types,
        std::span<Type_reference const> const output_parameter_types,
        bool const is_var_arg,
        Type_database const& type_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );
}
