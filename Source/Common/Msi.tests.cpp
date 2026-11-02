import iris.common.msi;

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#define NOMINMAX
#include <windows.h>
#include <msi.h>
#include <msiquery.h>

#include <catch2/catch_test_macros.hpp>

namespace iris::common
{
    namespace
    {
        constexpr std::string_view g_upgrade_code = "{6B3C1E2A-9F4D-4C1B-8E7A-2D5F0A9B3C41}";

        std::filesystem::path create_clean_temporary_directory(std::string_view const name)
        {
            std::filesystem::path const directory = std::filesystem::temp_directory_path() / "iris_msi_tests" / name;

            std::error_code error_code;
            std::filesystem::remove_all(directory, error_code);
            std::filesystem::create_directories(directory);

            return directory;
        }

        void write_file(std::filesystem::path const& path, std::string_view const contents)
        {
            std::filesystem::create_directories(path.parent_path());

            std::ofstream file{path, std::ios::binary};
            file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        }

        std::string read_file(std::filesystem::path const& path)
        {
            std::ifstream file{path, std::ios::binary};
            return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
        }

        struct Handle
        {
            MSIHANDLE value = 0;
            ~Handle() { if (value != 0) MsiCloseHandle(value); }
        };

        // Runs a query and returns the first column of every row as strings.
        std::vector<std::wstring> query_strings(MSIHANDLE const database, wchar_t const* const sql)
        {
            Handle view;
            REQUIRE(MsiDatabaseOpenViewW(database, sql, &view.value) == ERROR_SUCCESS);
            REQUIRE(MsiViewExecute(view.value, 0) == ERROR_SUCCESS);

            std::vector<std::wstring> rows;
            while (true)
            {
                Handle record;
                if (MsiViewFetch(view.value, &record.value) != ERROR_SUCCESS)
                    break;

                wchar_t buffer[1024];
                DWORD size = 1024;
                REQUIRE(MsiRecordGetStringW(record.value, 1, buffer, &size) == ERROR_SUCCESS);
                rows.emplace_back(buffer, size);
            }
            return rows;
        }

        std::wstring query_property(MSIHANDLE const database, std::wstring_view const property)
        {
            std::wstring const sql = std::format(L"SELECT `Value` FROM `Property` WHERE `Property` = '{}'", property);
            std::vector<std::wstring> const rows = query_strings(database, sql.c_str());
            REQUIRE(rows.size() == 1);
            return rows.front();
        }

        Msi_package_options create_options(std::filesystem::path const& working_directory)
        {
            std::filesystem::path const source_directory = working_directory / "source";
            write_file(source_directory / "bin" / "tool.exe", "not really an executable");
            write_file(source_directory / "bin" / "helper.dll", "helper");
            write_file(source_directory / "data" / "A long file name.config.json", "{ \"long\": true }");
            write_file(source_directory / "data" / "nested" / "leaf.bin", std::string_view{"\x00\x01\x02binary\xff", 10});

            return Msi_package_options
            {
                .source_directory = source_directory,
                .output_path = working_directory / "Tool.msi",
                .product_name = "Iris Test Tool",
                .manufacturer = "Iris Tests",
                .upgrade_code = std::pmr::string{g_upgrade_code},
                .product_version = "1.2.3",
                .architecture = "x64",
                .path_directory = std::filesystem::path{"bin"},
                .shortcuts = { Msi_shortcut{ .target = "bin/tool.exe", .name = "Iris Test Tool" } },
            };
        }
    }

    TEST_CASE("Create an MSI and read its tables back", "[Msi]")
    {
        std::filesystem::path const working_directory = create_clean_temporary_directory("tables");
        Msi_package_options const options = create_options(working_directory);

        std::optional<std::pmr::string> const error = create_msi_from_directory(options);
        INFO(error.value_or(std::pmr::string{}).c_str());
        REQUIRE(!error.has_value());
        REQUIRE(std::filesystem::exists(options.output_path));

        Handle database;
        REQUIRE(MsiOpenDatabaseW(options.output_path.wstring().c_str(), reinterpret_cast<LPCWSTR>(MSIDBOPEN_READONLY), &database.value) == ERROR_SUCCESS);

        CHECK(query_property(database.value, L"ProductVersion") == L"1.2.3");
        CHECK(query_property(database.value, L"UpgradeCode") == L"{6B3C1E2A-9F4D-4C1B-8E7A-2D5F0A9B3C41}");
        CHECK(query_property(database.value, L"ProductName") == L"Iris Test Tool");
        CHECK(query_property(database.value, L"ALLUSERS") == L"1");

        std::vector<std::wstring> const file_names = query_strings(database.value, L"SELECT `FileName` FROM `File`");
        CHECK(file_names.size() == 4);
        CHECK(std::ranges::find(file_names, L"tool.exe") != file_names.end());
        bool const has_long_name = std::ranges::any_of(file_names, [](std::wstring const& name) { return name.ends_with(L"|A long file name.config.json"); });
        CHECK(has_long_name);

        std::vector<std::wstring> const environment = query_strings(database.value, L"SELECT `Value` FROM `Environment`");
        REQUIRE(environment.size() == 1);
        CHECK(environment.front() == L"[~];[INSTALLDIR]bin");

        std::vector<std::wstring> const shortcuts = query_strings(database.value, L"SELECT `Target` FROM `Shortcut`");
        REQUIRE(shortcuts.size() == 1);
        CHECK(shortcuts.front().starts_with(L"[#f"));

        std::vector<std::wstring> const streams = query_strings(database.value, L"SELECT `Name` FROM `_Streams`");
        CHECK(std::ranges::find(streams, L"product.cab") != streams.end());
    }

    TEST_CASE("An administrative install of the MSI extracts every file", "[Msi]")
    {
        std::filesystem::path const working_directory = create_clean_temporary_directory("admin");
        Msi_package_options const options = create_options(working_directory);

        std::optional<std::pmr::string> const error = create_msi_from_directory(options);
        INFO(error.value_or(std::pmr::string{}).c_str());
        REQUIRE(!error.has_value());

        // An administrative install only unpacks the package; nothing is registered.
        std::filesystem::path const layout = working_directory / "layout";
        INSTALLUILEVEL const previous_ui_level = MsiSetInternalUI(INSTALLUILEVEL_NONE, nullptr);
        std::wstring const command_line = std::format(L"ACTION=ADMIN TARGETDIR=\"{}\"", layout.wstring());
        UINT const result = MsiInstallProductW(options.output_path.wstring().c_str(), command_line.c_str());
        MsiSetInternalUI(previous_ui_level, nullptr);
        REQUIRE(result == ERROR_SUCCESS);

        std::filesystem::path const installed = layout / "PFiles64" / "Iris Tests" / "Iris Test Tool";
        CHECK(read_file(installed / "bin" / "tool.exe") == "not really an executable");
        CHECK(read_file(installed / "bin" / "helper.dll") == "helper");
        CHECK(read_file(installed / "data" / "A long file name.config.json") == "{ \"long\": true }");
        CHECK(read_file(installed / "data" / "nested" / "leaf.bin") == std::string{"\x00\x01\x02binary\xff", 10});
    }

    TEST_CASE("Name-based GUIDs are stable and distinct", "[Msi]")
    {
        std::optional<std::pmr::string> const first = create_name_based_guid(g_upgrade_code, "BIN/TOOL.EXE");
        std::optional<std::pmr::string> const second = create_name_based_guid(g_upgrade_code, "BIN/TOOL.EXE");
        std::optional<std::pmr::string> const other = create_name_based_guid(g_upgrade_code, "BIN/HELPER.DLL");

        REQUIRE(first.has_value());
        CHECK(first == second);
        CHECK(first != other);
        CHECK(first->size() == 38);
        CHECK(first->at(15) == '5');

        // RFC 4122 appendix B: the DNS namespace and "www.example.com".
        CHECK(create_name_based_guid("{6BA7B810-9DAD-11D1-80B4-00C04FD430C8}", "www.example.com") == std::pmr::string{"{2ED6657D-E927-568B-95E1-2665A8AEA6A2}"});

        CHECK(!create_name_based_guid("not a guid", "x").has_value());
    }

    TEST_CASE("Creating an MSI rejects a bad version", "[Msi]")
    {
        std::filesystem::path const working_directory = create_clean_temporary_directory("bad_version");
        Msi_package_options options = create_options(working_directory);
        options.product_version = "1.2.300000";

        std::optional<std::pmr::string> const error = create_msi_from_directory(options);
        REQUIRE(error.has_value());
        CHECK(!std::filesystem::exists(options.output_path));
    }
}
