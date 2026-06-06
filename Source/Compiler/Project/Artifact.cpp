module;

#include <nlohmann/json.hpp>

#include <memory_resource>

module iris.compiler.artifact;

import std;

import iris.common;
import iris.compiler.common;
import iris.compiler.presets;
import iris.compiler.target;
import iris.core;

namespace iris::compiler
{
    static std::pmr::string substitute_variables(std::string_view const value, Environment_variables const& environment_variables, std::string_view const field_name)
    {
        std::pmr::string output;

        std::size_t index = 0;
        while (index < value.size())
        {
            std::size_t const start = value.find("${", index);
            if (start == std::string_view::npos)
            {
                output.append(value.substr(index));
                break;
            }

            output.append(value.substr(index, start - index));

            std::size_t const end = value.find('}', start + 2);
            if (end == std::string_view::npos)
                throw std::runtime_error(std::format("Missing '}}' in variable expression for field '{}'.", field_name));

            std::string_view const variable_name = value.substr(start + 2, end - (start + 2));
            if (variable_name.empty())
                throw std::runtime_error(std::format("Empty variable name in field '{}'.", field_name));

            auto const location = environment_variables.find(std::pmr::string{ variable_name });
            if (location == environment_variables.end())
                throw std::runtime_error(std::format("Missing environment variable '{}' in field '{}'.", variable_name, field_name));

            output.append(location->second);
            index = end + 1;
        }

        return output;
    }

    Version parse_version(std::string_view const string)
    {
        std::string_view::size_type const first_dot = string.find(".");
        if (first_dot == string.npos)
        {
            std::uint32_t major = 0;
            std::from_chars(string.data(), string.data() + string.size(), major);

            return Version
            {
                .major = major,
                .minor = 0,
                .patch = 0
            };
        }

        std::uint32_t major = 0;
        std::from_chars(string.data(), string.data() + first_dot, major);

        std::string_view::size_type const second_dot = string.find(".", first_dot + 1);

        if (second_dot == string.npos)
        {
            std::uint32_t minor = 0;
            std::from_chars(string.data() + first_dot + 1, string.data() + string.size(), minor);

            return Version
            {
                .major = major,
                .minor = minor,
                .patch = 0
            };
        }
        
        std::uint32_t minor = 0;
        std::from_chars(string.data() + first_dot + 1, string.data() + second_dot, minor);

        std::uint32_t patch = 0;
        std::from_chars(string.data() + second_dot + 1, string.data() + string.size(), patch);

        return Version
        {
            .major = major,
            .minor = minor,
            .patch = patch
        };
    }

    Artifact_type parse_artifact_type(std::string_view const string)
    {
        if (string == "executable")
            return Artifact_type::Executable;
        else if (string == "library")
            return Artifact_type::Library;

        iris::common::print_message_and_exit(std::format("Failed to parse artifact type '{}'", string));
        return Artifact_type{};
    }

    std::pmr::vector<Dependency> parse_dependencies(nlohmann::json const& json)
    {
        if (!json.contains("dependencies"))
            return {};

        nlohmann::json const dependencies_json = json.at("dependencies");

        std::pmr::vector<Dependency> dependencies;
        dependencies.reserve(json.size());

        for (nlohmann::json const& dependency_json : dependencies_json)
        {
            std::pmr::string name = dependency_json.at("name").get<std::pmr::string>();

            dependencies.push_back(
                Dependency
                {
                    .artifact_name = std::move(name)
                }
            );
        }

        return dependencies;
    }

    std::pmr::vector<std::pmr::string> parse_string_array(nlohmann::json const& json)
    {
        std::pmr::vector<std::pmr::string> includes;
        includes.reserve(json.size());

        for (nlohmann::json const& element : json)
        {
            includes.push_back(
                element.get<std::pmr::string>()
            );
        }

        return includes;
    }

    std::pmr::vector<std::pmr::string> parse_string_array_with_substitution(
        nlohmann::json const& json,
        Environment_variables const& environment_variables,
        std::string_view const field_name
    )
    {
        std::pmr::vector<std::pmr::string> values;
        values.reserve(json.size());

        for (nlohmann::json const& element : json)
        {
            if (!element.is_string())
                throw std::runtime_error(std::format("'{}' must contain only strings.", field_name));

            values.push_back(
                substitute_variables(element.get<std::string>(), environment_variables, field_name)
            );
        }

        return values;
    }

    std::pmr::vector<std::pmr::string> parse_string_array_at(nlohmann::json const& json, std::string_view const key)
    {
        if (!json.contains(key))
            return {};

        nlohmann::json const& json_array = json.at(key);        
        return parse_string_array(json_array);
    }

    std::pmr::vector<std::filesystem::path> parse_path_array(nlohmann::json const& json)
    {
        std::pmr::vector<std::filesystem::path> includes;
        includes.reserve(json.size());

        for (nlohmann::json const& element : json)
        {
            includes.push_back(
                element.get<std::pmr::string>()
            );
        }

        return includes;
    }

    std::pmr::vector<std::filesystem::path> parse_path_array_with_substitution(
        nlohmann::json const& json,
        Environment_variables const& environment_variables,
        std::string_view const field_name
    )
    {
        std::pmr::vector<std::filesystem::path> values;
        values.reserve(json.size());

        for (nlohmann::json const& element : json)
        {
            if (!element.is_string())
                throw std::runtime_error(std::format("'{}' must contain only strings.", field_name));

            values.push_back(
                std::filesystem::path{substitute_variables(element.get<std::string>(), environment_variables, field_name)}
            );
        }

        return values;
    }

    std::pmr::vector<std::filesystem::path> parse_path_array_at(nlohmann::json const& json, std::string_view const key)
    {
        if (!json.contains(key))
            return {};

        nlohmann::json const& json_array = json.at(key);        
        return parse_path_array(json_array);
    }

    std::pmr::vector<std::filesystem::path> convert_paths_to_absolute(std::span<std::filesystem::path const> const values, std::filesystem::path const& root_directory, std::pmr::polymorphic_allocator<> const& output_allocator)
    {
        std::pmr::vector<std::filesystem::path> output{output_allocator};

        for (std::filesystem::path const& value : values)
        {
            if (value.is_absolute())
            {
                output.push_back(
                    value.lexically_normal()
                );
            }
            else
            {
                output.push_back(
                    (root_directory / value).lexically_normal()
                );
            }
        }

        return output;
    }


    Executable_info parse_executable_info(nlohmann::json const& json, Environment_variables const& environment_variables)
    {
        Executable_info info{};

        if (json.contains("source"))
            info.source = std::filesystem::path{substitute_variables(json.at("source").get<std::string>(), environment_variables, "executable.source")};
        
        if (json.contains("entry_point"))
            info.source = json.at("entry_point").get<std::pmr::string>();
        
        return info;
    }

    std::pmr::vector<C_header> parse_c_headers(nlohmann::json const& json, Environment_variables const& environment_variables)
    {
        std::pmr::vector<C_header> headers;
        headers.reserve(json.size());

        for (nlohmann::json const& element : json)
        {
            C_header header
            {
                .module_name = element.at("name").get<std::pmr::string>(),
                .header = substitute_variables(element.at("header").get<std::string>(), environment_variables, "sources.headers.header"),
                .dependencies = parse_string_array_at(element, "dependencies"),
            };

            if (json.contains("allow_errors"))
                header.allow_errors = json.at("allow_errors").get<bool>();

            headers.push_back(std::move(header));
        }

        return headers;
    }

    std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> parse_external_library(nlohmann::json const& json, Environment_variables const& environment_variables)
    {
        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> map;
        map.reserve(json.size());

        for (auto const& pair : json.items())
        {
            std::pmr::string const key = substitute_variables(pair.key(), environment_variables, "library.external_libraries.key");
            
            nlohmann::json const& values = pair.value();
            for (auto const& value : values)
            {
                map.insert(std::make_pair(key, substitute_variables(value.get<std::string>(), environment_variables, "library.external_libraries.value")));
            }
        }

        return map;
    }

    Library_info parse_library_info(nlohmann::json const& json, Environment_variables const& environment_variables)
    {
        Library_info library_info;

        if (json.contains("external_libraries"))
            library_info.external_libraries = parse_external_library(json.at("external_libraries"), environment_variables);

        return library_info;
    }

    std::optional<Source_group::Data_type> parse_source_group_data(nlohmann::json const& json, Environment_variables const& environment_variables)
    {
        std::pmr::string const type = json.at("type").get<std::pmr::string>();

        if (type == "import_c_header")
        {
            Import_c_header_source_group data = {};

            if (json.contains("headers"))
                data.c_headers = parse_c_headers(json.at("headers"), environment_variables);

            if (json.contains("search_paths"))
                data.search_paths = parse_path_array_with_substitution(json.at("search_paths"), environment_variables, "sources.search_paths");

            if (json.contains("public_prefixes"))
                data.public_prefixes = parse_string_array(json.at("public_prefixes"));

            if (json.contains("remove_prefixes"))
                data.remove_prefixes = parse_string_array(json.at("remove_prefixes"));

            return data;
        }
        else if (type == "export_c_header")
        {
            Export_c_header_source_group data{};

            if (json.contains("output_directory"))
                data.output_directory = std::filesystem::path{substitute_variables(json.at("output_directory").get<std::string>(), environment_variables, "sources.output_directory")};

            return data;
        }
        else if (type == "c++")
        {
            return Cpp_source_group{};
        }
        else if (type == "iris")
        {
            return Iris_source_group{};
        }
        else
        {
            return std::nullopt;
        }
    }

    std::pmr::vector<Source_group> parse_source_groups(nlohmann::json const& json, Environment_variables const& environment_variables)
    {
        if (!json.contains("sources"))
            return {};

        nlohmann::json const& groups_json = json.at("sources");

        std::pmr::vector<Source_group> groups;
        groups.reserve(json.size());

        for (std::size_t index = 0; index < groups_json.size(); ++index)
        {
            nlohmann::json const& group_json = groups_json[index];

            if (!group_json.contains("type"))
                continue;

            std::optional<Source_group::Data_type> data = parse_source_group_data(group_json, environment_variables);
            std::pmr::vector<std::pmr::string> include = parse_string_array_at(group_json, "include");
            std::pmr::vector<std::pmr::string> additional_flags = parse_string_array_with_substitution(group_json.contains("additional_flags") ? group_json.at("additional_flags") : nlohmann::json::array(), environment_variables, "sources.additional_flags");

            groups.push_back(
                Source_group
                {
                    .data = std::move(data),
                    .include = std::move(include),
                    .additional_flags = std::move(additional_flags),
                }
            );
        }

        return groups;
    }

    std::optional<std::variant<Executable_info, Library_info>> parse_info(nlohmann::json const& json, Environment_variables const& environment_variables)
    {
        if (json.contains("executable"))
        {
            return parse_executable_info(json.at("executable"), environment_variables);
        }
        else if (json.contains("library"))
        {
            return parse_library_info(json.at("library"), environment_variables);
        }
        else
        {
            return std::nullopt;
        }
    }

    std::pmr::vector<Copy_entry> parse_copy_entries(nlohmann::json const& json, Environment_variables const& environment_variables)
    {
        if (!json.contains("copy"))
            return {};

        nlohmann::json const& copy_json = json.at("copy");

        std::pmr::vector<Copy_entry> entries;
        entries.reserve(copy_json.size());

        for (nlohmann::json const& entry_json : copy_json)
        {
            entries.push_back(
                Copy_entry
                {
                    .source = std::filesystem::path{substitute_variables(entry_json.at("source").get<std::string>(), environment_variables, "copy.source")},
                    .destination = std::filesystem::path{substitute_variables(entry_json.at("destination").get<std::string>(), environment_variables, "copy.destination")},
                }
            );
        }

        return entries;
    }

    Artifact get_artifact(std::filesystem::path const& artifact_file_path)
    {
        Environment_variables const environment_variables;
        return get_artifact(artifact_file_path, environment_variables);
    }

    Artifact get_artifact(
        std::filesystem::path const& artifact_file_path,
        Environment_variables const& environment_variables
    )
    {
        std::optional<std::pmr::string> const json_data = iris::common::get_file_contents(artifact_file_path.c_str());
        if (!json_data.has_value())
            iris::common::print_message_and_exit(std::format("Failed to read contents of {}", artifact_file_path.generic_string()));

        nlohmann::json const json = nlohmann::json::parse(json_data.value());

        std::pmr::string const name = json.at("name").get<std::string>().c_str();

        Version const version = parse_version(json.at("version").get<std::string>());

        Artifact_type const type = parse_artifact_type(json.at("type").get<std::string>());

        std::pmr::vector<Dependency> dependencies = parse_dependencies(json);

        std::pmr::vector<Source_group> source_groups = parse_source_groups(json, environment_variables);

        std::pmr::vector<std::filesystem::path> public_include_directories = json.contains("public_include_directories")
            ? parse_path_array_with_substitution(json.at("public_include_directories"), environment_variables, "public_include_directories")
            : std::pmr::vector<std::filesystem::path>{};

        std::optional<std::variant<Executable_info, Library_info>> info = parse_info(json, environment_variables);

        std::pmr::vector<Copy_entry> copy_entries = parse_copy_entries(json, environment_variables);

        std::filesystem::path const root_directory = artifact_file_path.parent_path();
        for (Copy_entry& entry : copy_entries)
        {
            if (!entry.source.is_absolute())
                entry.source = (root_directory / entry.source).lexically_normal();
        }

        return Artifact
        {
            .file_path = artifact_file_path,
            .name = std::move(name),
            .version = version,
            .type = type,
            .dependencies = std::move(dependencies),
            .sources = std::move(source_groups),
            .public_include_directories = std::move(public_include_directories),
            .info = std::move(info),
            .copy_entries = std::move(copy_entries),
        };
    }

    nlohmann::json path_array_to_json(
        std::span<std::filesystem::path const> const array
    )
    {
        nlohmann::json json;

        for (std::filesystem::path const& value : array)
        {
            json.push_back(value.generic_string());
        }

        return json;
    }

    void write_artifact_to_file(Artifact const& artifact, std::filesystem::path const& artifact_file_path)
    {
        nlohmann::json json;
        json["name"] = artifact.name;
        json["version"] = std::format("{}.{}.{}", artifact.version.major, artifact.version.minor, artifact.version.patch);

        if (artifact.type == Artifact_type::Executable)
            json["type"] = "executable";
        else if (artifact.type == Artifact_type::Library)
            json["type"] = "library";
        else
            iris::common::print_message_and_exit("Did not handle artifact.type!");

        if (!artifact.dependencies.empty())
        {
            nlohmann::json dependencies_json;

            for (Dependency const& dependency : artifact.dependencies)
            {
                nlohmann::json dependency_json
                {
                    { "name", dependency.artifact_name }
                };

                dependencies_json.push_back(std::move(dependency_json));
            }

            json["dependencies"] = std::move(dependencies_json);
        }

        if (!artifact.sources.empty())
        {
            nlohmann::json groups_json;

            for (Source_group const& group : artifact.sources)
            {
                nlohmann::json group_json;

                if (group.data.has_value())
                {
                    if (std::holds_alternative<Export_c_header_source_group>(*group.data))
                        group_json["type"] = "export_c_header";
                    else if (std::holds_alternative<Import_c_header_source_group>(*group.data))
                        group_json["type"] = "import_c_header";
                    else if (std::holds_alternative<Cpp_source_group>(*group.data))
                        group_json["type"] = "c++";
                    else if (std::holds_alternative<Iris_source_group>(*group.data))
                        group_json["type"] = "iris";

                    if (std::holds_alternative<Export_c_header_source_group>(*group.data))
                    {
                        Export_c_header_source_group const& c_headers_group = std::get<Export_c_header_source_group>(*group.data);

                        if (c_headers_group.output_directory.has_value())
                            group_json["output_directory"] = c_headers_group.output_directory->generic_string();
                    }
                    else if (std::holds_alternative<Import_c_header_source_group>(*group.data))
                    {
                        Import_c_header_source_group const& c_headers_group = std::get<Import_c_header_source_group>(*group.data);
                        
                        if (!c_headers_group.c_headers.empty())
                        {
                            nlohmann::json c_headers_json;

                            for (C_header const& c_header : c_headers_group.c_headers)
                            {
                                nlohmann::json c_header_json
                                {
                                    { "name", c_header.module_name },
                                    { "header", c_header.header }
                                };

                                if (c_header.allow_errors.has_value())
                                    c_header_json["allow_errors"] = c_header.allow_errors.value();

                                if (!c_header.dependencies.empty())
                                    c_header_json["dependencies"] = c_header.dependencies;

                                c_headers_json.push_back(std::move(c_header_json));
                            }

                            group_json["headers"] = std::move(c_headers_json);
                        }
                        
                        if (!c_headers_group.search_paths.empty())
                            group_json["search_paths"] = path_array_to_json(c_headers_group.search_paths);
                        
                        if (!c_headers_group.public_prefixes.empty())
                            group_json["public_prefixes"] = c_headers_group.public_prefixes;

                        if (!c_headers_group.remove_prefixes.empty())
                            group_json["remove_prefixes"] = c_headers_group.remove_prefixes;
                    }
                }

                if (!group.include.empty())
                    group_json["include"] = group.include;

                if (!group.additional_flags.empty())
                    group_json["additional_flags"] = group.additional_flags;

                groups_json.push_back(std::move(group_json));
            }

            json["sources"] = std::move(groups_json);
        }

        if (!artifact.public_include_directories.empty())
            json["public_include_directories"] = path_array_to_json(artifact.public_include_directories);

        if (artifact.info.has_value())
        {
            if (std::holds_alternative<Executable_info>(*artifact.info))
            {
                Executable_info const& executable_info = std::get<Executable_info>(*artifact.info);

                nlohmann::json executable_json
                {
                    { "source", executable_info.source },
                    { "entry_point", executable_info.entry_point },
                };

                json["executable"] = std::move(executable_json);
            }
            else if (std::holds_alternative<Library_info>(*artifact.info))
            {
                Library_info const& library_info = std::get<Library_info>(*artifact.info);

                nlohmann::json library_json;

                if (!library_info.external_libraries.empty())
                {
                    nlohmann::json external_libraries_json;

                    for (auto const& pair : library_info.external_libraries)
                    {
                        external_libraries_json[pair.first.c_str()].push_back(pair.second);
                    }

                    library_json["external_libraries"] = std::move(external_libraries_json);
                }

                if (!library_json.empty())
                    json["library"] = std::move(library_json);
            }
        }

        if (!artifact.copy_entries.empty())
        {
            nlohmann::json copy_json;

            for (Copy_entry const& entry : artifact.copy_entries)
            {
                copy_json.push_back(
                    nlohmann::json
                    {
                        { "source", entry.source.generic_string() },
                        { "destination", entry.destination.generic_string() },
                    }
                );
            }

            json["copy"] = std::move(copy_json);
        }

        std::string const json_string = json.dump(4);
        iris::common::write_to_file(artifact_file_path, json_string);
    }

    std::pmr::vector<std::filesystem::path> get_public_include_directories(Artifact const& artifact, std::span<Artifact const> const artifacts, std::pmr::polymorphic_allocator<> const& output_allocator, std::pmr::polymorphic_allocator<> const& temporaries_allocator)
    {
        std::pmr::vector<Artifact const*> const dependencies = get_artifact_dependencies(artifact, artifacts, true, temporaries_allocator, temporaries_allocator);

        std::pmr::vector<std::filesystem::path> public_include_directories{temporaries_allocator};
        for (Artifact const* const dependency : dependencies)
        {
            std::filesystem::path const root_directory = dependency->file_path.parent_path();
            std::pmr::vector<std::filesystem::path> const directories = convert_paths_to_absolute(dependency->public_include_directories, root_directory, temporaries_allocator);
            public_include_directories.insert(public_include_directories.end(), directories.begin(), directories.end());
        }

        std::filesystem::path const root_directory = artifact.file_path.parent_path();
        std::pmr::vector<std::filesystem::path> const directories = convert_paths_to_absolute(artifact.public_include_directories, root_directory, temporaries_allocator);
        public_include_directories.insert(public_include_directories.end(), directories.begin(), directories.end());

        return std::pmr::vector<std::filesystem::path>{public_include_directories, output_allocator};
    }

    std::pmr::vector<Source_group const*> get_export_c_header_source_groups(Artifact const& artifact, std::pmr::polymorphic_allocator<> const& output_allocator)
    {
        std::pmr::vector<Source_group const*> groups{output_allocator};
        groups.reserve(artifact.sources.size());

        for (Source_group const& group : artifact.sources)
        {
            if (std::holds_alternative<Export_c_header_source_group>(*group.data))
            {
                groups.push_back(&group);
            }
        }

        return groups;
    }

    std::pmr::vector<Source_group const*> get_c_header_source_groups(Artifact const& artifact, std::pmr::polymorphic_allocator<> const& output_allocator)
    {
        std::pmr::vector<Source_group const*> groups{output_allocator};
        groups.reserve(artifact.sources.size());

        for (Source_group const& group : artifact.sources)
        {
            if (std::holds_alternative<Import_c_header_source_group>(*group.data))
            {
                groups.push_back(&group);
            }
        }

        return groups;
    }

    
    bool contains_any_compilable_source(Artifact const& artifact)
    {
        for (Source_group const& group : artifact.sources)
        {
            if (group.data.has_value())
            {
                if (std::holds_alternative<Cpp_source_group>(*group.data) || std::holds_alternative<Iris_source_group>(*group.data))
                    return true;
            }
        }

        return false;
    }


    std::pmr::vector<C_header> get_c_headers(Artifact const& artifact, std::pmr::polymorphic_allocator<> const& output_allocator)
    {
        std::pmr::vector<C_header> headers{output_allocator};

        for (Source_group const& group : artifact.sources)
        {
            if (std::holds_alternative<Import_c_header_source_group>(*group.data))
            {
                Import_c_header_source_group const& c_header_group = std::get<Import_c_header_source_group>(*group.data);

                headers.insert(headers.end(), c_header_group.c_headers.begin(), c_header_group.c_headers.end());
            }
        }

        return headers;
    }

    C_header const* find_c_header(Artifact const& artifact, std::string_view const module_name)
    {
        for (Source_group const& group : artifact.sources)
        {
            if (std::holds_alternative<Import_c_header_source_group>(*group.data))
            {
                Import_c_header_source_group const& c_header_group = std::get<Import_c_header_source_group>(*group.data);

                auto const is_c_header = [&](C_header const& c_header) -> bool
                {
                    return c_header.module_name == module_name;
                };

                auto const location = std::find_if(c_header_group.c_headers.begin(), c_header_group.c_headers.end(), is_c_header);
                if (location != c_header_group.c_headers.end())
                {
                    C_header const& c_header = *location;
                    return &c_header;
                }
            }
        }

        return nullptr;
    }

    static std::optional<std::filesystem::path> search_file(
        std::string_view const filename,
        std::span<std::filesystem::path const> const search_paths
    )
    {
        std::filesystem::path const file_path = filename;
        if (file_path.is_absolute())
            return file_path;

        for (std::filesystem::path const& search_path : search_paths)
        {
            std::filesystem::path const absolute_file_path = search_path / file_path;
            if (std::filesystem::exists(absolute_file_path))
                return absolute_file_path.lexically_normal();
        }

        for (std::filesystem::path const& search_path : search_paths)
        {
            if (!std::filesystem::exists(search_path) || !std::filesystem::is_directory(search_path))
                continue;

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

    std::optional<std::filesystem::path> find_c_header_path(
        std::string_view const c_header,
        std::span<std::filesystem::path const> search_paths
    )
    {
        return search_file(c_header, search_paths);
    }


    static std::regex create_regex(std::string_view const regular_expression)
    {
        std::pmr::string modified_regular_expression{ regular_expression.begin(), regular_expression.begin() + regular_expression.size() };
        for (std::size_t index = 0; index < modified_regular_expression.size(); ++index)
        {
            // Escape dots:
            if (modified_regular_expression[index] == '.')
            {
                std::string_view const insert_value = "\\";
                modified_regular_expression.insert(index, insert_value);
                index += insert_value.size();
            }
            // '*' needs to match any valid path character:
            else if (modified_regular_expression[index] == '*')
            {
                std::string_view const insert_value = "[A-Za-z0-9\\-_\\.]";
                modified_regular_expression.insert(index, insert_value);
                index += insert_value.size();
            }
        }
        // Match the whole expression:
        modified_regular_expression.insert(0, "^");
        modified_regular_expression.push_back('$');

        std::regex const regex{ modified_regular_expression };

        return regex;
    }


    bool visit_included_files(
        std::filesystem::path const& root_path,
        std::string_view const regular_expression,
        std::function<bool(std::filesystem::path)> const& predicate
    )
    {
        // Current directory:
        if (regular_expression.starts_with("./"))
        {
            return visit_included_files(root_path, regular_expression.substr(2), predicate);
        }
        // Go to parent directory:
        else if (regular_expression.starts_with("../"))
        {
            return visit_included_files(root_path.parent_path(), regular_expression.substr(3), predicate);
        }
        // Recursively iterate through subdirectories
        else if (regular_expression.starts_with("**/"))
        {
            // Files in the current directory:
            {
                bool const done = visit_included_files(root_path, regular_expression.substr(3), predicate);
                if (done)
                    return true;
            }

            // Files in subdirectories:
            for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator{ root_path })
            {
                if (entry.is_directory())
                {
                    bool const done = visit_included_files(entry.path(), regular_expression.substr(3), predicate);
                    if (done)
                        return true;
                }
            }

            return false;
        }

        // Go to next directory:
        {
            auto const location = regular_expression.find_first_of("/");
            if (location != std::string_view::npos)
            {
                std::string_view const next_directory{ regular_expression.begin(), regular_expression.begin() + location };
                return visit_included_files(root_path / next_directory, regular_expression.substr(location + 1), predicate);
            }
        }

        std::regex const regex = create_regex(regular_expression);

        // Find finds in the current directory:
        {
            for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator{ root_path })
            {
                if (entry.is_regular_file())
                {
                    std::filesystem::path const entry_relative_path = std::filesystem::relative(entry.path(), root_path);

                    if (std::regex_match(entry_relative_path.generic_string(), regex))
                    {
                        if (predicate(entry.path()))
                            return true;
                    }
                }
            }

            return false;
        }
    }

    bool visit_included_files(
        Artifact const& artifact,
        std::function<bool(std::filesystem::path)> const& predicate
    )
    {
        for (Source_group const& group : artifact.sources)
        {
            if (std::holds_alternative<Iris_source_group>(*group.data))
            {
                for (std::string_view const regular_expression : group.include)
                {
                    bool const done = visit_included_files(artifact.file_path.parent_path(), regular_expression, predicate);
                    if (done)
                        return true;
                }
            }
        }

        return false;
    }

    std::pmr::vector<std::filesystem::path> find_included_files(
        std::filesystem::path const& root_path,
        std::string_view const regular_expression,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<std::filesystem::path> found_paths{ output_allocator };

        std::function<bool(std::filesystem::path)> predicate = [&found_paths](std::filesystem::path const& file_path) -> bool
        {
            found_paths.push_back(file_path.lexically_normal());
            return false;
        };

        visit_included_files(root_path, regular_expression, predicate);

        return found_paths;
    }

    std::pmr::vector<std::filesystem::path> find_included_files(
        std::filesystem::path const& root_path,
        std::span<std::pmr::string const> const regular_expressions,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<std::filesystem::path> found_paths{ temporaries_allocator };

        for (std::pmr::string const& regular_expression : regular_expressions)
        {
            std::pmr::vector<std::filesystem::path> included_files = find_included_files(
                root_path,
                regular_expression,
                temporaries_allocator
            );

            found_paths.insert(found_paths.end(), included_files.begin(), included_files.end());
        }

        return std::pmr::vector<std::filesystem::path>{found_paths, output_allocator};
    }

    std::pmr::vector<std::filesystem::path> find_included_files(
        std::filesystem::path const& root_path,
        std::span<std::pmr::string const> const regular_expressions,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::monotonic_buffer_resource temporaries_buffer_resource;
        std::pmr::polymorphic_allocator<> temporaries_allocator{ &temporaries_buffer_resource };

        std::pmr::vector<std::filesystem::path> all_found_files{ temporaries_allocator };

        for (std::string_view const regular_expression : regular_expressions)
        {
            std::pmr::vector<std::filesystem::path> const found_files = find_included_files(root_path, regular_expression, temporaries_allocator);

            all_found_files.insert(all_found_files.end(), found_files.begin(), found_files.end());
        }

        std::pmr::vector<std::filesystem::path> output{ output_allocator };
        output.assign(all_found_files.begin(), all_found_files.end());
        return output;
    }

    std::pmr::vector<std::filesystem::path> get_artifact_iris_source_files(
        Artifact const& artifact,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<std::filesystem::path> all_included_files{temporaries_allocator};

        for (Source_group const& group : artifact.sources)
        {
            if (std::holds_alternative<Iris_source_group>(*group.data))
            {
                std::pmr::vector<std::filesystem::path> included_files = find_included_files(artifact.file_path.parent_path(), group.include, temporaries_allocator);
                all_included_files.insert(all_included_files.end(), included_files.begin(), included_files.end());
            }
        }

        return std::pmr::vector<std::filesystem::path>{all_included_files.begin(), all_included_files.end(), output_allocator};
    }

    std::pmr::vector<std::filesystem::path> get_artifact_cpp_source_files(
        Artifact const& artifact,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<std::filesystem::path> all_included_files{temporaries_allocator};

        for (Source_group const& group : artifact.sources)
        {
            if (std::holds_alternative<Cpp_source_group>(*group.data))
            {
                std::pmr::vector<std::filesystem::path> included_files = find_included_files(artifact.file_path.parent_path(), group.include, temporaries_allocator);
                all_included_files.insert(all_included_files.end(), included_files.begin(), included_files.end());
            }
        }

        return std::pmr::vector<std::filesystem::path>{all_included_files.begin(), all_included_files.end(), output_allocator};
    }


    std::filesystem::path find_root_include_directory(
        std::filesystem::path const& root_path,
        std::string_view const regular_expression
    )
    {
        // Current directory:
        if (regular_expression.starts_with("./"))
        {
            return find_root_include_directory(root_path, regular_expression.substr(2));
        }
        // Go to parent directory:
        else if (regular_expression.starts_with("../"))
        {
            return find_root_include_directory(root_path.parent_path(), regular_expression.substr(3));
        }
        // Recursively iterate through subdirectories
        else if (regular_expression.starts_with("**/"))
        {
            return root_path;
        }

        // Go to next directory:
        {
            auto const location = regular_expression.find_first_of("/");
            if (location != std::string_view::npos)
            {
                std::string_view const next_directory{ regular_expression.begin(), regular_expression.begin() + location };
                return find_root_include_directory(root_path / next_directory, regular_expression.substr(location + 1));
            }
        }

        return root_path;
    }

    std::pmr::vector<std::filesystem::path> find_root_include_directories(
        std::filesystem::path const& root_path,
        std::span<std::pmr::string const> const regular_expressions,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::monotonic_buffer_resource temporaries_buffer_resource;
        std::pmr::polymorphic_allocator<> temporaries_allocator{ &temporaries_buffer_resource };

        std::pmr::vector<std::filesystem::path> all_found_roots{ temporaries_allocator };

        for (std::string_view const regular_expression : regular_expressions)
        {
            std::filesystem::path const found_root = find_root_include_directory(root_path, regular_expression);
            all_found_roots.push_back(found_root);
        }

        std::pmr::vector<std::filesystem::path> output{ output_allocator };
        output.assign(all_found_roots.begin(), all_found_roots.end());
        return output;
    }

    std::pmr::vector<std::filesystem::path> find_root_include_directories(
        Artifact const& artifact,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<std::filesystem::path> all_included_files{temporaries_allocator};

        for (Source_group const& group : artifact.sources)
        {
            if (std::holds_alternative<Iris_source_group>(*group.data))
            {
                std::pmr::vector<std::filesystem::path> included_files = find_root_include_directories(artifact.file_path.parent_path(), group.include, temporaries_allocator);
                all_included_files.insert(all_included_files.end(), included_files.begin(), included_files.end());
            }
        }

        return std::pmr::vector<std::filesystem::path>{all_included_files.begin(), all_included_files.end(), output_allocator};
    }

    std::optional<std::size_t> find_artifact_index_that_includes_source_file(
        std::span<Artifact const> const artifacts,
        std::filesystem::path const& source_file_path,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        for (std::size_t index = 0; index < artifacts.size(); ++index)
        {
            Artifact const& artifact = artifacts[index];

            std::pmr::vector<std::filesystem::path> const source_files = get_artifact_iris_source_files(artifact, temporaries_allocator, temporaries_allocator);
            for (std::filesystem::path const& current_source_file : source_files)
            {
                if (current_source_file == source_file_path)
                    return index;
            }
        }

        return std::nullopt;
    }

    static void add_artifact_dependencies(
        Artifact const& artifact,
        std::span<Artifact const> const all_artifacts,
        bool const recursive,
        std::pmr::vector<Artifact const*>& dependencies
    )
    {
        for (Dependency const& dependency : artifact.dependencies)
        {
            auto const dependency_artifact_location = std::find_if(
                all_artifacts.begin(),
                all_artifacts.end(),
                [&dependency](Artifact const& artifact) -> bool
                {
                    return artifact.name == dependency.artifact_name;
                }
            );

            if (dependency_artifact_location == all_artifacts.end())
                continue;

            Artifact const& dependency_artifact = *dependency_artifact_location;

            if (recursive)
                add_artifact_dependencies(dependency_artifact, all_artifacts, true, dependencies);

            dependencies.push_back(&dependency_artifact);
        }
    }

    std::pmr::vector<Artifact const*> get_artifact_dependencies(
        Artifact const& artifact,
        std::span<Artifact const> const all_artifacts,
        bool const recursive,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<Artifact const*> dependencies{temporaries_allocator};
        add_artifact_dependencies(artifact, all_artifacts, recursive, dependencies);

        std::sort(dependencies.begin(), dependencies.end());
        dependencies.erase(std::unique(dependencies.begin(), dependencies.end()), dependencies.end());

        return std::pmr::vector<Artifact const*>{dependencies, output_allocator};
    }

    std::pmr::vector<iris::Module const*> get_artifact_modules_and_dependencies(
        Artifact const& artifact,
        std::span<Artifact const> const all_artifacts,
        std::span<iris::Module const> const header_modules,
        std::span<iris::Module const> const core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<iris::Module const*> output{ temporaries_allocator };

        auto const add_modules = [&](std::span<std::filesystem::path const> const source_files) -> void
        {
            output.reserve(output.size() + source_files.size());

            for (std::filesystem::path const& source_file : source_files)
            {
                auto const location = std::find_if(
                    core_modules.begin(),
                    core_modules.end(),
                    [&source_file](iris::Module const& module) -> bool
                    {
                        return module.source_file_path.has_value() && *module.source_file_path == source_file;
                    }
                );

                if (location != core_modules.end())
                    output.push_back(&*location);
            }
        };

        auto const add_headers = [&](std::span<C_header const> const headers) -> void
        {
            output.reserve(output.size() + headers.size());

            for (C_header const& header : headers)
            {
                auto const location = std::find_if(
                    header_modules.begin(),
                    header_modules.end(),
                    [&header](iris::Module const& module) -> bool
                    {
                        return module.name == header.module_name;
                    }
                );

                if (location != header_modules.end())
                    output.push_back(&*location);
            }
        };

        std::pmr::vector<std::filesystem::path> const source_files = get_artifact_iris_source_files(artifact, temporaries_allocator, temporaries_allocator);
        std::span<C_header const> const c_headers = get_c_headers(artifact, temporaries_allocator);

        add_modules(source_files);
        add_headers(c_headers);

        for (Dependency const& dependency : artifact.dependencies)
        {
            auto const dependency_artifact_location = std::find_if(
                all_artifacts.begin(),
                all_artifacts.end(),
                [&dependency](Artifact const& artifact) -> bool
                {
                    return artifact.name == dependency.artifact_name;
                }
            );

            if (dependency_artifact_location == all_artifacts.end())
                continue;

            Artifact const& dependency_artifact = *dependency_artifact_location;

            std::pmr::vector<std::filesystem::path> const dependency_source_files = get_artifact_iris_source_files(dependency_artifact, temporaries_allocator, temporaries_allocator);
            std::pmr::vector<C_header> const dependency_c_headers = get_c_headers(dependency_artifact, temporaries_allocator);
            
            add_modules(dependency_source_files);
            add_headers(dependency_c_headers);
        }

        return std::pmr::vector<iris::Module const*>{std::move(output), output_allocator};
    }

    std::optional<External_library_info> get_external_library(
        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const& external_libraries,
        Target const& target,
        bool const prefer_debug,
        bool const prefer_dynamic
    )
    {
        std::array<bool, 2> const debug_priority
        {
            prefer_debug,
            !prefer_debug
        };
        
        std::array<bool, 2> const dynamic_priority
        {
            prefer_dynamic,
            !prefer_dynamic,
        };

        for (std::size_t debug_index = 0; debug_index < debug_priority.size(); ++debug_index)
        {
            for (std::size_t dynamic_index = 0; dynamic_index < dynamic_priority.size(); ++dynamic_index)
            {
                bool const is_debug = debug_priority[debug_index];
                bool const is_dynamic = dynamic_priority[dynamic_index];
                std::string const target_library = std::format("{}-{}-{}", target.operating_system, is_dynamic ? "dynamic" : "static", is_debug ? "debug" : "release");

                auto const range = external_libraries.equal_range(target_library.c_str());
                if (range.first != external_libraries.end())
                {
                    std::pmr::vector<std::pmr::string> names;
                    names.reserve(std::distance(range.first, range.second));

                    for (auto iterator = range.first; iterator != range.second; ++iterator)
                    {
                        names.push_back(iterator->second);
                    }

                    return External_library_info
                    {
                        .key = std::pmr::string{target_library},
                        .names = std::move(names),
                        .is_debug = is_debug,
                        .is_dynamic = is_dynamic,
                    };
                }
            }
        }

        return std::nullopt;
    }

    std::pmr::vector<std::string_view> get_external_library_dlls(
        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const& external_libraries,
        std::string_view const key
    )
    {
        std::string const target_library = std::format("{}-dll", key);

        auto const range = external_libraries.equal_range(target_library.c_str());
        if (range.first == external_libraries.end())
            return {};

        std::pmr::vector<std::string_view> dlls;
        dlls.reserve(std::distance(range.first, range.second));

        for (auto iterator = range.first; iterator != range.second; ++iterator)
        {
            dlls.push_back(iterator->second);
        }
        
        return dlls;
    }
}
