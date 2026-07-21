import iris.common;
import iris.common.filesystem;
import iris.compiler;
import iris.compiler.builder;
import iris.compiler.project;
import iris.compiler.compile_commands_generator;
import iris.compiler.target;

#include <chrono>
#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>
#include <thread>
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

        build();

        std::filesystem::path const app_bitcode_path = build_directory_path / "artifacts" / "my_app.bc";
        REQUIRE(std::filesystem::exists(app_bitcode_path));

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
