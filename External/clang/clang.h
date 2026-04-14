#pragma warning(push, 0)

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclBase.h>
#include <clang/AST/Redeclarable.h>
#include <clang/AST/Type.h>

#include <clang/Basic/Builtins.h>
#include <clang/Basic/CodeGenOptions.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticIDs.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/IdentifierTable.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/TargetInfo.h>

#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>

#include <clang/CodeGen/CodeGenABITypes.h>
#include <clang/CodeGen/CGFunctionInfo.h>
#include <clang/CodeGen/ModuleBuilder.h>

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>

#include <clang/lib/CodeGen/CodeGenModule.h>
#include <clang/lib/CodeGen/CodeGenTypes.h>
#include <clang/lib/CodeGen/CGRecordLayout.h>

#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Lex/PreprocessorOptions.h>

#pragma warning(pop)
