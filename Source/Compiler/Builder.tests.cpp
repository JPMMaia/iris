import iris.common;
import iris.common.filesystem;
import iris.compiler;
import iris.compiler.builder;
import iris.compiler.compile_commands_generator;
import iris.compiler.target;

#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>
#include <vector>

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

    void test_builder(
        std::string_view const project_name,
        std::pmr::vector<std::filesystem::path> const& artifact_paths,
        iris::compiler::Target const& target,
        std::span<std::filesystem::path const> const additional_repository_paths,
        std::span<std::filesystem::path const> const expected_output_paths,
        std::optional<std::string_view> const temporary_directory_name = std::nullopt,
        iris::compiler::Builder_options const builder_options = {}
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

        iris::compiler::Compilation_options const compilation_options
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
    
        build_artifacts(builder, artifact_absolute_paths);

        for (std::filesystem::path const& expected_output_path : expected_output_paths)
        {
            std::filesystem::path const output_path = build_directory_path / expected_output_path;
            CHECK(std::filesystem::exists(output_path));
        }
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
                [](std::pmr::string const& argument) -> bool { return argument.starts_with("/clang:-IC:/Program Files"); }
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
}
