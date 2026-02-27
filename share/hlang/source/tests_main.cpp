#include <cstdio>
#include <cstring>
#include <filesystem>
#include <numeric>
#include <regex>
#include <optional>
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
    return { tests, count };
}

static std::optional<std::string_view> search_argument(int const argc, char const* const argv[], std::string_view const name)
{
    for (int index = 0; index < argc; ++index)
    {
        std::string_view const current = argv[index];
        if (current.starts_with(name))
            return current;
    }

    return std::nullopt;
}

static bool should_list_tests(int const argc, char const* const argv[])
{
    std::optional<std::string_view> const argument = search_argument(argc, argv, "--list-tests");
    return argument.has_value();
}

static bool should_output_json(int const argc, char const* const argv[])
{
    std::optional<std::string_view> const argument = search_argument(argc, argv, "--output-format");
    if (argument.has_value())
        return argument->starts_with("--output-format=json");

    return false;
}

static std::filesystem::path get_output_json_file_path(int const argc, char const* const argv[])
{
    std::string_view const argument_start = "--output-format=json:";
    std::optional<std::string_view> const argument = search_argument(argc, argv, argument_start);
    if (argument.has_value())
        return argument->substr(argument_start.size());

    return "test_detail.json";
}

static std::pmr::vector<std::uint64_t> filter_tests(int const argc, char const* const argv[], std::span<char const* const> const all_test_names)
{
    std::string_view const argument_prefix = "--test-name=";
    bool const has_test_name_arguments = std::any_of(argv, argv + argc, [&](std::string_view const argument) { return argument.starts_with(argument_prefix); });
    if (has_test_name_arguments)
    {
        std::pmr::vector<std::uint64_t> filtered_tests;
        filtered_tests.reserve(all_test_names.size());

        for (int index = 1; index < argc; ++index)
        {
            std::string_view const argument = argv[index];
            if (argument.starts_with(argument_prefix))
            {
                std::string_view const test_name = argument.substr(argument_prefix.size());
                auto const location = std::find_if(all_test_names.begin(), all_test_names.end(), [&](std::string_view const& current) -> bool { return current == test_name; });
                if (location != all_test_names.end())
                {
                    auto const test_index = std::distance(all_test_names.begin(), location);
                    filtered_tests.push_back(test_index);
                }
            }
        }
    }

    std::pmr::vector<std::uint64_t> filtered_tests;
    filtered_tests.resize(all_test_names.size());
    std::iota(filtered_tests.begin(), filtered_tests.end(), std::uint64_t{ 0 });
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

static void print_test_names_json(std::span<char const* const> const test_names, std::filesystem::path const& output_file_path)
{
    using namespace std::literals;

    // Write JSON matching the format expected by find_tests():
    // { "suites": [ { "name": "<module>", "tests": [ { "name":"<module>.<test>", "file":"<path>", "line": 551 } ] } ] }

    // attempt to open the target file with C API
    const std::string path_string = output_file_path.generic_string();
    FILE* output = nullptr;
    fopen_s(&output, path_string.c_str(), "w");
    if (output == nullptr)
    {
        std::fprintf(stderr, "Failed to open json output file '%s' for writing\n", path_string.c_str());
        return;
    }

    // group tests by module in order encountered
    std::pmr::vector<std::pair<std::string_view, std::pmr::vector<std::string_view>>> modules;
    modules.reserve(test_names.size());

    for (std::string_view const test : test_names)
    {
        std::uint64_t const position = test.find("."sv);
        std::string_view module = position == std::string_view::npos ? test : test.substr(0, position);
        std::string_view name = position == std::string_view::npos ? test : test.substr(position + 1);

        if (modules.empty() || modules.back().first != module)
            modules.emplace_back(module, std::pmr::vector<std::string_view>());
        modules.back().second.push_back(name);
    }

    auto json_escape = [](std::string_view s) -> std::string
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s)
        {
            switch (c)
            {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                }
                else {
                    out += c;
                }
            }
        }
        return out;
    };

    bool first_suite = true;
    std::fprintf(output, "{\"suites\": [");
    for (auto const& [module, names] : modules)
    {
        if (!first_suite)
            std::fprintf(output, ",");
        first_suite = false;

        const std::string esc_module = json_escape(module);
        std::fprintf(output, "{\"name\":\"%s\",\"tests\": [", esc_module.c_str());

        bool first_test = true;
        for (auto const& name : names)
        {
            if (!first_test) std::fprintf(output, ",");
            first_test = false;

            // TODO
            std::filesystem::path const file_name = "C:/Users/JPMMa/Desktop/source/rts-game/source/game/main.hltxt";
            std::string const file_name_string = file_name.generic_string();
            std::uint64_t const line = 551;

            std::string const full = std::string(module) + "." + std::string(name);
            const std::string esc_full = json_escape(full);
            const std::string esc_file = json_escape(file_name_string);

            std::fprintf(output, "{\"name\":\"%s\",\"file\":\"%s\",\"line\":%llu}", esc_full.c_str(), esc_file.c_str(), (unsigned long long)line);
        }

        std::fprintf(output, "]}");
    }
    std::fprintf(output, "]}\n");
    std::fclose(output);
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
        if (should_list_tests(argc, argv))
        {
            std::span<char const* const> const all_test_names = get_all_test_names();
            if (should_output_json(argc, argv))
            {
                std::filesystem::path const output_file_path = get_output_json_file_path(argc, argv);
                print_test_names_json(all_test_names, output_file_path);
            }
            else
            {
                print_test_names(all_test_names);
            }
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
