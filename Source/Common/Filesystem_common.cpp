module;

#include <curl/curl.h>
#include <miniz.h>
#include <reproc/run.h>

module iris.common.filesystem_common;

import std;

import iris.common.filesystem;

namespace iris::common
{
    std::optional<std::filesystem::path> search_file(
        std::string_view const filename,
        std::span<std::filesystem::path const> const search_paths
    )
    {
        {
            std::filesystem::path const file_path = filename;
            if (file_path.is_absolute())
                return file_path;
        }

        for (std::filesystem::path const& search_path : search_paths)
        {
            for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator{ search_path })
            {
                if (entry.path().filename() == filename)
                {
                    return entry.path();
                }
            }
        }

        return std::nullopt;
    }

    std::pmr::vector<std::filesystem::path> search_files(
        std::filesystem::path const& root_directory,
        std::string_view const filename,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<std::filesystem::path> found_files{temporaries_allocator};

        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator{ root_directory })
        {
            if (entry.is_regular_file())
            {
                std::filesystem::path const& entry_path = entry.path();
                std::filesystem::path const filename_path = entry_path.filename();

                if (filename_path.generic_string() == filename)
                {
                    found_files.push_back(entry_path);
                }
            }
        }

        return std::pmr::vector<std::filesystem::path>{std::move(found_files), output_allocator};
    }

    std::filesystem::path get_share_path(std::filesystem::path const& relative_path)
    {
        std::filesystem::path const executable_directory_include_path = get_executable_directory().parent_path() / "share" / "iris" / relative_path;
        if (std::filesystem::exists(executable_directory_include_path))
            return executable_directory_include_path;

        std::filesystem::path const executable_directory_parent_include_path = get_executable_directory().parent_path().parent_path().parent_path() / "share" / "iris" / relative_path;
        if (std::filesystem::exists(executable_directory_parent_include_path))
            return executable_directory_parent_include_path;

        std::filesystem::path const current_directory_include_path = std::filesystem::current_path() / "share" / "iris" / relative_path;
        if (std::filesystem::exists(current_directory_include_path))
            return current_directory_include_path;

        std::filesystem::path const parent_directory_include_path = std::filesystem::current_path().parent_path() / "share" / "iris" / relative_path;
        return parent_directory_include_path;
    }

    std::filesystem::path get_builtin_include_directory()
    {
        return get_share_path("include").lexically_normal();
    }

    std::filesystem::path get_builtin_module_file_path()
    {
        return get_share_path("source/Builtin.iris").lexically_normal();
    }

    std::filesystem::path get_json_module_file_path()
    {
        return get_share_path("libraries/Iris_standard_library/json.iris").lexically_normal();
    }

    std::filesystem::path get_tests_main_file_path()
    {
        return get_share_path("source/tests_main.cpp").lexically_normal();
    }

    std::filesystem::path get_standard_repository_file_path()
    {
        return get_share_path("libraries/iris_repository.json").lexically_normal();
    }

    std::filesystem::path get_visualizers_file_path()
    {
        return get_share_path("visualizers").lexically_normal();
    }

    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::pmr::string* out)
    {
        size_t const total_size = size * nmemb;
        out->append(static_cast<char const*>(contents), total_size);
        return total_size;
    }

    std::optional<std::pmr::string> download_file(
        std::string_view const url,
        std::filesystem::path const& destination_path
    )
    {
        CURL* const handle = curl_easy_init();
        if (!handle)
            return std::nullopt;

        std::pmr::string buffer;

        curl_easy_setopt(handle, CURLOPT_URL, url.data());
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);

        CURLcode const res = curl_easy_perform(handle);

        if (res != CURLE_OK)
        {
            std::string const error_message = std::format("curl error {}: {}", static_cast<int>(res), curl_easy_strerror(res));
            curl_easy_cleanup(handle);
            return std::pmr::string{error_message};
        }

        curl_easy_cleanup(handle);

        // Write to a temporary file first, then rename on success to avoid partial downloads
        std::string const temp_path_str = destination_path.generic_string() + ".tmp";
        std::filesystem::path const temp_path{temp_path_str};
        std::ofstream file(temp_path, std::ios::binary);
        if (!file.is_open())
        {
            std::string const error_message = std::format("Failed to create temporary file {}", temp_path.generic_string());
            return std::pmr::string{error_message};
        }

        file.write(buffer.data(), buffer.size());
        if (!file.good())
        {
            file.close();
            std::filesystem::remove(temp_path);
            std::string const error_message = std::format("Failed to write to temporary file {}", temp_path.generic_string());
            return std::pmr::string{error_message};
        }
        file.close();

        // Rename temp file to final destination (atomic on most filesystems)
        std::error_code ec;
        std::filesystem::rename(temp_path, destination_path, ec);
        if (ec)
        {
            std::filesystem::remove(temp_path);
            std::string const error_message = std::format("Failed to rename temporary file: {}", ec.message());
            return std::pmr::string{error_message};
        }

        return std::move(buffer);
    }

    bool extract_zip(std::filesystem::path const& archive_path, std::filesystem::path const& destination_directory)
    {
        if (std::filesystem::exists(destination_directory))
        {
            std::printf("Skipped extract zip to '%s' since it already exists.\n", destination_directory.generic_string().c_str());
            return true;
        }
        std::printf("Extract zip to '%s'.\n", destination_directory.generic_string().c_str());

        mz_zip_archive zip_archive;
        std::memset(&zip_archive, 0, sizeof(zip_archive));

        if (!mz_zip_reader_init_file(&zip_archive, reinterpret_cast<char const*>(archive_path.u8string().c_str()), 0))
            return false;

        mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);
        for (mz_uint i = 0; i < num_files; ++i)
        {
            mz_zip_archive_file_stat file_stat;
            if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
                continue;

            std::string const filename = file_stat.m_filename;

            // Strip leading directory component so files extract directly into destination_directory
            std::string const relative_name = [&filename]() -> std::string
            {
                std::size_t const sep = filename.find_first_of("/\\");
                if (sep != std::string::npos)
                    return filename.substr(sep + 1);
                return filename;
            }();

            std::filesystem::path const out_path = destination_directory / relative_name;

            if (file_stat.m_uncomp_size == 0 && relative_name.empty())
            {
                std::filesystem::create_directories(out_path);
                continue;
            }

            std::filesystem::create_directories(out_path.parent_path());

            if (!mz_zip_reader_extract_file_to_file(&zip_archive, filename.c_str(), out_path.generic_string().c_str(), 0))
                continue;
        }

        mz_zip_reader_end(&zip_archive);
        return true;
    }

    static std::pmr::vector<std::pmr::string> split_command(std::string_view const command, std::pmr::polymorphic_allocator<std::pmr::string> const& alloc)
    {
        std::pmr::vector<std::pmr::string> args{alloc};
        std::pmr::string current;
        bool in_quotes = false;
        char quote_char = '\0';

        for (char c : command)
        {
            if (!in_quotes)
            {
                if (c == '"' || c == '\'')
                {
                    in_quotes = true;
                    quote_char = c;
                }
                else if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
                {
                    if (!current.empty())
                    {
                        args.push_back(std::move(current));
                        current.clear();
                    }
                }
                else
                {
                    current += c;
                }
            }
            else
            {
                if (c == quote_char)
                {
                    in_quotes = false;
                }
                else
                {
                    current += c;
                }
            }
        }

        if (!current.empty())
            args.push_back(std::move(current));

        return args;
    }

    int execute_command(std::filesystem::path const& working_directory, std::string_view const command)
    {
        std::pmr::polymorphic_allocator<> allocator;
        std::pmr::vector<std::pmr::string> const args = split_command(command, allocator);

        std::pmr::vector<char const*> argv{allocator};
        argv.reserve(args.size() + 1);
        for (std::pmr::string const& arg : args)
        {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);

        std::string const working_directory_string = working_directory.generic_string();
        reproc_options const options
        {
            .working_directory = working_directory_string.c_str()
        };

        return reproc_run(argv.data(), options);
    }

    bool is_git_url(std::string_view url)
    {
        if (url.size() >= 4 && url.substr(url.size() - 4) == ".git")
            return true;

        return false;
    }

    std::optional<std::filesystem::path> download_git_repo(
        std::string_view const url,
        std::string_view const git_ref,
        bool recurse_submodules
    )
    {
        // Use system temp directory for cloning
        std::pmr::polymorphic_allocator<> allocator;
        std::filesystem::path const temp_directory = std::filesystem::temp_directory_path();

        // Extract repo name from URL and create temporary clone directory
        std::filesystem::path const repository_name = url.substr(url.find_last_of("/\\") + 1);
        std::filesystem::path const clone_directory = temp_directory / repository_name;

        // Remove clone directory if it already exists
        if (std::filesystem::exists(clone_directory))
        {
            std::filesystem::remove_all(clone_directory);
        }

        // Build the git clone command
        std::pmr::vector<std::pmr::string> args{allocator};
        args.push_back("git");
        args.push_back("clone");
        args.push_back("--depth");
        args.push_back("1");

        if (recurse_submodules)
        {
            args.push_back("--recurse-submodules");
            args.push_back("--shallow-submodules");
        }

        if (!git_ref.empty())
        {
            args.push_back("--branch");
            args.push_back(std::pmr::string{git_ref});
        }

        args.push_back(std::pmr::string{url});
        args.push_back(std::pmr::string{clone_directory.generic_string()});

        // Build command string
        std::pmr::vector<std::pmr::string> cmd_strings{allocator};
        for (auto const& arg : args)
        {
            // Quote arguments that might contain spaces
            if (arg.find(' ') != std::string_view::npos || arg.find('"') != std::string_view::npos)
            {
                cmd_strings.push_back(std::pmr::string{std::format("\"{}\"", arg)});
            }
            else
            {
                cmd_strings.push_back(arg);
            }
        }
        std::pmr::string command_str;
        for (std::size_t i = 0; i < cmd_strings.size(); ++i)
        {
            if (i > 0) command_str += " ";
            command_str += cmd_strings[i];
        }

        std::printf("Cloning git repository: %s\n", command_str.c_str());
        int const clone_result = execute_command(temp_directory, command_str);
        if (clone_result != 0)
        {
            return std::pmr::string{std::format("git clone failed with exit code {}", clone_result)};
        }

        // Pull LFS if available (no-op if no LFS files)
        std::pmr::string const lfs_command{"git lfs pull"};
        int const lfs_result = execute_command(clone_directory, lfs_command);
        // Ignore LFS errors (might not be installed or no LFS files)
        if (lfs_result != 0)
        {
            std::printf("Note: git lfs pull returned %d (may be expected if LFS not configured)\n", lfs_result);
        }

        return clone_directory;
    }

    std::optional<std::pmr::string> create_zip_from_directory(
        std::filesystem::path const& source_directory,
        std::filesystem::path const& output_zip_path
    )
    {
        std::printf("Creating zip archive: %s\n", output_zip_path.generic_string().c_str());

        // Create parent directories if needed
        std::error_code ec;
        std::filesystem::create_directories(output_zip_path.parent_path(), ec);
        if (ec)
        {
            return std::pmr::string{std::format("Failed to create output directory: {}", ec.message())};
        }

        // Initialize miniz zip writer
        mz_zip_archive zip_archive;
        std::memset(&zip_archive, 0, sizeof(zip_archive));

        std::filesystem::path const root_directory_name = output_zip_path.stem();

        std::string const output_path_str = output_zip_path.generic_string();
        if (!mz_zip_writer_init_file(&zip_archive, output_path_str.c_str(), 0))
        {
            return std::pmr::string{"Failed to initialize zip writer"};
        }

        // Add each file to the archive
        for (auto const& entry : std::filesystem::recursive_directory_iterator(source_directory))
        {
            if (entry.is_regular_file())
            {
                std::filesystem::path const& file_path = entry.path();
                std::filesystem::path const relative_path = root_directory_name / std::filesystem::relative(file_path, source_directory);

                mz_bool const result = mz_zip_writer_add_file(
                    &zip_archive,
                    relative_path.generic_string().c_str(),
                    file_path.generic_string().c_str(),
                    nullptr,
                    0,
                    MZ_BEST_COMPRESSION
                );

                if (!result)
                {
                    mz_zip_writer_end(&zip_archive);
                    std::filesystem::remove(output_zip_path, ec);
                    return std::pmr::string{std::format("Failed to add file '{}' to archive", relative_path.generic_string())};
                }       
            }
        }

        // Finalize and end
        if (!mz_zip_writer_finalize_archive(&zip_archive))
        {
            mz_zip_writer_end(&zip_archive);
            std::filesystem::remove(output_zip_path, ec);
            return std::pmr::string{"Failed to finalize archive"};
        }

        mz_zip_writer_end(&zip_archive);
        return std::nullopt; // success, no error
    }
}
