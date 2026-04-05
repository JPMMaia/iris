module;

#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>

module h.compiler.debug_info;

import std;

import h.compiler.types;
import h.core;
import h.core.types;

namespace h::compiler
{
    llvm::DIScope* get_debug_scope(
        Debug_info& debug_info
    )
    {
        return debug_info.llvm_scopes.empty() ? debug_info.main_llvm_compile_unit : debug_info.llvm_scopes.back();
    }

    void push_debug_scope(
        Debug_info& debug_info,
        llvm::DIScope* scope
    )
    {
        debug_info.llvm_scopes.push_back(scope);
    }

    void push_debug_lexical_block_scope(
        Debug_info& debug_info,
        Source_position const source_position
    )
    {
        llvm::DIScope* const parent_scope = get_debug_scope(
            debug_info
        );

        llvm::DILexicalBlock* const lexical_block = debug_info.llvm_builder->createLexicalBlock(
            parent_scope,
            parent_scope->getFile(),
            source_position.line,
            source_position.column
        );

        push_debug_scope(debug_info, lexical_block);
    }

    void pop_debug_scope(
        Debug_info& debug_info
    )
    {
        debug_info.llvm_scopes.pop_back();
    }

    void set_debug_location(
        llvm::IRBuilder<>& llvm_builder,
        Debug_info& debug_info,
        unsigned const line,
        unsigned const column
    )
    {
        llvm::DIScope* const scope = get_debug_scope(debug_info);

        llvm::DILocation* debug_location = llvm::DILocation::get(
            scope->getContext(),
            line,
            column,
            scope
        );

        llvm_builder.SetCurrentDebugLocation(debug_location);
    }

    void unset_debug_location(
        llvm::IRBuilder<>& llvm_builder
    )
    {
        llvm_builder.SetCurrentDebugLocation(llvm::DebugLoc{});
    }

    void set_debug_location_at_statement(
        llvm::IRBuilder<>& llvm_builder,
        Debug_info& debug_info,
        h::Statement const& statement
    )
    {
        if (!statement.expressions.empty())
        {
            std::optional<h::Source_range> const& source_range = statement.expressions[0].source_range;
            set_debug_location_at_range(llvm_builder, debug_info, source_range);
        }
    }
    
    void set_debug_location_at_range(
        llvm::IRBuilder<>& llvm_builder,
        Debug_info& debug_info,
        std::optional<h::Source_range> const& source_range
    )
    {
        if (source_range.has_value())
            set_debug_location(llvm_builder, debug_info, source_range->start.line, source_range->start.column);
    }

    static bool contains_debug_type_name(
        std::span<std::pmr::string const> const names,
        std::string_view const name
    )
    {
        auto const location = std::find(names.begin(), names.end(), name);
        return location != names.end();
    }

    static bool add_debug_type_name(
        Debug_type_names_per_module& requested_debug_types,
        std::string_view const module_name,
        std::string_view const type_name,
        std::pmr::polymorphic_allocator<> const& allocator
    )
    {
        Debug_type_names& names = requested_debug_types[std::pmr::string{module_name, allocator}];
        if (contains_debug_type_name(names, type_name))
            return false;

        names.push_back(std::pmr::string{type_name, allocator});
        return true;
    }

    static Module const* find_debug_module(
        std::pmr::unordered_map<std::pmr::string, Module> const& core_module_dependencies,
        std::string_view const module_name,
        std::pmr::polymorphic_allocator<> const& allocator
    )
    {
        auto const location = core_module_dependencies.find(std::pmr::string{module_name, allocator});
        if (location == core_module_dependencies.end())
            return nullptr;

        return &location->second;
    }

    static void add_nested_requested_debug_types(
        Module const& owner_module,
        Type_reference const& type_reference,
        std::pmr::unordered_map<std::pmr::string, Module> const& core_module_dependencies,
        Debug_type_names_per_module& requested_debug_types,
        std::pmr::unordered_set<std::pmr::string>& visited_debug_types,
        std::pmr::polymorphic_allocator<> const& allocator
    );

    static void expand_requested_debug_type(
        std::string_view const module_name,
        std::string_view const type_name,
        std::pmr::unordered_map<std::pmr::string, Module> const& core_module_dependencies,
        Debug_type_names_per_module& requested_debug_types,
        std::pmr::unordered_set<std::pmr::string>& visited_debug_types,
        std::pmr::polymorphic_allocator<> const& allocator
    )
    {
        std::pmr::string const visited_key = std::pmr::string{std::format("{}:{}", module_name, type_name), allocator};
        auto const [_, is_new] = visited_debug_types.insert(visited_key);
        if (!is_new)
            return;

        Module const* const module = find_debug_module(core_module_dependencies, module_name, allocator);
        if (module == nullptr)
            return;

        if (std::optional<Alias_type_declaration const*> const alias_declaration = find_alias_type_declaration(*module, type_name))
        {
            if (!alias_declaration.value()->type.empty())
                add_nested_requested_debug_types(*module, alias_declaration.value()->type[0], core_module_dependencies, requested_debug_types, visited_debug_types, allocator);
            return;
        }

        if (std::optional<Struct_declaration const*> const struct_declaration = find_struct_declaration(*module, type_name))
        {
            for (Type_reference const& member_type : struct_declaration.value()->member_types)
                add_nested_requested_debug_types(*module, member_type, core_module_dependencies, requested_debug_types, visited_debug_types, allocator);
            return;
        }

        if (std::optional<Union_declaration const*> const union_declaration = find_union_declaration(*module, type_name))
        {
            for (Type_reference const& member_type : union_declaration.value()->member_types)
                add_nested_requested_debug_types(*module, member_type, core_module_dependencies, requested_debug_types, visited_debug_types, allocator);
            return;
        }
    }

    static void add_nested_requested_debug_types(
        Module const& owner_module,
        Type_reference const& type_reference,
        std::pmr::unordered_map<std::pmr::string, Module> const& core_module_dependencies,
        Debug_type_names_per_module& requested_debug_types,
        std::pmr::unordered_set<std::pmr::string>& visited_debug_types,
        std::pmr::polymorphic_allocator<> const& allocator
    )
    {
        auto const process_type = [&](Type_reference const& nested_type_reference) -> bool
        {
            if (!std::holds_alternative<Custom_type_reference>(nested_type_reference.data))
                return false;

            Custom_type_reference const& custom_type_reference = std::get<Custom_type_reference>(nested_type_reference.data);
            std::string_view const nested_module_name = find_module_name(owner_module, custom_type_reference.module_reference);
            if (nested_module_name.empty())
                return false;

            if (add_debug_type_name(requested_debug_types, nested_module_name, custom_type_reference.name, allocator))
                expand_requested_debug_type(nested_module_name, custom_type_reference.name, core_module_dependencies, requested_debug_types, visited_debug_types, allocator);

            return false;
        };

        visit_type_references_recursively(type_reference, process_type);
    }

    Debug_type_names_per_module create_requested_dependency_debug_types(
        Module const& core_module,
        std::pmr::unordered_map<std::pmr::string, Module> const& core_module_dependencies,
        std::pmr::polymorphic_allocator<> const& allocator
    )
    {
        Debug_type_names_per_module requested_debug_types{allocator};
        std::pmr::unordered_set<std::pmr::string> visited_debug_types{allocator};

        for (Import_module_with_alias const& alias_import : core_module.dependencies.alias_imports)
        {
            for (std::pmr::string const& usage : alias_import.usages)
            {
                if (add_debug_type_name(requested_debug_types, alias_import.module_name, usage, allocator))
                    expand_requested_debug_type(alias_import.module_name, usage, core_module_dependencies, requested_debug_types, visited_debug_types, allocator);
            }
        }

        for (auto& [_, names] : requested_debug_types)
            std::sort(names.begin(), names.end());

        return requested_debug_types;
    }
}
