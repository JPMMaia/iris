import iris.common.archive;

#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include <catch2/catch_test_macros.hpp>

namespace iris::common
{
    static std::filesystem::path create_clean_temporary_directory(std::string_view const name)
    {
        std::filesystem::path const directory = std::filesystem::temp_directory_path() / "iris_archive_tests" / name;

        std::error_code error_code;
        std::filesystem::remove_all(directory, error_code);
        std::filesystem::create_directories(directory);

        return directory;
    }

    static void write_file(std::filesystem::path const& path, std::string_view const contents)
    {
        std::filesystem::create_directories(path.parent_path());

        std::ofstream file{path, std::ios::binary};
        file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }

    static std::string read_file(std::filesystem::path const& path)
    {
        std::ifstream file{path, std::ios::binary};
        return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    }

    static void create_source_tree(std::filesystem::path const& source_directory)
    {
        write_file(source_directory / "root.txt", "root file contents");
        write_file(source_directory / "nested" / "inner.txt", "nested file contents");
        write_file(source_directory / "nested" / "deeper" / "leaf.bin", std::string_view{"\x00\x01\x02binary\xff", 10});
        std::filesystem::create_directories(source_directory / "empty_directory");
    }

    static void check_round_trip(std::string_view const test_name, std::string_view const extension)
    {
        std::filesystem::path const working_directory = create_clean_temporary_directory(test_name);
        std::filesystem::path const source_directory = working_directory / "source";
        std::filesystem::path const archive_path = working_directory / std::format("Payload-1.0{}", extension);
        std::filesystem::path const destination_directory = working_directory / "extracted";

        create_source_tree(source_directory);

        std::optional<std::pmr::string> const create_error = create_archive_from_directory(source_directory, archive_path);
        INFO(create_error.value_or(std::pmr::string{}).c_str());
        REQUIRE(!create_error.has_value());
        REQUIRE(std::filesystem::exists(archive_path));

        std::optional<std::pmr::string> const extract_error = extract_archive(archive_path, destination_directory);
        INFO(extract_error.value_or(std::pmr::string{}).c_str());
        REQUIRE(!extract_error.has_value());

        // The archive's single root directory is stripped on extraction.
        CHECK(read_file(destination_directory / "root.txt") == "root file contents");
        CHECK(read_file(destination_directory / "nested" / "inner.txt") == "nested file contents");
        CHECK(read_file(destination_directory / "nested" / "deeper" / "leaf.bin") == std::string{"\x00\x01\x02binary\xff", 10});
        CHECK(std::filesystem::is_directory(destination_directory / "empty_directory"));
        CHECK(!std::filesystem::exists(destination_directory / "Payload-1.0"));
    }

    TEST_CASE("Round trip a directory through a .zip archive", "[Archive]")
    {
        check_round_trip("round_trip_zip", ".zip");
    }

    TEST_CASE("Round trip a directory through a .7z archive", "[Archive]")
    {
        check_round_trip("round_trip_7z", ".7z");
    }

    TEST_CASE("Extracting a missing archive reports an error", "[Archive]")
    {
        std::filesystem::path const working_directory = create_clean_temporary_directory("missing_archive");

        std::optional<std::pmr::string> const error = extract_archive(
            working_directory / "does_not_exist.7z",
            working_directory / "extracted"
        );

        REQUIRE(error.has_value());
    }

    TEST_CASE("Extracting an unsupported extension reports an error", "[Archive]")
    {
        std::filesystem::path const working_directory = create_clean_temporary_directory("unsupported_extension");

        std::filesystem::path const archive_path = working_directory / "archive.tar.bz2";
        write_file(archive_path, "not an archive");

        std::optional<std::pmr::string> const error = extract_archive(archive_path, working_directory / "extracted");

        REQUIRE(error.has_value());
    }

    TEST_CASE("Extracting a corrupt archive reports an error", "[Archive]")
    {
        std::filesystem::path const working_directory = create_clean_temporary_directory("corrupt_archive");

        std::filesystem::path const archive_path = working_directory / "corrupt.7z";
        write_file(archive_path, "7z\xBC\xAF\x27\x1C this is not a valid archive body");

        std::optional<std::pmr::string> const error = extract_archive(archive_path, working_directory / "extracted");

        REQUIRE(error.has_value());
    }

    TEST_CASE("Extracting into an existing directory is skipped", "[Archive]")
    {
        std::filesystem::path const working_directory = create_clean_temporary_directory("existing_destination");
        std::filesystem::path const source_directory = working_directory / "source";
        std::filesystem::path const archive_path = working_directory / "Payload-1.0.7z";
        std::filesystem::path const destination_directory = working_directory / "extracted";

        create_source_tree(source_directory);
        REQUIRE(!create_archive_from_directory(source_directory, archive_path).has_value());

        std::filesystem::create_directories(destination_directory);

        REQUIRE(!extract_archive(archive_path, destination_directory).has_value());
        CHECK(!std::filesystem::exists(destination_directory / "root.txt"));
    }

    TEST_CASE("Creating an archive from a missing directory reports an error", "[Archive]")
    {
        std::filesystem::path const working_directory = create_clean_temporary_directory("missing_source");

        std::optional<std::pmr::string> const error = create_archive_from_directory(
            working_directory / "does_not_exist",
            working_directory / "Payload-1.0.7z"
        );

        REQUIRE(error.has_value());
    }
}
