#include <filesystem>
#include <optional>
#include <string_view>
#include <string>
#include <span>
#include <vector>

import iris.tools.tests_results_replacer;

using namespace iris::tools::tests_results_replacer;

int main(int const argc, char const* const* const argv)
{
    if (argc < 3)
    {
        return 1;
    }

    std::filesystem::path const test_application_file_path = argv[1];
    std::filesystem::path const test_source_file_path = argv[2];

    std::optional<std::pmr::string> const test_output = run_tests_and_get_output(test_application_file_path);
    if (!test_output.has_value())
        return 2;

    std::pmr::vector<Test_result> const test_results = extract_test_results(test_output.value());
    
    std::optional<std::pmr::string> const test_source = get_file_contents(test_source_file_path);
    if (!test_source.has_value())
        return 3;

    std::pmr::string const new_test_source = replace_test_contents(test_source.value(), test_results);

    write_to_file(test_source_file_path, new_test_source);

    return 0;
}
