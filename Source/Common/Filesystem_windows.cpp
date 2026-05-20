module;

#include <windows.h>

module iris.common.filesystem;

import std;

import iris.common;

namespace iris::common
{
    std::filesystem::path get_executable_directory()
    {
        std::pmr::string buffer;
        buffer.resize(1024);

        int const bytes = GetModuleFileName(nullptr, buffer.data(), buffer.size());

        std::filesystem::path const executable_path = std::string_view{ buffer.data(), buffer.data() + bytes + 1 };
        return executable_path.parent_path();
    }

    static std::filesystem::path find_default_windows_kit_subdirectory_path(
        std::string_view const subdirectory
    )
    {
        std::filesystem::path const windows_kit_root = "C:/Program Files (x86)/Windows Kits/10";
        std::filesystem::path const library_path = windows_kit_root / subdirectory;

        if (!std::filesystem::exists(library_path))
            iris::common::print_message_and_exit(std::format("{} does not exist! Is Windows 10 Kit installed?", library_path.generic_string()));

        std::optional<std::filesystem::path> best_match = std::nullopt;
        for (std::filesystem::directory_entry const& entry : std::filesystem::directory_iterator{ library_path })
        {
            if (!best_match)
            {
                best_match = entry.path();
            }
            else
            {
                if (entry.path() > best_match)
                    best_match = entry.path();
            }
        }

        if (!best_match)
            iris::common::print_message_and_exit(std::format("Could not find an Windows 10 Kit version in {}! Is Windows 10 Kit installed?", library_path.generic_string()));

        return *best_match;
    }

    static std::optional<std::pmr::string> execute_process_capture_stdout(
        std::filesystem::path const& executable,
        std::string_view const arguments
    )
    {
        std::wstring const command = executable.wstring() + L" " + std::wstring{ arguments.begin(), arguments.end() };

        SECURITY_ATTRIBUTES security_attributes
        {
            .nLength = sizeof(SECURITY_ATTRIBUTES),
            .lpSecurityDescriptor = nullptr,
            .bInheritHandle = TRUE,
        };

        HANDLE read_pipe = nullptr;
        HANDLE write_pipe = nullptr;
        if (!CreatePipe(&read_pipe, &write_pipe, &security_attributes, 0))
            return std::nullopt;

        STARTUPINFOW startup_info
        {
            .cb = sizeof(STARTUPINFOW),
            .dwFlags = STARTF_USESTDHANDLES,
            .hStdInput = GetStdHandle(STD_INPUT_HANDLE),
            .hStdOutput = write_pipe,
            .hStdError = GetStdHandle(STD_ERROR_HANDLE),
        };

        PROCESS_INFORMATION process_info{};
        if (!CreateProcessW(nullptr, const_cast<LPWSTR>(command.c_str()), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup_info, &process_info))
        {
            CloseHandle(read_pipe);
            CloseHandle(write_pipe);
            return std::nullopt;
        }

        CloseHandle(write_pipe);
        CloseHandle(process_info.hThread);

        std::pmr::string result;
        char buffer[8192];
        DWORD bytes_read = 0;
        while (ReadFile(read_pipe, buffer, sizeof(buffer), &bytes_read, nullptr) && bytes_read > 0)
            result.append(buffer, bytes_read);

        CloseHandle(read_pipe);
        CloseHandle(process_info.hProcess);

        return result.empty() ? std::nullopt : std::optional{ std::move(result) };
    }

    static std::pmr::string trim_string(std::string_view const input)
    {
        std::size_t const start = input.find_first_not_of(" \t\n\r");
        if (start == std::string_view::npos)
            return {};
        std::string_view const trimmed_start = input.substr(start);

        std::size_t const end = trimmed_start.find_last_not_of(" \t\n\r");
        return { trimmed_start.data(), static_cast<std::size_t>(end - start + 1) };
    }

    static std::optional<std::filesystem::path> find_vc_tools_install_path()
    {
        char const* env = getenv("VCToolsInstallDir");
        if (env != nullptr)
            return env;

        std::filesystem::path const vswhere_path = std::filesystem::path{ getenv("ProgramFiles(x86)") } / "Microsoft Visual Studio/Installer/vswhere.exe";
        if (!std::filesystem::exists(vswhere_path))
            return std::nullopt;

        // Returns something like: C:\Program Files\Microsoft Visual Studio\2022\Community
        std::optional<std::pmr::string> const vswhere_output = execute_process_capture_stdout(
            vswhere_path,
            "-latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath"
        );
        if (!vswhere_output)
            return std::nullopt;

        std::pmr::string const trimmed = trim_string(*vswhere_output);
        if (trimmed.empty())
            return std::nullopt;

        std::filesystem::path const vs_install_path{ trimmed };

        std::filesystem::path const vc_tools_base = vs_install_path / "VC" / "Tools" / "MSVC";
        if (!std::filesystem::exists(vc_tools_base))
            return std::nullopt;

        std::optional<std::filesystem::path> best_version;
        for (auto const& entry : std::filesystem::directory_iterator{ vc_tools_base })
        {
            if (entry.is_directory())
            {
                if (!best_version || entry.path() > *best_version)
                {
                    best_version = entry.path();
                }
            }
        }

        return best_version;
    }

    std::pmr::vector<std::filesystem::path> get_default_header_search_directories()
    {
        std::filesystem::path const windows_kit_include_path = find_default_windows_kit_subdirectory_path("Include");

        std::pmr::vector<std::filesystem::path> include_directories
        {
            windows_kit_include_path / "ucrt"
        };

        if (std::optional<std::filesystem::path> const vc_tools_path = find_vc_tools_install_path(); vc_tools_path)
            include_directories.push_back(*vc_tools_path / "include");

        return include_directories;
    }

    std::pmr::vector<std::filesystem::path> get_default_library_directories()
    {
        std::filesystem::path const windows_kit_library_path = find_default_windows_kit_subdirectory_path("Lib");
        
        std::pmr::vector<std::filesystem::path> library_directories
        {
            windows_kit_library_path / "ucrt" / "x64",
            windows_kit_library_path / "um" / "x64",
        };

        if (std::optional<std::filesystem::path> const vc_tools_path = find_vc_tools_install_path(); vc_tools_path)
            library_directories.push_back(*vc_tools_path / "lib" / "x64");

        return library_directories;
    }
}
