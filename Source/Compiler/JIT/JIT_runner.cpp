module h.compiler.jit_runner;

import std;
import llvm;

import h.common;
import h.common.filesystem;
import h.compiler;
import h.compiler.artifact;
import h.compiler.common;
import h.compiler.core_module_layer;
import h.compiler.file_watcher;
import h.core.hash;
import h.compiler.jit_compiler;
import h.compiler.recompilation;
import h.compiler.repository;
import h.compiler.target;
import h.core;
import h.c_header_converter;
import h.json_serializer;
import h.parser.convertor;
import h.parser.parser;

namespace h::compiler
{
    JIT_runner::~JIT_runner()
    {
        this->file_watcher.reset();
        this->protected_data.symbol_to_module_name_map.clear();
        this->unprotected_data.jit_data.reset();
        this->unprotected_data.llvm_data.reset();
    }

    static std::optional<std::pmr::string> read_module_name(std::filesystem::path const& unparsed_file_path)
    {
        std::string const path_string = unparsed_file_path.generic_string();
        std::FILE* file_stream = std::fopen(path_string.c_str(), "r");
        if (file_stream == nullptr)
            return std::nullopt;

        std::optional<std::pmr::string> module_name = std::nullopt;

        constexpr int line_size = 1000;
        char line[line_size];
        while (true)
        {
            if (std::fgets(line, line_size, file_stream) == nullptr)
                break;

            char const* const end = std::find(line, line + line_size, '\0');

            std::string_view const line_view{ line, end };

            std::string_view::size_type const line_without_spaces_begin = line_view.find_first_not_of(' ');
            if (line_without_spaces_begin == std::string_view::npos)
                continue;

            std::string_view const line_without_spaces{ line + line_without_spaces_begin, end - 1 };

            if (line_without_spaces.starts_with("module ") && line_without_spaces.ends_with(';'))
            {
                module_name = line_without_spaces.substr(7, line_without_spaces.size() - 8);
                break;
            }
        }

        std::fclose(file_stream);

        return module_name;
    }

    std::optional<std::filesystem::path> get_module_source_file_path(
        std::string_view const module_name,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& module_name_to_source_file_path
    )
    {
        auto location = module_name_to_source_file_path.find(module_name.data());
        if (location != module_name_to_source_file_path.end())
            return location->second;
        else
            return std::nullopt;
    }

    std::optional<std::filesystem::path> find_module_source_file_path(
        std::string_view const module_name,
        JIT_runner_unprotected_data const& unprotected_data,
        JIT_runner_protected_data& protected_data
    )
    {
        std::shared_lock<std::shared_mutex> lock{ protected_data.mutex };

        std::optional<std::filesystem::path> module_source_file_path = get_module_source_file_path(module_name, protected_data.module_name_to_source_file_path);
        if (module_source_file_path)
            return module_source_file_path;

        auto const predicate = [module_name, &module_source_file_path](std::filesystem::path const& file_path) -> bool
        {
            std::optional<std::pmr::string> const current_module_name = read_module_name(file_path);
            if (!current_module_name)
                return false;

            if (*current_module_name == module_name)
            {
                module_source_file_path = file_path;
                return true;
            }

            return false;
        };

        for (auto const& pair : protected_data.artifacts)
        {
            bool const found = visit_included_files(pair.second, predicate);
            if (found)
                return module_source_file_path;

            std::pmr::vector<C_header> const c_headers = get_c_headers(pair.second, {});
            for (C_header const& c_header : c_headers)
            {
                if (c_header.module_name == module_name)
                    return find_c_header_path(c_header.header, unprotected_data.header_search_paths);
            }
        }

        for (auto const& repository_pair : protected_data.repositories)
        {
            Repository const& repository = repository_pair.second;

            for (auto const& artifact_pair : repository.artifact_to_location)
            {
                Artifact const artifact = get_artifact(artifact_pair.second);
                bool const found = visit_included_files(artifact, predicate);
                if (found)
                    return module_source_file_path;

                std::pmr::vector<C_header> const c_headers = get_c_headers(artifact, {});
                for (C_header const& c_header : c_headers)
                {
                    if (c_header.module_name == module_name)
                        return find_c_header_path(c_header.header, unprotected_data.header_search_paths);
                }
            }
        }

        return std::nullopt;
    }

    std::optional<Artifact> find_module_artifact(
        std::pmr::string const& module_name,
        JIT_runner_protected_data& protected_data
    )
    {
        std::shared_lock<std::shared_mutex> lock{ protected_data.mutex };

        auto const artifact_file_path_location = protected_data.module_name_to_artifact_path.find(module_name);
        if (artifact_file_path_location == protected_data.module_name_to_artifact_path.end())
            return std::nullopt;

        auto const artifact_location = protected_data.artifacts.find(artifact_file_path_location->second);
        if (artifact_location == protected_data.artifacts.end())
            return std::nullopt;

        return artifact_location->second;
    }

    struct Parsed_module_info
    {
        std::filesystem::path parsed_file_path;
        bool is_c_header;
    };

    h::c::Options create_c_header_options_from_artifact(
        std::string_view const module_name,
        std::optional<Artifact> const& artifact
    )
    {
        if (!artifact.has_value())
            return {};

        for (Source_group const& group : artifact->sources)
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
                    return
                    {
                        .target_triple = std::nullopt,
                        .include_directories = c_header_group.search_paths,
                        .public_prefixes = c_header_group.public_prefixes,
                        .remove_prefixes = c_header_group.remove_prefixes,
                    };
                }
            }
        }

        return
        {
            .target_triple = std::nullopt,
            .include_directories = {},
            .public_prefixes = {},
            .remove_prefixes = {},
        };
    }

    std::optional<Parsed_module_info> find_module_and_parse(
        std::string_view const module_name,
        JIT_runner_unprotected_data const& unprotected_data,
        JIT_runner_protected_data& protected_data
    )
    {
        std::optional<std::filesystem::path> const module_source_file_path = find_module_source_file_path(
            module_name,
            unprotected_data,
            protected_data
        );
        if (!module_source_file_path)
            return std::nullopt;

        {
            std::unique_lock<std::shared_mutex> lock{ protected_data.mutex };
            protected_data.module_name_to_source_file_path.insert(std::make_pair(std::pmr::string{ module_name }, *module_source_file_path));
        }

        std::filesystem::path const parsed_file_path = unprotected_data.build_directory_path / module_source_file_path->filename().replace_extension("hlb");

        if (module_source_file_path->extension() == ".iris")
        {
            std::optional<h::Module> const core_module = h::parser::parse_and_convert_to_module(
                *module_source_file_path,
                {},
                {}
            );
            if (!core_module.has_value())
                return std::nullopt;
            
            h::json::write_module_to_file(parsed_file_path, core_module.value());

            return Parsed_module_info
            {
                .parsed_file_path = parsed_file_path,
                .is_c_header = false
            };
        }
        else if (module_source_file_path->extension() == ".h")
        {
            std::optional<Artifact> const artifact = find_module_artifact(
                std::pmr::string{ module_name },
                protected_data
            );

            h::c::Options const options = create_c_header_options_from_artifact(module_name, artifact);
            std::optional<h::Module> const header_module = h::c::import_header_and_write_to_file(module_name, *module_source_file_path, parsed_file_path, options);
            if (!header_module.has_value())
                return std::nullopt;

            return Parsed_module_info
            {
                .parsed_file_path = parsed_file_path,
                .is_c_header = true
            };
        }

        return std::nullopt;
    }

    bool find_and_parse_core_module_dependencies(
        h::Module const& core_module,
        JIT_runner_unprotected_data const& unprotected_data,
        JIT_runner_protected_data& protected_data,
        std::pmr::unordered_map<std::pmr::string, h::Module>& core_module_dependecies
    )
    {
        for (Import_module_with_alias const& import_alias : core_module.dependencies.alias_imports)
        {
            if (core_module_dependecies.contains(import_alias.module_name))
                continue;

            std::optional<Parsed_module_info> const parsed_module_info = find_module_and_parse(
                import_alias.module_name,
                unprotected_data,
                protected_data
            );

            if (!parsed_module_info)
                return false;

            {
                std::unique_lock<std::shared_mutex> lock{ protected_data.mutex };
                protected_data.module_name_to_module_file_path.insert(std::make_pair(import_alias.module_name, parsed_module_info->parsed_file_path));
            }

            std::filesystem::path const& module_file_path = parsed_module_info->parsed_file_path;

            std::optional<h::Module> import_core_module = h::compiler::read_core_module_declarations(module_file_path);
            if (!import_core_module.has_value())
            {
                ::printf("Failed to read contents of %s (invalid module)\n", module_file_path.generic_string().c_str());
                return false;
            }

            core_module_dependecies.insert(std::make_pair(import_alias.module_name, std::move(*import_core_module)));

            bool const success = find_and_parse_core_module_dependencies(
                core_module_dependecies.at(import_alias.module_name),
                unprotected_data,
                protected_data,
                core_module_dependecies
            );
            if (!success)
                return false;
        }

        return true;
    }

    std::optional<std::pmr::unordered_map<std::pmr::string, h::Module>> find_and_parse_core_module_dependencies(
        h::Module const& core_module,
        JIT_runner_unprotected_data const& unprotected_data,
        JIT_runner_protected_data& protected_data
    )
    {
        std::pmr::unordered_map<std::pmr::string, h::Module> module_dependecies;
        module_dependecies.reserve(core_module.dependencies.alias_imports.size());

        bool const success = find_and_parse_core_module_dependencies(
            core_module,
            unprotected_data,
            protected_data,
            module_dependecies
        );
        if (!success)
            return std::nullopt;

        return module_dependecies;
    }

    inline void insert_symbol_to_module_name_entries(
        h::Module const& core_module,
        llvm::orc::MangleAndInterner& mangle,
        llvm::DenseMap<llvm::orc::SymbolStringPtr, std::pmr::string>& symbol_to_module_name_map
    )
    {
        for (h::Function_declaration const& declaration : core_module.export_declarations.function_declarations)
        {
            std::string const mangled_name = mangle_name(core_module, declaration.name, declaration.unique_name);
            llvm::orc::SymbolStringPtr const symbol = mangle(mangled_name);

            symbol_to_module_name_map.insert(std::make_pair(symbol, core_module.name));
        }
    }

    void insert_symbol_to_module_name_entries(
        std::pmr::unordered_map<std::pmr::string, h::Module> const& core_modules,
        llvm::orc::MangleAndInterner& mangle,
        JIT_runner_protected_data& protected_data
    )
    {
        std::unique_lock<std::shared_mutex> lock{ protected_data.mutex };

        for (std::pair<std::pmr::string const, h::Module> const& core_module : core_modules)
        {
            insert_symbol_to_module_name_entries(core_module.second, mangle, protected_data.symbol_to_module_name_map);
        }
    }

    bool add_module_for_compilation(
        std::filesystem::path const& module_file_path,
        llvm::orc::JITDylib& library,
        JIT_runner_unprotected_data const& unprotected_data,
        JIT_runner_protected_data& protected_data,
        bool const recompile_reverse_dependencies
    )
    {
        std::optional<h::Module> const core_module = h::compiler::read_core_module(module_file_path);
        if (!core_module.has_value())
        {
            ::printf("Failed to read contents of module %s\n", module_file_path.generic_string().c_str());
            return false;
        }

        {
            std::unique_lock<std::shared_mutex> lock{ protected_data.mutex };
            insert_symbol_to_module_name_entries(*core_module, *unprotected_data.jit_data->mangle, protected_data.symbol_to_module_name_map);
        }

        std::optional<std::pmr::unordered_map<std::pmr::string, h::Module>> core_module_dependencies =
            find_and_parse_core_module_dependencies(*core_module, unprotected_data, protected_data);
        if (!core_module_dependencies.has_value())
        {
            ::printf("Failed to read module dependencies of module %s\n", module_file_path.generic_string().c_str());
            return false;
        }

        insert_symbol_to_module_name_entries(*core_module_dependencies, *unprotected_data.jit_data->mangle, protected_data);

        {
            std::unique_lock<std::shared_mutex> lock{ protected_data.mutex };

            // TODO remove all entries where pair.second == core_module->name

            for (std::pair<std::pmr::string const, h::Module> const& core_module_dependency : *core_module_dependencies)
            {
                protected_data.module_name_to_reverse_dependencies.insert(std::make_pair(core_module_dependency.first, core_module->name));
            }
        }

        if (recompile_reverse_dependencies)
        {
            Symbol_name_to_hash new_symbol_name_to_hash_map = hash_module_declarations(*core_module, {});

            {
                std::unique_lock<std::shared_mutex> lock{ protected_data.mutex };
                auto const previous_symbol_name_to_hash_map_location = protected_data.module_name_to_symbol_hashes.find(core_module->name);

                std::pmr::vector<std::pmr::string> const modules_to_recompile = find_modules_to_recompile(
                    *core_module,
                    previous_symbol_name_to_hash_map_location != protected_data.module_name_to_symbol_hashes.end() ? previous_symbol_name_to_hash_map_location->second : Symbol_name_to_hash{},
                    new_symbol_name_to_hash_map,
                    protected_data.module_name_to_module_file_path,
                    protected_data.module_name_to_reverse_dependencies,
                    {},
                    {}
                );

                for (std::pmr::string const& module_to_recompile_name : modules_to_recompile)
                {
                    std::filesystem::path const& module_file_path = protected_data.module_name_to_module_file_path.at(module_to_recompile_name);

                    lock.unlock();
                    bool const success = add_module_for_compilation(
                        module_file_path,
                        library,
                        unprotected_data,
                        protected_data,
                        false
                    );

                    if (!success)
                        return false;

                    lock.lock();
                }

                protected_data.module_name_to_symbol_hashes.insert(std::make_pair(core_module->name, std::move(new_symbol_name_to_hash_map)));
            }
        }

        Core_module_compilation_data core_compilation_data
        {
            .llvm_data = *unprotected_data.llvm_data,
            .core_module = std::move(*core_module),
            .core_module_dependencies = std::move(*core_module_dependencies),
            .compilation_options = unprotected_data.compilation_options,
        };
        return add_core_module(*unprotected_data.jit_data, library, std::move(core_compilation_data));
    }

    static std::pmr::unordered_map<std::filesystem::path, Artifact>::const_iterator find_artifact(
        std::pmr::unordered_map<std::filesystem::path, Artifact> const& artifacts,
        std::string_view const artifact_name
    )
    {
        for (auto iterator = artifacts.begin(); iterator != artifacts.end(); ++iterator)
        {
            Artifact const& artifact = iterator->second;
            if (artifact.name == artifact_name)
                return iterator;
        }

        return artifacts.end();
    }

    static std::optional<std::filesystem::path> find_artifact_file_path(
        std::pmr::unordered_map<std::filesystem::path, Repository> const& repositories,
        std::string_view const artifact_name
    )
    {
        for (auto const& repository_pair : repositories)
        {
            Repository const& repository = repository_pair.second;

            auto const location = repository.artifact_to_location.find(artifact_name.data());
            if (location != repository.artifact_to_location.end())
                return location->second;
        }

        return std::nullopt;
    }

    static void add_artifact_for_compilation(
        std::filesystem::path const& artifact_configuration_file_path,
        JIT_runner_unprotected_data const& unprotected_data,
        JIT_runner_protected_data& protected_data,
        File_watcher& file_watcher
    )
    {
        // TODO if artifact is updated on the fly, then we need to review the mutex protection here

        {
            Artifact artifact = get_artifact(artifact_configuration_file_path);

            std::unique_lock<std::shared_mutex> lock{ protected_data.mutex };
            protected_data.artifacts.insert(std::make_pair(artifact_configuration_file_path, std::move(artifact)));
        }

        Artifact const& artifact = protected_data.artifacts.at(artifact_configuration_file_path);

        std::pmr::vector<std::filesystem::path> dependencies_artifacts_configuration_file_paths;
        {
            std::shared_lock<std::shared_mutex> lock{ protected_data.mutex };

            for (Dependency const& dependency : artifact.dependencies)
            {
                auto const iterator = find_artifact(protected_data.artifacts, dependency.artifact_name);
                if (iterator == protected_data.artifacts.end())
                {
                    std::optional<std::filesystem::path> const dependency_file_path = find_artifact_file_path(protected_data.repositories, dependency.artifact_name);
                    if (dependency_file_path)
                        dependencies_artifacts_configuration_file_paths.push_back(std::move(*dependency_file_path));
                }
            }
        }

        for (std::filesystem::path const& dependency_file_path : dependencies_artifacts_configuration_file_paths)
        {
            add_artifact_for_compilation(dependency_file_path, unprotected_data, protected_data, file_watcher);
        }
        
        {
            std::pmr::vector<C_header> const c_headers = get_c_headers(artifact, {});
            std::unique_lock<std::shared_mutex> lock{ protected_data.mutex };
            for (C_header const& c_header : c_headers)
            {
                protected_data.module_name_to_artifact_path.insert(std::make_pair(c_header.module_name, artifact_configuration_file_path));
            }
        }

        if (artifact.info.has_value())
        {
            if (std::holds_alternative<Executable_info>(*artifact.info))
            {
                Executable_info const& executable_info = std::get<Executable_info>(*artifact.info);

                std::filesystem::path const source_file_path = artifact.file_path.parent_path() / executable_info.source;

                std::filesystem::path const parsed_file_path = unprotected_data.build_directory_path / source_file_path.filename().replace_extension("hlb");

                std::optional<h::Module> const core_module = h::parser::parse_and_convert_to_module(
                    source_file_path,
                    {},
                    {}
                );
                if (core_module.has_value())
                    h::json::write_module_to_file(parsed_file_path, core_module.value());

                {
                    std::optional<std::pmr::string> const module_name = read_module_name(source_file_path);

                    if (module_name)
                    {
                        std::unique_lock<std::shared_mutex> lock{ protected_data.mutex };
                        protected_data.module_name_to_source_file_path.insert(std::make_pair(*module_name, source_file_path));
                        protected_data.module_name_to_module_file_path.insert(std::make_pair(*module_name, parsed_file_path));
                        protected_data.module_name_to_artifact_path.insert(std::make_pair(*module_name, artifact_configuration_file_path));
                    }
                }

                add_module_for_compilation(
                    parsed_file_path,
                    get_main_library(*unprotected_data.jit_data),
                    unprotected_data,
                    protected_data,
                    false
                );
            }
            else if (std::holds_alternative<Library_info>(*artifact.info))
            {
                Library_info const& library_info = std::get<Library_info>(*artifact.info);

                std::optional<External_library_info> const external_library = get_external_library(library_info.external_libraries, unprotected_data.target, unprotected_data.compilation_options.debug, true);
                if (external_library.has_value())
                {
                    for (std::pmr::string const& name : external_library->names)
                        link_static_library(*unprotected_data.jit_data, name.c_str());

                    std::pmr::vector<std::string_view> const external_library_dlls = get_external_library_dlls(library_info.external_libraries, external_library->key);
                    for (std::string_view const& dll_name : external_library_dlls)
                    {
                        load_platform_dynamic_library(*unprotected_data.jit_data, dll_name.data());
                    }
                }
            }
        }

        watch_artifact_directories(file_watcher, artifact);
    }

    std::optional<std::string_view> get_module_name(
        llvm::orc::SymbolStringPtr const symbol,
        JIT_runner_protected_data& protected_data
    )
    {
        std::shared_lock<std::shared_mutex> lock{ protected_data.mutex };

        auto const module_name_location = protected_data.symbol_to_module_name_map.find(symbol);
        if (module_name_location == protected_data.symbol_to_module_name_map.end())
            return std::nullopt;

        std::string_view const module_name = module_name_location->second;
        return module_name;
    }

    class H_definition_generator : public llvm::orc::DefinitionGenerator
    {
    public:

        H_definition_generator(
            JIT_runner_unprotected_data const& unprotected_data,
            JIT_runner_protected_data& protected_data
        ) :
            m_unprotected_data{ unprotected_data },
            m_protected_data{ protected_data }
        {
        }

        virtual ~H_definition_generator()
        {
        }

        /// DefinitionGenerators should override this method to insert new
        /// definitions into the parent JITDylib. K specifies the kind of this
        /// lookup. JD specifies the target JITDylib being searched, and
        /// JDLookupFlags specifies whether the search should match against
        /// hidden symbols. Finally, Symbols describes the set of unresolved
        /// symbols and their associated lookup flags.
        virtual llvm::Error tryToGenerate(
            llvm::orc::LookupState& lookup_state,
            llvm::orc::LookupKind lookup_kind,
            llvm::orc::JITDylib& library,
            llvm::orc::JITDylibLookupFlags lookup_flags,
            llvm::orc::SymbolLookupSet const& symbol_lookup_set
        ) final
        {
            for (std::pair<llvm::orc::SymbolStringPtr, llvm::orc::SymbolLookupFlags> const& symbol_lookup : symbol_lookup_set)
            {
                llvm::orc::SymbolStringPtr const symbol = symbol_lookup.first;

                std::optional<std::string_view> const module_name = get_module_name(symbol, m_protected_data);
                if (!module_name)
                    continue;

                std::optional<Parsed_module_info> const parsed_module_info = find_module_and_parse(*module_name, m_unprotected_data, m_protected_data);
                if (!parsed_module_info)
                    continue;

                {
                    std::unique_lock<std::shared_mutex> lock{ m_protected_data.mutex };
                    m_protected_data.module_name_to_module_file_path.insert(std::make_pair(std::pmr::string{ *module_name }, parsed_module_info->parsed_file_path));
                }

                if (parsed_module_info->is_c_header)
                    continue;

                add_module_for_compilation(
                    parsed_module_info->parsed_file_path,
                    library,
                    m_unprotected_data,
                    m_protected_data,
                    false
                );

                // TODO
                // Cache module paths:
                /*{
                    std::unique_lock<std::shared_mutex> lock{ m_module_name_to_source_file_path.mutex };

                    m_module_name_to_source_file_path.map.emplace(std::pmr::string{ module_name }, *module_file_path);

                    for (std::size_t index = 0; index < module_dependecies_names.size(); ++index)
                    {
                        std::string_view const dependency_module_name = module_dependecies_names[index];
                        std::filesystem::path const& dependency_file_path = module_dependencies_file_paths.value()[index];

                        m_module_name_to_source_file_path.map.insert(std::make_pair(std::pmr::string{ module_name }, dependency_file_path));
                    }
                }*/
            }

            return llvm::Error::success();
        }

    private:
        JIT_runner_unprotected_data const& m_unprotected_data;
        JIT_runner_protected_data& m_protected_data;
    };

    bool is_inside_directory(
        std::filesystem::path const& path,
        std::filesystem::path const& directory_path
    )
    {
        auto const [directory_path_end, nothing] = std::mismatch(directory_path.begin(), directory_path.end(), path.begin());
        return directory_path_end == directory_path.end();
    }

    static void handle_file_change(
        JIT_runner_unprotected_data const& unprotected_data,
        JIT_runner_protected_data& protected_data,
        wtr::event const event
    )
    {
        constexpr bool print_events = true;
        if (print_events)
        {
            /*std::pmr::string const effect_type = wtr::to<std::pmr::string>(event.effect_type);
            std::pmr::string const path_type = wtr::to<std::pmr::string>(event.path_type);
            std::pmr::string const path_name = wtr::to<std::pmr::string>(event.path_name);
            std::pmr::string const associated = (event.associated ? " -> " + wtr::to<std::pmr::string>(event.associated->path_name) : "");

            std::string const output_string = std::format("{} {} {} {}\n", effect_type, path_type, path_name, associated);
            std::puts(output_string.c_str());*/
        }

        std::filesystem::path const& source_file_path = event.path_name;

        // Ignore changes in the build directory:
        if (is_inside_directory(source_file_path, unprotected_data.build_directory_path))
            return;

        // Any time there is a watched file is modified, add to the recompile module layer:
        if (event.effect_type == wtr::event::effect_type::create || event.effect_type == wtr::event::effect_type::modify)
        {
            if (source_file_path.extension() == ".iris")
            {
                using namespace std::chrono_literals;

                if (unprotected_data.log_level >= 1)
                    std::puts(std::format("Detected change on {}", source_file_path.generic_string()).c_str());

                std::chrono::high_resolution_clock::time_point const begin_parsing = std::chrono::high_resolution_clock::now();

                std::filesystem::path const parsed_file_path = unprotected_data.build_directory_path / source_file_path.filename().replace_extension("hlb");
                std::optional<h::Module> const core_module = h::parser::parse_and_convert_to_module(
                    source_file_path,
                    {},
                    {}
                );
                if (core_module.has_value())
                    h::json::write_module_to_file(parsed_file_path, core_module.value());

                {
                    std::optional<std::pmr::string> const module_name = read_module_name(source_file_path);

                    if (module_name)
                    {
                        std::unique_lock<std::shared_mutex> lock{ protected_data.mutex };
                        protected_data.module_name_to_source_file_path.insert(std::make_pair(*module_name, source_file_path));
                        protected_data.module_name_to_module_file_path.insert(std::make_pair(*module_name, parsed_file_path));
                    }
                }

                std::chrono::high_resolution_clock::time_point const begin_processing = std::chrono::high_resolution_clock::now();

                bool const success = add_module_for_compilation(
                    parsed_file_path,
                    get_main_library(*unprotected_data.jit_data),
                    unprotected_data,
                    protected_data,
                    true
                );

                std::chrono::high_resolution_clock::time_point const end_processing = std::chrono::high_resolution_clock::now();

                if (unprotected_data.log_level >= 1)
                    std::puts(std::format("{} {}. Parsing took {}ms. Creating materialization units took {}ms. Total time was {}ms", success ? "Created materialization units for" : "Failed to create materialization units for", source_file_path.generic_string(), (begin_processing - begin_parsing) / 1ms, (end_processing - begin_processing) / 1ms, (end_processing - begin_parsing) / 1ms).c_str());
            }
        }
        else if (event.effect_type == wtr::event::effect_type::rename)
        {
            if (std::filesystem::exists(source_file_path))
            {
                std::optional<std::pmr::string> const module_name = read_module_name(source_file_path);
                if (module_name)
                {
                    std::unique_lock<std::shared_mutex> lock{ protected_data.mutex };
                    protected_data.module_name_to_source_file_path[*module_name] = source_file_path;
                }
            }
        }

        {
            std::unique_lock<std::shared_mutex> lock{ protected_data.mutex };
            protected_data.processed_files += 1;
        }
        protected_data.condition_variable.notify_all();

        // TODO when a file is created, check if it matches the includes and then (optional) add to the watch list and to the recompile module layer
        // TODO when a file is removed, maybe remove the definitions?
    }

    std::unique_ptr<JIT_runner> setup_jit_and_watch(
        std::filesystem::path const& artifact_configuration_file_path,
        std::span<std::filesystem::path const> const repositories_file_paths,
        std::filesystem::path const& build_directory_path,
        std::span<std::filesystem::path const> const header_search_paths,
        Target const& target,
        Compilation_options const& compilation_options
    )
    {
        // Print internal LLVM messages:
        //llvm::DebugFlag = true;

        std::unique_ptr<JIT_runner> jit_runner = std::make_unique<JIT_runner>();

        // Create readonly and protected data:
        {
            std::unique_ptr<h::compiler::LLVM_data> llvm_data = std::make_unique<h::compiler::LLVM_data>(h::compiler::initialize_llvm(compilation_options));
            std::unique_ptr<JIT_data> jit_data = create_jit_data(llvm_data->data_layout, h::common::get_default_library_directories(), compilation_options.debug);

            jit_runner->unprotected_data =
            {
                .build_directory_path = build_directory_path,
                .header_search_paths = { header_search_paths.begin(), header_search_paths.end() },
                .target = target,
                .parser = h::parser::create_parser(),
                .llvm_data = std::move(llvm_data),
                .jit_data = std::move(jit_data),
                .log_level = 1,
                .compilation_options = compilation_options
            };

            for (std::filesystem::path const& repository_file_path : repositories_file_paths)
            {
                Repository repository = get_repository(repository_file_path);
                jit_runner->protected_data.repositories.insert(std::make_pair(repository_file_path, std::move(repository)));
            }
        }

        // Create file watcher:
        {
            JIT_runner_unprotected_data const& unprotected_data = jit_runner->unprotected_data;
            JIT_runner_protected_data& protected_data = jit_runner->protected_data;

            std::function<void(wtr::watcher::event const&)> callback = [&](wtr::event const event) -> void
            {
                handle_file_change(unprotected_data, protected_data, event);
            };

            std::unique_ptr<File_watcher> file_watcher = create_file_watcher(std::move(callback));
            watch_repository_directories(*file_watcher, repositories_file_paths);
            jit_runner->file_watcher = std::move(file_watcher);
        }

        // Add generator:
        {
            JIT_runner_unprotected_data const& unprotected_data = jit_runner->unprotected_data;
            JIT_runner_protected_data& protected_data = jit_runner->protected_data;

            add_generator(*jit_runner->unprotected_data.jit_data, std::make_unique<H_definition_generator>(unprotected_data, protected_data));
        }

        // Add entry point artifact:
        {
            JIT_runner_unprotected_data const& unprotected_data = jit_runner->unprotected_data;
            JIT_runner_protected_data& protected_data = jit_runner->protected_data;

            add_artifact_for_compilation(artifact_configuration_file_path, unprotected_data, protected_data, *jit_runner->file_watcher);
        }

        return jit_runner;
    }

    std::uint64_t get_processed_files(
        JIT_runner& jit_runner
    )
    {
        std::shared_lock<std::shared_mutex> lock{ jit_runner.protected_data.mutex };
        return jit_runner.protected_data.processed_files;
    }

    void wait_for(
        JIT_runner& jit_runner,
        std::uint64_t const processed_files
    )
    {
        auto const has_enough_processed_files = [&]() { return jit_runner.protected_data.processed_files >= processed_files; };

        std::unique_lock<std::shared_mutex> lock{ jit_runner.protected_data.mutex };
        jit_runner.protected_data.condition_variable.wait(lock, has_enough_processed_files);
    }
}
