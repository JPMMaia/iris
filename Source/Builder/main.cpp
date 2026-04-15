#include <argparse/argparse.hpp>

#include <signal.h>

import std;
import std.compat;

import h.c_header_converter;
import h.common;
import h.compiler;
import h.compiler.builder;
import h.compiler.expressions;
import h.compiler.jit_runner;
import h.compiler.linker;
import h.compiler.repository;
import h.compiler.target;
import h.core;
import h.parser.parser;

std::pmr::vector<std::filesystem::path> convert_to_path(std::span<std::string const> const values)
{
    std::pmr::vector<std::filesystem::path> output;
    output.reserve(values.size());

    for (std::string const& value : values)
    {
        output.push_back(value);
    }

    return output;
}

std::pmr::vector<std::pmr::string> convert_to_vector(std::span<std::string const> const values)
{
    std::pmr::vector<std::pmr::string> output;
    output.reserve(values.size());

    for (std::string const& value : values)
    {
        output.push_back(std::pmr::string{value});
    }

    return output;
}

argparse::Argument& add_artifact_file_argument(argparse::ArgumentParser& command)
{
    return command.add_argument("--artifact-file")
        .help("Path to the artifact file")
        .default_value("iris_artifact.json");
}

argparse::Argument& add_build_directory_argument(argparse::ArgumentParser& command)
{
    return command.add_argument("--build-directory")
        .help("Directory where build artifacts will be written to")
        .default_value("build");
}

argparse::Argument& add_header_search_path_argument(argparse::ArgumentParser& command)
{
    return command.add_argument("--header-search-path")
        .help("Search directories for C header files.")
        .default_value<std::vector<std::string>>({})
        .append();
}

argparse::Argument& add_header_public_prefix_argument(argparse::ArgumentParser& command)
{
    return command.add_argument("--header-public-prefix")
        .help("Set public prefix for the imported C header file. Declarations that begin with the specified prefix will be made public, and the rest will be made private.")
        .default_value<std::vector<std::string>>({})
        .append();
}

argparse::Argument& add_header_remove_prefix_argument(argparse::ArgumentParser& command)
{
    return command.add_argument("--header-remove-prefix")
        .help("Set public prefix for the imported C header file. If a declaration name begins with specified prefix, the prefix will be removed from the name. The unique name stays unchaged.")
        .default_value<std::vector<std::string>>({})
        .append();
}

argparse::Argument& add_module_search_path_argument(argparse::ArgumentParser& command)
{
    return command.add_argument("--module-search-path")
        .help("Search directories for module files.")
        .default_value<std::vector<std::string>>({})
        .append();
}

argparse::Argument& add_repository_argument(argparse::ArgumentParser& command)
{
    return command.add_argument("--repository")
        .help("Specify a repository")
        .default_value<std::vector<std::string>>({})
        .append();
}

argparse::Argument& add_no_debug_argument(argparse::ArgumentParser& command)
{
    return command.add_argument("--no-debug")
        .help("Do not add debug information")
        .flag();
}

argparse::Argument& add_output_llvm_ir_argument(argparse::ArgumentParser& command)
{
    return command.add_argument("--output-llvm-ir")
        .help("Output LLVM-IR to a file with .ll extension")
        .flag();
}

argparse::Argument& add_function_contract_options_argument(argparse::ArgumentParser& command)
{
    return command.add_argument("--function-contracts")
        .help("Specify function contracts behaviour. Possible values are 'disabled', and 'log_error_and_abort'.")
        .default_value("log_error_and_abort");
}

h::compiler::Contract_options get_function_contract_options_argument(argparse::ArgumentParser const& subprogram)
{
    std::string_view const value = subprogram.get<std::string>("--function-contracts");
    if (value == "disabled")
        return h::compiler::Contract_options::Disabled;

    return h::compiler::Contract_options::Log_error_and_abort;
}

argparse::Argument& add_target_triple_argument(argparse::ArgumentParser& command)
{
    return command.add_argument("--target")
        .help("Target triple that identifies the platform.")
        .default_value("default");
}

std::optional<std::string_view> get_target_triple(argparse::ArgumentParser const& subprogram)
{
    std::string const target_triple = subprogram.get<std::string>("--target");
    return (!target_triple.empty() && target_triple != "default") ? std::optional<std::string_view>{target_triple} : std::nullopt;
}

void print_arguments(int const argc, char const* const* argv)
{
    for (int index = 0; index < argc; ++index)
    {
        std::fputc('"', stdout);
        std::fputs(argv[index], stdout);
        std::fputc('"', stdout);
        std::fputc(' ', stdout);
    }
    
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

h::compiler::Compilation_options create_compilation_options(
    h::compiler::Target const& target,
    bool const no_debug,
    h::compiler::Contract_options const contract_options
)
{
    bool const output_debug_code_view = !no_debug && target.operating_system == "windows";

    h::compiler::Compilation_options const compilation_options =
    {
        .target_triple = std::nullopt, // TODO
        .is_optimized = false, // TODO
        .debug = !no_debug,
        .output_debug_code_view = output_debug_code_view,
        .contract_options = contract_options,
    };

    return compilation_options;
}

int main(int const argc, char const* const* argv)
{
    h::common::install_abort_handlers();

    argparse::ArgumentParser program("hlang");

    // hlang build-artifact [--artifact-file=<artifact_file>] [--build-directory=<build_directory>] [--header-search-path=<header_search_path>]... [--repository=<repository_path>]...
    argparse::ArgumentParser build_artifact_command("build-artifact");
    build_artifact_command.add_description("Build an artifact");
    add_artifact_file_argument(build_artifact_command);
    add_build_directory_argument(build_artifact_command);
    add_header_search_path_argument(build_artifact_command);
    add_repository_argument(build_artifact_command);
    add_no_debug_argument(build_artifact_command);
    add_output_llvm_ir_argument(build_artifact_command);
    add_function_contract_options_argument(build_artifact_command);
    program.add_subparser(build_artifact_command);
    
    // hlang build-tests [--artifact-file=<artifact_file>]... [--build-directory=<build_directory>] [--header-search-path=<header-search-path>]... [--repository=<repository_path>]...
    argparse::ArgumentParser build_tests_command("build-tests");
    build_tests_command.add_description("Build one or more artifacts in test mode. If no artifacts are specified the current working directory is searched for iris_artifact.json files.");
    build_tests_command.add_argument("--artifact-file")
        .help("Path to an artifact file")
        .default_value<std::vector<std::string>>({})
        .append();
    add_build_directory_argument(build_tests_command);
    add_header_search_path_argument(build_tests_command);
    add_repository_argument(build_tests_command);
    add_no_debug_argument(build_tests_command);
    add_output_llvm_ir_argument(build_tests_command);
    add_function_contract_options_argument(build_tests_command);
    program.add_subparser(build_tests_command);
    
    // hlang run-with-jit [--artifact-file=<artifact_file>] [--build-directory=<build_directory>] [--header-search-path=<header_search_path>]... [--repository=<repository_path>]...
    argparse::ArgumentParser run_with_jit_command("run-with-jit");
    run_with_jit_command.add_description("Use Just-in-time (JIT) compilation and run the program. Any changes detected during runtime will be applied.");
    add_artifact_file_argument(run_with_jit_command);
    add_build_directory_argument(run_with_jit_command);
    add_header_search_path_argument(run_with_jit_command);
    add_repository_argument(run_with_jit_command);
    add_no_debug_argument(run_with_jit_command);
    add_function_contract_options_argument(run_with_jit_command);
    program.add_subparser(run_with_jit_command);

    // hlang import-c-header <module_name> <header> <output>
    argparse::ArgumentParser import_c_header_command("import-c-header");
    import_c_header_command.add_description("Parse a C header file, convert it into an hlang module and write the result to a file.");
    import_c_header_command.add_argument("module_name")
        .help("Module name of the output hlang module");
    import_c_header_command.add_argument("header")
        .help("C Header file path to import");
    import_c_header_command.add_argument("output")
        .help("Write hlang module to this location");
    add_target_triple_argument(import_c_header_command);
    add_header_search_path_argument(import_c_header_command);
    add_header_public_prefix_argument(import_c_header_command);
    add_header_remove_prefix_argument(import_c_header_command);
    program.add_subparser(import_c_header_command);

    // hlang print-struct-layout <file> <struct_name> [--target=<target_triple>]
    argparse::ArgumentParser print_struct_layout_command("print-struct-layout");
    print_struct_layout_command.add_description("Print a JSON describing the layout of the specified struct.");
    print_struct_layout_command.add_argument("file")
        .help("Path to the core module file that contains the struct");
    print_struct_layout_command.add_argument("struct_name")
        .help("Name of the struct");
    add_target_triple_argument(print_struct_layout_command);
    program.add_subparser(print_struct_layout_command);

    // hlang generate-compile-commands [--artifact-file=<artifact_file>] [--output-file=<output_file>] [--build-directory=<build_directory>] [--header-search-path=<header_search_path>]... [--repository=<repository_path>]...
    argparse::ArgumentParser generate_compile_commands_command("generate-compile-commands");
    generate_compile_commands_command.add_description("Generate compile_commands.json for C++ files.");
    add_artifact_file_argument(generate_compile_commands_command);
    generate_compile_commands_command.add_argument("--output-file")
        .help("Path to the compile_commands.json file")
        .default_value("build/compile_commands.json");
    add_build_directory_argument(generate_compile_commands_command);
    add_header_search_path_argument(generate_compile_commands_command);
    add_repository_argument(generate_compile_commands_command);
    add_target_triple_argument(generate_compile_commands_command);
    program.add_subparser(generate_compile_commands_command);

    try
    {
        program.parse_args(argc, argv);
    }
    catch (std::exception const& error)
    {
        std::cerr << error.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    if (program.is_subcommand_used("build-artifact"))
    {
        print_arguments(argc, argv);

        argparse::ArgumentParser const& subprogram = program.at<argparse::ArgumentParser>("build-artifact");

        std::filesystem::path const artifact_file_path = subprogram.get<std::string>("--artifact-file");
        std::filesystem::path const build_directory_path = subprogram.get<std::string>("--build-directory");
        std::pmr::vector<std::filesystem::path> const header_search_paths = convert_to_path(subprogram.get<std::vector<std::string>>("--header-search-path"));
        std::pmr::vector<std::filesystem::path> const repository_paths = convert_to_path(subprogram.get<std::vector<std::string>>("--repository"));
        bool const no_debug = subprogram.get<bool>("--no-debug");
        h::compiler::Contract_options const contract_options = get_function_contract_options_argument(subprogram);

        h::compiler::Target const target = h::compiler::get_default_target();
        h::compiler::Compilation_options const compilation_options = create_compilation_options(target, no_debug, contract_options);

        h::compiler::Builder_options const builder_options =
        {
            .output_llvm_ir = subprogram.get<bool>("--output-llvm-ir"),
        };

        h::compiler::Builder builder = h::compiler::create_builder(
            target,
            build_directory_path,
            header_search_paths,
            repository_paths,
            compilation_options,
            builder_options,
            {}
        );

        try
        {
            h::compiler::build_artifact(builder, artifact_file_path);
        }
        catch (std::exception const& error)
        {
            std::cerr << error.what() << std::endl;
            std::cerr << program;
            std::exit(1);
        }
    }
    else if (program.is_subcommand_used("build-tests"))
    {
        print_arguments(argc, argv);

        argparse::ArgumentParser const& subprogram = program.at<argparse::ArgumentParser>("build-tests");

        std::vector<std::string> const artifact_strings = subprogram.get<std::vector<std::string>>("--artifact-file");
        std::filesystem::path const build_directory_path = subprogram.get<std::string>("--build-directory");
        std::pmr::vector<std::filesystem::path> const header_search_paths = convert_to_path(subprogram.get<std::vector<std::string>>("--header-search-path"));
        std::pmr::vector<std::filesystem::path> repository_paths = convert_to_path(subprogram.get<std::vector<std::string>>("--repository"));
        bool const no_debug = subprogram.get<bool>("--no-debug");
        h::compiler::Contract_options const contract_options = get_function_contract_options_argument(subprogram);

        h::compiler::Target const target = h::compiler::get_default_target();
        h::compiler::Compilation_options const compilation_options = create_compilation_options(target, no_debug, contract_options);

        h::compiler::Builder_options const builder_options =
        {
            .output_llvm_ir = subprogram.get<bool>("--output-llvm-ir"),
            .is_test_mode = true,
        };

        h::compiler::Builder builder = h::compiler::create_builder(
            target,
            build_directory_path,
            header_search_paths,
            repository_paths,
            compilation_options,
            builder_options,
            {}
        );

        std::pmr::vector<std::filesystem::path> artifact_paths =
            artifact_strings.empty() ?
            h::compiler::find_artifact_file_paths(std::filesystem::current_path(), {}, {}) :
            convert_to_path(artifact_strings);

        try
        {
            h::compiler::build_artifacts(builder, artifact_paths);
        }
        catch (std::exception const& error)
        {
            std::cerr << error.what() << std::endl;
            std::cerr << program;
            std::exit(1);
        }
    }
    else if (program.is_subcommand_used("run-with-jit"))
    {
        print_arguments(argc, argv);

        argparse::ArgumentParser const& subprogram = program.at<argparse::ArgumentParser>("run-with-jit");

        std::filesystem::path const artifact_file_path = subprogram.get<std::string>("--artifact-file");
        std::filesystem::path const build_directory_path = subprogram.get<std::string>("--build-directory");
        std::pmr::vector<std::filesystem::path> const header_search_paths = convert_to_path(subprogram.get<std::vector<std::string>>("--header-search-path"));
        std::pmr::vector<std::filesystem::path> const repository_paths = convert_to_path(subprogram.get<std::vector<std::string>>("--repository"));
        std::pmr::vector<h::compiler::Repository> const repositories = h::compiler::get_repositories(repository_paths);
        bool const no_debug = subprogram.get<bool>("--no-debug");
        h::compiler::Contract_options const contract_options = get_function_contract_options_argument(subprogram);

        h::compiler::Target const target = h::compiler::get_default_target();
        h::compiler::Compilation_options const compilation_options = create_compilation_options(target, no_debug, contract_options);

        std::unique_ptr<h::compiler::JIT_runner> const jit_runner = h::compiler::setup_jit_and_watch(artifact_file_path, repository_paths, build_directory_path, header_search_paths, target, compilation_options);

        void(*function_pointer)() = h::compiler::get_entry_point_function<void(*)()>(*jit_runner, artifact_file_path);
        if (function_pointer == nullptr)
        {
            std::cerr << std::format("Could not find entry point of artifact '{}'\n", artifact_file_path.generic_string());
            return -1;
        }

        function_pointer();
    }
    else if (program.is_subcommand_used("import-c-header"))
    {
        print_arguments(argc, argv);

        argparse::ArgumentParser const& subprogram = program.at<argparse::ArgumentParser>("import-c-header");

        std::string const module_name = subprogram.get<std::string>("module_name");
        std::filesystem::path const input_file_path = subprogram.get<std::string>("header");
        std::filesystem::path const output_file_path = subprogram.get<std::string>("output");
        std::optional<std::string_view> const target_triple = get_target_triple(subprogram);
        std::pmr::vector<std::filesystem::path> const header_search_paths = convert_to_path(subprogram.get<std::vector<std::string>>("--header-search-path"));
        std::pmr::vector<std::pmr::string> const public_prefixes = convert_to_vector(subprogram.get<std::vector<std::string>>("--header-public-prefix"));
        std::pmr::vector<std::pmr::string> const remove_prefixes = convert_to_vector(subprogram.get<std::vector<std::string>>("--header-remove-prefix"));

        h::c::Options const options
        {
            .target_triple = target_triple,
            .include_directories = header_search_paths,
            .public_prefixes = public_prefixes,
            .remove_prefixes = remove_prefixes,
        };

        std::optional<h::Module> const header_module = h::c::import_header_and_write_to_file(module_name, input_file_path, output_file_path, options);
        if (!header_module.has_value())
            return -1;
    }
    else if (program.is_subcommand_used("print-struct-layout"))
    {
        argparse::ArgumentParser const& subprogram = program.at<argparse::ArgumentParser>("print-struct-layout");

        std::filesystem::path const input_file_path = subprogram.get<std::string>("file");
        std::string const struct_name = subprogram.get<std::string>("struct_name");
        std::optional<std::string_view> const target_triple = get_target_triple(subprogram);

        h::compiler::print_struct_layout(
            input_file_path,
            struct_name,
            target_triple
        );
    }
    else if (program.is_subcommand_used("generate-compile-commands"))
    {
        argparse::ArgumentParser const& subprogram = program.at<argparse::ArgumentParser>("generate-compile-commands");

        std::filesystem::path const artifact_file_path = subprogram.get<std::string>("--artifact-file");
        std::filesystem::path const output_file_path = subprogram.get<std::string>("--output-file");
        std::filesystem::path const build_directory_path = subprogram.get<std::string>("--build-directory");
        std::pmr::vector<std::filesystem::path> const header_search_paths = convert_to_path(subprogram.get<std::vector<std::string>>("--header-search-path"));
        std::pmr::vector<std::filesystem::path> const repository_paths = convert_to_path(subprogram.get<std::vector<std::string>>("--repository"));

        h::compiler::Target const target = h::compiler::get_default_target();
        h::compiler::Compilation_options const compilation_options = create_compilation_options(target, false, h::compiler::Contract_options::Log_error_and_abort);

        h::compiler::Builder_options const builder_options =
        {
            .output_llvm_ir = false,
        };

        h::compiler::Builder builder = h::compiler::create_builder(
            target,
            build_directory_path,
            header_search_paths,
            repository_paths,
            compilation_options,
            builder_options,
            {}
        );

        h::compiler::write_compile_commands_json_to_file(
            builder,
            artifact_file_path,
            compilation_options,
            output_file_path
        );
    }

    return 0;
}
