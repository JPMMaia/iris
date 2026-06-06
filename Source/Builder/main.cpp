#include <argparse/argparse.hpp>

#include <signal.h>

import std;
import std.compat;

import iris.c_header_converter;
import iris.common;
import iris.common.filesystem_common;
import iris.compiler;
import iris.compiler.artifact;
import iris.compiler.builder;
import iris.compiler.project;
import iris.compiler.expressions;
import iris.compiler.linker;
import iris.compiler.presets;
import iris.compiler.repository;
import iris.compiler.target;
import iris.core;
import iris.parser.parser;

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

argparse::Argument& add_no_bounds_checks_argument(argparse::ArgumentParser& command)
{
    return command.add_argument("--no-bounds-checks")
        .help("Disable bounds checks")
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

iris::compiler::Contract_options get_function_contract_options_argument(argparse::ArgumentParser const& subprogram)
{
    std::string_view const value = subprogram.get<std::string>("--function-contracts");
    if (value == "disabled")
        return iris::compiler::Contract_options::Disabled;

    return iris::compiler::Contract_options::Log_error_and_abort;
}

static constexpr std::string_view g_default_build_directory = "build";
static constexpr std::string_view g_default_function_contracts = "log_error_and_abort";

std::optional<iris::compiler::Presets> get_local_presets()
{
    std::filesystem::path const presets_file_path = std::filesystem::current_path() / "iris_presets.json";
    return iris::compiler::try_get_presets(presets_file_path);
}

iris::compiler::Environment_variables get_effective_environment_variables(
    std::optional<iris::compiler::Presets> const& presets
)
{
    if (presets.has_value())
        return presets->environment_variables;

    return {};
}

std::pmr::vector<std::filesystem::path> deduplicate_paths(
    std::span<std::filesystem::path const> const paths
)
{
    std::pmr::vector<std::filesystem::path> merged_paths;
    std::pmr::set<std::pmr::string> seen_paths;

    auto append_unique = [&](std::span<std::filesystem::path const> const source)
    {
        for (std::filesystem::path const& path : source)
        {
            std::filesystem::path const normalized = path.lexically_normal();
            std::pmr::string key = std::pmr::string{ normalized.generic_string() };
            if (seen_paths.contains(key))
                continue;

            seen_paths.insert(std::move(key));
            merged_paths.push_back(normalized);
        }
    };

    append_unique(paths);

    return merged_paths;
}

std::pmr::vector<std::filesystem::path> merge_paths_with_dedup(
    std::span<std::filesystem::path const> const presets_paths,
    std::span<std::filesystem::path const> const command_paths
)
{
    std::pmr::vector<std::filesystem::path> merged_paths;
    std::pmr::set<std::pmr::string> seen_paths;

    auto append_unique = [&](std::span<std::filesystem::path const> const source)
    {
        for (std::filesystem::path const& path : source)
        {
            std::filesystem::path const normalized = path.lexically_normal();
            std::pmr::string key = std::pmr::string{ normalized.generic_string() };
            if (seen_paths.contains(key))
                continue;

            seen_paths.insert(std::move(key));
            merged_paths.push_back(normalized);
        }
    };

    append_unique(presets_paths);
    append_unique(command_paths);

    return merged_paths;
}

std::filesystem::path get_effective_build_directory_argument(
    argparse::ArgumentParser const& subprogram,
    std::optional<iris::compiler::Presets> const& presets
)
{
    std::filesystem::path const build_directory = subprogram.get<std::string>("--build-directory");
    if (
        std::string_view{ build_directory.generic_string() } == g_default_build_directory &&
        presets.has_value() &&
        presets->build_directory_path.has_value()
    )
    {
        return presets->build_directory_path.value();
    }

    return build_directory;
}

std::pmr::vector<std::filesystem::path> get_effective_header_search_paths_argument(
    argparse::ArgumentParser const& subprogram,
    std::optional<iris::compiler::Presets> const& presets
)
{
    std::pmr::vector<std::filesystem::path> const command_paths = convert_to_path(subprogram.get<std::vector<std::string>>("--header-search-path"));
    if (!presets.has_value())
        return command_paths;

    return merge_paths_with_dedup(presets->header_search_paths, command_paths);
}

std::pmr::vector<std::filesystem::path> get_effective_repository_paths_argument(
    argparse::ArgumentParser const& subprogram,
    std::optional<iris::compiler::Presets> const& presets
)
{
    std::pmr::vector<std::filesystem::path> repository_paths = convert_to_path(subprogram.get<std::vector<std::string>>("--repository"));
    repository_paths.push_back(iris::common::get_standard_repository_file_path());

    if (presets.has_value())
        repository_paths.insert(repository_paths.end(), presets->repository_paths.begin(), presets->repository_paths.end());

    return deduplicate_paths(repository_paths);
}

iris::compiler::Contract_options get_effective_function_contract_options_argument(
    argparse::ArgumentParser const& subprogram,
    std::optional<iris::compiler::Presets> const& presets
)
{
    std::string const command_value = subprogram.get<std::string>("--function-contracts");
    if (command_value != g_default_function_contracts)
        return get_function_contract_options_argument(subprogram);

    if (presets.has_value() && presets->function_contract_options.has_value())
        return presets->function_contract_options.value();

    return get_function_contract_options_argument(subprogram);
}

bool get_effective_output_llvm_ir_argument(
    argparse::ArgumentParser const& subprogram,
    std::optional<iris::compiler::Presets> const& presets
)
{
    bool const command_value = subprogram.get<bool>("--output-llvm-ir");
    if (command_value)
        return true;

    if (presets.has_value() && presets->output_llvm_ir.has_value())
        return presets->output_llvm_ir.value();

    return false;
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

iris::compiler::Compilation_options create_compilation_options(
    iris::compiler::Target const& target,
    bool const no_debug,
    iris::compiler::Contract_options const contract_options,
    bool const no_bounds_checks
)
{
    bool const output_debug_code_view = !no_debug && target.operating_system == "windows";

    iris::compiler::Compilation_options const compilation_options =
    {
        .target_triple = std::nullopt, // TODO
        .is_optimized = false, // TODO
        .debug = !no_debug,
        .output_debug_code_view = output_debug_code_view,
        .contract_options = contract_options,
        .enable_bounds_checks = !no_bounds_checks,
    };

    return compilation_options;
}

std::pmr::vector<std::filesystem::path> find_discovered_artifact_paths()
{
    return iris::compiler::find_artifact_file_paths(std::filesystem::current_path(), {}, {});
}

std::pmr::vector<std::filesystem::path> select_artifact_paths(
    std::optional<std::string> const& artifact_name,
    iris::compiler::Environment_variables const& environment_variables
)
{
    std::pmr::vector<std::filesystem::path> const discovered_paths = find_discovered_artifact_paths();
    if (discovered_paths.empty())
        throw std::runtime_error("Could not find any iris_artifact.json files in the current directory tree.");

    if (!artifact_name.has_value())
        return discovered_paths;

    std::pmr::vector<std::filesystem::path> selected_paths;
    for (std::filesystem::path const& artifact_path : discovered_paths)
    {
        iris::compiler::Artifact const artifact = iris::compiler::get_artifact(artifact_path, environment_variables);
        if (std::string_view{artifact.name} == std::string_view{artifact_name.value()})
            selected_paths.push_back(artifact_path);
    }

    if (selected_paths.empty())
    {
        throw std::runtime_error(std::format("Could not find artifact named '{}'.", artifact_name.value()));
    }

    if (selected_paths.size() > 1)
    {
        std::stringstream stream;
        stream << std::format("Found more than one artifact named '{}':", artifact_name.value()) << '\n';
        for (std::filesystem::path const& selected_path : selected_paths)
            stream << std::format("- {}", selected_path.generic_string()) << '\n';

        throw std::runtime_error(stream.str());
    }

    return selected_paths;
}

std::filesystem::path get_artifact_output_path(
    iris::compiler::Artifact const& artifact,
    std::filesystem::path const& build_directory_path,
    iris::compiler::Target const& target
)
{
    if (!iris::compiler::contains_any_compilable_source(artifact))
        return {};

    if (artifact.type == iris::compiler::Artifact_type::Library)
    {
        std::filesystem::path output_path = build_directory_path / "lib" / artifact.name;
        output_path += (target.operating_system == "windows") ? ".lib" : ".a";
        return output_path;
    }

    if (artifact.info.has_value() && std::holds_alternative<iris::compiler::Executable_info>(*artifact.info))
    {
        std::filesystem::path output_path = build_directory_path / "bin" / artifact.name;
        if (target.operating_system == "windows")
            output_path += ".exe";
        return output_path;
    }

    return {};
}

std::filesystem::path get_test_output_path(
    iris::compiler::Artifact const& artifact,
    std::filesystem::path const& build_directory_path,
    iris::compiler::Target const& target
)
{
    std::filesystem::path output_path = build_directory_path / "bin" / (artifact.name + ".iris.test");
    if (target.operating_system == "windows")
        output_path += ".exe";

    return output_path;
}

int run_test_executables(
    std::span<std::filesystem::path const> const artifact_paths,
    std::filesystem::path const& build_directory_path,
    iris::compiler::Target const& target,
    iris::compiler::Environment_variables const& environment_variables
)
{
    std::pmr::set<std::pmr::string> seen_artifact_names;
    int failed_count = 0;
    int executed_count = 0;

    for (std::filesystem::path const& artifact_path : artifact_paths)
    {
        iris::compiler::Artifact const artifact = iris::compiler::get_artifact(artifact_path, environment_variables);

        if (seen_artifact_names.contains(artifact.name))
            continue;

        seen_artifact_names.insert(artifact.name);

        std::filesystem::path const test_output_path = get_test_output_path(artifact, build_directory_path, target);
        if (!std::filesystem::exists(test_output_path))
            continue;

        ++executed_count;
        std::string const command = std::format("\"{}\"", test_output_path.generic_string());
        std::printf("Running tests '%s'\n", test_output_path.generic_string().c_str());
        int const exit_code = std::system(command.c_str());
        if (exit_code != 0)
            ++failed_count;
    }

    if (executed_count == 0)
    {
        std::puts("No test executables were generated for the selected artifacts.");
    }

    return failed_count;
}

int main(int const argc, char const* const* argv)
{
    iris::common::install_abort_handlers();

    argparse::ArgumentParser program("iris");

    // iris build [artifact_name] [--build-directory=<build_directory>] [--header-search-path=<header_search_path>]... [--repository=<repository_path>]...
    argparse::ArgumentParser build_command("build");
    build_command.add_description("Build one or more artifacts. If no artifact name is provided all artifacts are discovered recursively.");
    build_command.add_argument("artifact_name")
        .help("Optional artifact name")
        .nargs(argparse::nargs_pattern::optional);
    add_build_directory_argument(build_command);
    add_header_search_path_argument(build_command);
    add_repository_argument(build_command);
    add_no_debug_argument(build_command);
    add_no_bounds_checks_argument(build_command);
    add_output_llvm_ir_argument(build_command);
    add_function_contract_options_argument(build_command);
    program.add_subparser(build_command);
    
    // iris build-tests [artifact_name] [--build-directory=<build_directory>] [--header-search-path=<header-search-path>]... [--repository=<repository_path>]...
    argparse::ArgumentParser build_tests_command("build-tests");
    build_tests_command.add_description("Build one or more artifacts in test mode. If no artifact name is provided all artifacts are discovered recursively.");
    build_tests_command.add_argument("artifact_name")
        .help("Optional artifact name")
        .nargs(argparse::nargs_pattern::optional);
    add_build_directory_argument(build_tests_command);
    add_header_search_path_argument(build_tests_command);
    add_repository_argument(build_tests_command);
    add_no_debug_argument(build_tests_command);
    add_no_bounds_checks_argument(build_tests_command);
    add_output_llvm_ir_argument(build_tests_command);
    add_function_contract_options_argument(build_tests_command);
    program.add_subparser(build_tests_command);

    // iris test [artifact_name] [--build-directory=<build_directory>] [--header-search-path=<header-search-path>]... [--repository=<repository_path>]...
    argparse::ArgumentParser test_command("test");
    test_command.add_description("Build tests and execute test binaries. If no artifact name is provided all artifacts are discovered recursively.");
    test_command.add_argument("artifact_name")
        .help("Optional artifact name")
        .nargs(argparse::nargs_pattern::optional);
    add_build_directory_argument(test_command);
    add_header_search_path_argument(test_command);
    add_repository_argument(test_command);
    add_no_debug_argument(test_command);
    add_no_bounds_checks_argument(test_command);
    add_output_llvm_ir_argument(test_command);
    add_function_contract_options_argument(test_command);
    program.add_subparser(test_command);

    // iris list [--build-directory=<build_directory>]
    argparse::ArgumentParser list_command("list");
    list_command.add_description("List all discovered artifacts and the output path where each will be built.");
    add_build_directory_argument(list_command);
    program.add_subparser(list_command);

    // iris import-c-header <module_name> <header> <output>
    argparse::ArgumentParser import_c_header_command("import-c-header");
    import_c_header_command.add_description("Parse a C header file, convert it into an iris module and write the result to a file.");
    import_c_header_command.add_argument("module_name")
        .help("Module name of the output iris module");
    import_c_header_command.add_argument("header")
        .help("C Header file path to import");
    import_c_header_command.add_argument("output")
        .help("Write iris module to this location");
    add_target_triple_argument(import_c_header_command);
    add_header_search_path_argument(import_c_header_command);
    add_header_public_prefix_argument(import_c_header_command);
    add_header_remove_prefix_argument(import_c_header_command);
    program.add_subparser(import_c_header_command);

    // iris print-struct-layout <file> <struct_name> [--target=<target_triple>]
    argparse::ArgumentParser print_struct_layout_command("print-struct-layout");
    print_struct_layout_command.add_description("Print a JSON describing the layout of the specified struct.");
    print_struct_layout_command.add_argument("file")
        .help("Path to the core module file that contains the struct");
    print_struct_layout_command.add_argument("struct_name")
        .help("Name of the struct");
    add_target_triple_argument(print_struct_layout_command);
    program.add_subparser(print_struct_layout_command);

    // iris generate-compile-commands [--artifact-file=<artifact_file>] [--output-file=<output_file>] [--build-directory=<build_directory>] [--header-search-path=<header_search_path>]... [--repository=<repository_path>]...
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

    // iris download-dependencies [--project=<project_file>] [--target=<dep_name>]
    argparse::ArgumentParser download_deps_command("download-dependencies");
    download_deps_command.add_description("Download dependency source archives.");
    download_deps_command.add_argument("--project")
        .help("Path to iris_project.json")
        .default_value("iris_project.json");
    download_deps_command.add_argument("--target")
        .help("Download only this dependency (repeatable)")
        .default_value<std::vector<std::string>>({})
        .append();
    program.add_subparser(download_deps_command);

    // iris build-dependencies [--project=<project_file>] [--target=<dep_name>]
    argparse::ArgumentParser build_deps_command("build-dependencies");
    build_deps_command.add_description("Build dependencies.");
    build_deps_command.add_argument("--project")
        .help("Path to iris_project.json")
        .default_value("iris_project.json");
    build_deps_command.add_argument("--target")
        .help("Build only this dependency (repeatable)")
        .default_value<std::vector<std::string>>({})
        .append();
    program.add_subparser(build_deps_command);

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

    if (program.is_subcommand_used("build"))
    {
        print_arguments(argc, argv);

        argparse::ArgumentParser const& subprogram = program.at<argparse::ArgumentParser>("build");
        std::optional<iris::compiler::Presets> const presets = get_local_presets();
        iris::compiler::Environment_variables const environment_variables = get_effective_environment_variables(presets);

        std::filesystem::path const build_directory_path = get_effective_build_directory_argument(subprogram, presets);
        std::pmr::vector<std::filesystem::path> const header_search_paths = get_effective_header_search_paths_argument(subprogram, presets);
        std::pmr::vector<std::filesystem::path> const repository_paths = get_effective_repository_paths_argument(subprogram, presets);
        bool const no_debug = subprogram.get<bool>("--no-debug");
        bool const no_bounds_checks = subprogram.get<bool>("--no-bounds-checks");
        iris::compiler::Contract_options const contract_options = get_effective_function_contract_options_argument(subprogram, presets);

        iris::compiler::Target const target = iris::compiler::get_default_target();
        iris::compiler::Compilation_options const compilation_options = create_compilation_options(target, no_debug, contract_options, no_bounds_checks);

        iris::compiler::Builder_options const builder_options =
        {
            .output_llvm_ir = get_effective_output_llvm_ir_argument(subprogram, presets),
            .environment_variables = environment_variables,
        };

        iris::compiler::Builder builder = iris::compiler::create_builder(
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
            std::optional<std::string> const artifact_name = subprogram.present<std::string>("artifact_name");
            std::pmr::vector<std::filesystem::path> const artifact_paths = select_artifact_paths(artifact_name, environment_variables);
            iris::compiler::build_artifacts(builder, artifact_paths);
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
        std::optional<iris::compiler::Presets> const presets = get_local_presets();
        iris::compiler::Environment_variables const environment_variables = get_effective_environment_variables(presets);

        std::filesystem::path const build_directory_path = get_effective_build_directory_argument(subprogram, presets);
        std::pmr::vector<std::filesystem::path> const header_search_paths = get_effective_header_search_paths_argument(subprogram, presets);
        std::pmr::vector<std::filesystem::path> repository_paths = get_effective_repository_paths_argument(subprogram, presets);
        bool const no_debug = subprogram.get<bool>("--no-debug");
        bool const no_bounds_checks = subprogram.get<bool>("--no-bounds-checks");
        iris::compiler::Contract_options const contract_options = get_effective_function_contract_options_argument(subprogram, presets);

        iris::compiler::Target const target = iris::compiler::get_default_target();
        iris::compiler::Compilation_options const compilation_options = create_compilation_options(target, no_debug, contract_options, no_bounds_checks);

        iris::compiler::Builder_options const builder_options =
        {
            .output_llvm_ir = get_effective_output_llvm_ir_argument(subprogram, presets),
            .is_test_mode = true,
            .environment_variables = environment_variables,
        };

        iris::compiler::Builder builder = iris::compiler::create_builder(
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
            std::optional<std::string> const artifact_name = subprogram.present<std::string>("artifact_name");
            std::pmr::vector<std::filesystem::path> const artifact_paths = select_artifact_paths(artifact_name, environment_variables);
            iris::compiler::build_artifacts(builder, artifact_paths);
        }
        catch (std::exception const& error)
        {
            std::cerr << error.what() << std::endl;
            std::cerr << program;
            std::exit(1);
        }
    }
    else if (program.is_subcommand_used("test"))
    {
        print_arguments(argc, argv);

        argparse::ArgumentParser const& subprogram = program.at<argparse::ArgumentParser>("test");
        std::optional<iris::compiler::Presets> const presets = get_local_presets();
        iris::compiler::Environment_variables const environment_variables = get_effective_environment_variables(presets);

        std::filesystem::path const build_directory_path = get_effective_build_directory_argument(subprogram, presets);
        std::pmr::vector<std::filesystem::path> const header_search_paths = get_effective_header_search_paths_argument(subprogram, presets);
        std::pmr::vector<std::filesystem::path> repository_paths = get_effective_repository_paths_argument(subprogram, presets);
        bool const no_debug = subprogram.get<bool>("--no-debug");
        bool const no_bounds_checks = subprogram.get<bool>("--no-bounds-checks");
        iris::compiler::Contract_options const contract_options = get_effective_function_contract_options_argument(subprogram, presets);

        iris::compiler::Target const target = iris::compiler::get_default_target();
        iris::compiler::Compilation_options const compilation_options = create_compilation_options(target, no_debug, contract_options, no_bounds_checks);

        iris::compiler::Builder_options const builder_options =
        {
            .output_llvm_ir = get_effective_output_llvm_ir_argument(subprogram, presets),
            .is_test_mode = true,
            .environment_variables = environment_variables,
        };

        iris::compiler::Builder builder = iris::compiler::create_builder(
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
            std::optional<std::string> const artifact_name = subprogram.present<std::string>("artifact_name");
            std::pmr::vector<std::filesystem::path> const artifact_paths = select_artifact_paths(artifact_name, environment_variables);
            iris::compiler::build_artifacts(builder, artifact_paths);
            int const failed_tests = run_test_executables(artifact_paths, build_directory_path, target, environment_variables);
            if (failed_tests != 0)
            {
                std::cerr << std::format("{} test executable(s) failed.\n", failed_tests);
                return 1;
            }
        }
        catch (std::exception const& error)
        {
            std::cerr << error.what() << std::endl;
            std::cerr << program;
            std::exit(1);
        }
    }
    else if (program.is_subcommand_used("list"))
    {
        print_arguments(argc, argv);

        argparse::ArgumentParser const& subprogram = program.at<argparse::ArgumentParser>("list");

        std::filesystem::path const build_directory_path = subprogram.get<std::string>("--build-directory");
        std::optional<iris::compiler::Presets> const presets = get_local_presets();
        iris::compiler::Environment_variables const environment_variables = get_effective_environment_variables(presets);
        std::pmr::vector<std::filesystem::path> const artifact_paths = find_discovered_artifact_paths();
        if (artifact_paths.empty())
        {
            std::puts("No artifacts found.");
            return 0;
        }

        iris::compiler::Target const target = iris::compiler::get_default_target();

        for (std::filesystem::path const& artifact_path : artifact_paths)
        {
            iris::compiler::Artifact const artifact = iris::compiler::get_artifact(artifact_path, environment_variables);
            std::filesystem::path const output_path = get_artifact_output_path(artifact, build_directory_path, target);

            std::printf("%s -> %s\n", artifact.name.c_str(), output_path.empty() ? "<no build output>" : output_path.generic_string().c_str());
        }
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

        iris::c::Options const options
        {
            .target_triple = target_triple,
            .include_directories = header_search_paths,
            .public_prefixes = public_prefixes,
            .remove_prefixes = remove_prefixes,
        };

        std::optional<iris::Module> const header_module = iris::c::import_header_and_write_to_file(module_name, input_file_path, output_file_path, options);
        if (!header_module.has_value())
            return -1;
    }
    else if (program.is_subcommand_used("print-struct-layout"))
    {
        argparse::ArgumentParser const& subprogram = program.at<argparse::ArgumentParser>("print-struct-layout");

        std::filesystem::path const input_file_path = subprogram.get<std::string>("file");
        std::string const struct_name = subprogram.get<std::string>("struct_name");
        std::optional<std::string_view> const target_triple = get_target_triple(subprogram);

        iris::compiler::print_struct_layout(
            input_file_path,
            struct_name,
            target_triple
        );
    }
    else if (program.is_subcommand_used("generate-compile-commands"))
    {
        argparse::ArgumentParser const& subprogram = program.at<argparse::ArgumentParser>("generate-compile-commands");
        std::optional<iris::compiler::Presets> const presets = get_local_presets();
        iris::compiler::Environment_variables const environment_variables = get_effective_environment_variables(presets);

        std::filesystem::path const artifact_file_path = subprogram.get<std::string>("--artifact-file");
        std::filesystem::path const output_file_path = subprogram.get<std::string>("--output-file");
        std::filesystem::path const build_directory_path = get_effective_build_directory_argument(subprogram, presets);
        std::pmr::vector<std::filesystem::path> const header_search_paths = get_effective_header_search_paths_argument(subprogram, presets);
        std::pmr::vector<std::filesystem::path> const repository_paths = get_effective_repository_paths_argument(subprogram, presets);

        iris::compiler::Target const target = iris::compiler::get_default_target();
        iris::compiler::Compilation_options const compilation_options = create_compilation_options(target, false, iris::compiler::Contract_options::Log_error_and_abort, false);

        iris::compiler::Builder_options const builder_options =
        {
            .output_llvm_ir = false,
            .environment_variables = environment_variables,
        };

        iris::compiler::Builder builder = iris::compiler::create_builder(
            target,
            build_directory_path,
            header_search_paths,
            repository_paths,
            compilation_options,
            builder_options,
            {}
        );

        iris::compiler::write_compile_commands_json_to_file(
            builder,
            artifact_file_path,
            compilation_options,
            output_file_path
        );
    }
    else if (program.is_subcommand_used("download-dependencies"))
    {
        print_arguments(argc, argv);

        argparse::ArgumentParser const& subprogram = program.at<argparse::ArgumentParser>("download-dependencies");
        std::filesystem::path const project_path = subprogram.get<std::string>("--project");

        iris::compiler::Iris_project const project = iris::compiler::get_iris_project(project_path);

        std::vector<std::string> const targets = subprogram.get<std::vector<std::string>>("--target");
        if (!targets.empty())
        {
            std::vector<iris::compiler::Project_dependency> filtered_deps;
            for (std::string_view const target_name : targets)
            {
                auto const it = std::find_if(project.dependencies.begin(), project.dependencies.end(),
                    [&target_name](iris::compiler::Project_dependency const& dep) { return dep.name == target_name; });
                if (it != project.dependencies.end())
                    filtered_deps.push_back(*it);
            }
            iris::compiler::download_dependencies(project, std::span<iris::compiler::Project_dependency const>{filtered_deps});
        }
        else
        {
            iris::compiler::download_dependencies(project);
        }
    }
    else if (program.is_subcommand_used("build-dependencies"))
    {
        print_arguments(argc, argv);

        argparse::ArgumentParser const& subprogram = program.at<argparse::ArgumentParser>("build-dependencies");
        std::filesystem::path const project_path = subprogram.get<std::string>("--project");

        iris::compiler::Iris_project const project = iris::compiler::get_iris_project(project_path);

        std::vector<std::string> const targets = subprogram.get<std::vector<std::string>>("--target");
        if (!targets.empty())
        {
            std::vector<iris::compiler::Project_dependency> filtered_deps;
            for (std::string_view const target_name : targets)
            {
                auto const it = std::find_if(project.dependencies.begin(), project.dependencies.end(),
                    [&target_name](iris::compiler::Project_dependency const& dep) { return dep.name == target_name; });
                if (it != project.dependencies.end())
                    filtered_deps.push_back(*it);
            }
            iris::compiler::build_dependencies(project, std::span<iris::compiler::Project_dependency const>{filtered_deps});
        }
        else
        {
            iris::compiler::build_dependencies(project);
        }
    }

    return 0;
}
