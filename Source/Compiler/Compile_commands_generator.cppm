module;

#include <nlohmann/json.hpp>

export module h.compiler.compile_commands_generator;

import std;

import h.common;
import h.compiler.artifact;
import h.compiler.clang_compiler;

namespace h::compiler
{
    export struct Compile_command
    {
        std::filesystem::path directory;
        std::pmr::vector<std::pmr::string> arguments;
        std::filesystem::path file;
        std::filesystem::path output;

        friend auto operator<=>(Compile_command const&, Compile_command const&) = default;
    };

    export std::pmr::vector<Compile_command> create_compile_commands(
        std::span<Artifact const> const artifacts,
        std::filesystem::path const build_directory_path,
        bool const use_clang_cl,
        bool const use_objects,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::string_view const extension = use_objects ? "obj" : "bc";
        std::filesystem::path const clang_path = find_clang(use_clang_cl);

        std::pmr::vector<Compile_command> output{temporaries_allocator};

        for (Artifact const& artifact : artifacts)
        {
            std::pmr::vector<std::filesystem::path> const public_include_directories = get_public_include_directories(artifact, artifacts, temporaries_allocator, temporaries_allocator);
            std::pmr::vector<std::pmr::string> const public_include_directories_strings = h::common::convert_path_to_string(public_include_directories, temporaries_allocator);

            for (Source_group const& group : artifact.sources)
            {
                if (!group.data.has_value() || !std::holds_alternative<Cpp_source_group>(*group.data))
                    continue;

                std::pmr::vector<std::filesystem::path> const source_file_paths = find_included_files(
                    artifact.file_path.parent_path(),
                    group.include,
                    temporaries_allocator,
                    temporaries_allocator
                );

                for (std::filesystem::path const& source_file_path : source_file_paths)
                {
                    std::filesystem::path const output_assembly_file = build_directory_path / std::format("{}.{}.{}", artifact.name, source_file_path.stem().generic_string(), extension);
                    std::filesystem::path const output_dependency_file = build_directory_path / std::format("{}.{}.d", artifact.name, source_file_path.stem().generic_string());

                    std::pmr::vector<std::pmr::string> arguments = create_compile_cpp_arguments(
                        clang_path,
                        source_file_path,
                        output_assembly_file,
                        output_dependency_file,
                        build_directory_path,
                        public_include_directories_strings,
                        group.additional_flags,
                        use_clang_cl,
                        false,
                        temporaries_allocator
                    );

                    Compile_command command
                    {
                        .directory = build_directory_path,
                        .arguments = std::move(arguments),
                        .file = source_file_path,
                        .output = output_assembly_file,
                    };

                    output.push_back(std::move(command));
                }
            }
        }

        return std::pmr::vector<Compile_command>{output, output_allocator};
    }

    export void write_compile_commands_to_file(std::span<Compile_command const> const commands, std::filesystem::path const& output_path)
    {
        nlohmann::json json;

        for (Compile_command const& command : commands)
        {
            nlohmann::json command_json;
            command_json["directory"] = command.directory.generic_string();
            command_json["arguments"] = command.arguments;
            command_json["file"] = command.file.generic_string();
            command_json["output"] = command.output.generic_string();

            json.push_back(std::move(command_json));
        }

        std::string const content = json.dump(4);

        std::filesystem::path const& parent_path = output_path.parent_path();
        if (!std::filesystem::exists(parent_path))
            std::filesystem::create_directories(parent_path);

        h::common::write_to_file(output_path, content);
    }

    export std::pmr::vector<Compile_command> read_compile_commands_from_file(std::filesystem::path const& file_path)
    {
        std::optional<std::pmr::string> const contents = h::common::get_file_contents(file_path);
        if (!contents.has_value())
            return {};

        nlohmann::json const json = nlohmann::json::parse(contents.value());

        std::pmr::vector<Compile_command> output;
        output.reserve(json.size());

        for (nlohmann::json const& command_json : json)
        {
            Compile_command command
            {
                .directory = command_json.at("directory").get<std::pmr::string>(),
                .arguments = command_json.at("arguments").get<std::pmr::vector<std::pmr::string>>(),
                .file = command_json.at("file").get<std::pmr::string>(),
                .output = command_json.at("output").get<std::pmr::string>(),
            };

            output.push_back(std::move(command));
        }

        return output;
    }
}
