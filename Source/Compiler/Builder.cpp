module;

#include <assert.h>
#include <stdio.h>

module iris.compiler.builder;

import std;
import llvm;

import iris.binary_serializer;
import iris.core;
import iris.core.struct_layout;
import iris.common;
import iris.common.filesystem;
import iris.common.filesystem_common;
import iris.compiler;
import iris.compiler.analysis;
import iris.compiler.artifact;
import iris.compiler.clang_code_generation;
import iris.compiler.clang_compiler;
import iris.compiler.clang_data;
import iris.compiler.compile_commands_generator;
import iris.compiler.linker;
import iris.compiler.profiler;
import iris.compiler.repository;
import iris.compiler.target;
import iris.compiler.test_framework;
import iris.compiler.types;
import iris.c_header_converter;
import iris.c_header_exporter;
import iris.json_serializer;
import iris.parser.convertor;
import iris.parser.parse_tree;
import iris.parser.parser;

namespace iris::compiler
{
    static std::filesystem::path get_hl_build_directory(
        std::filesystem::path const& build_directory_path
    )
    {
        return build_directory_path / "artifacts";
    }

    static std::filesystem::path get_bitcode_build_directory(
        std::filesystem::path const& build_directory_path
    )
    {
        return build_directory_path / "artifacts";
    }

    static void create_directory_if_it_does_not_exist(std::filesystem::path const& path)
    {
        if (!std::filesystem::exists(path))
        {
            std::filesystem::create_directories(path);
        }
    }

    Profiler* get_profiler(Builder& builder)
    {
        return builder.use_profiler ? &builder.profiler : nullptr;
    }

    Builder create_builder(
        iris::compiler::Target const& target,
        std::filesystem::path const& build_directory_path,
        std::span<std::filesystem::path const> header_search_paths,
        std::span<std::filesystem::path const> repository_paths,
        iris::compiler::Compilation_options const& compilation_options,
        Builder_options const& builder_options,
        std::pmr::polymorphic_allocator<> output_allocator
    )
    {
        std::pmr::vector<std::filesystem::path> all_header_search_paths{output_allocator};
        all_header_search_paths.reserve(header_search_paths.size() + 1);
        all_header_search_paths.push_back(build_directory_path / "include");
        all_header_search_paths.insert(all_header_search_paths.end(), header_search_paths.begin(), header_search_paths.end());

        std::pmr::vector<std::filesystem::path> all_repository_paths(output_allocator);
        all_repository_paths.reserve(repository_paths.size() + 1);
        all_repository_paths.insert(all_repository_paths.end(), repository_paths.begin(), repository_paths.end());

        if (builder_options.is_test_mode)
        {
            std::filesystem::path const standard_repository_file_path = iris::common::get_standard_repository_file_path();
            auto const location = std::find(all_repository_paths.begin(), all_repository_paths.end(), standard_repository_file_path);
            if (location == all_repository_paths.end())
                all_repository_paths.push_back(standard_repository_file_path);
        }

        return
        {
            .target = target,
            .build_directory_path = build_directory_path,
            .header_search_paths = std::move(all_header_search_paths),
            .repositories = get_repositories(all_repository_paths),
            .compilation_options = compilation_options,
            .profiler = {},
            .use_profiler = true,
            .output_module_json = false,
            .output_llvm_ir = builder_options.output_llvm_ir,
            .is_test_mode = builder_options.is_test_mode,
        };
    }

    void build_artifact(
        Builder& builder,
        std::filesystem::path const& artifact_file_path
    )
    {
        build_artifacts(builder, { &artifact_file_path, 1 });
    }

    void build_artifacts(
        Builder& builder,
        std::span<std::filesystem::path const> const artifact_file_paths
    )
    {
        start_timer(get_profiler(builder), "build_artifact");

        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        std::filesystem::path const hl_build_directory = get_hl_build_directory(
            builder.build_directory_path
        );
        create_directory_if_it_does_not_exist(hl_build_directory);

        std::pmr::vector<Artifact> const artifacts = get_sorted_artifacts(
            artifact_file_paths,
            builder.repositories,
            builder.is_test_mode,
            output_allocator,
            temporaries_allocator
        );

        std::pmr::vector<std::filesystem::path> const source_file_paths = get_artifacts_source_files(
            artifacts,
            output_allocator,
            temporaries_allocator
        );

        std::pmr::vector<iris::Module> core_modules = parse_source_files_and_cache(
            builder,
            source_file_paths,
            output_allocator,
            temporaries_allocator
        );
        if (builder.is_test_mode)
            core_modules.reserve(core_modules.size() + artifact_file_paths.size());
        std::span<iris::Module const> const non_test_modules = core_modules;

        if (builder.is_test_mode)
        {
            std::pmr::vector<iris::Module> const test_artifact_modules = create_test_artifact_modules(
                builder,
                artifact_file_paths,
                artifacts,
                core_modules,
                builder.compilation_options,
                temporaries_allocator
            );
            assert(core_modules.size() + test_artifact_modules.size() <= core_modules.capacity());
            core_modules.insert(core_modules.end(), test_artifact_modules.begin(), test_artifact_modules.end());
        }

        Modules_and_declaration_database modules_and_declaration_database = import_and_export_c_headers(
            builder,
            artifacts,
            core_modules,
            false,
            output_allocator,
            temporaries_allocator
        );
        std::span<iris::Module const> const header_modules = modules_and_declaration_database.header_modules;
        std::span<iris::Module const* const> const sorted_modules = modules_and_declaration_database.sorted_modules;

        validate_modules_and_exit_if_needed(core_modules, modules_and_declaration_database.declaration_database, temporaries_allocator);

        Compilation_options const& compilation_options = builder.compilation_options;

        LLVM_data llvm_data = initialize_llvm(
            compilation_options
        );

        if (!compile_cpp_and_write_to_bitcode_files(builder, artifacts, llvm_data, compilation_options, temporaries_allocator))
            iris::common::print_message_and_exit(std::format("Failed to compile c++."));

        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map = create_module_name_to_file_path_map(
            builder,
            header_modules,
            core_modules,
            output_allocator,
            temporaries_allocator
        );

        compile_and_write_to_bitcode_files(
            builder,
            non_test_modules,
            module_name_to_file_path_map,
            llvm_data,
            modules_and_declaration_database.declaration_database,
            compilation_options,
            false
        );

        if (builder.is_test_mode)
        {
            iris::compiler::Compilation_options test_compilation_options = compilation_options;
            test_compilation_options.is_test_mode = true;
            compile_and_write_to_bitcode_files(
                builder,
                core_modules,
                module_name_to_file_path_map,
                llvm_data,
                modules_and_declaration_database.declaration_database,
                test_compilation_options,
                true
            );
        }

        std::pmr::vector<iris::compiler::Artifact const*> const artifacts_to_link = get_artifact_pointers(artifacts, temporaries_allocator);
        link_artifacts(
            builder,
            artifacts,
            artifacts_to_link,
            compilation_options.debug,
            false,
            temporaries_allocator
        );

        if (builder.is_test_mode)
        {
            std::pmr::vector<iris::compiler::Artifact const*> const artifacts_to_test = filter_test_artifacts(artifact_file_paths, artifacts, core_modules, temporaries_allocator);
            link_artifacts(
                builder,
                artifacts,
                artifacts_to_test,
                compilation_options.debug,
                true,
                temporaries_allocator
            );
        }

        if (builder.target.operating_system == "windows")
        {
            copy_dlls(
                builder,
                artifacts,
                temporaries_allocator
            );
        }

        end_timer(get_profiler(builder), "build_artifact");

        print_profiler_timings(get_profiler(builder));
    }

    void add_artifact_dependencies(
        std::pmr::vector<Artifact>& dependencies,
        Artifact const& artifact,
        std::span<Repository const> repositories,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        for (iris::compiler::Dependency const& dependency : artifact.dependencies)
        {
            auto const location = std::find_if(
                dependencies.begin(),
                dependencies.end(),
                [&](Artifact const& artifact) -> bool { return artifact.name == dependency.artifact_name; }
            );
            if (location != dependencies.end())
                continue;

            std::optional<std::filesystem::path> const dependency_location = iris::compiler::get_artifact_location(repositories, dependency.artifact_name);
            if (!dependency_location.has_value())
            {
                std::fprintf(stderr, "Could not find dependency '%s'.", dependency.artifact_name.c_str());
                continue;
            }

            std::filesystem::path const dependency_configuration_file_path = dependency_location.value();

            Artifact dependency_artifact = iris::compiler::get_artifact(dependency_configuration_file_path);
            
            add_artifact_dependencies(
                dependencies,
                dependency_artifact,
                repositories,
                output_allocator,
                temporaries_allocator
            );

            dependencies.push_back(std::move(dependency_artifact));
        }
    }

    std::pmr::vector<Artifact> get_sorted_artifacts(
        std::span<std::filesystem::path const> const artifact_file_paths,
        std::span<Repository const> repositories,
        bool const is_test_mode,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<Artifact> artifacts{temporaries_allocator};

        if (is_test_mode)
        {
            auto const location = std::find_if(artifacts.begin(), artifacts.end(), [](Artifact const& artifact) -> bool { return artifact.name == "Cpp_standard_library"; });
            if (location == artifacts.end())
            {
                std::optional<std::filesystem::path> const cpp_standard_library_artifact_file_path = iris::compiler::get_artifact_location(repositories, "Cpp_standard_library");
                if (!cpp_standard_library_artifact_file_path.has_value())
                    iris::common::print_message_and_exit("Could not find dependency 'Cpp_standard_library'");

                Artifact const artifact = get_artifact(cpp_standard_library_artifact_file_path.value());

                add_artifact_dependencies(
                    artifacts,
                    artifact,
                    repositories,
                    output_allocator,
                    temporaries_allocator
                );
                
                artifacts.push_back(artifact);
            }
        }

        for (std::filesystem::path const& artifact_file_path : artifact_file_paths)
        {
            Artifact const artifact = get_artifact(artifact_file_path);

            add_artifact_dependencies(
                artifacts,
                artifact,
                repositories,
                output_allocator,
                temporaries_allocator
            );
            
            artifacts.push_back(artifact);
        }

        return std::pmr::vector<Artifact>{std::move(artifacts), output_allocator};
    }


    std::pmr::vector<std::filesystem::path> get_artifacts_source_files(
        std::span<Artifact const> const artifacts,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<std::filesystem::path> source_files{temporaries_allocator};

        source_files.push_back(iris::common::get_builtin_module_file_path());

        for (Artifact const& artifact : artifacts)
        {
            std::pmr::vector<std::filesystem::path> artifact_source_files = get_artifact_iris_source_files(
                artifact,
                temporaries_allocator,
                temporaries_allocator
            );

            source_files.insert(source_files.end(), artifact_source_files.begin(), artifact_source_files.end());
        }

        return std::pmr::vector<std::filesystem::path>{std::move(source_files), output_allocator};
    }

    struct C_header_groups
    {
        std::pmr::vector<C_header> c_headers;
        std::pmr::vector<std::pmr::vector<std::filesystem::path>> header_search_paths;
        std::pmr::vector<Import_c_header_source_group const*> source_groups;
    };

    static C_header_groups get_c_headers(
        std::span<iris::compiler::Artifact const> const artifacts,
        std::span<std::filesystem::path const> const builder_header_search_paths,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::filesystem::path const builtin_include_directory = iris::common::get_builtin_include_directory();

        std::pmr::vector<C_header> all_c_headers{temporaries_allocator};
        std::pmr::vector<std::pmr::vector<std::filesystem::path>> all_header_search_paths{temporaries_allocator};
        std::pmr::vector<Import_c_header_source_group const*> all_source_groups{temporaries_allocator};
        
        for (iris::compiler::Artifact const& artifact : artifacts)
        {
            std::filesystem::path const artifact_parent_path = artifact.file_path.parent_path();
            std::pmr::vector<Source_group const*> const c_header_source_groups = get_c_header_source_groups(artifact, temporaries_allocator);

            for (Source_group const* source_group : c_header_source_groups)
            {
                Import_c_header_source_group const& c_header_source_group = std::get<Import_c_header_source_group>(*source_group->data);
                std::span<C_header const> const c_headers = c_header_source_group.c_headers;
                all_c_headers.insert(all_c_headers.end(), c_headers.begin(), c_headers.end());

                std::pmr::vector<std::filesystem::path> header_search_paths{temporaries_allocator};
                header_search_paths.reserve(builder_header_search_paths.size() + c_header_source_group.search_paths.size() + 2);
                header_search_paths.push_back(builtin_include_directory);
                header_search_paths.push_back(artifact_parent_path);
                header_search_paths.insert(header_search_paths.end(), c_header_source_group.search_paths.begin(), c_header_source_group.search_paths.end());
                header_search_paths.insert(header_search_paths.end(), builder_header_search_paths.begin(), builder_header_search_paths.end());

                for (std::size_t index = 0; index < c_headers.size(); ++index)
                {
                    all_header_search_paths.push_back(header_search_paths);
                    all_source_groups.push_back(&c_header_source_group);
                }
            }
        }

        assert(all_c_headers.size() == all_source_groups.size());
        return {
            std::pmr::vector<C_header>{all_c_headers.begin(), all_c_headers.end(), output_allocator},
            std::pmr::vector<std::pmr::vector<std::filesystem::path>>{all_header_search_paths.begin(), all_header_search_paths.end(), output_allocator},
            std::pmr::vector<Import_c_header_source_group const*>{all_source_groups.begin(), all_source_groups.end(), output_allocator},
        };
    }

    static std::optional<iris::Module> parse_c_header_and_cache(
        std::filesystem::path const& build_directory_path,
        bool const output_module_json,
        std::span<std::filesystem::path const> const header_search_paths,
        C_header const& c_header,
        Import_c_header_source_group const& source_group,
        bool const force_allow_errors
    )
    {
        std::string_view const header_module_name = c_header.module_name;
        std::string_view const header_filename = c_header.header;

        std::optional<std::filesystem::path> const header_path = iris::compiler::find_c_header_path(header_filename, header_search_paths);
        if (!header_path.has_value())
            iris::common::print_message_and_exit(std::format("Could not find header {}. Please provide its location using --header-search-path.", header_filename));

        std::filesystem::path const header_module_filename = std::format("{}.irisb", header_module_name);
        std::filesystem::path const output_header_module_path = build_directory_path / "artifacts" / header_module_filename;

        if (std::filesystem::exists(output_header_module_path))
        {
            if (is_file_newer_than(output_header_module_path, header_path.value()))
            {
                std::optional<Module> header_module = iris::binary_serializer::read_module_from_file(output_header_module_path);

                if (!header_module.has_value())
                    iris::common::print_message_and_exit(std::format("Failed to read cached module {}.", output_header_module_path.generic_string()));

                return header_module.value();
            }
        }

        iris::c::Options const options
        {
            .target_triple = std::nullopt,
            .include_directories = header_search_paths,
            .public_prefixes = source_group.public_prefixes,
            .remove_prefixes = source_group.remove_prefixes,
            .allow_errors = force_allow_errors ? true : (c_header.allow_errors.has_value() ? c_header.allow_errors.value() : false),
        };

        ::printf("Importing c header \"%s\"\n    output is \"%s\"\n", header_path->generic_string().c_str(), output_header_module_path.generic_string().c_str());
        if (!options.include_directories.empty())
        {
            ::printf("    header search paths\n");
            for (std::filesystem::path const& include_directory : options.include_directories)
            {
                ::printf("    - \"%s\"\n", include_directory.generic_string().c_str());
            }
        }

        std::optional<iris::Module> header_module = iris::c::import_header_and_write_to_file(header_module_name, header_path.value(), output_header_module_path, options);
        if (!header_module.has_value())
            return std::nullopt;

        if (output_module_json)
        {
            std::filesystem::path const output_module_json_filename = std::format("{}.irisb.json", header_module_name);
            std::filesystem::path const output_module_json_path = get_hl_build_directory(build_directory_path) / output_module_json_filename;
            iris::json::write_module_to_file(output_module_json_path, *header_module);
        }

        return header_module.value();
    }

    static std::filesystem::path create_output_header_path(
        Builder& builder,
        std::optional<std::filesystem::path> const& output_directory,
        std::string_view const module_name,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::filesystem::path module_path = {};
        std::pmr::vector<std::string_view> const parts = iris::common::split_string(module_name, '.', temporaries_allocator);
        for (std::string_view const part : parts)
            module_path = module_path / part;

        if (output_directory.has_value())
        {
            if (output_directory.value().is_absolute())
                return output_directory.value() / module_path;
            else
                return builder.build_directory_path / output_directory.value() / module_path;
        }

        return (builder.build_directory_path / "include" / module_path).replace_extension(".h");
    }

    static std::pmr::unordered_map<std::pmr::string, std::filesystem::path> create_output_header_paths(
        Builder& builder,
        std::span<Artifact const> const artifacts,
        std::span<iris::Module const> const core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> output_header_paths{temporaries_allocator};

        for (iris::compiler::Artifact const& artifact : artifacts)
        {
            std::pmr::vector<Source_group const*> const source_groups = get_export_c_header_source_groups(artifact, temporaries_allocator);
            
            for (Source_group const* group : source_groups)
            {
                iris::compiler::Export_c_header_source_group const& export_group = std::get<iris::compiler::Export_c_header_source_group>(group->data.value());
                
                std::pmr::vector<std::filesystem::path> const included_files = iris::compiler::find_included_files(artifact.file_path.parent_path(), group->include, temporaries_allocator, temporaries_allocator);
                
                for (iris::Module const& core_module : core_modules)
                {
                    if (!core_module.source_file_path.has_value())
                        continue;

                    auto const location = std::find(included_files.begin(), included_files.end(), core_module.source_file_path.value());
                    if (location == included_files.end())
                        continue;

                    std::filesystem::path output_header_path = create_output_header_path(builder, export_group.output_directory, core_module.name, temporaries_allocator);
                    output_header_paths.insert(std::make_pair(core_module.name, std::move(output_header_path)));
                }
            }
        }

        return std::pmr::unordered_map<std::pmr::string, std::filesystem::path>{output_header_paths, output_allocator};
    }

    static void generate_c_header_file(
        iris::Module const& core_module,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& output_header_paths,
        iris::Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::filesystem::path const& output_c_header_path = output_header_paths.at(core_module.name);
        std::filesystem::path const output_cpp_header_path = std::filesystem::path{output_c_header_path}.replace_extension("hpp");

        if (core_module.source_file_path.has_value())
        {
            std::filesystem::path const& source_file_path = core_module.source_file_path.value();

            if (std::filesystem::exists(output_c_header_path) && std::filesystem::exists(output_cpp_header_path))
            {
                if (is_file_newer_than(output_c_header_path, source_file_path))
                {
                    return;
                }
            }
        }

        create_directory_if_it_does_not_exist(output_c_header_path.parent_path());
        
        ::printf("Generating c header \"%s\"\n", output_c_header_path.generic_string().c_str());
        iris::c::Exported_c_header const c_header = iris::c::export_module_as_c_header(core_module, declaration_database, output_header_paths, temporaries_allocator, temporaries_allocator);
        iris::common::write_to_file(output_c_header_path, c_header.content);

        ::printf("Generating c++ header \"%s\"\n", output_cpp_header_path.generic_string().c_str());
        iris::c::Exported_cpp_header const cpp_header = iris::c::export_module_as_cpp_header(core_module, output_c_header_path, temporaries_allocator, temporaries_allocator);
        iris::common::write_to_file(output_cpp_header_path, cpp_header.content);
    }

    enum class Source_file_node_type
    {
        Module = 0,
        Export_c_header,
        Import_c_header,
    };

    struct Source_file_node
    {
        std::string_view module_name;
        Source_file_node_type type;
        std::size_t index;
    };

    struct Source_file_graph_rank_range
    {
        std::size_t start_index;
        std::size_t count;
    };

    struct Source_file_graph
    {
        std::pmr::vector<Source_file_node> nodes;
        std::pmr::vector<Source_file_graph_rank_range> ranks;
    };

    Source_file_graph create_source_file_graph(
        std::span<C_header const> const c_headers,
        std::span<iris::Module const> const core_modules,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& output_header_paths,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::unordered_map<std::string_view, std::size_t> module_name_to_node_index;
        std::pmr::vector<Source_file_node> nodes{temporaries_allocator};
        std::unordered_multimap<std::size_t, std::size_t> edges;

        for (std::size_t index = 0; index < c_headers.size(); ++index)
        {
            C_header const& c_header = c_headers[index];

            Source_file_node const node
            {
                .module_name = c_header.module_name,
                .type = Source_file_node_type::Import_c_header,
                .index = index,
            };
            module_name_to_node_index[c_header.module_name] = nodes.size();
            nodes.push_back(node);
        }

        for (std::size_t index = 0; index < core_modules.size(); ++index)
        {
            iris::Module const& core_module = core_modules[index];

            auto const output_header_path_location = output_header_paths.find(core_module.name);
            Source_file_node_type const node_type = output_header_path_location != output_header_paths.end() ? Source_file_node_type::Export_c_header : Source_file_node_type::Module;

            Source_file_node const node
            {
                .module_name = core_module.name,
                .type = node_type,
                .index = index,
            };
            module_name_to_node_index[core_module.name] = nodes.size();
            nodes.push_back(node);
        }

        for (iris::compiler::C_header const& c_header : c_headers)
        {
            std::size_t const source_node_index = module_name_to_node_index.at(c_header.module_name);
            for (std::string_view const dependency : c_header.dependencies)
            {
                std::size_t const destination_node_index = module_name_to_node_index.at(dependency);
                edges.insert(std::make_pair(source_node_index, destination_node_index));
            }
        }

        for (iris::Module const& core_module : core_modules)
        {
            std::size_t const source_node_index = module_name_to_node_index.at(core_module.name);
            for (iris::Import_module_with_alias const& import_module : core_module.dependencies.alias_imports)
            {
                std::size_t const destination_node_index = module_name_to_node_index.at(import_module.module_name);
                edges.insert(std::make_pair(source_node_index, destination_node_index));
            }
        }
        
        std::pmr::vector<Source_file_graph_rank_range> ranks{temporaries_allocator};
        std::pmr::vector<std::size_t> sorted_nodes_indices{temporaries_allocator};
        sorted_nodes_indices.reserve(nodes.size());
        
        while (sorted_nodes_indices.size() < nodes.size())
        {
            std::size_t rank_start_index = sorted_nodes_indices.size();
            
            for (std::size_t node_index = 0; node_index < nodes.size(); ++node_index)
            {
                if (!edges.contains(node_index))
                {
                    auto const location = std::find(sorted_nodes_indices.begin(), sorted_nodes_indices.end(), node_index);
                    if (location == sorted_nodes_indices.end())
                        sorted_nodes_indices.push_back(node_index);
                }
            }
            
            for (std::size_t index = rank_start_index; index < sorted_nodes_indices.size(); ++index)
            {
                std::size_t const node_index = sorted_nodes_indices[index];
                std::erase_if(edges, [&](std::pair<std::size_t const, std::size_t> const& edge) -> bool { return edge.second == node_index; });
            }
            
            ranks.push_back({ .start_index = rank_start_index, .count = sorted_nodes_indices.size() - rank_start_index });
        }
        
        Source_file_graph graph{ .nodes{temporaries_allocator}, .ranks{temporaries_allocator} };
        graph.nodes.resize(sorted_nodes_indices.size());

        for (std::size_t index = 0; index < sorted_nodes_indices.size(); ++index)
        {
            std::size_t node_index = sorted_nodes_indices[index];
            graph.nodes[index] = nodes[node_index];
        }

        graph.ranks = std::move(ranks);

        return graph;
    }

    Modules_and_declaration_database import_and_export_c_headers(
        Builder& builder,
        std::span<Artifact const> const artifacts,
        std::span<iris::Module> const core_modules,
        bool const force_allow_errors,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        start_timer(get_profiler(builder), "import_and_export_c_headers");

        create_directory_if_it_does_not_exist(builder.build_directory_path / "artifacts");

        Declaration_database declaration_database = create_declaration_database();

        C_header_groups const c_header_groups = get_c_headers(artifacts, builder.header_search_paths, temporaries_allocator, temporaries_allocator);
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const output_header_paths = create_output_header_paths(builder, artifacts, core_modules, temporaries_allocator, temporaries_allocator);

        Source_file_graph const graph = create_source_file_graph(c_header_groups.c_headers, core_modules, output_header_paths, temporaries_allocator);

        std::pmr::vector<iris::Module> header_modules{output_allocator};
        header_modules.resize(c_header_groups.c_headers.size());

        for (Source_file_graph_rank_range const rank : graph.ranks)
        {
            for (std::size_t index = 0; index < rank.count; ++index)
            {
                std::size_t const node_index = rank.start_index + index;
                Source_file_node const node = graph.nodes[node_index];

                if (node.type == Source_file_node_type::Module || node.type == Source_file_node_type::Export_c_header)
                {
                    iris::Module const& core_module = core_modules[node.index];
                    add_declarations(declaration_database, core_module);
                }
            }

            // TODO can be done in parallel
            for (std::size_t index = 0; index < rank.count; ++index)
            {
                std::size_t const node_index = rank.start_index + index;
                Source_file_node const node = graph.nodes[node_index];

                if (node.type == Source_file_node_type::Export_c_header)
                {
                    iris::Module const& core_module = core_modules[node.index];
                    
                    generate_c_header_file(
                        core_module,
                        output_header_paths,
                        declaration_database,
                        temporaries_allocator
                    );
                }
                else if (node.type == Source_file_node_type::Import_c_header)
                {
                    C_header const& c_header = c_header_groups.c_headers[node.index];
                    std::span<std::filesystem::path const> const header_search_paths = c_header_groups.header_search_paths[node.index];
                    Import_c_header_source_group const& source_group = *c_header_groups.source_groups[node.index];

                    std::optional<iris::Module> header_module = parse_c_header_and_cache(
                        builder.build_directory_path,
                        builder.output_module_json,
                        header_search_paths,
                        c_header,
                        source_group,
                        force_allow_errors
                    );
                    if (header_module.has_value())
                        header_modules[node.index] = std::move(header_module.value());
                }
            }

            for (std::size_t index = 0; index < rank.count; ++index)
            {
                std::size_t const node_index = rank.start_index + index;
                Source_file_node const node = graph.nodes[node_index];

                if (node.type == Source_file_node_type::Import_c_header)
                {
                    iris::Module const& header_module = header_modules[node.index];
                    add_declarations(declaration_database, header_module);
                }
            }
        }

        std::pmr::vector<iris::Module const*> sorted_modules{output_allocator};
        sorted_modules.resize(graph.nodes.size());

        for (std::size_t index = 0; index < graph.nodes.size(); ++index)
        {
            Source_file_node const& node = graph.nodes[index];
            if (node.type == Source_file_node_type::Module || node.type == Source_file_node_type::Export_c_header)
                sorted_modules[index] = &core_modules[node.index];
            else
                sorted_modules[index] = &header_modules[node.index];
        }

        for (iris::Module& core_module : core_modules)
            iris::compiler::add_import_usages(core_module, output_allocator);

        // TODO can be done in parallel but declaration_database.call_instances needs to be guarded...
        for (Module& core_module : core_modules)
        {
            process_module(core_module, declaration_database, {.validate=false}, temporaries_allocator);
        }

        end_timer(get_profiler(builder), "import_and_export_c_headers");

        return Modules_and_declaration_database {
            .header_modules = std::move(header_modules),
            .sorted_modules = std::move(sorted_modules),
            .declaration_database = std::move(declaration_database),
        };
    }

    void validate_modules_and_exit_if_needed(
        std::span<iris::Module> const core_modules,
        Declaration_database& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        for (Module& core_module : core_modules)
        {
            Analysis_result result = process_module(core_module, declaration_database, {.validate=true}, temporaries_allocator);

            if (!result.diagnostics.empty())
            {
                print_diagnostics_and_exit_if_needed(result.diagnostics, temporaries_allocator);
            }
        }
    }

    static bool is_compiled_cpp_up_to_date(std::filesystem::path const& output_dependency_file)
    {
        if (!std::filesystem::exists(output_dependency_file))
            return false;

        std::optional<std::pmr::string> const file_contents = iris::common::get_file_contents(output_dependency_file);
        if (!file_contents.has_value())
            return false;

        std::size_t start_index = 0;
        std::size_t current_index = 0;

        std::pmr::vector<std::string_view> files;

        std::pmr::string const& input = file_contents.value();
        while (current_index < input.size())
        {
            char const current_character = input[current_index];
            if (current_character == '\n')
            {
                std::string_view const view{ &input[start_index], current_index - start_index };
                
                std::size_t end_index = current_index - 1;
                while (end_index > 0)
                {
                    char const end_character = input[end_index];
                    if (std::isalpha(end_character))
                    {
                        files.push_back(std::string_view{&input[start_index], end_index + 1 - start_index});
                        break;
                    }

                    end_index -= 1;
                }

                start_index = current_index;
                while (start_index < input.size())
                {
                    char const start_character = input[start_index];
                    if (std::isalpha(start_character))
                        break;

                    start_index += 1;
                }
            }

            current_index += 1;
        }

        std::filesystem::file_time_type const output_time = std::filesystem::last_write_time(output_dependency_file);

        for (std::string_view const& file_path_string : files)
        {
            std::filesystem::path const file_path = file_path_string;
            if (!std::filesystem::exists(file_path))
                return false;

            std::filesystem::file_time_type const input_time = std::filesystem::last_write_time(file_path);
            if (input_time > output_time)
                return false;
        }

        return true;
    }

    bool compile_cpp_and_write_to_bitcode_files(
        Builder& builder,
        std::span<Artifact const> const artifacts,
        LLVM_data& llvm_data,
        Compilation_options const& compilation_options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        start_timer(get_profiler(builder), "compile_cpp_and_write_to_bitcode_files");

        bool const use_objects = builder.compilation_options.output_debug_code_view;
        std::string_view const extension = use_objects ? "obj" : "bc";
        std::filesystem::path const build_directory_path = get_hl_build_directory(builder.build_directory_path);

        bool const use_clang_cl = builder.target.operating_system == "windows";

        for (Artifact const& artifact : artifacts)
        {
            std::pmr::vector<std::filesystem::path> const public_include_directories = get_public_include_directories(artifact, artifacts, temporaries_allocator, temporaries_allocator);
            std::pmr::vector<std::pmr::string> const public_include_directories_strings = iris::common::convert_path_to_string(public_include_directories, temporaries_allocator);

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
                    std::filesystem::path const output_llvm_ir_file = build_directory_path / std::format("{}.{}.ll", artifact.name, source_file_path.stem().generic_string());
                    std::filesystem::path const output_dependency_file = build_directory_path / std::format("{}.{}.d", artifact.name, source_file_path.stem().generic_string());

                    if (is_compiled_cpp_up_to_date(output_dependency_file))
                        continue;

                    if (builder.output_llvm_ir)
                    {
                        std::filesystem::path const output_file_path = build_directory_path / std::format("{}.{}.{}", artifact.name, source_file_path.stem().generic_string(), "ll");
                        bool const success = compile_cpp(
                            *llvm_data.clang_data,
                            llvm_data.target_triple,
                            source_file_path,
                            output_llvm_ir_file,
                            std::nullopt,
                            build_directory_path,
                            public_include_directories_strings,
                            group.additional_flags,
                            use_clang_cl,
                            compilation_options.debug,
                            temporaries_allocator
                        );
                        if (!success)
                        {
                            end_timer(get_profiler(builder), "compile_cpp_and_write_to_bitcode_files");
                            return false;
                        } 
                    }

                    bool const success = compile_cpp(
                        *llvm_data.clang_data,
                        llvm_data.target_triple,
                        source_file_path,
                        output_assembly_file,
                        output_dependency_file,
                        build_directory_path,
                        public_include_directories_strings,
                        group.additional_flags,
                        use_clang_cl,
                        compilation_options.debug,
                        temporaries_allocator
                    );
                    if (!success)
                    {
                        end_timer(get_profiler(builder), "compile_cpp_and_write_to_bitcode_files");
                        return false;
                    }
                }
            }
        }

        if (builder.is_test_mode)
        {
            std::filesystem::path const tests_main_file_path = iris::common::get_tests_main_file_path();

            std::filesystem::path const output_assembly_file = build_directory_path / std::format("iris.tests_main.{}", extension);
            std::filesystem::path const output_dependency_file = build_directory_path / std::format("iris.tests_main.d");

            if (!is_compiled_cpp_up_to_date(output_dependency_file))
            {
                std::array<std::pmr::string, 1> const additional_flags
                {
                    "-std=c++20",
                };

                bool const success = compile_cpp(
                    *llvm_data.clang_data,
                    llvm_data.target_triple,
                    tests_main_file_path,
                    output_assembly_file,
                    output_dependency_file,
                    build_directory_path,
                    {},
                    additional_flags,
                    use_clang_cl,
                    compilation_options.debug,
                    temporaries_allocator
                );
                if (!success)
                {
                    end_timer(get_profiler(builder), "compile_cpp_and_write_to_bitcode_files");
                    return false;
                }
            }
        }

        end_timer(get_profiler(builder), "compile_cpp_and_write_to_bitcode_files");
        return true;
    }

    std::pmr::vector<iris::Module> parse_source_files_and_cache(
        Builder& builder,
        std::span<std::filesystem::path const> const source_files_paths,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        start_timer(get_profiler(builder), "parse_source_files_and_cache");

        std::pmr::vector<iris::Module> core_modules{output_allocator};
        core_modules.resize(source_files_paths.size(), iris::Module{});

        iris::parser::Parser parser = iris::parser::create_parser();

        for (std::size_t index = 0; index < source_files_paths.size(); ++index)
        {
            std::filesystem::path const& source_file_path = source_files_paths[index];

            std::optional<std::pmr::string> const module_name = iris::parser::read_module_name(source_file_path);
            if (!module_name.has_value())
                iris::common::print_message_and_exit(std::format("Could not read module name of source file {}.", source_file_path.generic_string()));

            std::filesystem::path const output_module_filename = std::format("{}.irisb", module_name.value());
            std::filesystem::path const output_module_path = get_hl_build_directory(builder.build_directory_path) / output_module_filename;

            if (std::filesystem::exists(output_module_path))
            {
                if (is_file_newer_than(output_module_path, source_file_path))
                {
                    std::optional<Module> core_module = iris::binary_serializer::read_module_from_file(output_module_path);
                    if (!core_module.has_value())
                        iris::common::print_message_and_exit(std::format("Failed to read cached module {}.", output_module_path.generic_string()));

                    core_modules[index] = std::move(core_module.value());

                    continue;
                }
            }

            std::optional<std::pmr::string> const source_content = iris::common::get_file_contents(source_file_path);
            if (!source_content.has_value())
                iris::common::print_message_and_exit(std::format("Could not read source file {}.", source_file_path.generic_string()));

            std::pmr::u8string const utf_8_source_content{reinterpret_cast<char8_t const*>(source_content->data()), source_content->size(), temporaries_allocator};
            iris::parser::Parse_tree parse_tree = iris::parser::parse(parser, std::move(utf_8_source_content));

            iris::parser::Parse_node const root = get_root_node(parse_tree);
    
            std::optional<iris::Module> core_module = iris::parser::parse_node_to_module(
                parse_tree,
                root,
                source_file_path,
                output_allocator,
                temporaries_allocator
            );
            if (!core_module.has_value())
                iris::common::print_message_and_exit(std::format("Could not parse source file {}.", source_file_path.generic_string()));

            iris::binary_serializer::write_module_to_file(output_module_path, core_module.value(), {});

            if (builder.output_module_json)
            {
                std::filesystem::path const output_module_json_filename = std::format("{}.irisb.json", module_name.value());
                std::filesystem::path const output_module_json_path = get_hl_build_directory(builder.build_directory_path) / output_module_json_filename;
                iris::json::write_module_to_file(output_module_json_path, core_module.value());
            }

            core_modules[index] = std::move(core_module.value());

            iris::parser::destroy_tree(std::move(parse_tree));
        }

        iris::parser::destroy_parser(std::move(parser));

        end_timer(get_profiler(builder), "parse_source_files_and_cache");

        return core_modules;
    }

    std::pmr::vector<iris::Module> create_test_artifact_modules(
        Builder& builder,
        std::span<std::filesystem::path const> const artifact_file_paths,
        std::span<Artifact const> const artifacts,
        std::span<iris::Module const> const core_modules,
        Compilation_options const& compilation_options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        start_timer(get_profiler(builder), "create_test_artifact_modules");

        bool const use_objects = builder.compilation_options.output_debug_code_view;
        std::string_view const extension = use_objects ? "obj" : "bc";

        std::pmr::vector<iris::Module> test_modules{temporaries_allocator};

        for (std::filesystem::path const& artifact_file_path : artifact_file_paths)
        {
            auto const artifact_location = std::find_if(artifacts.begin(), artifacts.end(), [&](Artifact const& artifact) -> bool { return artifact.file_path == artifact_file_path; });
            if (artifact_location == artifacts.end())
                continue;

            Artifact const& artifact = *artifact_location;

            std::pmr::vector<std::filesystem::path> const iris_source_files = get_artifact_iris_source_files(
                artifact,
                temporaries_allocator,
                temporaries_allocator
            );

            std::pmr::string const test_module_name = iris::compiler::get_test_module_name(artifact.name);
            std::filesystem::path const output_assembly_file = get_bitcode_build_directory(builder.build_directory_path) / std::format("{}.{}", test_module_name, extension);
            if (std::filesystem::exists(output_assembly_file))
            {
                bool is_up_to_date = true;

                for (std::filesystem::path const& source_file : iris_source_files)
                {
                    if (is_file_newer_than(source_file, output_assembly_file))
                    {
                        is_up_to_date = false;
                        break;
                    }
                }

                if (is_up_to_date)
                    continue;
            }

            std::pmr::vector<iris::Module const*> artifact_core_modules;
            artifact_core_modules.reserve(iris_source_files.size());

            for (std::filesystem::path const& source_file_path : iris_source_files)
            {
                std::optional<std::pmr::string> const module_name = iris::parser::read_module_name(source_file_path);
                if (!module_name.has_value())
                    iris::common::print_message_and_exit(std::format("Could not read module name of source file {}.", source_file_path.generic_string()));

                auto const location = std::find_if(core_modules.begin(), core_modules.end(), [&](iris::Module const& core_module) -> bool { return core_module.name == module_name.value(); });
                if (location != core_modules.end())
                    artifact_core_modules.push_back(&(*location));
            }

            std::optional<iris::Module> test_module = create_test_module(
                artifact.name,
                artifact_core_modules,
                temporaries_allocator
            );

            if (test_module.has_value())
                test_modules.push_back(std::move(test_module.value()));
        }

        end_timer(get_profiler(builder), "create_test_artifact_modules");

        return test_modules;
    }

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> create_module_name_to_file_path_map(
        Builder const& builder,
        std::span<iris::Module const> const header_modules,
        std::span<iris::Module const> const core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> map{temporaries_allocator};

        auto const insert_entries = [&](std::span<iris::Module const> const elements) -> void {
            for (iris::Module const& core_module : elements)
            {
                std::string_view const module_name = core_module.name;

                std::filesystem::path const module_filename = std::format("{}.irisb", module_name);
                std::filesystem::path const output_module_path = get_hl_build_directory(builder.build_directory_path) / module_filename;

                map.insert(std::make_pair(std::pmr::string{ module_name }, output_module_path));
            }
        };

        insert_entries(header_modules);
        insert_entries(core_modules);

        return std::pmr::unordered_map<std::pmr::string, std::filesystem::path>{std::move(map), output_allocator};
    }

    static void compile_and_write_to_bitcode_file(
        Builder& builder,
        iris::Module const& core_module,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& module_name_to_file_path_map,
        LLVM_data& llvm_data,
        Declaration_database const& declaration_database,
        Compilation_options const& compilation_options,
        bool const is_test_mode
    )
    {
        bool const use_objects = compilation_options.output_debug_code_view;
        std::string_view const extension = use_objects ? "obj" : "bc";
        std::string_view const test_extension = is_test_mode ? ".test" : "";

        std::filesystem::path const output_assembly_file = get_bitcode_build_directory(builder.build_directory_path) / std::format("{}{}.{}", core_module.name, test_extension, extension);
        std::filesystem::path const output_llvm_ir_file = get_bitcode_build_directory(builder.build_directory_path) / std::format("{}{}.{}", core_module.name, test_extension, "ll");

        if (std::filesystem::exists(output_assembly_file))
        {
            if (!builder.output_llvm_ir || std::filesystem::exists(output_llvm_ir_file))
            {
                auto const input_module_file_iterator = module_name_to_file_path_map.find(core_module.name);

                if (input_module_file_iterator != module_name_to_file_path_map.end() && std::filesystem::exists(input_module_file_iterator->second) && is_file_newer_than(output_assembly_file, input_module_file_iterator->second))
                {
                    return;
                }
            }
        }

        ::printf("Compiling '%s'%s\n", core_module.name.c_str(), is_test_mode ? " tests" : "");
        if (core_module.source_file_path.has_value())
            ::printf("    input is \"%s\"\n", core_module.source_file_path->generic_string().c_str());
        if (builder.output_llvm_ir)
            ::printf("    output llvm IR is \"%s\"\n", output_llvm_ir_file.generic_string().c_str());
        ::printf("    output is \"%s\"\n", output_assembly_file.generic_string().c_str());

        std::unique_ptr<llvm::Module> llvm_module = create_llvm_module(
            llvm_data,
            core_module,
            module_name_to_file_path_map,
            declaration_database,
            compilation_options
        );

        if (builder.output_llvm_ir)
            iris::compiler::write_llvm_ir_to_file(*llvm_module, output_llvm_ir_file);

        if (use_objects)
            iris::compiler::write_object_file(llvm_data, *llvm_module, output_assembly_file);
        else
            iris::compiler::write_bitcode_to_file(llvm_data, *llvm_module, output_assembly_file);
    }

    void compile_and_write_to_bitcode_files(
        Builder& builder,
        std::span<iris::Module const> const core_modules,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& module_name_to_file_path_map,
        LLVM_data& llvm_data,
        Declaration_database const& declaration_database,
        Compilation_options const& compilation_options,
        bool is_test_mode
    )
    {
        start_timer(get_profiler(builder), "compile_and_write_to_bitcode_files");

        // TODO to paralelize, llvm_data and compilation_database should be const

        for (std::size_t index = 0; index < core_modules.size(); ++index)
        {
            iris::Module const& core_module = core_modules[index];
            compile_and_write_to_bitcode_file(
                builder,
                core_module,
                module_name_to_file_path_map,
                llvm_data,
                declaration_database,
                compilation_options,
                is_test_mode
            );
        }

        end_timer(get_profiler(builder), "compile_and_write_to_bitcode_files");
    }

    std::pmr::vector<iris::compiler::Artifact const*> get_artifact_pointers(
        std::span<iris::compiler::Artifact const> const artifacts,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<iris::compiler::Artifact const*> output{output_allocator};
        output.reserve(artifacts.size());

        for (iris::compiler::Artifact const& artifact : artifacts)
            output.push_back(&artifact);

        return output;
    }

    std::pmr::vector<iris::compiler::Artifact const*> filter_test_artifacts(
        std::span<std::filesystem::path const> const artifact_file_paths,
        std::span<iris::compiler::Artifact const> const artifacts,
        std::span<iris::Module const> const core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<iris::compiler::Artifact const*> output{output_allocator};
        output.reserve(artifacts.size());

        for (std::filesystem::path const& artifact_file_path : artifact_file_paths)
        {
            auto const artifact_location = std::find_if(artifacts.begin(), artifacts.end(), [&](Artifact const& artifact) -> bool { return artifact.file_path == artifact_file_path; });
            if (artifact_location == artifacts.end())
                continue;

            std::pmr::string const test_module_name = iris::compiler::get_test_module_name(artifact_location->name);
            auto const test_location = std::find_if(core_modules.begin(), core_modules.end(), [&](iris::Module const& core_module) -> bool { return core_module.name == test_module_name; });
            if (test_location == core_modules.end())
                continue;

            output.push_back(&(*artifact_location));
        }

        return output;
    }

    static std::pmr::vector<std::filesystem::path> get_artifact_bitcode_files(
        Builder const& builder,
        Artifact const& artifact,
        bool const is_test_mode,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<std::filesystem::path> bitcode_files{temporaries_allocator};

        std::filesystem::path const build_directory_path = get_hl_build_directory(builder.build_directory_path);

        std::pmr::vector<std::filesystem::path> const iris_source_files = get_artifact_iris_source_files(
            artifact,
            temporaries_allocator,
            temporaries_allocator
        );

        bool const use_objects = builder.compilation_options.output_debug_code_view;
        std::string_view const extension = use_objects ? "obj" : "bc";
        std::string_view const test_extension = is_test_mode ? ".test" : "";

        for (std::filesystem::path const& source_file_path : iris_source_files)
        {
            std::optional<std::pmr::string> const module_name = iris::parser::read_module_name(source_file_path);
            if (!module_name.has_value())
                iris::common::print_message_and_exit(std::format("Could not read module name of source file {}.", source_file_path.generic_string()));

            std::filesystem::path bitcode_file = build_directory_path / std::format("{}{}.{}", module_name.value(), test_extension, extension);
            bitcode_files.push_back(std::move(bitcode_file));
        }
        
        std::pmr::vector<std::filesystem::path> const cpp_source_files = get_artifact_cpp_source_files(
            artifact,
            temporaries_allocator,
            temporaries_allocator
        );

        for (std::filesystem::path const& source_file_path : cpp_source_files)
        {
            std::filesystem::path bitcode_file = build_directory_path / std::format("{}.{}.{}", artifact.name, source_file_path.stem().generic_string(), extension);
            bitcode_files.push_back(std::move(bitcode_file));
        }

        if (is_test_mode)
        {
            std::filesystem::path main_test_bitcode_file = build_directory_path / std::format("iris.tests_main.{}", extension);
            bitcode_files.push_back(std::move(main_test_bitcode_file));

            std::pmr::string const test_module_name = iris::compiler::get_test_module_name(artifact.name);
            std::filesystem::path generated_test_bitcode_file = build_directory_path / std::format("{}.test.{}", test_module_name, extension);
            bitcode_files.push_back(std::move(generated_test_bitcode_file));
        }

        return bitcode_files;
    }

    struct Artifact_libraries
    {
        std::pmr::vector<std::pmr::string> libraries;
        std::pmr::vector<std::pmr::string> dll_names;
    };

    static void add_dependency_libraries(
        Artifact_libraries& artifact_libraries,
        Artifact const& artifact,
        std::span<Artifact const> const artifacts,
        iris::compiler::Target const& target,
        std::filesystem::path const& build_directory_path,
        bool const is_debug
    )
    {
        if (artifact.info.has_value())
        {
            if (std::holds_alternative<Library_info>(*artifact.info))
            {
                Library_info const& library_info = std::get<Library_info>(*artifact.info);
        
                std::optional<iris::compiler::External_library_info> const external_library = iris::compiler::get_external_library(library_info.external_libraries, target, is_debug, true);
                if (external_library.has_value())
                {
                    for (std::pmr::string const& name : external_library->names)
                        artifact_libraries.libraries.push_back(name);

                    std::pmr::vector<std::string_view> const dll_names = iris::compiler::get_external_library_dlls(library_info.external_libraries, external_library.value().key);
                    for (std::string_view const dll_name : dll_names)
                    {
                        artifact_libraries.dll_names.push_back(std::pmr::string{dll_name});
                    }
                }
            }
        }

        for (Dependency const& dependency : artifact.dependencies)
        {
            auto const location = std::find_if(
                artifacts.begin(),
                artifacts.end(),
                [&](Artifact const& artifact) -> bool { return artifact.name == dependency.artifact_name; }
            );
            if (location == artifacts.end())
                continue;

            if (contains_any_compilable_source(*location))
            {
                std::filesystem::path const output_path = build_directory_path / "lib" / location->name;
                artifact_libraries.libraries.push_back(std::pmr::string{output_path.generic_string()});
            }

            add_dependency_libraries(
                artifact_libraries,
                *location,
                artifacts,
                target,
                build_directory_path,
                is_debug
            );
        }
    }

    static Artifact_libraries get_artifact_libraries_for_linking(
        Artifact const& artifact,
        std::span<Artifact const> const artifacts,
        iris::compiler::Target const& target,
        std::filesystem::path const& build_directory_path,
        bool const is_debug,
        bool const is_test_mode,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        Artifact_libraries artifact_libraries
        {
            .libraries = std::pmr::vector<std::pmr::string>{temporaries_allocator},
            .dll_names = std::pmr::vector<std::pmr::string>{temporaries_allocator}
        };
        
        add_dependency_libraries(
            artifact_libraries,
            artifact,
            artifacts,
            target,
            build_directory_path,
            is_debug
        );

        if (is_test_mode)
        {
            auto const artifact_location = std::find_if(artifacts.begin(), artifacts.end(), [](Artifact const& artifact) -> bool { return artifact.name == "Cpp_standard_library"; });
            if (artifact_location == artifacts.end())
                iris::common::print_message_and_exit("Could not find artifact 'Cpp_standard_library'");

            add_dependency_libraries(
                artifact_libraries,
                *artifact_location,
                artifacts,
                target,
                build_directory_path,
                is_debug
            );
        }

        return
        {
            .libraries = std::pmr::vector<std::pmr::string>{std::move(artifact_libraries.libraries), output_allocator},
            .dll_names = std::pmr::vector<std::pmr::string>{std::move(artifact_libraries.dll_names), output_allocator}
        };
    }

    void link_artifacts(
        Builder& builder,
        std::span<Artifact const> const artifacts,
        std::span<Artifact const* const> const artifacts_to_link,
        bool const debug,
        bool const is_test_mode,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        start_timer(get_profiler(builder), "link_artifacts");

        for (std::size_t index = 0; index < artifacts_to_link.size(); ++index)
        {
            Artifact const& artifact = *artifacts_to_link[index];

            std::pmr::vector<std::filesystem::path> const bitcode_files = get_artifact_bitcode_files(
                builder,
                artifact,
                is_test_mode,
                temporaries_allocator
            );
            if (bitcode_files.empty())
                continue;

            Artifact_libraries const artifact_libraries = get_artifact_libraries_for_linking(
                artifact,
                artifacts,
                builder.target,
                builder.build_directory_path,
                debug,
                is_test_mode,
                temporaries_allocator,
                temporaries_allocator
            );

            if (is_test_mode)
            {
                iris::compiler::Linker_options const linker_options
                {
                    .entry_point = "main",
                    .debug = debug,
                    .link_type = iris::compiler::Link_type::Executable
                };

                std::filesystem::path const output = builder.build_directory_path / "bin" / (artifact.name + ".iris.test");
                create_directory_if_it_does_not_exist(output.parent_path());

                bool const result = iris::compiler::link(
                    bitcode_files,
                    artifact_libraries.libraries,
                    output,
                    linker_options
                );
                if (!result)
                    iris::common::print_message_and_exit(std::format("Failed to link executable '{}.test'.", artifact.name));
            }
            else if (artifact.type == Artifact_type::Library)
            {
                iris::compiler::Linker_options const linker_options
                {
                    .entry_point = std::nullopt,
                    .debug = debug,
                    .link_type = iris::compiler::Link_type::Static_library
                };

                std::filesystem::path const output = builder.build_directory_path / "lib" / artifact.name;
                create_directory_if_it_does_not_exist(output.parent_path());

                bool const result = iris::compiler::create_static_library(
                    bitcode_files,
                    artifact_libraries.libraries,
                    output,
                    linker_options
                );
                if (!result)
                    iris::common::print_message_and_exit(std::format("Failed to link static library '{}'.", artifact.name));
            }
            else if (artifact.info.has_value() && std::holds_alternative<iris::compiler::Executable_info>(*artifact.info))
            {
                iris::compiler::Executable_info const& executable_info = std::get<iris::compiler::Executable_info>(*artifact.info);

                iris::compiler::Linker_options const linker_options
                {
                    .entry_point = executable_info.entry_point,
                    .debug = debug,
                    .link_type = iris::compiler::Link_type::Executable
                };

                std::filesystem::path const output = builder.build_directory_path / "bin" / artifact.name;
                create_directory_if_it_does_not_exist(output.parent_path());

                bool const result = iris::compiler::link(
                    bitcode_files,
                    artifact_libraries.libraries,
                    output,
                    linker_options
                );
                if (!result)
                    iris::common::print_message_and_exit(std::format("Failed to link executable '{}'.", artifact.name));
            }
        }

        end_timer(get_profiler(builder), "link_artifacts");
    }

    void copy_dll(
        std::string_view const dll_name,
        std::filesystem::path const& output_directory
    )
    {
        std::filesystem::path const dll_path = dll_name;
        if (!std::filesystem::exists(dll_path))
        {
            std::fprintf(stderr, "Copy dll: could not find dll '%s'.\n", dll_name.data());
            return;
        }

        std::filesystem::path const destination_path = output_directory / dll_path.filename();

        if (std::filesystem::exists(destination_path) && is_file_newer_than(destination_path, dll_path))
            return;

        std::string const source_string = dll_path.generic_string();
        std::string const destination_string = destination_path.generic_string();

        std::filesystem::copy_options const copy_options = std::filesystem::copy_options::update_existing;
        bool const success = std::filesystem::copy_file(dll_path, destination_path, copy_options);
        if (success)
            std::printf("Copy dll: copied '%s' to '%s'.\n", source_string.c_str(), destination_string.c_str());
    }

    void copy_dlls(
        Builder const& builder,
        std::span<Artifact const> const artifacts,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::filesystem::path const output_directory = builder.build_directory_path / "bin";

        for (std::size_t artifact_index = 0; artifact_index < artifacts.size(); ++artifact_index)
        {
            Artifact const& artifact = artifacts[artifact_index];

            if (artifact.info.has_value() && std::holds_alternative<Library_info>(artifact.info.value()))
            {
                Library_info const& library_info = std::get<Library_info>(artifact.info.value());

                bool const prefer_debug = builder.compilation_options.debug;
                bool const prefer_dynamic = true;

                std::optional<External_library_info> const external_library_info = get_external_library(
                    library_info.external_libraries,
                    builder.target,
                    prefer_debug,
                    prefer_dynamic
                );

                if (external_library_info.has_value())
                {
                    std::string_view const key = external_library_info.value().key;

                    std::pmr::vector<std::string_view> const external_library_dlls = get_external_library_dlls(
                        library_info.external_libraries,
                        key
                    );

                    for (std::string_view const dll_name : external_library_dlls)
                    {
                        copy_dll(dll_name, output_directory);
                    }
                }
            }
        }
    }

    bool is_file_newer_than(
        std::filesystem::path const& first,
        std::filesystem::path const& second
    )
    {
        std::filesystem::file_time_type first_time = std::filesystem::last_write_time(first);
        std::filesystem::file_time_type second_time = std::filesystem::last_write_time(second);
        return first_time > second_time;
    }

    void print_struct_layout(
        std::filesystem::path const input_file_path,
        std::string_view const struct_name,
        std::optional<std::string_view> const target_triple
    )
    {
        std::optional<iris::Module> core_module = iris::compiler::read_core_module(input_file_path);
        if (!core_module.has_value())
            iris::common::print_message_and_exit(std::format("Failed to read module of '{}'", input_file_path.generic_string()));

        iris::compiler::Compilation_options const options
        {
            .target_triple = target_triple,
            .is_optimized = false,
            .debug = true,
        };
        iris::compiler::LLVM_data llvm_data = iris::compiler::initialize_llvm(options);

        iris::Declaration_database declaration_database = iris::create_declaration_database();
        iris::add_declarations(declaration_database, *core_module);

        std::pmr::vector<iris::Module const*> core_modules{ &core_module.value() };
        iris::compiler::Clang_module_data_pointer clang_module_data = iris::compiler::create_clang_module_data(
            *llvm_data.context,
            *llvm_data.clang_data,
            "Iris_clang_module",
            core_modules,
            declaration_database
        );

        iris::compiler::Type_database type_database = iris::compiler::create_type_database(*llvm_data.context);
        iris::compiler::add_module_types(type_database, *llvm_data.context, llvm_data.data_layout, *clang_module_data, *core_module);

        std::optional<iris::Struct_layout> const struct_layout = iris::compiler::calculate_struct_layout(llvm_data.data_layout, type_database, core_module->name, struct_name);
        if (!struct_layout.has_value())
        {
            std::puts("<error>");
            return;
        }

        std::stringstream string_stream;
        string_stream << struct_layout.value();
        std::string const output = string_stream.str();
        std::puts(output.c_str());
    }

    void write_compile_commands_json_to_file(
        Builder const& builder,
        std::filesystem::path const& artifact_file_path,
        iris::compiler::Compilation_options const& compilation_options,
        std::filesystem::path const output_file_path
    )
    {
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        std::filesystem::path const build_directory_path = get_hl_build_directory(builder.build_directory_path);
        bool const use_clang_cl = builder.target.operating_system == "windows";
        bool const use_objects = builder.compilation_options.output_debug_code_view;

        std::pmr::vector<Artifact> const artifacts = get_sorted_artifacts(
            { &artifact_file_path, 1 },
            builder.repositories,
            false,
            temporaries_allocator,
            temporaries_allocator
        );

        std::pmr::vector<Compile_command> const commands = create_compile_commands(
            artifacts,
            build_directory_path,
            use_clang_cl,
            use_objects,
            temporaries_allocator,
            temporaries_allocator
        );

        write_compile_commands_to_file(commands, output_file_path);
    }

    std::pmr::vector<std::filesystem::path> find_artifact_file_paths(
        std::filesystem::path const& path,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        return iris::common::search_files(
            path,
            "iris_artifact.json",
            temporaries_allocator,
            output_allocator
        );
    }
}
