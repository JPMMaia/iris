module;

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module iris.tools.tests_results_replacer;

namespace iris::tools::tests_results_replacer
{
    export struct Test_result
    {
        std::pmr::string test_name;
        std::pmr::string actual;
        std::pmr::string expected;
    };

    export std::optional<std::pmr::string> get_file_contents(std::filesystem::path const& path);
    export void write_to_file(std::filesystem::path const& path, std::string_view const content);

    export std::optional<std::pmr::string> run_tests_and_get_output(std::filesystem::path const& test_application_file_path);
    export std::pmr::vector<Test_result> extract_test_results(std::string_view const input_text);
    export std::pmr::string replace_test_contents(std::string_view const input_text, std::span<Test_result const> const test_results);
}
