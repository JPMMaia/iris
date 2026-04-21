export module iris.compiler;

import std;
import llvm;

import iris.core;
import iris.core.declarations;
import iris.compiler.clang_data;
import iris.compiler.diagnostic;
import iris.compiler.expressions;
import iris.compiler.types;

namespace iris::compiler
{
    export struct Optimization_managers
    {
        std::unique_ptr<llvm::LoopAnalysisManager> loop_analysis_manager;
        std::unique_ptr<llvm::FunctionAnalysisManager> function_analysis_manager;
        std::unique_ptr<llvm::CGSCCAnalysisManager> cgscc_analysis_manager;
        std::unique_ptr<llvm::ModuleAnalysisManager> module_analysis_manager;
        llvm::ModulePassManager module_pass_manager;
    };

    export struct LLVM_data
    {
        std::string target_triple;
        llvm::Target const* target;
        llvm::TargetMachine* target_machine;
        llvm::DataLayout data_layout;
        std::unique_ptr<llvm::LLVMContext> context;
        Optimization_managers optimization_managers;
        std::unique_ptr<Clang_data, void(*)(Clang_data*)> clang_data;
    };

    export struct LLVM_module_data
    {
        std::pmr::unordered_map<std::pmr::string, iris::Module> dependencies;
        std::unique_ptr<llvm::Module> module;
    };

    export struct Compilation_options
    {
        std::optional<std::string_view> target_triple;
        bool is_optimized = false;
        bool debug = true;
        bool output_debug_code_view = false;
        Contract_options contract_options = Contract_options::Log_error_and_abort;
        bool is_test_mode = false;
    };

    export std::optional<iris::Module> read_core_module(
        std::filesystem::path const& path
    );

    export std::optional<iris::Module> read_core_module_declarations(
        std::filesystem::path const& path
    );

    export LLVM_data initialize_llvm(
        Compilation_options const& compilation_options
    );

    export std::unique_ptr<llvm::Module> create_llvm_module(
        LLVM_data& llvm_data,
        Module const& core_module,
        std::pmr::unordered_map<std::pmr::string, Module> const& core_module_dependencies,
        std::optional<std::span<std::string_view const>> const functions_to_compile,
        Compilation_options const& compilation_options
    );

    export std::unique_ptr<llvm::Module> create_llvm_module(
        LLVM_data& llvm_data,
        Module const& core_module,
        std::pmr::unordered_map<std::pmr::string, Module> const& core_module_dependencies,
        Compilation_options const& compilation_options
    );

    export LLVM_module_data create_llvm_module(
        LLVM_data& llvm_data,
        Module const& core_module,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& module_name_to_file_path_map,
        Compilation_options const& compilation_options
    );

    export std::pmr::vector<iris::Module const*> sort_core_modules(
        std::span<iris::Module const> const core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<iris::Module const*> sort_core_modules(
        std::pmr::unordered_map<std::pmr::string, iris::Module> const& core_module_dependencies,
        iris::Module const* const core_module,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export Declaration_database create_declaration_database_and_add_modules(
        std::span<iris::Module const> const header_modules,
        std::span<iris::Module const* const> const sorted_core_modules
    );

    export struct Declaration_database_and_sorted_modules
    {
        std::pmr::vector<iris::Module const*> sorted_core_modules;
        Declaration_database declaration_database;
        std::pmr::vector<iris::compiler::Diagnostic> diagnostics;
    };

    export void print_diagnostics_and_exit_if_needed(
        std::span<iris::compiler::Diagnostic const> const diagnostics,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export struct Compilation_database
    {
        Clang_module_data_pointer clang_module_data;
        Type_database type_database;
    };

    export Compilation_database process_modules_and_create_compilation_database(
        LLVM_data& llvm_data,
        Clang_context_pointer&& clang_context,
        std::span<iris::Module const* const> const sorted_modules,
        Declaration_database& declaration_database,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::unique_ptr<llvm::Module> create_llvm_module(
        LLVM_data& llvm_data,
        iris::Module core_module,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& module_name_to_file_path_map,
        Declaration_database declaration_database,
        Compilation_options const& compilation_options
    );

    export void optimize_llvm_module(
        LLVM_data& llvm_data,
        llvm::Module& llvm_module
    );

    export std::string to_string(
        llvm::Module const& llvm_module
    );

    export void write_bitcode_to_file(
        LLVM_data const& llvm_data,
        llvm::Module& llvm_module,
        std::filesystem::path const& output_file_path
    );

    export void write_llvm_ir_to_file(
        llvm::Module& llvm_module,
        std::filesystem::path const& output_file_path
    );

    export void write_object_file(
        LLVM_data const& llvm_data,
        llvm::Module& llvm_module,
        std::filesystem::path const& output_file_path
    );

    export void generate_object_file(
        std::filesystem::path const& output_file_path,
        Module const& core_module,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& module_name_to_file_path_map,
        Compilation_options const& compilation_options
    );

    export void add_import_usages(
        iris::Module& core_module,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );
}
