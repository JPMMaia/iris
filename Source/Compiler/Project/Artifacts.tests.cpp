import iris.compiler.artifact;

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

    TEST_CASE("Artifact parser reads external_libraries", "[Artifact]")
    {
        std::filesystem::path const temporary_directory = create_clean_temporary_directory("iris_artifact_tests_external_libraries");
        std::filesystem::path const artifact_path = temporary_directory / "iris_artifact.json";

        std::ofstream file{ artifact_path };
        file << R"({
  "name": "my_library",
  "version": "0.1.0",
  "type": "library",
  "library": {
    "external_libraries": {
      "windows-dynamic-debug": ["foo_d.lib"],
      "windows-dynamic-release": ["foo.lib"]
    }
  }
})";
        file.close();

        Artifact const artifact = get_artifact(artifact_path);
        REQUIRE(artifact.info.has_value());
        REQUIRE(std::holds_alternative<Library_info>(*artifact.info));

        Library_info const& library_info = std::get<Library_info>(*artifact.info);
        CHECK(library_info.external_libraries.contains("windows-dynamic-debug"));
        CHECK(library_info.external_libraries.contains("windows-dynamic-release"));

        auto const debug_range = library_info.external_libraries.equal_range("windows-dynamic-debug");
        REQUIRE(debug_range.first != debug_range.second);
        CHECK(debug_range.first->second == "foo_d.lib");
    }

    TEST_CASE("Artifact parser ignores external_library legacy key", "[Artifact]")
    {
        std::filesystem::path const temporary_directory = create_clean_temporary_directory("iris_artifact_tests_external_library_legacy");
        std::filesystem::path const artifact_path = temporary_directory / "iris_artifact.json";

        std::ofstream file{ artifact_path };
        file << R"({
  "name": "my_library",
  "version": "0.1.0",
  "type": "library",
  "library": {
    "external_library": {
      "windows-dynamic-debug": ["foo_d.lib"]
    }
  }
})";
        file.close();

        Artifact const artifact = get_artifact(artifact_path);
        REQUIRE(artifact.info.has_value());
        REQUIRE(std::holds_alternative<Library_info>(*artifact.info));

        Library_info const& library_info = std::get<Library_info>(*artifact.info);
        CHECK(library_info.external_libraries.empty());
    }

    TEST_CASE("Artifact writer emits external_libraries and round-trips", "[Artifact]")
    {
        std::filesystem::path const temporary_directory = create_clean_temporary_directory("iris_artifact_tests_round_trip");
        std::filesystem::path const artifact_path = temporary_directory / "iris_artifact.json";

        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> external_libraries;
        external_libraries.insert(std::make_pair(std::pmr::string{"windows-dynamic-debug"}, std::pmr::string{"foo_d.lib"}));

        Artifact const artifact
        {
            .file_path = artifact_path,
            .name = "my_library",
            .version = Version{.major = 0, .minor = 1, .patch = 0},
            .type = Artifact_type::Library,
            .dependencies = {},
            .sources = {},
            .public_include_directories = {},
            .info = Library_info{.external_libraries = std::move(external_libraries)},
        };

        write_artifact_to_file(artifact, artifact_path);

        Artifact const parsed = get_artifact(artifact_path);
        REQUIRE(parsed.info.has_value());
        REQUIRE(std::holds_alternative<Library_info>(*parsed.info));

        Library_info const& library_info = std::get<Library_info>(*parsed.info);
        CHECK(library_info.external_libraries.contains("windows-dynamic-debug"));
    }

    TEST_CASE("Artifact parser still parses executable artifacts", "[Artifact]")
    {
        std::filesystem::path const temporary_directory = create_clean_temporary_directory("iris_artifact_tests_executable");
        std::filesystem::path const artifact_path = temporary_directory / "iris_artifact.json";

        std::ofstream file{ artifact_path };
        file << R"({
  "name": "my_app",
  "version": "0.1.0",
  "type": "executable",
  "sources": [
    {
      "type": "iris",
      "include": ["./**/*.iris"]
    }
  ],
  "executable": {
    "source": "main.iris"
  }
})";
        file.close();

        Artifact const artifact = get_artifact(artifact_path);
        CHECK(artifact.type == Artifact_type::Executable);
        REQUIRE(artifact.info.has_value());
        REQUIRE(std::holds_alternative<Executable_info>(*artifact.info));
        CHECK(std::get<Executable_info>(*artifact.info).source == std::filesystem::path{"main.iris"});
    }
}
