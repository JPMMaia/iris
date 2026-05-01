import iris.compiler.repository;

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

    TEST_CASE("Repository parser reads name and artifacts", "[Repository]")
    {
        std::filesystem::path const temporary_directory = create_clean_temporary_directory("iris_repository_tests_parse");
        std::filesystem::path const repository_path = temporary_directory / "iris_repository.json";

        std::ofstream file{ repository_path };
        file << R"({
  "name": "my_repository",
  "artifacts": [
    {
      "name": "my_library",
      "location": "my_library/iris_artifact.json"
    },
    {
      "name": "my_app",
      "location": "my_app/iris_artifact.json"
    }
  ]
})";
        file.close();

        Repository const repository = get_repository(repository_path);

        CHECK(repository.name == "my_repository");
        CHECK(repository.artifact_to_location.size() == 2);
        CHECK(repository.artifact_to_location.contains("my_library"));
        CHECK(repository.artifact_to_location.contains("my_app"));
    }

    TEST_CASE("Repository parser resolves relative artifact paths", "[Repository]")
    {
        std::filesystem::path const temporary_directory = create_clean_temporary_directory("iris_repository_tests_relative_paths");
        std::filesystem::path const repository_path = temporary_directory / "nested" / "iris_repository.json";
        std::filesystem::create_directories(repository_path.parent_path());

        std::ofstream file{ repository_path };
        file << R"({
  "name": "my_repository",
  "artifacts": [
    {
      "name": "my_library",
      "location": "../my_library/iris_artifact.json"
    }
  ]
})";
        file.close();

        Repository const repository = get_repository(repository_path);
        auto const location = repository.artifact_to_location.find("my_library");
        REQUIRE(location != repository.artifact_to_location.end());
        CHECK(location->second == (repository_path.parent_path() / "../my_library/iris_artifact.json"));
    }

    TEST_CASE("Repository lookup returns expected location and nullopt", "[Repository]")
    {
        std::filesystem::path const temporary_directory = create_clean_temporary_directory("iris_repository_tests_lookup");
        std::filesystem::path const repository_path = temporary_directory / "iris_repository.json";

        std::ofstream file{ repository_path };
        file << R"({
  "name": "my_repository",
  "artifacts": [
    {
      "name": "my_library",
      "location": "my_library/iris_artifact.json"
    }
  ]
})";
        file.close();

        Repository const repository = get_repository(repository_path);

        std::optional<std::filesystem::path> const found = get_artifact_location({ &repository, 1 }, "my_library");
        REQUIRE(found.has_value());
        CHECK(found.value() == repository_path.parent_path() / "my_library/iris_artifact.json");

        std::optional<std::filesystem::path> const missing = get_artifact_location({ &repository, 1 }, "does_not_exist");
        CHECK(!missing.has_value());
    }
}
