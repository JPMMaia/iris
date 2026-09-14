#if !defined(_WIN32)
    // sigsetjmp/siglongjmp and SIGBUS are POSIX, not ISO C. This file is compiled with -std=c++20
    // (not gnu++20), which defines __STRICT_ANSI__ and would otherwise hide them.
    #define _POSIX_C_SOURCE 200809L
#endif

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory_resource>
#include <numeric>
#include <regex>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if !defined(_WIN32)
#include <setjmp.h>
#endif

static constexpr char const* ANSI_GREEN = "\033[32m";
static constexpr char const* ANSI_RED   = "\033[31m";
static constexpr char const* ANSI_RESET = "\033[0m";

static constexpr std::uint64_t g_maximum_listed_not_run_tests = 20;

using Test_function_pointer = void(*)();

extern "C" uint64_t iris_get_test_count();
extern "C" char const* const* iris_get_test_names();
extern "C" char const* iris_get_test_source_file(uint64_t test_index);
extern "C" uint64_t const* iris_get_test_source_file_lines();
extern "C" Test_function_pointer* iris_get_tests();

extern "C" struct iris_test_context
{
    bool success = true;
};

static iris_test_context* g_iris_current_test_context;

extern "C" void iris_test_check(bool const condition, char const* const source_file_path, uint64_t const line)
{
    if (g_iris_current_test_context == nullptr)
        return;

    if (!condition)
    {
        g_iris_current_test_context->success = false;
        std::fprintf(stderr, "Test check failed @ \"%s:%llu\"\n", source_file_path, line);
        std::fflush(stderr);
    }
}

static std::span<char const* const> get_all_test_names()
{
    std::uint64_t const count = iris_get_test_count();
    char const* const* const tests = iris_get_test_names();
    return { tests, count };
}

static std::span<std::uint64_t const> get_all_source_file_lines()
{
    std::uint64_t const count = iris_get_test_count();
    std::uint64_t const* const lines = iris_get_test_source_file_lines();
    return { lines, count };
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

static bool should_print_help(int const argc, char const* const argv[])
{
    std::optional<std::string_view> const argument = search_argument(argc, argv, "--help");
    return argument.has_value();
}

static bool should_list_tests(int const argc, char const* const argv[])
{
    std::optional<std::string_view> const argument = search_argument(argc, argv, "--list-tests");
    return argument.has_value();
}

static bool should_stop_on_crash(int const argc, char const* const argv[])
{
    std::optional<std::string_view> const argument = search_argument(argc, argv, "--stop-on-crash");
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

struct Filtered_tests
{
    std::pmr::vector<std::uint64_t> indices;
    std::pmr::vector<std::pmr::string> unknown_names;
};

static Filtered_tests filter_tests(int const argc, char const* const argv[], std::span<char const* const> const all_test_names)
{
    std::string_view const argument_prefix = "--test-name=";
    bool const has_test_name_arguments = std::any_of(argv + 1, argv + argc, [&](std::string_view const argument) { return argument.starts_with(argument_prefix); });
    if (has_test_name_arguments)
    {
        Filtered_tests filtered_tests;
        filtered_tests.indices.reserve(all_test_names.size());

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
                    filtered_tests.indices.push_back(test_index);
                }
                else
                {
                    filtered_tests.unknown_names.push_back(std::pmr::string{ test_name });
                }
            }
        }

        return filtered_tests;
    }

    Filtered_tests filtered_tests;
    filtered_tests.indices.resize(all_test_names.size());
    std::iota(filtered_tests.indices.begin(), filtered_tests.indices.end(), std::uint64_t{ 0 });
    return filtered_tests;
}

static std::pmr::vector<Test_function_pointer> get_tests_function_pointers(std::span<std::uint64_t const> const tests_indices)
{
    std::pmr::vector<Test_function_pointer> output;
    output.reserve(tests_indices.size());

    Test_function_pointer* const tests = iris_get_tests();

    for (std::uint64_t const test_index : tests_indices)
        output.push_back(tests[test_index]);

    return output;
}

static std::pmr::vector<char const*> get_filtered_test_names(std::span<std::uint64_t const> const tests_indices, std::span<char const* const> const all_test_names)
{
    std::pmr::vector<char const*> output;
    output.reserve(tests_indices.size());

    for (std::uint64_t const test_index : tests_indices)
        output.push_back(all_test_names[test_index]);

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

static void print_test_names_json(std::filesystem::path const& output_file_path)
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

    struct Test_case_info
    {
        std::string_view name;
        std::uint64_t index;
    };

    std::span<char const* const> const test_names = get_all_test_names();
    std::span<std::uint64_t const> const source_file_lines = get_all_source_file_lines();

    // group tests by module in order encountered
    std::pmr::vector<std::pair<std::string_view, std::pmr::vector<Test_case_info>>> modules;
    modules.reserve(test_names.size());

    for (std::uint64_t index = 0; index < test_names.size(); ++index)
    {
        std::string_view const test = test_names[index];
        std::uint64_t const position = test.find("."sv);
        std::string_view module = position == std::string_view::npos ? test : test.substr(0, position);
        std::string_view name = position == std::string_view::npos ? test : test.substr(position + 1);

        if (modules.empty() || modules.back().first != module)
            modules.emplace_back(module, std::pmr::vector<Test_case_info>());
        modules.back().second.push_back({name, index});
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
    for (auto const& [module, test_case_infos] : modules)
    {
        if (!first_suite)
            std::fprintf(output, ",");
        first_suite = false;

        const std::string esc_module = json_escape(module);
        std::fprintf(output, "{\"name\":\"%s\",\"tests\": [", esc_module.c_str());

        bool first_test = true;
        for (Test_case_info const& test_case_info : test_case_infos)
        {
            if (!first_test) std::fprintf(output, ",");
            first_test = false;

            std::string_view const source_file = iris_get_test_source_file(test_case_info.index);
            std::uint64_t const line = source_file_lines[test_case_info.index];

            std::string const full = std::string(module) + "." + std::string(test_case_info.name);
            const std::string full_escaped = json_escape(full);
            const std::string file_escaped = json_escape(source_file);

            std::fprintf(output, "{\"name\":\"%s\",\"file\":\"%s\",\"line\":%llu}", full_escaped.c_str(), file_escaped.c_str(), line);
        }

        std::fprintf(output, "]}");
    }
    std::fprintf(output, "]}\n");
    std::fclose(output);
}

struct Test_results
{
    std::uint64_t selected_count = 0;
    std::uint64_t run_count = 0;
    std::uint64_t success_count = 0;
    std::uint64_t failed_count = 0;
    std::uint64_t crashed_count = 0;
};

void print_test_results(Test_results const& test_results)
{
    std::uint64_t const not_run_count = test_results.selected_count - test_results.run_count;
    bool const everything_is_fine = test_results.failed_count == 0 && not_run_count == 0;

    std::printf(
        "%s%llu tests selected, %llu run, %llu passed, %llu failed (%llu crashed), %llu not run%s\n",
        everything_is_fine ? ANSI_GREEN : ANSI_RED,
        test_results.selected_count,
        test_results.run_count,
        test_results.success_count,
        test_results.failed_count,
        test_results.crashed_count,
        not_run_count,
        ANSI_RESET
    );
    std::fflush(stdout);
}

static void print_not_run_tests(std::span<char const* const> const test_names, std::uint64_t const first_not_run_index)
{
    if (first_not_run_index >= test_names.size())
        return;

    std::uint64_t const not_run_count = test_names.size() - first_not_run_index;
    std::printf("%s%llu tests were not executed:%s\n", ANSI_RED, not_run_count, ANSI_RESET);

    std::uint64_t const listed_count = not_run_count < g_maximum_listed_not_run_tests ? not_run_count : g_maximum_listed_not_run_tests;
    for (std::uint64_t index = 0; index < listed_count; ++index)
        std::printf("  %s\n", test_names[first_not_run_index + index]);

    if (listed_count < not_run_count)
        std::printf("  ... and %llu more\n", not_run_count - listed_count);

    std::fflush(stdout);
}

static char const* describe_crash_code(unsigned long const code)
{
#if defined(_WIN32)
    switch (code)
    {
    case 0xC00000FDul: return "stack overflow";
    case 0xC0000005ul: return "access violation";
    case 0xC000001Dul: return "illegal instruction";
    case 0xC0000094ul: return "integer divide by zero";
    case 0xC0000096ul: return "privileged instruction";
    case 0xC0000006ul: return "in-page error";
    case 0xC000008Cul: return "array bounds exceeded";
    case 0xC000008Eul: return "float divide by zero";
    default: return "unknown";
    }
#else
    switch (static_cast<int>(code))
    {
    case SIGSEGV: return "segmentation fault";
    case SIGBUS: return "bus error";
    case SIGFPE: return "arithmetic exception";
    case SIGILL: return "illegal instruction";
    case SIGABRT: return "aborted";
    default: return "unknown";
    }
#endif
}

static void print_crash_line(char const* const test_name, unsigned long const crash_code)
{
#if defined(_WIN32)
    std::printf("[    %sCRASH%s ] \"%s\" (%s, code 0x%08lX)\n", ANSI_RED, ANSI_RESET, test_name, describe_crash_code(crash_code), crash_code);
#else
    std::printf("[    %sCRASH%s ] \"%s\" (%s, signal %lu)\n", ANSI_RED, ANSI_RESET, test_name, describe_crash_code(crash_code), crash_code);
#endif
    std::fflush(stdout);
}

#if !defined(_WIN32)
static sigjmp_buf g_crash_jump_buffer;
static volatile std::sig_atomic_t g_crash_signal_number = 0;

static void crash_signal_handler(int const signal_number)
{
    g_crash_signal_number = signal_number;
    siglongjmp(g_crash_jump_buffer, 1);
}

static void arm_crash_signal_handlers()
{
    std::signal(SIGSEGV, crash_signal_handler);
    std::signal(SIGBUS, crash_signal_handler);
    std::signal(SIGFPE, crash_signal_handler);
    std::signal(SIGILL, crash_signal_handler);
}
#endif

// Returns 0 when the test returned normally, otherwise the SEH exception code (Windows) or the
// signal number (POSIX) that took it down.
static unsigned long invoke_test_guarded(Test_function_pointer const test)
{
#if defined(_WIN32)
    // clang-cl drives this file in CL mode, so -fms-extensions is on and __try / _exception_code()
    // need no header and no extra flag. EXCEPTION_EXECUTE_HANDLER is spelled as the literal 1 so
    // that including <windows.h> does not append /DEFAULTLIB:uuid.lib to every test link.
    __try
    {
        test();
        return 0;
    }
    __except (1)
    {
        return static_cast<unsigned long>(_exception_code());
    }
#else
    // savemask = 1: the faulting signal is blocked while the handler runs and must be unblocked on
    // the way out. signal() dispositions are re-armed before every test.
    g_crash_signal_number = 0;
    arm_crash_signal_handlers();

    if (sigsetjmp(g_crash_jump_buffer, 1) == 0)
    {
        test();
        return 0;
    }

    return static_cast<unsigned long>(g_crash_signal_number);
#endif
}

static std::span<char const* const> g_running_test_names;
static Test_results* g_running_results = nullptr;

static void abort_signal_handler(int const signal_number)
{
    std::fflush(stdout);

    if (g_running_results != nullptr && g_running_results->run_count < g_running_test_names.size())
    {
        std::printf("[    %sCRASH%s ] \"%s\" (aborted, signal %d)\n", ANSI_RED, ANSI_RESET, g_running_test_names[g_running_results->run_count], signal_number);
        std::fflush(stdout);

        g_running_results->run_count += 1;
        g_running_results->failed_count += 1;
        g_running_results->crashed_count += 1;

        print_test_results(*g_running_results);
        print_not_run_tests(g_running_test_names, g_running_results->run_count);
    }

    std::fflush(stdout);
    std::fflush(stderr);
    std::_Exit(2);
}

static Test_results run_tests(
    std::span<Test_function_pointer const> const tests_function_pointers,
    std::span<char const* const> const test_names,
    bool const stop_on_crash
)
{
    Test_results results = {};
    results.selected_count = tests_function_pointers.size();

    g_running_test_names = test_names;
    g_running_results = &results;

    for (std::uint64_t i = 0; i < tests_function_pointers.size(); ++i)
    {
        iris_test_context current_test_context = {};

        std::printf("[ RUN      ] \"%s\"\n", test_names[i]);
        std::fflush(stdout);

        g_iris_current_test_context = &current_test_context;
        unsigned long const crash_code = invoke_test_guarded(tests_function_pointers[i]);
        g_iris_current_test_context = nullptr;

        results.run_count += 1;

        if (crash_code != 0)
        {
            print_crash_line(test_names[i], crash_code);
            results.failed_count += 1;
            results.crashed_count += 1;

            if (stop_on_crash)
                break;
        }
        else if (current_test_context.success)
        {
            std::printf("[       %sOK%s ] \"%s\"\n", ANSI_GREEN, ANSI_RESET, test_names[i]);
            std::fflush(stdout);
            results.success_count += 1;
        }
        else
        {
            std::printf("[     %sFAIL%s ] \"%s\"\n", ANSI_RED, ANSI_RESET, test_names[i]);
            std::fflush(stdout);
            results.failed_count += 1;
        }
    }

    g_running_results = nullptr;
    g_running_test_names = {};

    return results;
}

static void print_help(char const* const program_name)
{
    std::printf(
        "Usage: %s [OPTIONS]\n"
        "\n"
        "Options:\n"
        "  --help                              Show this help message and exit\n"
        "  --list-tests                        List all available test names and exit\n"
        "  --output-format=json[:<file>]       Write test list as JSON (default file: test_detail.json); use with --list-tests\n"
        "  --test-name=<name>                  Run only the test with this name (repeatable)\n"
        "  --stop-on-crash                     Stop the run when a test crashes instead of continuing with the next one\n"
        "\n"
        "Exit codes:\n"
        "  0  every selected test ran and passed\n"
        "  1  the run completed; some tests failed or crashed\n"
        "  2  the run is incomplete: tests were not executed, or --test-name matched nothing\n",
        program_name
    );
}

int main(int const argc, char const* const argv[])
{
    // Unbuffered rather than line buffered: the Windows CRT treats _IOLBF as _IOFBF, so this is the
    // only setting under which a crashing test's own output survives the fault.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    if (argc >= 2)
    {
        if (should_print_help(argc, argv))
        {
            print_help(argv[0]);
            return 0;
        }

        if (should_list_tests(argc, argv))
        {
            if (should_output_json(argc, argv))
            {
                std::filesystem::path const output_file_path = get_output_json_file_path(argc, argv);
                print_test_names_json(output_file_path);
            }
            else
            {
                std::span<char const* const> const all_test_names = get_all_test_names();
                print_test_names(all_test_names);
            }
            return 0;
        }
    }

    std::signal(SIGABRT, abort_signal_handler);

    std::span<char const* const> const all_test_names = get_all_test_names();
    Filtered_tests const filtered_tests = filter_tests(argc, argv, all_test_names);

    if (!filtered_tests.unknown_names.empty())
    {
        for (std::pmr::string const& unknown_name : filtered_tests.unknown_names)
            std::fprintf(stderr, "error: no test named '%s'. Use --list-tests to see the available tests.\n", unknown_name.c_str());

        std::fflush(stderr);
        return 2;
    }

    std::pmr::vector<Test_function_pointer> const tests_function_pointers = get_tests_function_pointers(filtered_tests.indices);
    std::pmr::vector<char const*> const filtered_test_names = get_filtered_test_names(filtered_tests.indices, all_test_names);

    Test_results const results = run_tests(tests_function_pointers, filtered_test_names, should_stop_on_crash(argc, argv));
    print_test_results(results);
    print_not_run_tests(filtered_test_names, results.run_count);

    if (results.run_count < results.selected_count)
        return 2;

    return results.failed_count == 0 ? 0 : 1;
}
