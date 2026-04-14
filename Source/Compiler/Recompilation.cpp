module h.compiler.recompilation;

import std;

import h.common;
import h.compiler;
import h.compiler.common;
import h.core.hash;
import h.core;
import h.core.types;
import h.json_serializer;

namespace h::compiler
{
    std::optional<std::uint64_t> get_hash(
        Symbol_name_to_hash const& symbol_name_to_hash,
        std::pmr::string const& symbol_name
    )
    {
        auto const location = symbol_name_to_hash.find(symbol_name);
        if (location == symbol_name_to_hash.end())
            return std::nullopt;
        return location->second;
    }

    std::pmr::unordered_set<std::pmr::string> compute_symbols_that_changed(
        h::Module const& core_module,
        Symbol_name_to_hash const& previous_symbol_name_to_hash,
        Symbol_name_to_hash const& new_symbol_name_to_hash
    )
    {
        std::pmr::unordered_set<std::pmr::string> symbols_that_changed;

        for (auto const& pair : previous_symbol_name_to_hash)
        {
            std::optional<std::uint64_t> const new_hash = get_hash(new_symbol_name_to_hash, pair.first);
            if (new_hash != pair.second)
            {
                symbols_that_changed.insert(pair.first);
            }
        }
        for (auto const& pair : new_symbol_name_to_hash)
        {
            auto const location = previous_symbol_name_to_hash.find(pair.first);
            if (location == previous_symbol_name_to_hash.end())
            {
                symbols_that_changed.insert(pair.first);
            }
        }

        std::pmr::unordered_set<std::pmr::string> visited_symbols;

        std::function<bool(std::string_view, h::Type_reference const&)> process_custom_type_reference;

        process_custom_type_reference = [&](
            std::string_view const declaration_name,
            h::Type_reference const& type_reference
            ) -> bool
        {
            if (std::holds_alternative<h::Custom_type_reference>(type_reference.data))
            {
                h::Custom_type_reference const& custom_type_reference = std::get<h::Custom_type_reference>(type_reference.data);
                std::string_view const type_module_name = find_module_name(core_module, custom_type_reference.module_reference);

                if (type_module_name == core_module.name)
                {
                    if (!visited_symbols.contains(custom_type_reference.name))
                    {
                        visited_symbols.insert(custom_type_reference.name);

                        if (symbols_that_changed.contains(custom_type_reference.name))
                        {
                            symbols_that_changed.insert(std::pmr::string{ declaration_name });
                            return true;
                        }

                        h::visit_type_references_recursively_with_declaration_name(core_module, custom_type_reference.name, process_custom_type_reference);
                    }
                }
            }

            return false;
        };

        h::visit_type_references_recursively_with_declaration_name(core_module.export_declarations, process_custom_type_reference);

        return symbols_that_changed;
    }

    std::pmr::unordered_set<std::pmr::string> compute_symbols_that_changed(
        h::Module const& core_module,
        h::Module const& dependency_module,
        std::pmr::unordered_set<std::pmr::string> const& dependency_symbols_that_changed
    )
    {
        std::pmr::unordered_set<std::pmr::string> symbols_that_changed;
        std::pmr::unordered_set<std::pmr::string> visited_symbols;

        std::function<bool(std::string_view, h::Type_reference const&)> process_custom_type_reference;

        process_custom_type_reference = [&](
            std::string_view const declaration_name,
            h::Type_reference const& type_reference
            ) -> bool
        {
            if (std::holds_alternative<h::Custom_type_reference>(type_reference.data))
            {
                h::Custom_type_reference const& custom_type_reference = std::get<h::Custom_type_reference>(type_reference.data);
                std::string_view const type_module_name = find_module_name(core_module, custom_type_reference.module_reference);

                if (type_module_name == dependency_module.name)
                {
                    if (dependency_symbols_that_changed.contains(custom_type_reference.name))
                    {
                        symbols_that_changed.insert(std::pmr::string{ declaration_name });
                        return true;
                    }
                }

                if (type_module_name == core_module.name)
                {
                    if (!visited_symbols.contains(custom_type_reference.name))
                    {
                        visited_symbols.insert(custom_type_reference.name);
                        h::visit_type_references_recursively_with_declaration_name(core_module, custom_type_reference.name, process_custom_type_reference);
                    }
                }
            }

            return false;
        };

        h::visit_type_references_recursively_with_declaration_name(core_module.export_declarations, process_custom_type_reference);

        return symbols_that_changed;
    }

    void find_modules_to_recompile(
        std::pmr::unordered_set<std::pmr::string>& modules_to_recompile,
        h::Module const& core_module,
        std::pmr::unordered_set<std::pmr::string> const& symbols_that_changed,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& module_name_to_file_path,
        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const& module_name_to_reverse_dependencies
    )
    {
        auto const reverse_dependencies_range = module_name_to_reverse_dependencies.equal_range(core_module.name);

        for (auto iterator = reverse_dependencies_range.first; iterator != reverse_dependencies_range.second; ++iterator)
        {
            std::pmr::string const& reverse_dependency_name = iterator->second;

            if (modules_to_recompile.contains(reverse_dependency_name))
                continue;

            std::filesystem::path const& reverse_dependency_file_path = module_name_to_file_path.at(reverse_dependency_name);

            std::optional<h::Module> const reverse_dependency = h::compiler::read_core_module_declarations(reverse_dependency_file_path);
            if (!reverse_dependency)
            {
                std::puts(std::format("Could not read '{}'!", reverse_dependency_file_path.generic_string()).c_str());
                continue;
            }

            auto const alias_import_location = std::find_if(
                reverse_dependency->dependencies.alias_imports.begin(),
                reverse_dependency->dependencies.alias_imports.end(),
                [&](Import_module_with_alias const& alias_import) -> bool { return alias_import.module_name == core_module.name; }
            );
            if (alias_import_location == reverse_dependency->dependencies.alias_imports.end())
                continue;

            for (std::pmr::string const& usage : alias_import_location->usages)
            {
                if (symbols_that_changed.contains(usage))
                {
                    modules_to_recompile.insert(reverse_dependency_name);

                    std::pmr::unordered_set<std::pmr::string> const reverse_dependency_symbols_that_changed =
                        compute_symbols_that_changed(
                            *reverse_dependency,
                            core_module,
                            symbols_that_changed
                        );

                    find_modules_to_recompile(
                        modules_to_recompile,
                        *reverse_dependency,
                        reverse_dependency_symbols_that_changed,
                        module_name_to_file_path,
                        module_name_to_reverse_dependencies
                    );

                    break;
                }
            }
        }
    }

    std::pmr::vector<std::pmr::string> find_modules_to_recompile(
        h::Module const& core_module,
        Symbol_name_to_hash const& previous_symbol_name_to_hash,
        Symbol_name_to_hash const& new_symbol_name_to_hash,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& module_name_to_file_path,
        std::pmr::unordered_multimap<std::pmr::string, std::pmr::string> const& module_name_to_reverse_dependencies,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::unordered_set<std::pmr::string> const symbols_that_changed = compute_symbols_that_changed(core_module, previous_symbol_name_to_hash, new_symbol_name_to_hash);

        std::pmr::unordered_set<std::pmr::string> modules_to_recompile{ temporaries_allocator };

        find_modules_to_recompile(
            modules_to_recompile,
            core_module,
            symbols_that_changed,
            module_name_to_file_path,
            module_name_to_reverse_dependencies
        );

        std::pmr::vector<std::pmr::string> output{ output_allocator };
        output.reserve(modules_to_recompile.size());

        for (std::pmr::string const& module_name : modules_to_recompile)
        {
            output.push_back(module_name);
        }

        return output;
    }
}
