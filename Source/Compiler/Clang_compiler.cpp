module;

#include <stdio.h>

module iris.compiler.clang_compiler;

import std;
import llvm;
import clang;

import iris.common;
import iris.common.filesystem;
import iris.compiler.clang_data;

namespace iris::compiler
{
    std::filesystem::path find_clang(bool const use_clang_cl)
    {
        std::filesystem::path const local_path{use_clang_cl ? "clang-cl.exe" : "clang++"};
        std::filesystem::path const absolute_path = iris::common::get_executable_directory() / local_path;
        if (!std::filesystem::exists(absolute_path))
            iris::common::print_message_and_exit(std::format("Could not find clang in '{}'", absolute_path.generic_string()));

        return absolute_path;
    }

    static void add_argument(std::pmr::vector<std::pmr::string>& arguments, std::string_view const& value, bool const use_clang_cl)
    {
        if (use_clang_cl)
        {
            std::pmr::string const prefix = "/clang:";
            arguments.push_back(prefix + std::pmr::string{value});
        }
        else
        {
            arguments.push_back(std::pmr::string{value});
        }
    }

    static void add_include_directory_argument(std::pmr::vector<std::pmr::string>& arguments, std::string_view const& value, bool const use_clang_cl)
    {
        if (use_clang_cl)
        {
            arguments.push_back("/clang:-I" + std::pmr::string{value});
        }
        else
        {
            arguments.push_back("-I");
            arguments.push_back(std::pmr::string{value});
        }
    }

    static void add_output_argument(std::pmr::vector<std::pmr::string>& arguments, std::string_view const& value, bool const use_clang_cl)
    {
        if (use_clang_cl)
        {
            arguments.push_back(std::pmr::string{"/clang:-o"} + std::pmr::string{value});
        }
        else
        {
            arguments.push_back("-o");
            arguments.push_back(std::pmr::string{value});
        }
    }

    static std::pmr::vector<char const*> to_c_string_vector(std::span<std::pmr::string const> const values)
    {
        std::pmr::vector<char const*> output;
        output.reserve(values.size());

        for (std::pmr::string const& value : values)
        {
            output.push_back(value.c_str());
        }

        return output;
    }

    std::pmr::vector<std::pmr::string> create_compile_cpp_arguments(
        std::filesystem::path const& clang_path,
        std::filesystem::path const& source_file_path,
        std::filesystem::path const& output_file_path,
        std::optional<std::filesystem::path> const output_dependency_file_path,
        std::filesystem::path const& build_artifacts_directory,
        std::span<std::pmr::string const> const include_directories,
        std::span<std::pmr::string const> const additional_flags,
        bool const use_clang_cl,
        bool const debug,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::string const source_file_path_string = source_file_path.generic_string();
        std::string const output_file_path_string = output_file_path.generic_string();

        std::pmr::vector<std::pmr::string> arguments{output_allocator};
        arguments.reserve(10 + include_directories.size() + additional_flags.size());

        arguments.push_back(std::pmr::string{clang_path.generic_string()});

        std::filesystem::path const builtin_include_directory = iris::common::get_builtin_include_directory();
        add_include_directory_argument(arguments, builtin_include_directory.generic_string(), use_clang_cl);

        std::filesystem::path const build_directory = build_artifacts_directory.parent_path();
        std::filesystem::path const generated_headers_directory = std::filesystem::weakly_canonical(build_directory / "include");
        add_include_directory_argument(arguments, generated_headers_directory.generic_string(), use_clang_cl);

        for (std::pmr::string const& include_directory : include_directories)
        {
            add_include_directory_argument(arguments, include_directory, use_clang_cl);
        }

        for (std::pmr::string const& additional_flag : additional_flags)
            add_argument(arguments, additional_flag, use_clang_cl);

        add_output_argument(arguments, output_file_path_string, use_clang_cl);

        if (debug)
            add_argument(arguments, "-g", use_clang_cl);

        if (use_clang_cl)
        {
            if (debug)
                arguments.push_back("/MDd");
            else
                arguments.push_back("/MD");
        }

        if (output_dependency_file_path.has_value())
        {
            add_argument(arguments, "-MMD", use_clang_cl);
            std::pmr::string const dependency_file_argument = std::pmr::string{"-MF"} + std::pmr::string{output_dependency_file_path->generic_string()};
            add_argument(arguments, dependency_file_argument, use_clang_cl);
        }

        if (output_file_path_string.ends_with(".bc"))
        {
            add_argument(arguments, "-emit-llvm", use_clang_cl);
            add_argument(arguments, "-c", use_clang_cl);
        }
        else if (output_file_path_string.ends_with(".ll"))
        {
            add_argument(arguments, "-emit-llvm", use_clang_cl);
            add_argument(arguments, "-S", use_clang_cl);
        }
        else if (output_file_path_string.ends_with(".o") || output_file_path_string.ends_with(".obj"))
        {
            add_argument(arguments, "-c", use_clang_cl);
        }

        arguments.push_back(source_file_path_string.c_str());

        return arguments;
    }

    bool compile_cpp(
        Clang_data const& clang_data,
        std::string_view const target_triple,
        std::filesystem::path const& source_file_path,
        std::filesystem::path const& output_file_path,
        std::optional<std::filesystem::path> const output_dependency_file_path,
        std::filesystem::path const& build_artifacts_directory,
        std::span<std::pmr::string const> const include_directories,
        std::span<std::pmr::string const> const additional_flags,
        bool const use_clang_cl,
        bool const debug,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        clang::CompilerInstance& clang_compiler_instance = *clang_data.compiler_instance.get();

        llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> diagnostic_ids{new clang::DiagnosticIDs{}};
        llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> diagnostic_options{new clang::DiagnosticOptions{}};
        clang::TextDiagnosticPrinter diagnostic_printer{llvm::outs(), diagnostic_options.get()};
        clang::DiagnosticsEngine diagnostics_engine{diagnostic_ids, diagnostic_options, &diagnostic_printer, false};

        std::filesystem::path const clang_path = find_clang(use_clang_cl);
        
        std::pmr::vector<std::pmr::string> const arguments = create_compile_cpp_arguments(
            clang_path,
            source_file_path,
            output_file_path,
            output_dependency_file_path,
            build_artifacts_directory,
            include_directories,
            additional_flags,
            use_clang_cl,
            debug,
            temporaries_allocator
        );

        std::printf("Compiling \"%s\"\n  Output is \"%s\"\n  Command line: ", source_file_path.generic_string().c_str(), output_file_path.generic_string().c_str());
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            std::fputs(arguments[index].c_str(), stdout);
            std::fputs(" ", stdout);
        }
        std::fputs("\n", stdout);
        std::fflush(stdout);

        std::pmr::vector<char const*> c_string_arguments = to_c_string_vector(arguments);

        clang::driver::Driver clang_driver{clang_path.generic_string(), target_triple, diagnostics_engine};
        clang::driver::Compilation* const compilation = clang_driver.BuildCompilation(c_string_arguments);

        llvm::SmallVector<std::pair<int, const clang::driver::Command *>, 4> failing_commands;
        int const result = clang_driver.ExecuteCompilation(*compilation, failing_commands);
        return result == 0;
    }
}
