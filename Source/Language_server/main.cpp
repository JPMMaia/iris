#include <argparse/argparse.hpp>

#include <filesystem>
#include <exception>
#include <optional>
#include <span>
#include <string>
#include <vector>

import iris.common;

import iris.compiler.artifact;
import iris.compiler.repository;

import iris.language_server.breakpoint;
import iris.language_server.message_handler;
import iris.language_server.server;

struct Source_file_info
{
    std::filesystem::path file_path;
    std::pmr::string module_name;
};

struct Project
{
    std::filesystem::path project_directory;
    std::pmr::vector<iris::compiler::Repository> repositories;
    std::pmr::unordered_map<std::filesystem::path, iris::compiler::Artifact> artifacts;
    std::pmr::unordered_map<std::filesystem::path, Source_file_info> artifact_to_source_files_map;
};

std::pmr::vector<std::filesystem::path> convert_to_path(std::span<std::string const> const values)
{
    std::pmr::vector<std::filesystem::path> output;
    output.reserve(values.size());

    for (std::string const& value : values)
    {
        output.push_back(value);
    }

    return output;
}

using namespace iris::language_server;

int main(int const argc, char const* const* argv)
{
    iris::common::install_abort_handlers();

    Message_handler message_handler = create_message_handler();
    process_messages(message_handler);
    return 0;
}
