import iris.compiler.artifact;

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <unordered_map>
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

    TEST_CASE("Artifact parser substitutes environment variables in supported fields", "[Artifact]")
    {
        std::filesystem::path const temporary_directory = create_clean_temporary_directory("iris_artifact_tests_environment_variables_substitution");
        std::filesystem::path const artifact_path = temporary_directory / "iris_artifact.json";

        std::ofstream file{ artifact_path };
        file << R"({
  "name": "my_library",
  "version": "0.1.0",
  "type": "library",
  "public_include_directories": ["${PROJECT_ROOT}/include"],
  "sources": [
    {
      "type": "import_c_header",
      "headers": [
        {
          "name": "vulkan",
          "header": "${VULKAN_SDK}/Include/vulkan/vulkan_core.h"
        }
      ],
      "search_paths": ["${VULKAN_SDK}/Include"],
      "additional_flags": ["-I${VULKAN_SDK}/Include"]
    },
    {
      "type": "export_c_header",
      "output_directory": "${PROJECT_ROOT}/generated"
    }
  ],
  "library": {
    "external_libraries": {
      "windows-dynamic-${CONFIG}": ["${VULKAN_SDK}/Lib/vulkan-1.lib"]
    }
  },
  "copy": [
    {
      "source": "${VULKAN_SDK}/Bin/vulkan-1.dll",
      "destination": "${PROJECT_ROOT}/bin/vulkan-1.dll"
    }
  ]
})";
        file.close();

        std::pmr::unordered_map<std::pmr::string, std::pmr::string> const environment_variables
        {
            {"PROJECT_ROOT", "C:/src/project"},
            {"VULKAN_SDK", "C:/SDK/Vulkan"},
            {"CONFIG", "debug"},
        };

        Artifact const artifact = get_artifact(artifact_path, environment_variables);

        REQUIRE(artifact.public_include_directories.size() == 1);
        CHECK(artifact.public_include_directories[0] == std::filesystem::path{"C:/src/project/include"});

        REQUIRE(artifact.sources.size() == 2);

        REQUIRE(std::holds_alternative<Import_c_header_source_group>(*artifact.sources[0].data));
        Import_c_header_source_group const& import_group = std::get<Import_c_header_source_group>(*artifact.sources[0].data);
        REQUIRE(import_group.c_headers.size() == 1);
        CHECK(import_group.c_headers[0].header == "C:/SDK/Vulkan/Include/vulkan/vulkan_core.h");
        REQUIRE(import_group.search_paths.size() == 1);
        CHECK(import_group.search_paths[0] == std::filesystem::path{"C:/SDK/Vulkan/Include"});
        REQUIRE(artifact.sources[0].additional_flags.size() == 1);
        CHECK(artifact.sources[0].additional_flags[0] == "-IC:/SDK/Vulkan/Include");

        REQUIRE(std::holds_alternative<Export_c_header_source_group>(*artifact.sources[1].data));
        Export_c_header_source_group const& export_group = std::get<Export_c_header_source_group>(*artifact.sources[1].data);
        REQUIRE(export_group.output_directory.has_value());
        CHECK(export_group.output_directory.value() == std::filesystem::path{"C:/src/project/generated"});

        REQUIRE(artifact.info.has_value());
        REQUIRE(std::holds_alternative<Library_info>(*artifact.info));
        Library_info const& library_info = std::get<Library_info>(*artifact.info);
        CHECK(library_info.external_libraries.contains("windows-dynamic-debug"));
        auto const range = library_info.external_libraries.equal_range("windows-dynamic-debug");
        REQUIRE(range.first != range.second);
        CHECK(range.first->second == "C:/SDK/Vulkan/Lib/vulkan-1.lib");

        REQUIRE(artifact.copy_entries.size() == 1);
        CHECK(artifact.copy_entries[0].source == std::filesystem::path{"C:/SDK/Vulkan/Bin/vulkan-1.dll"});
        CHECK(artifact.copy_entries[0].destination == std::filesystem::path{"C:/src/project/bin/vulkan-1.dll"});
    }

    TEST_CASE("Artifact parser does not substitute include patterns", "[Artifact]")
    {
        std::filesystem::path const temporary_directory = create_clean_temporary_directory("iris_artifact_tests_environment_variables_include_not_substituted");
        std::filesystem::path const artifact_path = temporary_directory / "iris_artifact.json";

        std::ofstream file{ artifact_path };
        file << R"({
  "name": "my_library",
  "version": "0.1.0",
  "type": "library",
  "sources": [
    {
      "type": "iris",
      "include": ["${PROJECT_ROOT}/**/*.iris"]
    }
  ]
})";
        file.close();

        std::pmr::unordered_map<std::pmr::string, std::pmr::string> const environment_variables
        {
            {"PROJECT_ROOT", "C:/src/project"},
        };

        Artifact const artifact = get_artifact(artifact_path, environment_variables);
        REQUIRE(artifact.sources.size() == 1);
        REQUIRE(artifact.sources[0].include.size() == 1);
        CHECK(artifact.sources[0].include[0] == "${PROJECT_ROOT}/**/*.iris");
    }

    TEST_CASE("Artifact parser rejects missing environment variable", "[Artifact]")
    {
        std::filesystem::path const temporary_directory = create_clean_temporary_directory("iris_artifact_tests_environment_variables_missing");
        std::filesystem::path const artifact_path = temporary_directory / "iris_artifact.json";

        std::ofstream file{ artifact_path };
        file << R"({
  "name": "my_app",
  "version": "0.1.0",
  "type": "executable",
  "executable": {
    "source": "${PROJECT_ROOT}/main.iris"
  }
})";
        file.close();

        std::pmr::unordered_map<std::pmr::string, std::pmr::string> const environment_variables{};
        CHECK_THROWS(get_artifact(artifact_path, environment_variables));
    }

    TEST_CASE("Artifact writer outputs expanded values after parsing substitutions", "[Artifact]")
    {
        std::filesystem::path const temporary_directory = create_clean_temporary_directory("iris_artifact_tests_environment_variables_write_expanded");
        std::filesystem::path const artifact_path = temporary_directory / "iris_artifact.json";

        std::ofstream file{ artifact_path };
        file << R"({
  "name": "my_app",
  "version": "0.1.0",
  "type": "executable",
  "public_include_directories": ["${PROJECT_ROOT}/include"],
  "executable": {
    "source": "${PROJECT_ROOT}/main.iris"
  }
})";
        file.close();

        std::pmr::unordered_map<std::pmr::string, std::pmr::string> const environment_variables
        {
            {"PROJECT_ROOT", "C:/src/project"},
        };

        Artifact const artifact = get_artifact(artifact_path, environment_variables);
        write_artifact_to_file(artifact, artifact_path);

        std::ifstream output_file{ artifact_path };
        std::string output_data{std::istreambuf_iterator<char>{output_file}, std::istreambuf_iterator<char>{}};
        CHECK(output_data.find("${") == std::string::npos);
        CHECK(output_data.find("C:/src/project/main.iris") != std::string::npos);
    }
}
