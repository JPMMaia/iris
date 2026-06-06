export module iris.compiler.common;

import std;
import llvm;

import iris.core;
import iris.core.declarations;

namespace iris::compiler
{
    export std::string_view to_string_view(llvm::StringRef const string);

    export std::string mangle_name(
        std::string_view const module_name,
        std::string_view const declaration_name,
        std::optional<std::string_view> const unique_name
    );

    export std::string mangle_name(
        iris::Declaration_database const& declaration_database,
        std::string_view const module_name,
        std::string_view const declaration_name
    );

    export std::string mangle_name(
        Module const& core_module,
        std::string_view declaration_name,
        std::optional<std::string_view> unique_name
    );

    export std::string mangle_function_name(
        Module const& core_module,
        std::string_view declaration_name
    );

    export std::string mangle_struct_name(
        Module const& core_module,
        std::string_view declaration_name
    );

    export std::string mangle_union_name(
        Module const& core_module,
        std::string_view declaration_name
    );

    export llvm::Function* get_llvm_function(
        std::string_view const module_name,
        llvm::Module& llvm_module,
        std::string_view const name,
        std::optional<std::string_view> const unique_name
    );

    export llvm::Function* get_llvm_function(
        Module const& core_module,
        llvm::Module& llvm_module,
        std::string_view name
    );

    export iris::Module const* get_module(
        std::string_view const module_name,
        iris::Module const& core_module,
        std::pmr::unordered_map<std::pmr::string, iris::Module> const& core_module_dependencies
    );

    export llvm::GlobalValue::LinkageTypes to_linkage(
        Linkage const linkage,
        bool const is_test
    );
}
