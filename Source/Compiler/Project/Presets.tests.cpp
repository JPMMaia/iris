import iris.compiler.expressions;
import iris.compiler.presets;

#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

namespace iris::compiler
{
    static std::filesystem::path create_clean_temporary_directory(std::string_view const name)
    {
        std::filesystem::path const path = std::filesystem::temp_directory_path() / name;
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
        return path;
    }

    TEST_CASE("Presets parser returns nullopt when file is missing", "[Presets]")
    {
        std::filesystem::path const temporary_directory = create_clean_temporary_directory("iris_presets_tests_missing");
        std::filesystem::path const presets_path = temporary_directory / "iris_presets.json";

        std::optional<Presets> const presets = try_get_presets(presets_path);

        CHECK(!presets.has_value());
    }

    TEST_CASE("Presets parser resolves relative paths and deduplicates arrays", "[Presets]")
    {
        std::filesystem::path const temporary_directory = create_clean_temporary_directory("iris_presets_tests_valid");
        std::filesystem::create_directories(temporary_directory / "repo");
        std::filesystem::create_directories(temporary_directory / "include");

        std::filesystem::path const presets_path = temporary_directory / "iris_presets.json";
        std::ofstream file{ presets_path };
        file << R"({
  "build_directory": "project_build",
  "repository_paths": [
    "repo/iris_repository.json",
    "./repo/iris_repository.json"
  ],
  "header_search_paths": [
    "include",
    "./include"
  ],
  "function_contracts": "disabled",
  "output_llvm_ir": true
})";
        file.close();

        std::optional<Presets> const presets = try_get_presets(presets_path);
        REQUIRE(presets.has_value());

        REQUIRE(presets->build_directory_path.has_value());
        CHECK(presets->build_directory_path.value() == (temporary_directory / "project_build").lexically_normal());

        REQUIRE(presets->repository_paths.size() == 1);
        CHECK(presets->repository_paths[0] == (temporary_directory / "repo" / "iris_repository.json").lexically_normal());

        REQUIRE(presets->header_search_paths.size() == 1);
        CHECK(presets->header_search_paths[0] == (temporary_directory / "include").lexically_normal());

        REQUIRE(presets->function_contract_options.has_value());
        CHECK(presets->function_contract_options.value() == Contract_options::Disabled);

        REQUIRE(presets->output_llvm_ir.has_value());
        CHECK(presets->output_llvm_ir.value());
    }

    TEST_CASE("Presets parser rejects invalid schema", "[Presets]")
    {
        std::filesystem::path const temporary_directory = create_clean_temporary_directory("iris_presets_tests_invalid");
        std::filesystem::path const presets_path = temporary_directory / "iris_presets.json";
        std::ofstream file{ presets_path };
        file << R"({
  "repository_paths": "not-an-array"
})";
        file.close();

        CHECK_THROWS(try_get_presets(presets_path));
    }
}
