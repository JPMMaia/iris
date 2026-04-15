import iris.c_header_hash;

import iris.common;

#include <filesystem>
#include <string>

#include <catch2/catch_all.hpp>

namespace iris::c
{
    TEST_CASE("Calculates C header hash 0")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_hash" / "test_0";
        std::filesystem::create_directories(root_directory_path);

        std::string const dependency_0_content = R"(
struct My_data_0
{
    int a;
    int b;
};
)";

        std::string const dependency_1_content = R"(
#include "Dependency_0.h"

struct My_data_1
{
    int a;
    int b;
};
)";

        std::string const main_content = R"(
#include "Dependency_1.h"

struct My_data_2
{
    int a;
    int b;
};
)";

        std::filesystem::path const dependency_0_file_path = root_directory_path / "Dependency_0.h";
        iris::common::write_to_file(dependency_0_file_path, dependency_0_content);

        std::filesystem::path const dependency_1_file_path = root_directory_path / "Dependency_1.h";
        iris::common::write_to_file(dependency_1_file_path, dependency_1_content);
        
        std::filesystem::path const main_file_path = root_directory_path / "Main.h";
        iris::common::write_to_file(main_file_path, main_content);

        std::optional<std::uint64_t> const file_hash = iris::c::calculate_header_file_hash(main_file_path, std::nullopt, {});
        CHECK(*file_hash == 15268198287479747170ull);
    }
}
