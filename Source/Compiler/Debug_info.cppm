export module iris.compiler.debug_info;

import std;
import llvm;

import iris.compiler.types;
import iris.core;

namespace iris::compiler
{
    export struct Debug_info
    {
        std::unique_ptr<llvm::DIBuilder> llvm_builder;
        Debug_type_database type_database;
        llvm::DICompileUnit* main_llvm_compile_unit;
        std::pmr::vector<llvm::DIScope*> llvm_scopes;
        std::unordered_map<std::filesystem::path, llvm::DIFile*> llvm_debug_files;
    };

    export llvm::DIScope* get_debug_scope(
        Debug_info& debug_info
    );

    export void push_debug_scope(
        Debug_info& debug_info,
        llvm::DIScope* scope
    );

    export void push_debug_lexical_block_scope(
        Debug_info& debug_info,
        Source_position const source_position
    );

    export void pop_debug_scope(
        Debug_info& debug_info
    );

    export void set_debug_location(
        llvm::IRBuilder<>& llvm_builder,
        Debug_info& debug_info,
        unsigned const line,
        unsigned const column
    );

    export void set_debug_location(
        llvm::IRBuilder<>& llvm_builder,
        Debug_info& debug_info,
        std::optional<Source_position> const& position
    );

    export void unset_debug_location(
        llvm::IRBuilder<>& llvm_builder
    );

    export void set_debug_location_at_statement(
        llvm::IRBuilder<>& llvm_builder,
        Debug_info& debug_info,
        iris::Statement const& statement
    );

    export void set_debug_location_at_range(
        llvm::IRBuilder<>& llvm_builder,
        Debug_info& debug_info,
        std::optional<iris::Source_range> const& source_range
    );

    export using Debug_type_names = std::pmr::vector<std::pmr::string>;
    export using Debug_type_names_per_module = std::pmr::unordered_map<std::pmr::string, Debug_type_names>;

    export Debug_type_names_per_module create_requested_dependency_debug_types(
        Module const& core_module,
        std::pmr::unordered_map<std::pmr::string, Module const*> const& core_module_dependencies,
        std::pmr::polymorphic_allocator<> const& allocator
    );
}
