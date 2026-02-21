#include <cstdio>
#include <cstring>
#include <numeric>
#include <regex>
#include <span>
#include <sstream>
#include <string_view>
#include <vector>

using Test_function_pointer = void(*)();

extern "C" uint64_t hlang_get_test_count();
extern "C" char const* const* hlang_get_test_names();
extern "C" Test_function_pointer* hlang_get_tests();

extern "C" struct hlang_test_context
{
    bool success = true;
};

static hlang_test_context* g_hlang_current_test_context;

extern "C" void hlang_test_check(bool const condition, char const* const source_file_path, uint64_t const line)
{
    if (g_hlang_current_test_context == nullptr)
        return;

    if (!condition)
        g_hlang_current_test_context->success = false;

    std::fprintf(stderr, "Test check failed @ \"%s:%llu\"\n", source_file_path, line);
    std::fflush(stderr);
}

static std::span<char const* const> get_all_test_names()
{
    std::uint64_t const count = hlang_get_test_count();
    char const* const* const tests = hlang_get_test_names();
    return {tests, count};
}

static bool should_list_tests(std::string_view const argument)
{
    if (argument == "--gtest_list_tests")
        return true;

    return false;
}

static std::pmr::vector<std::uint64_t> filter_tests(int const argc, char const* const argv[], std::span<char const* const> const all_test_names)
{
    if (argc >= 2)
    {
        std::string_view const argument = argv[1];

        if (argument.starts_with("--gtest_filter="))
        {
            std::string_view const filter_value = argument.substr(15);
            std::regex const regex(filter_value.data());
            
            std::pmr::vector<std::uint64_t> filtered_tests;
            filtered_tests.reserve(all_test_names.size());

            for (std::size_t index = 0; index < all_test_names.size(); ++index)
            {
                char const* const test_name = all_test_names[index];
                if (std::regex_match(test_name, regex) > 0)
                    filtered_tests.push_back(index);
            }

            return filtered_tests;
        }
    }

    std::pmr::vector<std::uint64_t> filtered_tests;
    filtered_tests.resize(all_test_names.size());
    std::iota(filtered_tests.begin(), filtered_tests.end(), std::uint64_t{0});
    return filtered_tests;
}

static std::pmr::vector<Test_function_pointer> get_tests_function_pointers(std::span<std::uint64_t const> const tests_indices)
{
    std::pmr::vector<Test_function_pointer> output;
    output.reserve(tests_indices.size());

    Test_function_pointer* const tests = hlang_get_tests();

    for (std::uint64_t const test_index : tests_indices)
        output.push_back(tests[test_index]);

    return output;
}

static void print_test_names(std::span<char const* const> const test_names)
{
    using namespace std::literals;

    std::stringstream stream;

    std::string_view current_module_name = "";

    for (std::string_view const test : test_names)
    {
        std::uint64_t const position = test.find("."sv);

        std::string_view const test_module_name = test.substr(0, position);
        if (test_module_name != current_module_name)
        {
            current_module_name = test_module_name;
            stream << test_module_name << ".\n";
        }

        std::string_view const test_function_name = test.substr(position + 1);
        stream << "  " << test_function_name << '\n';
    }

    std::string const output = stream.str();
    std::puts(output.c_str());
}

struct Test_results
{
    std::uint64_t failed_count = 0;
    std::uint64_t success_count = 0;
};

static Test_results run_tests(std::span<Test_function_pointer const> const tests_function_pointers)
{
    Test_results results = {};

    for (Test_function_pointer const test_function_pointer : tests_function_pointers)
    {
        hlang_test_context current_test_context = {};
        
        g_hlang_current_test_context = &current_test_context;
        test_function_pointer();
        g_hlang_current_test_context = nullptr;

        if (current_test_context.success)
            results.success_count += 1;
        else
            results.failed_count += 1;
    }

    return results;
}

void print_test_results(Test_results const& test_results)
{
    std::printf("%llu tests passed\n%llu tests failed\n", test_results.success_count, test_results.failed_count);
}

int main(int const argc, char const* const argv[])
{
    if (argc >= 2)
    {
        std::string_view const argument = argv[1];

        if (should_list_tests(argument))
        {
            std::span<char const* const> const all_test_names = get_all_test_names();
            print_test_names(all_test_names);
            return 0;
        }
    }

    std::span<char const* const> const all_test_names = get_all_test_names();
    std::pmr::vector<std::uint64_t> const filtered_test_indices = filter_tests(argc, argv, all_test_names);
    std::pmr::vector<Test_function_pointer> const tests_function_pointers = get_tests_function_pointers(filtered_test_indices);
    
    Test_results const results = run_tests(tests_function_pointers);
    print_test_results(results);

    return results.failed_count == 0 ? 0 : -1; 
}
