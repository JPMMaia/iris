module;

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclBase.h>
#include <clang/AST/Type.h>
#include <clang/Basic/Builtins.h>
#include <clang/Basic/CodeGenOptions.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/IdentifierTable.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/CodeGen/CodeGenABITypes.h>
#include <clang/CodeGen/CGFunctionInfo.h>
#include <clang/CodeGen/ModuleBuilder.h>
#include "clang/Frontend/CompilerInstance.h"
#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/VirtualFileSystem.h>

#include <memory>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

export module h.compiler.clang_data;

import h.core;
import h.core.declarations;
import h.core.hash;
import h.core.string_hash;

namespace h::compiler
{    
    export struct Clang_data
    {
        std::unique_ptr<clang::CompilerInstance> compiler_instance;
    };

    export struct Clang_module_declarations
    {
        std::pmr::unordered_map<std::pmr::string, clang::FunctionDecl*, h::String_hash, h::String_equal> function_declarations;
        std::pmr::unordered_map<std::pmr::string, clang::TypedefDecl*, h::String_hash, h::String_equal> alias_type_declarations;
        std::pmr::unordered_map<std::pmr::string, clang::EnumDecl*, h::String_hash, h::String_equal> enum_declarations;
        std::pmr::unordered_map<std::pmr::string, clang::RecordDecl*, h::String_hash, h::String_equal> struct_declarations;
        std::pmr::unordered_map<std::pmr::string, clang::RecordDecl*, h::String_hash, h::String_equal> union_declarations;
    };

    export struct Clang_declaration_database
    {
        std::pmr::unordered_map<std::pmr::string, Clang_module_declarations, h::String_hash, h::String_equal> map;
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
