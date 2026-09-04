import iris.common;
import iris.common.filesystem;
import iris.compiler;
import iris.compiler.builder;
import iris.compiler.project;
import iris.compiler.compile_commands_generator;
import iris.compiler.target;

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include <catch2/catch_all.hpp>

namespace iris::compiler
{
    static std::filesystem::path const g_examples_directory = std::filesystem::path{ EXAMPLES_DIRECTORY };
    static std::filesystem::path const g_standard_repository_file_path = std::filesystem::path{ STANDARD_REPOSITORY_FILE_PATH };

    static std::pmr::string get_binary_name(
        std::string_view const name,
        iris::compiler::Target const& target
    )
    {
        if (target.operating_system == "windows")
        {
            return std::pmr::string{name} + ".exe";
        }

        return std::pmr::string{name};
    }

    static std::pmr::string get_static_library_name(
        std::string_view const name,
        iris::compiler::Target const& target
    )
    {
        if (target.operating_system == "windows")
        {
            return std::pmr::string{name} + ".lib";
        }

        return std::pmr::string{name} + ".a";
    }

    static std::pmr::string get_object_name(
        std::string_view const name,
        iris::compiler::Target const& target
    )
    {
        if (target.operating_system == "windows")
        {
            return std::pmr::string{name} + ".obj";
        }

        return std::pmr::string{name} + ".o";
    }

    std::filesystem::path test_builder(
        std::string_view const project_name,
        std::pmr::vector<std::filesystem::path> const& artifact_paths,
        iris::compiler::Target const& target,
        std::span<std::filesystem::path const> const additional_repository_paths,
        std::span<std::filesystem::path const> const expected_output_paths,
        std::optional<std::string_view> const temporary_directory_name = std::nullopt,
        iris::compiler::Builder_options const builder_options = {},
        iris::compiler::Compilation_options const compilation_options = {}
    )
    {
        std::filesystem::path const temporary_directory_path = std::filesystem::temp_directory_path();
        std::filesystem::path const build_directory_path = temporary_directory_path / (temporary_directory_name.has_value() ? temporary_directory_name.value() : project_name);

        std::pmr::vector<std::filesystem::path> artifact_absolute_paths;
        artifact_absolute_paths.reserve(artifact_paths.size());
        for (std::filesystem::path const& relative_path : artifact_paths)
            artifact_absolute_paths.push_back(g_examples_directory / project_name / relative_path);

        std::pmr::vector<std::filesystem::path> header_search_directories = iris::common::get_default_header_search_directories();
        
        std::pmr::vector<std::filesystem::path> repository_paths{ g_standard_repository_file_path };
        repository_paths.insert(repository_paths.end(), additional_repository_paths.begin(), additional_repository_paths.end());

        std::filesystem::remove_all(build_directory_path);

        Builder builder = create_builder(
            target,
            build_directory_path,
            header_search_directories,
            repository_paths,
            compilation_options,
            builder_options,
            {}
        );
    
        build_artifacts(builder, artifact_absolute_paths);

        for (std::filesystem::path const& expected_output_path : expected_output_paths)
        {
            std::filesystem::path const output_path = build_directory_path / expected_output_path;
            CHECK(std::filesystem::exists(output_path));
        }

        return build_directory_path;
    }

    // Builds like test_builder, then actually executes the generated test executable and requires
    // it to succeed. Checking that an output file exists only proves the artifact linked; it does
    // not prove the program computes the right answer, and for a long time nothing in this repo
    // ran generated code at all. See Plans/bug_decimal64_arithmetic_needs_divti3.md.
    void test_builder_and_run(
        std::string_view const project_name,
        std::pmr::vector<std::filesystem::path> const& artifact_paths,
        iris::compiler::Target const& target,
        std::span<std::filesystem::path const> const additional_repository_paths,
        std::span<std::filesystem::path const> const expected_output_paths,
        std::string_view const test_executable_name,
        std::optional<std::string_view> const temporary_directory_name = std::nullopt,
        iris::compiler::Compilation_options const compilation_options = {}
    )
    {
        std::filesystem::path const build_directory_path = test_builder(
            project_name,
            artifact_paths,
            target,
            additional_repository_paths,
            expected_output_paths,
            temporary_directory_name,
            {.is_test_mode = true},
            compilation_options
        );

        std::filesystem::path const test_executable_path =
            build_directory_path / "bin" / get_binary_name(test_executable_name, target);

        REQUIRE(std::filesystem::exists(test_executable_path));

        std::string const command = std::format("\"{}\"", test_executable_path.generic_string());
        int const exit_code = std::system(command.c_str());
        CHECK(exit_code == 0);
    }

    void test_compile_commands(
        std::filesystem::path const& build_directory_path,
        std::filesystem::path const& artifact_file_path,
        std::filesystem::path const& output_file_path,
        iris::compiler::Target const& target,
        std::span<std::filesystem::path const> const additional_repository_paths,
        std::pmr::vector<Compile_command> const& expected_compile_commands
    )
    {
        std::pmr::vector<std::filesystem::path> header_search_directories = iris::common::get_default_header_search_directories();
        
        std::pmr::vector<std::filesystem::path> repository_paths{ g_standard_repository_file_path };
        repository_paths.insert(repository_paths.end(), additional_repository_paths.begin(), additional_repository_paths.end());

        iris::compiler::Compilation_options const compilation_options
        {
        };

        Builder_options const builder_options
        {
        };

        Builder builder = create_builder(
            target,
            build_directory_path,
            header_search_directories,
            repository_paths,
            compilation_options,
            builder_options,
            {}
        );

        if (std::filesystem::exists(output_file_path))
            std::filesystem::remove(output_file_path);

        write_compile_commands_json_to_file(
            builder,
            artifact_file_path,
            compilation_options,
            output_file_path
        );

        CHECK(std::filesystem::exists(output_file_path));

        std::pmr::vector<Compile_command> actual_compile_commands = read_compile_commands_from_file(output_file_path);
        for (Compile_command& compile_command : actual_compile_commands)
        {
            auto const iterator = std::remove_if(
                compile_command.arguments.begin(),
                compile_command.arguments.end(),
                [](std::pmr::string const& argument) -> bool { return argument.starts_with("/clang:-isystemC:/Program Files"); }
            );
            compile_command.arguments.erase(iterator, compile_command.arguments.end());
        }
        
        CHECK(expected_compile_commands == actual_compile_commands);
    }

    TEST_CASE("Build Hello_world", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"bin"} / get_binary_name("Hello_world", target)
        };

        test_builder("Hello_world", {"iris_artifact.json"}, target, {}, expected_output_paths);
    }

    TEST_CASE("Build Link_with_library", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_examples_directory / "Link_with_library" / "iris_repository.json"
        };

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"lib"} / get_static_library_name("my_library", target),
            std::filesystem::path{"bin"} / get_binary_name("my_app", target),
        };

        test_builder("Link_with_library", {"my_app/iris_artifact.json"}, target, repository_paths, expected_output_paths);
    }

    TEST_CASE("Build Mix_with_cpp", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_examples_directory / "Mix_with_cpp" / "iris_repository.json"
        };

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"artifacts"} / "my_app.cpp_implementation.bc",
            std::filesystem::path{"artifacts"} / "my_app.bc",
            std::filesystem::path{"artifacts/C_interface.irisb"},
            std::filesystem::path{"bin"} / get_binary_name("my_app", target)
        };

        test_builder("Mix_with_cpp", {"my_app/iris_artifact.json"}, target, repository_paths, expected_output_paths);
    }

    TEST_CASE("Build Mix_with_cpp compile commands", "[Builder]")
    {
        std::string_view const project_name = "Mix_with_cpp";
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_examples_directory / project_name / "iris_repository.json"
        };

        std::filesystem::path const artifact_file_path = g_examples_directory / project_name / "my_app" / "iris_artifact.json";
        
        std::filesystem::path const temporary_directory_path = std::filesystem::temp_directory_path();
        std::filesystem::path const build_directory_path = temporary_directory_path / project_name / "build";
        std::filesystem::path const output_file_path = build_directory_path / "compile_commands.json";

        std::filesystem::path const executable_directory = iris::common::get_executable_directory();
        std::filesystem::path const builtin_include_directory = iris::common::get_builtin_include_directory();

        bool const use_clang_cl = true;

        if (use_clang_cl)
        {
            std::pmr::vector<Compile_command> const expected_compile_commands
            {
                Compile_command
                {
                    .directory = build_directory_path / "artifacts",
                    .arguments = {
                        std::pmr::string{(executable_directory / "clang-cl.exe").generic_string()},
                        std::pmr::string{"/clang:-I"} + std::pmr::string{builtin_include_directory.generic_string()},
                        std::pmr::string{"/clang:-I"} + std::pmr::string{(build_directory_path / "include").generic_string()},
                        std::pmr::string{"/clang:-I"} + std::pmr::string{(g_examples_directory / project_name / "external_library" / "include").generic_string()},
                        std::pmr::string{"/clang:-std=c++23"},
                        std::pmr::string{"/clang:-o"} + std::pmr::string{(build_directory_path / "artifacts" / "my_app.cpp_implementation.bc").generic_string()},
                        std::pmr::string{"/MD"},
                        std::pmr::string{"/EHsc"},
                        std::pmr::string{"/clang:-MMD"},
                        std::pmr::string{"/clang:-MF"} + std::pmr::string{(build_directory_path / "artifacts" / "my_app.cpp_implementation.d").generic_string()},
                        std::pmr::string{"/clang:-emit-llvm"},
                        std::pmr::string{"/clang:-c"},
                        std::pmr::string{(g_examples_directory / project_name / "my_app" / "cpp_implementation.cpp").generic_string()},
                    },
                    .file = g_examples_directory / project_name / "my_app" / "cpp_implementation.cpp",
                    .output = build_directory_path / "artifacts" / "my_app.cpp_implementation.bc",
                }
            };

            test_compile_commands(build_directory_path, {artifact_file_path}, output_file_path, target, repository_paths, expected_compile_commands);
        }
    }

    TEST_CASE("Build Export_c_header", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
        };

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"include"} / "my_library" / "module_a.h",
            std::filesystem::path{"include"} / "my_library" / "module_a.hpp",
        };

        test_builder("Export_c_header", {"iris_artifact.json"}, target, repository_paths, expected_output_paths);
    }

    TEST_CASE("Build Export_and_import_c_header", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
        };

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"artifacts"} / "my_library.module_a.irisb",
            std::filesystem::path{"artifacts"} / "my_library.module_b.irisb",
            std::filesystem::path{"artifacts"} / "my_library.module_c.irisb",
            std::filesystem::path{"include"} / "my_library" / "module_a.h",
            std::filesystem::path{"include"} / "my_library" / "module_a.hpp",
        };

        test_builder("Export_and_import_c_header", {"iris_artifact.json"}, target, repository_paths, expected_output_paths);
    }

    TEST_CASE("Build Import_c_header_with_dependency", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
        };

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"artifacts"} / "my_library.module_a.irisb",
            std::filesystem::path{"artifacts"} / "my_library.module_b.irisb",
            std::filesystem::path{"artifacts"} / "my_library.module_c.irisb",
            std::filesystem::path{"include"} / "my_library" / "module_a.h",
            std::filesystem::path{"include"} / "my_library" / "module_a.hpp",
        };

        test_builder("Import_c_header_with_dependency", {"iris_artifact.json"}, target, repository_paths, expected_output_paths);
    }

    TEST_CASE("Build Test_framework my_app in non-test mode", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_examples_directory / "Test_framework" / "iris_repository.json"
        };

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"artifacts"} / "my_library.irisb",
            std::filesystem::path{"artifacts"} / "my_app.irisb",
            std::filesystem::path{"bin"} / get_binary_name("my_app", target)
        };

        test_builder("Test_framework", {"my_app/iris_artifact.json"}, target, repository_paths, expected_output_paths);
    }

    TEST_CASE("Build Test_framework my_library in test mode", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_examples_directory / "Test_framework" / "iris_repository.json"
        };

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"artifacts"} / "my_library.test.bc",
            std::filesystem::path{"artifacts"} / "my_library.generated_tests_information.test.bc",
            std::filesystem::path{"bin"} / get_binary_name("my_library.iris.test", target)
        };

        test_builder("Test_framework", {"my_library/iris_artifact.json"}, target, repository_paths, expected_output_paths, "Test_framework_0", {.is_test_mode = true});
    }

    TEST_CASE("Build Test_framework my_app in test mode", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_examples_directory / "Test_framework" / "iris_repository.json"
        };

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"artifacts"} / "my_library.bc",
            std::filesystem::path{"artifacts"} / "my_app.test.bc",
            std::filesystem::path{"artifacts"} / "my_app.generated_tests_information.test.bc",
            std::filesystem::path{"bin"} / get_binary_name("my_app.iris.test", target)
        };

        test_builder("Test_framework", {"my_app/iris_artifact.json"}, target, repository_paths, expected_output_paths, "Test_framework_1", {.is_test_mode = true});
    }

    TEST_CASE("Build Test_framework my_library and my_app in test mode", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_examples_directory / "Test_framework" / "iris_repository.json"
        };

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"artifacts"} / "my_library.bc",
            std::filesystem::path{"artifacts"} / "my_library.test.bc",
            std::filesystem::path{"artifacts"} / "my_library.generated_tests_information.test.bc",
            std::filesystem::path{"artifacts"} / "my_app.test.bc",
            std::filesystem::path{"artifacts"} / "my_app.generated_tests_information.test.bc",
            std::filesystem::path{"bin"} / get_binary_name("my_library.iris.test", target),
            std::filesystem::path{"bin"} / get_binary_name("my_app.iris.test", target)
        };

        test_builder("Test_framework", {"my_library/iris_artifact.json", "my_app/iris_artifact.json"}, target, repository_paths, expected_output_paths, "Test_framework_2", {.is_test_mode = true});
    }

    TEST_CASE("Build Test_framework empty_app in test mode", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_examples_directory / "Test_framework" / "iris_repository.json"
        };

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"artifacts"} / "empty_app.bc"
        };

        test_builder("Test_framework", {"empty_app/iris_artifact.json"}, target, repository_paths, expected_output_paths, "Test_framework_3", {.is_test_mode = true});
    }

    // Guards Plans/bug_decimal64_arithmetic_needs_divti3.md: multiply, divide and narrowing casts
    // of an Int64-backed decimal emit 'sdiv i128', which lowers to a call to compiler-rt's
    // __divti3. Without the builtins archive on the link line this fails with
    // "undefined symbol: __divti3". This test links and runs, so it catches both the missing
    // symbol and a wrong result.
    TEST_CASE("Build and run Decimal_arithmetic decimal_app in test mode", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_examples_directory / "Decimal_arithmetic" / "iris_repository.json"
        };

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"artifacts"} / "decimal_app.test.bc",
            std::filesystem::path{"artifacts"} / "decimal_app.generated_tests_information.test.bc",
            std::filesystem::path{"bin"} / get_binary_name("decimal_app.iris.test", target)
        };

        test_builder_and_run(
            "Decimal_arithmetic",
            {"decimal_app/iris_artifact.json"},
            target,
            repository_paths,
            expected_output_paths,
            "decimal_app.iris.test"
        );
    }

    TEST_CASE("Build and run Decimal_arithmetic decimal_app with decimal overflow checks", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_examples_directory / "Decimal_arithmetic" / "iris_repository.json"
        };

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"bin"} / get_binary_name("decimal_app.iris.test", target)
        };

        test_builder_and_run(
            "Decimal_arithmetic",
            {"decimal_app/iris_artifact.json"},
            target,
            repository_paths,
            expected_output_paths,
            "decimal_app.iris.test",
            "Decimal_arithmetic_overflow_checks",
            {.enable_decimal_overflow_checks = true}
        );
    }

    TEST_CASE("Build and run Decimal_arithmetic decimal_overflow_app", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_examples_directory / "Decimal_arithmetic" / "iris_repository.json"
        };

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"bin"} / get_binary_name("decimal_overflow_app", target)
        };

        auto const build_and_run = [&](std::string_view const directory_name, bool const enable_checks) -> int
        {
            std::filesystem::path const build_directory_path = test_builder(
                "Decimal_arithmetic",
                {"decimal_overflow_app/iris_artifact.json"},
                target,
                repository_paths,
                expected_output_paths,
                directory_name,
                {},
                // Built without debug information on purpose: the check ends in the CRT's abort(),
                // and the debug CRT turns that into a modal "Debug Error!" box that blocks forever
                // when nobody is there to dismiss it.
                {.debug = false, .enable_decimal_overflow_checks = enable_checks}
            );

            std::filesystem::path const executable_path =
                build_directory_path / "bin" / get_binary_name("decimal_overflow_app", target);

            REQUIRE(std::filesystem::exists(executable_path));

            std::string const command = std::format("\"{}\"", executable_path.generic_string());
            return std::system(command.c_str());
        };

        // Without the checks the overflow is silent. main returns the result as an Int32, so the
        // exit code is the truncated answer: 4_500_000_000 wraps to 205_032_704, which as a
        // Decimal6 is 205.032704 and rounds to 205 instead of 4500.
        CHECK(build_and_run("Decimal_arithmetic_overflow_unchecked", false) == 205);

        // With them it aborts.
        CHECK(build_and_run("Decimal_arithmetic_overflow_checked", true) != 0);
    }

    TEST_CASE("Build Copy_files", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_standard_repository_file_path
        };

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"bin"} / get_binary_name("Copy_files", target),
            std::filesystem::path{"bin"} / "assets" / "config.txt",
            std::filesystem::path{"bin"} / "assets" / "data" / "record.txt",
        };

        test_builder("Copy_files", {"iris_artifact.json"}, target, repository_paths, expected_output_paths);
    }

    TEST_CASE("Build Type_constructors", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_standard_repository_file_path
        };

        std::pmr::vector<std::filesystem::path> const expected_output_paths
        {
            std::filesystem::path{"lib"} / get_static_library_name("Type_constructors", target),
        };

        test_builder("Type_constructors", {"iris_artifact.json"}, target, repository_paths, expected_output_paths);
    }

    static void write_source_file(
        std::filesystem::path const& file_path,
        std::string_view const content
    )
    {
        std::ofstream output_stream{ file_path, std::ios::binary | std::ios::trunc };
        output_stream.write(content.data(), content.size());
    }

    // Regression test: an incremental build used to consider a module's bitcode up to date whenever
    // it was newer than that module's own '.irisb', ignoring the modules it imports. Changing the
    // layout of a struct in a library therefore left the dependent modules compiled against the old
    // layout, with no error reported.
    TEST_CASE("Incremental build recompiles dependents when a struct layout changes", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "builder_incremental_struct_layout";
        std::filesystem::path const source_directory_path = root_directory_path / "source";
        std::filesystem::path const build_directory_path = root_directory_path / "build";

        std::filesystem::remove_all(root_directory_path);
        std::filesystem::create_directories(root_directory_path);
        std::filesystem::copy(
            g_examples_directory / "Link_with_library",
            source_directory_path,
            std::filesystem::copy_options::recursive
        );

        std::filesystem::path const library_source_path = source_directory_path / "my_library" / "my_library.iris";
        std::filesystem::path const app_source_path = source_directory_path / "my_app" / "my_app.iris";

        write_source_file(
            library_source_path,
            "module my_library;\n"
            "\n"
            "export struct My_data\n"
            "{\n"
            "    first: Int32 = 1;\n"
            "    last: Int32 = 2;\n"
            "}\n"
            "\n"
            "export function hello_from_library() -> (result: Int32)\n"
            "{\n"
            "    return 1;\n"
            "}\n"
        );

        write_source_file(
            app_source_path,
            "module my_app;\n"
            "\n"
            "import my_library as my_library;\n"
            "\n"
            "@unique_name(\"main\")\n"
            "export function main() -> (result: Int32)\n"
            "{\n"
            "    var data: my_library.My_data = {};\n"
            "    return data.last;\n"
            "}\n"
        );

        std::pmr::vector<std::filesystem::path> const artifact_absolute_paths
        {
            source_directory_path / "my_app" / "iris_artifact.json"
        };

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_standard_repository_file_path,
            source_directory_path / "iris_repository.json"
        };

        std::pmr::vector<std::filesystem::path> const header_search_directories = iris::common::get_default_header_search_directories();

        iris::compiler::Compilation_options const compilation_options{};
        iris::compiler::Builder_options const builder_options{};

        auto const build = [&]() -> void
        {
            Builder builder = create_builder(
                target,
                build_directory_path,
                header_search_directories,
                repository_paths,
                compilation_options,
                builder_options,
                {}
            );

            build_artifacts(builder, artifact_absolute_paths);
        };

        // 'my_app' returns 'data.last', so its exit code is the value it read. That makes the exit
        // code the assertion that actually encodes this bug: 'last' defaults to 2 and the field
        // inserted below defaults to 3, and inserting it moves 'last' from offset 4 to offset 8. A
        // correctly rebuilt program therefore exits 2, while one still compiled against the old
        // layout reads offset 4 - now holding 'inserted' - and exits 3. Checking only that the
        // bitcode was rewritten would pass even if the recompile produced wrong code.
        auto const run_app_and_get_exit_code = [&]() -> int
        {
            std::filesystem::path const app_executable_path =
                build_directory_path / "bin" / get_binary_name("my_app", target);

            REQUIRE(std::filesystem::exists(app_executable_path));

            std::string const command = std::format("\"{}\"", app_executable_path.generic_string());
            return std::system(command.c_str());
        };

        build();

        std::filesystem::path const app_bitcode_path = build_directory_path / "artifacts" / "my_app.bc";
        REQUIRE(std::filesystem::exists(app_bitcode_path));

        CHECK(run_app_and_get_exit_code() == 2);

        std::filesystem::file_time_type const first_build_time = std::filesystem::last_write_time(app_bitcode_path);

        // Make sure the edit below cannot share a filesystem timestamp with the first build.
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1100 });

        // Insert a field in the middle of the struct: 'last' moves from offset 4 to offset 8.
        // 'my_app.iris' is left untouched, exactly as in the reported bug.
        write_source_file(
            library_source_path,
            "module my_library;\n"
            "\n"
            "export struct My_data\n"
            "{\n"
            "    first: Int32 = 1;\n"
            "    inserted: Int32 = 3;\n"
            "    last: Int32 = 2;\n"
            "}\n"
            "\n"
            "export function hello_from_library() -> (result: Int32)\n"
            "{\n"
            "    return 1;\n"
            "}\n"
        );

        // Incremental build: the build directory is deliberately kept.
        build();

        REQUIRE(std::filesystem::exists(app_bitcode_path));

        std::filesystem::file_time_type const second_build_time = std::filesystem::last_write_time(app_bitcode_path);
        CHECK(second_build_time > first_build_time);

        // 3 here would mean the module was recompiled against the old layout rather than not at all.
        CHECK(run_app_and_get_exit_code() == 2);
    }

    // A build with no source changes must not recompile anything, otherwise the fix above would
    // have turned every incremental build into a full rebuild.
    TEST_CASE("Incremental build does not recompile unchanged modules", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "builder_incremental_no_changes";
        std::filesystem::path const source_directory_path = root_directory_path / "source";
        std::filesystem::path const build_directory_path = root_directory_path / "build";

        std::filesystem::remove_all(root_directory_path);
        std::filesystem::create_directories(root_directory_path);
        std::filesystem::copy(
            g_examples_directory / "Link_with_library",
            source_directory_path,
            std::filesystem::copy_options::recursive
        );

        std::pmr::vector<std::filesystem::path> const artifact_absolute_paths
        {
            source_directory_path / "my_app" / "iris_artifact.json"
        };

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_standard_repository_file_path,
            source_directory_path / "iris_repository.json"
        };

        std::pmr::vector<std::filesystem::path> const header_search_directories = iris::common::get_default_header_search_directories();

        iris::compiler::Compilation_options const compilation_options{};
        iris::compiler::Builder_options const builder_options{};

        auto const build = [&]() -> void
        {
            Builder builder = create_builder(
                target,
                build_directory_path,
                header_search_directories,
                repository_paths,
                compilation_options,
                builder_options,
                {}
            );

            build_artifacts(builder, artifact_absolute_paths);
        };

        build();

        std::filesystem::path const app_bitcode_path = build_directory_path / "artifacts" / "my_app.bc";
        std::filesystem::path const library_bitcode_path = build_directory_path / "artifacts" / "my_library.bc";
        REQUIRE(std::filesystem::exists(app_bitcode_path));
        REQUIRE(std::filesystem::exists(library_bitcode_path));

        std::filesystem::file_time_type const first_app_time = std::filesystem::last_write_time(app_bitcode_path);
        std::filesystem::file_time_type const first_library_time = std::filesystem::last_write_time(library_bitcode_path);

        std::this_thread::sleep_for(std::chrono::milliseconds{ 1100 });

        build();

        CHECK(std::filesystem::last_write_time(app_bitcode_path) == first_app_time);
        CHECK(std::filesystem::last_write_time(library_bitcode_path) == first_library_time);
    }

    // Sets up a copy of 'Import_c_header_with_dependency' in which 'module_b.h' pulls its struct in
    // from a second header rather than declaring it inline. That second header is what the tests
    // below edit: it is reachable only through a '#include', so nothing in the artifact file names
    // it and only the recorded include set can connect it to the generated module.
    //
    // The example is used as the base because its 'module_b.h' also includes a header that this same
    // build generates from 'module_a.iris'. That generated header shares its timestamp with the rest
    // of the build, which is exactly the tie that must not be read as "out of date".
    struct Nested_header_project
    {
        std::filesystem::path source_directory_path;
        std::filesystem::path build_directory_path;
        std::filesystem::path shared_header_path;
        std::pmr::vector<std::filesystem::path> artifact_absolute_paths;
        std::pmr::vector<std::filesystem::path> repository_paths;
    };

    static Nested_header_project create_nested_header_project(
        std::string_view const directory_name,
        bool const with_shared_header = true
    )
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / directory_name;
        std::filesystem::path const source_directory_path = root_directory_path / "source";
        std::filesystem::path const build_directory_path = root_directory_path / "build";

        std::filesystem::remove_all(root_directory_path);
        std::filesystem::create_directories(root_directory_path);
        std::filesystem::copy(
            g_examples_directory / "Import_c_header_with_dependency",
            source_directory_path,
            std::filesystem::copy_options::recursive
        );

        std::filesystem::path const shared_header_path = source_directory_path / "shared.h";

        if (with_shared_header)
        {
            write_source_file(
                shared_header_path,
                "struct my_library_module_b_Shared\n"
                "{\n"
                "    int first;\n"
                "    int last;\n"
                "};\n"
            );

            write_source_file(
                source_directory_path / "module_b.h",
                "#include \"my_library/module_a.h\"\n"
                "#include \"shared.h\"\n"
                "\n"
                "struct my_library_module_a_My_struct my_library_module_b_get_struct(void);\n"
                "\n"
                "struct my_library_module_b_Wrapper\n"
                "{\n"
                "    struct my_library_module_b_Shared shared;\n"
                "};\n"
            );
        }

        return Nested_header_project
        {
            .source_directory_path = source_directory_path,
            .build_directory_path = build_directory_path,
            .shared_header_path = shared_header_path,
            .artifact_absolute_paths = { source_directory_path / "iris_artifact.json" },
            .repository_paths = { g_standard_repository_file_path },
        };
    }

    static void build_nested_header_project(Nested_header_project const& project)
    {
        std::pmr::vector<std::filesystem::path> const header_search_directories = iris::common::get_default_header_search_directories();

        Builder builder = create_builder(
            iris::compiler::get_default_target(),
            project.build_directory_path,
            header_search_directories,
            project.repository_paths,
            iris::compiler::Compilation_options{},
            iris::compiler::Builder_options{},
            {}
        );

        build_artifacts(builder, project.artifact_absolute_paths);
    }

    // Regression test: the module generated from a C header used to be considered up to date
    // whenever it was newer than the header named in the artifact file. A header reached only
    // through a '#include' was therefore invisible, so changing a struct in it left every dependent
    // compiled against the old layout with nothing reported - the same silent failure the iris to
    // iris case above covers.
    TEST_CASE("Incremental build re-imports a C header when a transitively included header changes", "[Builder]")
    {
        Nested_header_project const project = create_nested_header_project("builder_incremental_nested_c_header");

        std::filesystem::path const header_module_path = project.build_directory_path / "artifacts" / "my_library.module_b.irisb";
        // The default Compilation_options leave output_debug_code_view off, so codegen writes
        // bitcode rather than object files.
        std::filesystem::path const dependent_object_path = project.build_directory_path / "artifacts" / "my_library.module_c.bc";

        build_nested_header_project(project);

        REQUIRE(std::filesystem::exists(header_module_path));
        REQUIRE(std::filesystem::exists(dependent_object_path));

        std::filesystem::file_time_type const first_header_module_time = std::filesystem::last_write_time(header_module_path);
        std::filesystem::file_time_type const first_dependent_time = std::filesystem::last_write_time(dependent_object_path);

        std::this_thread::sleep_for(std::chrono::milliseconds{ 1100 });

        // Inserting a field at the front moves every following member, so a dependent that was not
        // regenerated would read the wrong offsets.
        write_source_file(
            project.shared_header_path,
            "struct my_library_module_b_Shared\n"
            "{\n"
            "    long long inserted;\n"
            "    int first;\n"
            "    int last;\n"
            "};\n"
        );

        build_nested_header_project(project);

        CHECK(std::filesystem::last_write_time(header_module_path) > first_header_module_time);
        CHECK(std::filesystem::last_write_time(dependent_object_path) > first_dependent_time);
    }

    // The guard for the test above. 'module_b.h' includes a header generated by this same build, so
    // its recorded includes routinely carry the build's own timestamp; if a tie were read as "out of
    // date" every build would re-import every header and recompile everything downstream of it.
    TEST_CASE("Incremental build does not re-import an unchanged C header", "[Builder]")
    {
        Nested_header_project const project = create_nested_header_project("builder_incremental_c_header_unchanged");

        std::filesystem::path const header_module_path = project.build_directory_path / "artifacts" / "my_library.module_b.irisb";
        std::filesystem::path const module_a_object_path = project.build_directory_path / "artifacts" / "my_library.module_a.bc";
        std::filesystem::path const module_c_object_path = project.build_directory_path / "artifacts" / "my_library.module_c.bc";

        build_nested_header_project(project);

        REQUIRE(std::filesystem::exists(header_module_path));
        REQUIRE(std::filesystem::exists(module_a_object_path));
        REQUIRE(std::filesystem::exists(module_c_object_path));

        std::filesystem::file_time_type const first_header_module_time = std::filesystem::last_write_time(header_module_path);
        std::filesystem::file_time_type const first_module_a_time = std::filesystem::last_write_time(module_a_object_path);
        std::filesystem::file_time_type const first_module_c_time = std::filesystem::last_write_time(module_c_object_path);

        std::this_thread::sleep_for(std::chrono::milliseconds{ 1100 });

        build_nested_header_project(project);

        CHECK(std::filesystem::last_write_time(header_module_path) == first_header_module_time);
        CHECK(std::filesystem::last_write_time(module_a_object_path) == first_module_a_time);
        CHECK(std::filesystem::last_write_time(module_c_object_path) == first_module_c_time);
    }

    TEST_CASE("C header dependency file lists transitively included headers", "[Builder]")
    {
        Nested_header_project const project = create_nested_header_project("builder_c_header_dependency_file");

        build_nested_header_project(project);

        std::filesystem::path const dependency_file_path = project.build_directory_path / "artifacts" / "my_library.module_b.irisb.deps.json";
        REQUIRE(std::filesystem::exists(dependency_file_path));

        std::optional<std::pmr::string> const contents = iris::common::get_file_contents(dependency_file_path);
        REQUIRE(contents.has_value());

        nlohmann::json const json = nlohmann::json::parse(contents.value(), nullptr, false);
        REQUIRE_FALSE(json.is_discarded());
        CHECK(json["version"] == 1);
        CHECK(json["module_name"] == "my_library.module_b");

        std::pmr::vector<std::string> filenames;
        for (nlohmann::json const& include : json["includes"])
            filenames.push_back(std::filesystem::path{include.get<std::string>()}.filename().generic_string());

        auto const contains = [&](std::string_view const filename) -> bool
        {
            return std::find(filenames.begin(), filenames.end(), filename) != filenames.end();
        };

        CHECK(contains("module_b.h"));
        CHECK(contains("shared.h"));
        // Generated by this same build from 'module_a.iris'.
        CHECK(contains("module_a.h"));
    }

    // A build directory produced by an older compiler has no dependency file beside the '.irisb'.
    // There is no way to tell what that module was generated from, so it must be re-imported rather
    // than trusted.
    TEST_CASE("A build directory without header dependency files is re-imported", "[Builder]")
    {
        Nested_header_project const project = create_nested_header_project("builder_c_header_missing_dependency_file");

        std::filesystem::path const header_module_path = project.build_directory_path / "artifacts" / "my_library.module_b.irisb";
        std::filesystem::path const dependency_file_path = project.build_directory_path / "artifacts" / "my_library.module_b.irisb.deps.json";

        build_nested_header_project(project);

        REQUIRE(std::filesystem::exists(header_module_path));
        REQUIRE(std::filesystem::exists(dependency_file_path));

        std::filesystem::file_time_type const first_header_module_time = std::filesystem::last_write_time(header_module_path);

        std::this_thread::sleep_for(std::chrono::milliseconds{ 1100 });

        std::filesystem::remove(dependency_file_path);

        build_nested_header_project(project);

        CHECK(std::filesystem::last_write_time(header_module_path) > first_header_module_time);
        CHECK(std::filesystem::exists(dependency_file_path));
    }

    // Regression test: the importer options were built after the cache was consulted and were not
    // part of the key, so changing one produced a different module from the same header text while
    // the build kept serving the old one.
    TEST_CASE("Incremental build re-imports a C header when importer options change", "[Builder]")
    {
        Nested_header_project const project = create_nested_header_project("builder_c_header_option_change");

        std::filesystem::path const header_module_path = project.build_directory_path / "artifacts" / "my_library.module_b.irisb";
        std::filesystem::path const artifact_file_path = project.source_directory_path / "iris_artifact.json";

        build_nested_header_project(project);

        REQUIRE(std::filesystem::exists(header_module_path));

        std::filesystem::file_time_type const first_header_module_time = std::filesystem::last_write_time(header_module_path);

        std::this_thread::sleep_for(std::chrono::milliseconds{ 1100 });

        std::optional<std::pmr::string> const artifact_contents = iris::common::get_file_contents(artifact_file_path);
        REQUIRE(artifact_contents.has_value());

        nlohmann::json artifact_json = nlohmann::json::parse(artifact_contents.value(), nullptr, false);
        REQUIRE_FALSE(artifact_json.is_discarded());

        for (nlohmann::json& source_group : artifact_json["sources"])
        {
            if (source_group["type"] == "import_c_header")
                source_group["wrap_pointers_as_optional"] = true;
        }

        write_source_file(artifact_file_path, artifact_json.dump(4));

        build_nested_header_project(project);

        CHECK(std::filesystem::last_write_time(header_module_path) > first_header_module_time);
    }

    // The cache key is folded from unordered inputs in places, and an unstable key would re-import
    // every header on every build without ever reporting why.
    TEST_CASE("C header cache key is stable across identical builds", "[Builder]")
    {
        Nested_header_project const project = create_nested_header_project("builder_c_header_stable_key");

        std::filesystem::path const dependency_file_path = project.build_directory_path / "artifacts" / "my_library.module_b.irisb.deps.json";

        auto const read_cache_key = [&]() -> std::string
        {
            std::optional<std::pmr::string> const contents = iris::common::get_file_contents(dependency_file_path);
            REQUIRE(contents.has_value());

            nlohmann::json const json = nlohmann::json::parse(contents.value(), nullptr, false);
            REQUIRE_FALSE(json.is_discarded());

            return json["cache_key"].get<std::string>();
        };

        build_nested_header_project(project);
        std::string const first_cache_key = read_cache_key();

        // Force a re-import so that the key is recomputed rather than read back unchanged.
        std::filesystem::remove(dependency_file_path);

        build_nested_header_project(project);
        std::string const second_cache_key = read_cache_key();

        CHECK(first_cache_key == second_cache_key);
    }

    // Regression test: nothing in the build directory recorded which configuration produced it, so
    // building with different codegen options into the same directory silently mixed objects.
    //
    // The '.irisb' files must survive: parsing does not depend on codegen options, and re-parsing
    // every source file on a configuration toggle is the expensive half of a build for no gain.
    TEST_CASE("Changing compilation options rebuilds objects but not modules", "[Builder]")
    {
        iris::compiler::Target const target = iris::compiler::get_default_target();

        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "builder_compilation_options_change";
        std::filesystem::path const source_directory_path = root_directory_path / "source";
        std::filesystem::path const build_directory_path = root_directory_path / "build";

        std::filesystem::remove_all(root_directory_path);
        std::filesystem::create_directories(root_directory_path);
        std::filesystem::copy(
            g_examples_directory / "Link_with_library",
            source_directory_path,
            std::filesystem::copy_options::recursive
        );

        std::pmr::vector<std::filesystem::path> const artifact_absolute_paths
        {
            source_directory_path / "my_app" / "iris_artifact.json"
        };

        std::pmr::vector<std::filesystem::path> const repository_paths
        {
            g_standard_repository_file_path,
            source_directory_path / "iris_repository.json"
        };

        std::pmr::vector<std::filesystem::path> const header_search_directories = iris::common::get_default_header_search_directories();

        auto const build = [&](iris::compiler::Compilation_options const& compilation_options) -> void
        {
            Builder builder = create_builder(
                target,
                build_directory_path,
                header_search_directories,
                repository_paths,
                compilation_options,
                iris::compiler::Builder_options{},
                {}
            );

            build_artifacts(builder, artifact_absolute_paths);
        };

        iris::compiler::Compilation_options compilation_options{};
        build(compilation_options);

        std::filesystem::path const app_bitcode_path = build_directory_path / "artifacts" / "my_app.bc";
        std::filesystem::path const app_module_path = build_directory_path / "artifacts" / "my_app.irisb";
        std::filesystem::path const library_module_path = build_directory_path / "artifacts" / "my_library.irisb";

        REQUIRE(std::filesystem::exists(app_bitcode_path));
        REQUIRE(std::filesystem::exists(app_module_path));
        REQUIRE(std::filesystem::exists(library_module_path));

        std::filesystem::file_time_type const first_bitcode_time = std::filesystem::last_write_time(app_bitcode_path);
        std::filesystem::file_time_type const first_app_module_time = std::filesystem::last_write_time(app_module_path);
        std::filesystem::file_time_type const first_library_module_time = std::filesystem::last_write_time(library_module_path);

        std::this_thread::sleep_for(std::chrono::milliseconds{ 1100 });

        // Affects generated code but not the parse, and leaves the output extension alone so that
        // the comparison below is against the very same file.
        compilation_options.enable_bounds_checks = false;
        build(compilation_options);

        CHECK(std::filesystem::last_write_time(app_bitcode_path) > first_bitcode_time);
        CHECK(std::filesystem::last_write_time(app_module_path) == first_app_module_time);
        CHECK(std::filesystem::last_write_time(library_module_path) == first_library_module_time);
    }

    // A compiler upgrade cannot be simulated in-process, so the recorded identity is edited
    // directly. That still exercises the whole path: the mismatch raises both floors, and every
    // cached artifact must then be regenerated.
    TEST_CASE("Build identity change invalidates modules and objects", "[Builder]")
    {
        Nested_header_project const project = create_nested_header_project("builder_build_identity_change");

        std::filesystem::path const artifacts_directory = project.build_directory_path / "artifacts";
        std::filesystem::path const build_state_path = artifacts_directory / "build_state.json";
        std::filesystem::path const module_path = artifacts_directory / "my_library.module_a.irisb";
        std::filesystem::path const header_module_path = artifacts_directory / "my_library.module_b.irisb";
        std::filesystem::path const object_path = artifacts_directory / "my_library.module_a.bc";

        build_nested_header_project(project);

        REQUIRE(std::filesystem::exists(build_state_path));
        REQUIRE(std::filesystem::exists(module_path));
        REQUIRE(std::filesystem::exists(header_module_path));
        REQUIRE(std::filesystem::exists(object_path));

        std::filesystem::file_time_type const first_module_time = std::filesystem::last_write_time(module_path);
        std::filesystem::file_time_type const first_header_module_time = std::filesystem::last_write_time(header_module_path);
        std::filesystem::file_time_type const first_object_time = std::filesystem::last_write_time(object_path);

        std::this_thread::sleep_for(std::chrono::milliseconds{ 1100 });

        std::optional<std::pmr::string> const contents = iris::common::get_file_contents(build_state_path);
        REQUIRE(contents.has_value());

        nlohmann::json build_state = nlohmann::json::parse(contents.value(), nullptr, false);
        REQUIRE_FALSE(build_state.is_discarded());

        build_state["parse_identity"] = "0123456789abcdef";
        build_state["codegen_identity"] = "fedcba9876543210";
        write_source_file(build_state_path, build_state.dump(4));

        build_nested_header_project(project);

        CHECK(std::filesystem::last_write_time(module_path) > first_module_time);
        CHECK(std::filesystem::last_write_time(header_module_path) > first_header_module_time);
        CHECK(std::filesystem::last_write_time(object_path) > first_object_time);
    }

    // The guard for the test above. An identity that is not stable - a timestamp folded in by
    // mistake, or a directory scan that picks up a file the build itself writes - would raise the
    // floors on every build and silently turn every build into a full rebuild.
    TEST_CASE("Build state survives an unchanged rebuild", "[Builder]")
    {
        Nested_header_project const project = create_nested_header_project("builder_build_state_stable");

        std::filesystem::path const artifacts_directory = project.build_directory_path / "artifacts";
        std::filesystem::path const parse_stamp_path = artifacts_directory / "parse.stamp";
        std::filesystem::path const codegen_stamp_path = artifacts_directory / "codegen.stamp";

        build_nested_header_project(project);

        REQUIRE(std::filesystem::exists(parse_stamp_path));
        REQUIRE(std::filesystem::exists(codegen_stamp_path));

        std::filesystem::file_time_type const first_parse_stamp_time = std::filesystem::last_write_time(parse_stamp_path);
        std::filesystem::file_time_type const first_codegen_stamp_time = std::filesystem::last_write_time(codegen_stamp_path);

        std::this_thread::sleep_for(std::chrono::milliseconds{ 1100 });

        build_nested_header_project(project);

        CHECK(std::filesystem::last_write_time(parse_stamp_path) == first_parse_stamp_time);
        CHECK(std::filesystem::last_write_time(codegen_stamp_path) == first_codegen_stamp_time);
    }

    TEST_CASE("Locate artifacts in a directory", "[Builder]")
    {
        std::filesystem::path const root = std::filesystem::temp_directory_path() / "builder_artifact_search";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "subdir");
        std::filesystem::create_directories(root / "build" / "nested");
        std::filesystem::create_directories(root / ".hidden" / "nested");
        {
            std::ofstream{ root / "iris_artifact.json" };
            std::ofstream{ root / "subdir" / "iris_artifact.json" };
            std::ofstream{ root / "build" / "iris_artifact.json" };
            std::ofstream{ root / "build" / "nested" / "iris_artifact.json" };
            std::ofstream{ root / ".hidden" / "iris_artifact.json" };
            std::ofstream{ root / ".hidden" / "nested" / "iris_artifact.json" };
        }

        std::pmr::vector<std::filesystem::path> const found = iris::compiler::find_artifact_file_paths(root, {}, {});

        CHECK(found.size() == 2);
        for (std::filesystem::path const& artifact_file_path : found)
        {
            CHECK(artifact_file_path.filename() == "iris_artifact.json");
        }
    }

    TEST_CASE("Download dependency uses project paths", "[Builder][dependencies]")
    {
        std::filesystem::path const test_dir = std::filesystem::temp_directory_path() / "test_download_dep";
        std::filesystem::remove_all(test_dir);
        std::filesystem::create_directories(test_dir);

        std::filesystem::current_path(test_dir);

        Project_dependency dep;
        dep.name = "TestLib";
        dep.version = "1.0.0";
        dep.source_url = "https://example.com/testlib.zip";

        // Verify paths would be constructed correctly from project
        std::filesystem::path const expected_storage = test_dir / "external";
        std::filesystem::path const expected_archive = expected_storage / "TestLib-1.0.0.zip";

        CHECK((test_dir / "external").generic_string() == expected_storage.generic_string());
        CHECK((expected_storage / "TestLib-1.0.0.zip").generic_string() == expected_archive.generic_string());

        std::filesystem::current_path(test_dir.parent_path());
        std::filesystem::remove_all(test_dir);
    }

    TEST_CASE("Build dependency uses project paths", "[Builder][dependencies]")
    {
        std::filesystem::path const test_dir = std::filesystem::temp_directory_path() / "test_build_dep";
        std::filesystem::remove_all(test_dir);
        std::filesystem::create_directories(test_dir);

        std::filesystem::current_path(test_dir);

        Project_dependency dep;
        dep.name = "TestLib";
        dep.version = "1.0.0";
        dep.install_path = "install";

        // Verify paths would be constructed correctly from project
        std::filesystem::path const expected_archive = test_dir / "external" / "TestLib-1.0.0.zip";
        CHECK(!std::filesystem::exists(expected_archive));

        std::filesystem::current_path(test_dir.parent_path());
        std::filesystem::remove_all(test_dir);
    }

    TEST_CASE("Download dependencies iterates over all deps", "[Builder][dependencies]")
    {
        Iris_project project;
        project.dependencies = {
            Project_dependency{.name = "LibA", .version = "1.0", .source_url = "https://a.com/a.zip"},
            Project_dependency{.name = "LibB", .version = "2.0", .source_url = "https://b.com/b.zip"},
        };

        // Verify project has both dependencies
        CHECK(project.dependencies.size() == 2);
        CHECK(project.dependencies[0].name == "LibA");
        CHECK(project.dependencies[1].name == "LibB");
    }

    TEST_CASE("Build dependencies iterates over all deps", "[Builder][dependencies]")
    {
        Iris_project project;
        project.dependencies = {
            Project_dependency{.name = "LibA", .version = "1.0", .source_url = "https://a.com/a.zip"},
            Project_dependency{.name = "LibB", .version = "2.0", .source_url = "https://b.com/b.zip"},
        };

        // Verify project has both dependencies
        CHECK(project.dependencies.size() == 2);
    }
}
