export module h.compiler.jit_runner;

import std;
import llvm;

import h.compiler;
import h.compiler.artifact;
import h.compiler.file_watcher;
import h.core.hash;
import h.compiler.jit_compiler;
import h.compiler.repository;
import h.compiler.target;
import h.core;
import h.parser.parser;

namespace h::compiler
{
    struct JIT_runner_unprotected_data
    {
        std::filesystem::path build_directory_path;
        std::pmr::vector<std::filesystem::path> header_search_paths;
        Target target;
        h::parser::Parser parser;
        std::unique_ptr<h::compiler::LLVM_data> llvm_data;
        std::unique_ptr<JIT_data> jit_data;
        int log_level;
        Compilation_options compilation_options;
    };

    struct JIT_runner_protected_data
    {
        std::shared_mutex mutex;
        std::condition_variable_any condition_variable;
        std::uint64_t processed_files = 0;
        std::pmr::unordered_map<std::filesystem::path, Artifact> artifacts;
        std::pmr::unordered_map<std::filesystem::path, Repository> repositories;
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> module_name_to_source_file_path;
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> module_name_to_module_file_path;
        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> module_name_to_reverse_dependencies;
        std::pmr::unordered_map<std::pmr::string, Symbol_name_to_hash> module_name_to_symbol_hashes;
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> module_name_to_artifact_path;
        llvm::DenseMap<llvm::orc::SymbolStringPtr, std::pmr::string> symbol_to_module_name_map;
    };

    export struct JIT_runner
    {
        JIT_runner_unprotected_data unprotected_data;
        JIT_runner_protected_data protected_data;
        std::unique_ptr<File_watcher> file_watcher;

        ~JIT_runner();
    };

    export std::unique_ptr<JIT_runner> setup_jit_and_watch(
        std::filesystem::path const& artifact_configuration_file_path,
        std::span<std::filesystem::path const> repositories_file_paths,
        std::filesystem::path const& build_directory_path,
        std::span<std::filesystem::path const> header_search_paths,
        Target const& target,
        Compilation_options const& compilation_options
    );

    export
        template <typename Function_type>
    Function_type get_function(
        JIT_runner& jit_runner,
        std::string_view const mangled_function_name
    )
    {
        return get_function<Function_type>(*jit_runner.unprotected_data.jit_data, mangled_function_name);
    }

    export
        template <typename Function_type>
    Function_type get_entry_point_function(
        JIT_runner& jit_runner,
        std::filesystem::path const& artifact_configuration_file_path
    )
    {
        std::shared_lock<std::shared_mutex> lock{ jit_runner.protected_data.mutex };
        Artifact const& artifact = jit_runner.protected_data.artifacts[artifact_configuration_file_path];

        if (artifact.info.has_value())
        {
            if (std::holds_alternative<Executable_info>(*artifact.info))
            {
                Executable_info const& executable_info = std::get<Executable_info>(*artifact.info);

                return get_function<Function_type>(*jit_runner.unprotected_data.jit_data, executable_info.entry_point);
            }
        }

        return nullptr;
    }

    export std::uint64_t get_processed_files(
        JIT_runner& jit_runner
    );

    export void wait_for(
        JIT_runner& jit_runner,
        std::uint64_t const processed_files
    );
}
