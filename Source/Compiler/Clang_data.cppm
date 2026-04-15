export module iris.compiler.clang_data;

import std;
import clang;

import iris.core;
import iris.core.declarations;
import iris.core.hash;
import iris.core.string_hash;

namespace iris::compiler
{    
    export struct Clang_data
    {
        std::unique_ptr<clang::CompilerInstance> compiler_instance;
    };

    export struct Clang_module_declarations
    {
        std::pmr::unordered_map<std::pmr::string, clang::FunctionDecl*, iris::String_hash, iris::String_equal> function_declarations;
        std::pmr::unordered_map<std::pmr::string, clang::TypedefDecl*, iris::String_hash, iris::String_equal> alias_type_declarations;
        std::pmr::unordered_map<std::pmr::string, clang::EnumDecl*, iris::String_hash, iris::String_equal> enum_declarations;
        std::pmr::unordered_map<std::pmr::string, clang::RecordDecl*, iris::String_hash, iris::String_equal> struct_declarations;
        std::pmr::unordered_map<std::pmr::string, clang::RecordDecl*, iris::String_hash, iris::String_equal> union_declarations;
    };

    export struct Clang_declaration_database
    {
        std::pmr::unordered_map<std::pmr::string, Clang_module_declarations, iris::String_hash, iris::String_equal> map;
    };

    export struct Clang_context
    {
        clang::ASTContext& ast_context;
        std::unique_ptr<clang::CodeGenerator> code_generator;
    };

    export struct Clang_module_data
    {
        clang::ASTContext& ast_context;
        std::unique_ptr<clang::CodeGenerator> code_generator;
        Clang_declaration_database declaration_database;
    };
}
