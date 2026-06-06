module;

#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>
#include <sys/wait.h>

module iris.compiler.linker;

namespace iris::compiler
{
    bool invoke_mold(
        std::span<char const* const> const arguments
    )
    {
        pid_t pid = fork();

        bool const is_child_process = pid == 0;
        bool const is_parent_process = pid > 0;
        if (is_child_process)
        {
            execvp("mold", const_cast<char* const*>(arguments.data()));
            std::perror("exec failed");
            std::exit(-1);
        }
        else if (is_parent_process)
        {
            int status = 0;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
            {
                std::perror("Linking failed!");
                return false;
            }

            return true;
        }
        else
        {
            std::perror("Fork failed!");
            return false;
        }
    }

    bool link(
        std::span<std::filesystem::path const> const object_file_paths,
        std::span<std::pmr::string const> const libraries,
        std::filesystem::path const& output,
        Linker_options const& options
    )
    {
        if (options.link_type == Link_type::Static_library)
        {
            // TODO
            // run ar rcs libexample.a libexample.o
            return false;
        }

        std::pmr::vector<std::string> arguments_storage;
        arguments_storage.reserve(5 + libraries.size() + object_file_paths.size());

        if (options.link_type == Link_type::Executable)
        {
            arguments_storage.push_back(std::format("-e {}", options.entry_point.value_or(std::string_view{"main"})));
            arguments_storage.push_back(std::format("-o {}", output.generic_string()));
        }
        else if (options.link_type == Link_type::Shared_library)
        {
            arguments_storage.push_back(std::format("-shared"));
            arguments_storage.push_back(std::format("-o {}.so", output.generic_string()));
        }

        arguments_storage.push_back(std::format("-lc"));
        arguments_storage.push_back(std::format("-lm"));

        if (options.debug)
            arguments_storage.push_back("-g");

        for (std::string_view const library : libraries)
        {
            arguments_storage.push_back(std::format("-l{}", library));
        }

        for (std::filesystem::path const& object_file_path : object_file_paths)
        {
            arguments_storage.push_back(object_file_path.generic_string());
        }


        std::pmr::vector<char const*> arguments;
        arguments.reserve(2 + arguments_storage.size());

        arguments.push_back("mold");

        for (std::string const& argument : arguments_storage)
        {
            arguments.push_back(argument.c_str());
        }

        arguments.push_back(nullptr);

        invoke_mold(arguments);

        return true;
    }
}
