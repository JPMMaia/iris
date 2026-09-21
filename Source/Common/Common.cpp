module;

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <memory_resource>

#if _WIN32
#include <crtdbg.h>
#include <windows.h>
#endif

module iris.common;

import std;
import std.compat;

namespace iris::common
{
    void print_message_and_exit(std::string const& message)
    {
        print_message_and_exit(message.c_str());
    }

    void print_message_and_exit(char const* const message)
    {
        std::fflush(stdout);
        std::fprintf(stderr, "%s\n", message);
        std::fflush(stderr);
        std::exit(-1);
    }

    std::optional<std::pmr::vector<std::byte>> read_binary_file(char const* const path)
    {
        std::FILE* file = std::fopen(path, "rb");
        if (file == nullptr)
            return std::nullopt;

        std::pmr::vector<std::byte> contents;
        std::fseek(file, 0, SEEK_END);
        contents.resize(std::ftell(file));
        std::rewind(file);
        std::size_t const read_bytes = std::fread(&contents[0], 1, contents.size(), file);
        std::fclose(file);

        if (read_bytes < contents.size())
            contents.erase(contents.begin() + read_bytes, contents.end());

        return contents;
    }

    std::optional<std::pmr::vector<std::byte>> read_binary_file(std::filesystem::path const& path)
    {
        std::string const path_string = path.generic_string();
        std::optional<std::pmr::vector<std::byte>> const file_contents = read_binary_file(path_string.c_str());
        return file_contents;
    }

    void write_binary_file(char const* const path, std::span<std::byte const> const content)
    {
        std::FILE* const file = std::fopen(path, "wb");
        if (file == nullptr)
        {
            std::string const message = std::format("Cannot write to '{}'", path);
            std::perror(message.c_str());
            throw std::runtime_error{ message };
        }

        std::fwrite(content.data(), sizeof(std::byte), content.size(), file);

        std::fclose(file);
    }

    void write_binary_file(std::filesystem::path const& path, std::span<std::byte const> const content)
    {
        std::string const path_string = path.generic_string();
        write_binary_file(path_string.c_str(), content);
    }

    std::optional<std::pmr::string> get_file_contents(char const* const path)
    {
        std::FILE* file = std::fopen(path, "r");
        if (file == nullptr)
            return {};

        std::pmr::string contents;
        std::fseek(file, 0, SEEK_END);
        contents.resize(std::ftell(file));
        std::rewind(file);
        std::size_t const read_bytes = std::fread(&contents[0], 1, contents.size(), file);
        std::fclose(file);

        if (read_bytes < contents.size())
            contents.erase(contents.begin() + read_bytes, contents.end());

        return contents;
    }

    std::optional<std::pmr::string> get_file_contents(std::filesystem::path const& path)
    {
        std::string const path_string = path.generic_string();
        std::optional<std::pmr::string> const file_contents = get_file_contents(path_string.c_str());
        return file_contents;
    }

    std::optional<std::pmr::u8string> get_file_utf8_contents(char const* const path)
    {
        std::FILE* file = std::fopen(path, "r");
        if (file == nullptr)
            return {};

        std::pmr::u8string contents;
        std::fseek(file, 0, SEEK_END);
        contents.resize(std::ftell(file));
        std::rewind(file);
        std::size_t const read_bytes = std::fread(&contents[0], 1, contents.size(), file);
        std::fclose(file);

        if (read_bytes < contents.size())
            contents.erase(contents.begin() + read_bytes, contents.end());

        return contents;
    }

    std::optional<std::pmr::u8string> get_file_utf8_contents(std::filesystem::path const& path)
    {
        std::string const path_string = path.generic_string();
        std::optional<std::pmr::u8string> const file_contents = get_file_utf8_contents(path_string.c_str());
        return file_contents;
    }

    void write_to_file(char const* const path, std::string_view const content)
    {
        std::FILE* const file = std::fopen(path, "w");
        if (file == nullptr)
        {
            std::string const message = std::format("Cannot write to '{}'", path);
            std::perror(message.c_str());
            throw std::runtime_error{ message };
        }

        std::fwrite(content.data(), sizeof(std::string_view::value_type), content.size(), file);

        std::fclose(file);
    }

    void write_to_file(std::filesystem::path const& path, std::string_view const content)
    {
        std::string const path_string = path.generic_string();
        write_to_file(path_string.c_str(), content);
    }

    std::pmr::vector<std::pmr::string> convert_path_to_string(std::span<std::filesystem::path const> const values, std::pmr::polymorphic_allocator<> output_allocator)
    {
        std::pmr::vector<std::pmr::string> output{output_allocator};
        output.reserve(values.size());

        for (std::filesystem::path const& value : values)
        {
            output.push_back(std::pmr::string{value.generic_string()});
        }

        return output;
    }

    std::pmr::vector<std::string_view> split_string(std::string_view const value, char const separator, std::pmr::polymorphic_allocator<> const& output_allocator)
    {
        std::pmr::vector<std::string_view> output;

        std::size_t count = std::count(value.begin(), value.end(), separator);
        output.reserve(count + 1);

        std::string_view::size_type previous_position = 0;
        std::string_view::size_type current_position = 0;

        while((current_position = value.find(separator, current_position)) != std::string::npos)
        {
            std::string_view const substring = value.substr(previous_position, current_position-previous_position);
            output.push_back(substring);

            current_position += 1;
            previous_position = current_position;
        }

        output.push_back(value.substr(previous_position));

        return output;
    }

    void print_stacktrace()
    {
        auto const stacktrace = std::pmr::stacktrace::current(7, 16);
        std::string const stacktrace_string = std::format("Stacktrace:\n{}\n", stacktrace);
        std::fprintf(stderr, "%s", stacktrace_string.c_str());
        std::fflush(stderr);
    }

    static void signal_handler(int const signal)
    {
        std::fflush(stdout);
        std::fprintf(stderr, "Abort signal: %d\n", signal);
        print_stacktrace();
        std::fflush(stderr);
        std::exit(-1);
    }

#if _WIN32
    static char const* describe_seh_exception_code(DWORD const code)
    {
        switch (code)
        {
        case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
        case EXCEPTION_ACCESS_VIOLATION: return "access violation";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer divide by zero";
        case EXCEPTION_PRIV_INSTRUCTION: return "privileged instruction";
        case EXCEPTION_IN_PAGE_ERROR: return "in-page error";
        default: return "unknown";
        }
    }

    static LONG WINAPI unhandled_exception_filter(EXCEPTION_POINTERS* const exception_pointers)
    {
        DWORD const code =
            exception_pointers != nullptr && exception_pointers->ExceptionRecord != nullptr ?
            exception_pointers->ExceptionRecord->ExceptionCode :
            0;

        std::fflush(stdout);
        std::fprintf(
            stderr,
            "internal compiler error: the compiler crashed (%s, code 0x%08lX).\n",
            describe_seh_exception_code(code),
            static_cast<unsigned long>(code)
        );

        if (code == EXCEPTION_STACK_OVERFLOW)
            std::fprintf(stderr, "This is usually a cycle the compiler failed to detect.\n");

        std::fprintf(stderr, "This is a bug in the iris compiler. Please report it.\n");
        std::fflush(stderr);

        return EXCEPTION_EXECUTE_HANDLER;
    }
#endif

    static void invalid_parameter_handler(
        wchar_t const* const expression,
        wchar_t const* const function,
        wchar_t const* const file,
        unsigned int const line,
        uintptr_t const pReserved
    )
    {
        std::abort();
    }

    void install_abort_handlers()
    {
        std::signal(SIGABRT, signal_handler);

        std::set_terminate([]()
        {
            std::fflush(stdout);

            std::exception_ptr const exception = std::current_exception();
            if (exception)
            {
                try
                {
                    std::rethrow_exception(exception);
                }
                catch (std::exception const& error)
                {
                    std::fprintf(stderr, "Unhandled exception: %s\n", error.what());
                }
                catch (...)
                {
                    std::fprintf(stderr, "Unhandled exception of unknown type!\n");
                }
            }
            else
            {
                std::fprintf(stderr, "Terminate called without an active exception!\n");
            }

            print_stacktrace();
            std::fflush(stderr);
            std::_Exit(3);
        });

        #if _WIN32
        ULONG stack_reserve_bytes = 64 * 1024;
        SetThreadStackGuarantee(&stack_reserve_bytes);

        SetUnhandledExceptionFilter(unhandled_exception_filter);

        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        _set_invalid_parameter_handler(invalid_parameter_handler);
        #endif
    }
}
