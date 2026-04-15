#include <array>
#include <chrono>
#include <filesystem>
#include <string_view>
#include <thread>

#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassInstrumentation.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/Target/TargetMachine.h>

#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h>
#include <llvm/Support/Error.h>

#include <catch2/catch_all.hpp>

#include <iostream>

import iris.common;
import iris.common.filesystem;
import iris.compiler;
import iris.compiler.artifact;
import iris.compiler.jit_runner;
import iris.compiler.target;

namespace iris
{
    static std::filesystem::path const g_standard_repository_file_path = std::filesystem::path{ STANDARD_REPOSITORY_FILE_PATH };

    void write_to_file_and_wait(
        iris::compiler::JIT_runner& jit_runner,
        std::filesystem::path const& file_path,
        std::string_view const content
    )
    {
        std::uint64_t const fence = iris::compiler::get_processed_files(jit_runner);
        iris::common::write_to_file(file_path, content);
        iris::compiler::wait_for(jit_runner, fence + 1);
    }

    void remove_file_and_wait(
        iris::compiler::JIT_runner& jit_runner,
        std::filesystem::path const& file_path
    )
    {
        std::uint64_t const fence = iris::compiler::get_processed_files(jit_runner);
        std::filesystem::remove(file_path);
        iris::compiler::wait_for(jit_runner, fence + 1);
    }

    void rename_file_and_wait(
        iris::compiler::JIT_runner& jit_runner,
        std::filesystem::path const& file_path,
        std::filesystem::path const& new_file_path
    )
    {
        std::uint64_t const fence = iris::compiler::get_processed_files(jit_runner);
        std::filesystem::rename(file_path, new_file_path);
        iris::compiler::wait_for(jit_runner, fence + 2);
    }

    TEST_CASE("Run JIT and modify code", "[JIT]")
    {
        SKIP();

        std::filesystem::path const root_directory = std::filesystem::temp_directory_path() / "iris_test" / "jit_modify_code";

        if (std::filesystem::exists(root_directory))
            std::filesystem::remove_all(root_directory);

        std::filesystem::create_directories(root_directory);

        std::filesystem::path const build_directory_path = root_directory / "build";
        std::filesystem::create_directories(build_directory_path);

        std::filesystem::path const artifact_configuration_file_path = root_directory / "iris_artifact.json";
        iris::compiler::Artifact const artifact
        {
            .file_path = artifact_configuration_file_path,
            .name = "iris_artifact.json",
            .version = {
                .major = 0,
                .minor = 1,
                .patch = 0
            },
            .type = iris::compiler::Artifact_type::Executable,
            .dependencies = {},
            .sources = {
                iris::compiler::Source_group
                {
                    .data = iris::compiler::Iris_source_group{},
                    .include = {"./**/*.iris"}
                }
            },
            .info = iris::compiler::Executable_info
            {
                .source = "main.iris",
                .entry_point = "main",    
            }
        };

        iris::compiler::write_artifact_to_file(artifact, artifact_configuration_file_path);

        std::filesystem::path const main_file_path = root_directory / "main.iris";

        // TODO change module name to test.main
        std::string_view const initial_code = R"(    
            module test;

            function get_result() -> (result: Int32)
            {
                return 10;
            }

            export function main() -> (result: Int32)
            {
                return get_result();
            }
        )";
        iris::common::write_to_file(main_file_path, initial_code);

        iris::compiler::Target const target = iris::compiler::get_default_target();
        iris::compiler::Compilation_options const compilation_options =
        {
            .target_triple = std::nullopt,
            .is_optimized = false,
            .debug = false,
        };
        std::unique_ptr<iris::compiler::JIT_runner> jit_runner = iris::compiler::setup_jit_and_watch(artifact_configuration_file_path, {}, build_directory_path, {}, target, compilation_options);

        int(*function_pointer)() = iris::compiler::get_function<int(*)()>(*jit_runner, "test_main");
        REQUIRE(function_pointer != nullptr);

        int const first_result = function_pointer();
        CHECK(first_result == 10);

        std::string_view const new_code = R"(
            module test;

            function get_result() -> (result: Int32)
            {
                return 20;
            }

            export function main() -> (result: Int32)
            {
                return get_result();
            }
        )";
        write_to_file_and_wait(*jit_runner, main_file_path, new_code);

        int const second_result = function_pointer();
        CHECK(second_result == 20);
    }

    TEST_CASE("Run JIT with multiple modules", "[JIT]")
    {
        SKIP();

        std::filesystem::path const root_directory = std::filesystem::temp_directory_path() / "iris_test" / "jit_multiple_modules";

        if (std::filesystem::exists(root_directory))
            std::filesystem::remove_all(root_directory);

        std::filesystem::create_directories(root_directory);

        std::filesystem::path const build_directory_path = root_directory / "build";
        std::filesystem::create_directories(build_directory_path);

        std::filesystem::path const artifact_configuration_file_path = root_directory / "iris_artifact.json";
        iris::compiler::Artifact const artifact
        {
            .file_path = artifact_configuration_file_path,
            .name = "iris_artifact.json",
            .version = {
                .major = 0,
                .minor = 1,
                .patch = 0
            },
            .type = iris::compiler::Artifact_type::Executable,
            .dependencies = {},
            .sources = {
                iris::compiler::Source_group
                {
                    .data = iris::compiler::Iris_source_group{},
                    .include = {"./**/*.iris"}
                }
            },
            .info = iris::compiler::Executable_info
            {
                .source = "m0.iris",
                .entry_point = "m0_main",
            }
        };

        iris::compiler::write_artifact_to_file(artifact, artifact_configuration_file_path);

        std::filesystem::path const m0_file_path = root_directory / "m0.iris";
        std::string_view const m0_code = R"(    
            module m0;

            import m1 as m1;

            export function main() -> (result: Int32)
            {
                return m1.get_result();
            }
        )";
        iris::common::write_to_file(m0_file_path, m0_code);

        std::filesystem::path const m1_file_path = root_directory / "m1.iris";
        std::string_view const m1_code = R"(    
            module m1;

            export function get_result() -> (result: Int32)
            {
                return 5;
            }
        )";
        iris::common::write_to_file(m1_file_path, m1_code);

        iris::compiler::Target const target = iris::compiler::get_default_target();
        iris::compiler::Compilation_options const compilation_options =
        {
            .target_triple = std::nullopt,
            .is_optimized = false,
            .debug = false,
        };
        std::unique_ptr<iris::compiler::JIT_runner> jit_runner = iris::compiler::setup_jit_and_watch(artifact_configuration_file_path, {}, build_directory_path, {}, target, compilation_options);

        int(*function_pointer)() = iris::compiler::get_function<int(*)()>(*jit_runner, "m0_main");
        REQUIRE(function_pointer != nullptr);

        int const result = function_pointer();
        CHECK(result == 5);
    }

    TEST_CASE("Run JIT that uses a repository", "[JIT]")
    {
        SKIP();

        std::filesystem::path const root_directory = std::filesystem::temp_directory_path() / "iris_test" / "jit_repository_modules";

        if (std::filesystem::exists(root_directory))
            std::filesystem::remove_all(root_directory);

        std::filesystem::create_directories(root_directory);

        std::filesystem::path const build_directory_path = root_directory / "build";
        std::filesystem::create_directories(build_directory_path);

        std::filesystem::path const artifact_configuration_file_path = root_directory / "iris_artifact.json";
        iris::compiler::Artifact const artifact
        {
            .file_path = artifact_configuration_file_path,
            .name = "iris_artifact.json",
            .version = {
                .major = 0,
                .minor = 1,
                .patch = 0
            },
            .type = iris::compiler::Artifact_type::Executable,
            .dependencies =
            {
                {
                    .artifact_name = "C_standard_library"
                }
            },
            .sources = {
                iris::compiler::Source_group
                {
                    .data = iris::compiler::Iris_source_group{},
                    .include = {"./**/*.iris"}
                }
            },
            .info = iris::compiler::Executable_info
            {
                .source = "m0.iris",
                .entry_point = "m0_main",
            }
        };

        iris::compiler::write_artifact_to_file(artifact, artifact_configuration_file_path);

        std::filesystem::path const m0_file_path = root_directory / "m0.iris";
        std::string_view const m0_code = R"(    
            module m0;

            import c.stdio as stdio;

            export function main() -> (result: Int32)
            {
                stdio.puts("hello!"c);
                return 0;
            }
        )";
        iris::common::write_to_file(m0_file_path, m0_code);

        std::array<std::filesystem::path, 1> repositories =
        {
            g_standard_repository_file_path
        };

        std::pmr::vector<std::filesystem::path> header_search_paths =
            iris::common::get_default_header_search_directories();

        iris::compiler::Target const target = iris::compiler::get_default_target();
        iris::compiler::Compilation_options const compilation_options =
        {
            .target_triple = std::nullopt,
            .is_optimized = false,
            .debug = false,
        };
        std::unique_ptr<iris::compiler::JIT_runner> jit_runner = iris::compiler::setup_jit_and_watch(artifact_configuration_file_path, repositories, build_directory_path, header_search_paths, target, compilation_options);

        int(*function_pointer)() = iris::compiler::get_function<int(*)()>(*jit_runner, "m0_main");
        REQUIRE(function_pointer != nullptr);

        int const result = function_pointer();
        CHECK(result == 0);
    }

    TEST_CASE("Run JIT program that contains errors", "[JIT]")
    {
        SKIP();

        std::filesystem::path const root_directory = std::filesystem::temp_directory_path() / "iris_test" / "jit_errors";

        if (std::filesystem::exists(root_directory))
            std::filesystem::remove_all(root_directory);

        std::filesystem::create_directories(root_directory);

        std::filesystem::path const build_directory_path = root_directory / "build";
        std::filesystem::create_directories(build_directory_path);

        std::filesystem::path const artifact_configuration_file_path = root_directory / "iris_artifact.json";
        iris::compiler::Artifact const artifact
        {
            .file_path = artifact_configuration_file_path,
            .name = "iris_artifact.json",
            .version = {
                .major = 0,
                .minor = 1,
                .patch = 0
            },
            .type = iris::compiler::Artifact_type::Executable,
            .dependencies = {},
            .sources = {
                iris::compiler::Source_group
                {
                    .data = iris::compiler::Iris_source_group{},
                    .include = {"./**/*.iris"}
                }
            },
            .info = iris::compiler::Executable_info
            {
                .source = "m0.iris",
                .entry_point = "m0_main",
            }
        };

        iris::compiler::write_artifact_to_file(artifact, artifact_configuration_file_path);

        std::filesystem::path const m0_file_path = root_directory / "m0.iris";
        std::string_view const m0_code_with_errors = R"(
            module m0;

            export function main() -> (result: Int32)
            {
                ret
            }
        )";
        iris::common::write_to_file(m0_file_path, m0_code_with_errors);

        iris::compiler::Target const target = iris::compiler::get_default_target();
        iris::compiler::Compilation_options const compilation_options =
        {
            .target_triple = std::nullopt,
            .is_optimized = false,
            .debug = false,
        };
        std::unique_ptr<iris::compiler::JIT_runner> jit_runner = iris::compiler::setup_jit_and_watch(artifact_configuration_file_path, {}, build_directory_path, {}, target, compilation_options);

        {
            int(*function_pointer)() = iris::compiler::get_function<int(*)()>(*jit_runner, "m0_main");
            CHECK(function_pointer == nullptr);
        }

        std::string_view const m0_code_without_errors = R"(
            module m0;

            export function main() -> (result: Int32)
            {
                return 10;
            }
        )";
        write_to_file_and_wait(*jit_runner, m0_file_path, m0_code_without_errors);


        int(*function_pointer)() = iris::compiler::get_function<int(*)()>(*jit_runner, "m0_main");
        REQUIRE(function_pointer != nullptr);

        {
            int const result = function_pointer();
            CHECK(result == 10);
        }

        std::string_view const m0_code_with_errors_2 = R"(
            module m0;

            export function main() -> (result: Int32)
            {
                return foo;
            }
        )";
        write_to_file_and_wait(*jit_runner, m0_file_path, m0_code_with_errors_2);

        {
            int const result = function_pointer();
            CHECK(result == 10);
        }

        std::string_view const m0_code_without_errors_2 = R"(
            module m0;

            export function main() -> (result: Int32)
            {
                return 5;
            }
        )";
        write_to_file_and_wait(*jit_runner, m0_file_path, m0_code_without_errors_2);

        {
            int const result = function_pointer();
            CHECK(result == 5);
        }
    }

    TEST_CASE("Run JIT program that uses structs", "[JIT]")
    {
        SKIP();

        std::filesystem::path const root_directory = std::filesystem::temp_directory_path() / "iris_test" / "jit_structs";

        if (std::filesystem::exists(root_directory))
            std::filesystem::remove_all(root_directory);

        std::filesystem::create_directories(root_directory);

        std::filesystem::path const build_directory_path = root_directory / "build";
        std::filesystem::create_directories(build_directory_path);

        std::filesystem::path const artifact_configuration_file_path = root_directory / "iris_artifact.json";
        iris::compiler::Artifact const artifact
        {
            .file_path = artifact_configuration_file_path,
            .name = "iris_artifact.json",
            .version = {
                .major = 0,
                .minor = 1,
                .patch = 0
            },
            .type = iris::compiler::Artifact_type::Executable,
            .dependencies = {},
            .sources = {
                iris::compiler::Source_group
                {
                    .data = iris::compiler::Iris_source_group{},
                    .include = {"./**/*.iris"}
                }
            },
            .info = iris::compiler::Executable_info
            {
                .source = "m0.iris",
                .entry_point = "m0_main",
            }
        };

        iris::compiler::write_artifact_to_file(artifact, artifact_configuration_file_path);

        std::filesystem::path const m0_file_path = root_directory / "m0.iris";
        std::string_view const m0_code = R"(
            module m0;

            import m1 as m1;

            export function main() -> (result: Int32)
            {
                var foo: m1.Foo = {};
                return foo.a;
            }
        )";
        iris::common::write_to_file(m0_file_path, m0_code);

        std::filesystem::path const m1_file_path = root_directory / "m1.iris";
        std::string_view const m1_code = R"(
            module m1;

            export struct Foo
            {
                a: Int32 = 3;
            }
        )";
        iris::common::write_to_file(m1_file_path, m1_code);

        iris::compiler::Target const target = iris::compiler::get_default_target();
        iris::compiler::Compilation_options const compilation_options =
        {
            .target_triple = std::nullopt,
            .is_optimized = false,
            .debug = false,
        };
        std::unique_ptr<iris::compiler::JIT_runner> jit_runner = iris::compiler::setup_jit_and_watch(artifact_configuration_file_path, {}, build_directory_path, {}, target, compilation_options);

        {
            int(*function_pointer)() = iris::compiler::get_function<int(*)()>(*jit_runner, "m0_main");
            REQUIRE(function_pointer != nullptr);

            int const result = function_pointer();
            CHECK(result == 3);
        }
    }

    TEST_CASE("Run JIT program that updates a struct", "[JIT]")
    {
        SKIP();

        std::filesystem::path const root_directory = std::filesystem::temp_directory_path() / "iris_test" / "jit_update_structs";

        if (std::filesystem::exists(root_directory))
            std::filesystem::remove_all(root_directory);

        std::filesystem::create_directories(root_directory);

        std::filesystem::path const build_directory_path = root_directory / "build";
        std::filesystem::create_directories(build_directory_path);

        std::filesystem::path const artifact_configuration_file_path = root_directory / "iris_artifact.json";
        iris::compiler::Artifact const artifact
        {
            .file_path = artifact_configuration_file_path,
            .name = "iris_artifact.json",
            .version = {
                .major = 0,
                .minor = 1,
                .patch = 0
            },
            .type = iris::compiler::Artifact_type::Executable,
            .dependencies = {},
            .sources = {
                iris::compiler::Source_group
                {
                    .data = iris::compiler::Iris_source_group{},
                    .include = {"./**/*.iris"}
                }
            },
            .info = iris::compiler::Executable_info
            {
                .source = "m0.iris",
                .entry_point = "m0_main",
            }
        };

        iris::compiler::write_artifact_to_file(artifact, artifact_configuration_file_path);

        std::filesystem::path const m0_file_path = root_directory / "m0.iris";
        std::string_view const m0_code = R"(
            module m0;

            import m1 as m1;

            export function main() -> (result: Int32)
            {
                var foo: m1.Foo = {};
                return foo.a;
            }
        )";
        iris::common::write_to_file(m0_file_path, m0_code);

        std::filesystem::path const m1_file_path = root_directory / "m1.iris";
        std::string_view const m1_code = R"(
            module m1;

            export struct Foo
            {
                a: Int32 = 3;
            }
        )";
        iris::common::write_to_file(m1_file_path, m1_code);

        iris::compiler::Target const target = iris::compiler::get_default_target();
        iris::compiler::Compilation_options const compilation_options =
        {
            .target_triple = std::nullopt,
            .is_optimized = false,
            .debug = false,
        };
        std::unique_ptr<iris::compiler::JIT_runner> jit_runner = iris::compiler::setup_jit_and_watch(artifact_configuration_file_path, {}, build_directory_path, {}, target, compilation_options);

        {
            int(*function_pointer)() = iris::compiler::get_function<int(*)()>(*jit_runner, "m0_main");
            REQUIRE(function_pointer != nullptr);

            int const result = function_pointer();
            CHECK(result == 3);
        }

        std::string_view const m1_new_code = R"(
            module m1;

            export struct Foo
            {
                a: Int32 = 5;
            }
        )";
        write_to_file_and_wait(*jit_runner, m1_file_path, m1_new_code);

        {
            int(*function_pointer)() = iris::compiler::get_function<int(*)()>(*jit_runner, "m0_main");
            REQUIRE(function_pointer != nullptr);

            int const result = function_pointer();
            CHECK(result == 5);
        }
    }

    TEST_CASE("Run JIT program and create a new file", "[JIT]")
    {
        SKIP();

        std::filesystem::path const root_directory = std::filesystem::temp_directory_path() / "iris_test" / "jit_create_files";

        if (std::filesystem::exists(root_directory))
            std::filesystem::remove_all(root_directory);

        std::filesystem::create_directories(root_directory);

        std::filesystem::path const build_directory_path = root_directory / "build";
        std::filesystem::create_directories(build_directory_path);

        std::filesystem::path const artifact_configuration_file_path = root_directory / "iris_artifact.json";
        iris::compiler::Artifact const artifact
        {
            .file_path = artifact_configuration_file_path,
            .name = "iris_artifact.json",
            .version = {
                .major = 0,
                .minor = 1,
                .patch = 0
            },
            .type = iris::compiler::Artifact_type::Executable,
            .dependencies = {},
            .sources = {
                iris::compiler::Source_group
                {
                    .data = iris::compiler::Iris_source_group{},
                    .include = {"./**/*.iris"}
                }
            },
            .info = iris::compiler::Executable_info
            {
                .source = "m0.iris",
                .entry_point = "m0_main",
            }
        };

        iris::compiler::write_artifact_to_file(artifact, artifact_configuration_file_path);

        std::filesystem::path const m0_file_path = root_directory / "m0.iris";
        std::string_view const m0_code = R"(
            module m0;

            import m1 as m1;

            export function main() -> (result: Int32)
            {
                var foo: m1.Foo = {};
                return m1.get_a(foo);
            }
        )";
        iris::common::write_to_file(m0_file_path, m0_code);

        iris::compiler::Target const target = iris::compiler::get_default_target();
        iris::compiler::Compilation_options const compilation_options =
        {
            .target_triple = std::nullopt,
            .is_optimized = false,
            .debug = false,
        };
        std::unique_ptr<iris::compiler::JIT_runner> jit_runner = iris::compiler::setup_jit_and_watch(artifact_configuration_file_path, {}, build_directory_path, {}, target, compilation_options);

        {
            int(*function_pointer)() = iris::compiler::get_function<int(*)()>(*jit_runner, "m0_main");
            CHECK(function_pointer == nullptr);
        }

        std::filesystem::path const m1_file_path = root_directory / "m1.iris";
        std::string_view const m1_code = R"(
            module m1;

            export struct Foo
            {
                a: Int32 = 3;
            }

            export function get_a(foo: Foo) -> (result: Int32)
            {
                return foo.a;
            }
        )";
        write_to_file_and_wait(*jit_runner, m1_file_path, m1_code);

        {
            int(*function_pointer)() = iris::compiler::get_function<int(*)()>(*jit_runner, "m0_main");
            REQUIRE(function_pointer != nullptr);

            int const result = function_pointer();
            CHECK(result == 3);
        }
    }

    TEST_CASE("Run JIT program and remove a file", "[JIT]")
    {
        SKIP();

        std::filesystem::path const root_directory = std::filesystem::temp_directory_path() / "iris_test" / "jit_remove_files";

        if (std::filesystem::exists(root_directory))
            std::filesystem::remove_all(root_directory);

        std::filesystem::create_directories(root_directory);

        std::filesystem::path const build_directory_path = root_directory / "build";
        std::filesystem::create_directories(build_directory_path);

        std::filesystem::path const artifact_configuration_file_path = root_directory / "iris_artifact.json";
        iris::compiler::Artifact const artifact
        {
            .file_path = artifact_configuration_file_path,
            .name = "iris_artifact.json",
            .version = {
                .major = 0,
                .minor = 1,
                .patch = 0
            },
            .type = iris::compiler::Artifact_type::Executable,
            .dependencies = {},
            .sources = {
                iris::compiler::Source_group
                {
                    .data = iris::compiler::Iris_source_group{},
                    .include = {"./**/*.iris"}
                }
            },
            .info = iris::compiler::Executable_info
            {
                .source = "m0.iris",
                .entry_point = "m0_main",
            }
        };

        iris::compiler::write_artifact_to_file(artifact, artifact_configuration_file_path);

        std::filesystem::path const m0_file_path = root_directory / "m0.iris";
        std::string_view const m0_code = R"(
            module m0;

            import m1 as m1;

            export function main() -> (result: Int32)
            {
                return m1.get_result();
            }
        )";
        iris::common::write_to_file(m0_file_path, m0_code);

        std::filesystem::path const m1_file_path = root_directory / "m1.iris";
        std::string_view const m1_code = R"(
            module m1;

            export function get_result() -> (result: Int32)
            {
                return 3;
            }
        )";
        iris::common::write_to_file(m1_file_path, m1_code);

        iris::compiler::Target const target = iris::compiler::get_default_target();
        iris::compiler::Compilation_options const compilation_options =
        {
            .target_triple = std::nullopt,
            .is_optimized = false,
            .debug = false,
        };
        std::unique_ptr<iris::compiler::JIT_runner> jit_runner = iris::compiler::setup_jit_and_watch(artifact_configuration_file_path, {}, build_directory_path, {}, target, compilation_options);

        {
            int(*function_pointer)() = iris::compiler::get_function<int(*)()>(*jit_runner, "m0_main");
            REQUIRE(function_pointer != nullptr);

            int const result = function_pointer();
            CHECK(result == 3);
        }

        remove_file_and_wait(*jit_runner, m1_file_path);

        {
            int(*function_pointer)() = iris::compiler::get_function<int(*)()>(*jit_runner, "m0_main");
            REQUIRE(function_pointer != nullptr);

            int const result = function_pointer();
            CHECK(result == 3);
        }

        std::string_view const m1_new_code = R"(
            module m1;

            export function get_result() -> (result: Int32)
            {
                return 5;
            }
        )";
        write_to_file_and_wait(*jit_runner, m1_file_path, m1_new_code);

        {
            int(*function_pointer)() = iris::compiler::get_function<int(*)()>(*jit_runner, "m0_main");
            REQUIRE(function_pointer != nullptr);

            int const result = function_pointer();
            CHECK(result == 5);
        }
    }

    TEST_CASE("Run JIT program and rename a file", "[JIT]")
    {
        SKIP();

        std::filesystem::path const root_directory = std::filesystem::temp_directory_path() / "iris_test" / "jit_rename_files";

        if (std::filesystem::exists(root_directory))
            std::filesystem::remove_all(root_directory);

        std::filesystem::create_directories(root_directory);

        std::filesystem::path const build_directory_path = root_directory / "build";
        std::filesystem::create_directories(build_directory_path);

        std::filesystem::path const artifact_configuration_file_path = root_directory / "iris_artifact.json";
        iris::compiler::Artifact const artifact
        {
            .file_path = artifact_configuration_file_path,
            .name = "iris_artifact.json",
            .version = {
                .major = 0,
                .minor = 1,
                .patch = 0
            },
            .type = iris::compiler::Artifact_type::Executable,
            .dependencies = {},
            .sources = {
                iris::compiler::Source_group
                {
                    .data = iris::compiler::Iris_source_group{},
                    .include = {"./**/*.iris"}
                }
            },
            .info = iris::compiler::Executable_info
            {
                .source = "m0.iris",
                .entry_point = "m0_main",
            }
        };

        iris::compiler::write_artifact_to_file(artifact, artifact_configuration_file_path);

        std::filesystem::path const m0_file_path = root_directory / "m0.iris";
        std::string_view const m0_code = R"(
            module m0;

            import m1 as m1;

            export function main() -> (result: Int32)
            {
                return m1.get_result();
            }
        )";
        iris::common::write_to_file(m0_file_path, m0_code);

        std::filesystem::path const m1_file_path = root_directory / "m1.iris";
        std::string_view const m1_code = R"(
            module m1;

            export function get_result() -> (result: Int32)
            {
                return 3;
            }
        )";
        iris::common::write_to_file(m1_file_path, m1_code);

        iris::compiler::Target const target = iris::compiler::get_default_target();
        iris::compiler::Compilation_options const compilation_options =
        {
            .target_triple = std::nullopt,
            .is_optimized = false,
            .debug = false,
        };
        std::unique_ptr<iris::compiler::JIT_runner> jit_runner = iris::compiler::setup_jit_and_watch(artifact_configuration_file_path, {}, build_directory_path, {}, target, compilation_options);

        {
            int(*function_pointer)() = iris::compiler::get_function<int(*)()>(*jit_runner, "m0_main");
            REQUIRE(function_pointer != nullptr);

            int const result = function_pointer();
            CHECK(result == 3);
        }

        std::filesystem::path const m1_new_file_path = root_directory / "m1_renamed.iris";
        rename_file_and_wait(*jit_runner, m1_file_path, m1_new_file_path);

        {
            int(*function_pointer)() = iris::compiler::get_function<int(*)()>(*jit_runner, "m0_main");
            REQUIRE(function_pointer != nullptr);

            int const result = function_pointer();
            CHECK(result == 3);
        }
    }

    // TODO test making changes to Artifact
    // TODO test making changes to Repository
}
