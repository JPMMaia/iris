module;

#include <stdexcept>

export module iris.compiler.pass_test_helpers;

import std;

import iris.core;
import iris.core.declarations;
import iris.core.formatter;
import iris.parser.convertor;

namespace iris::compiler::tests
{
    export struct Parsed_module_context
    {
        iris::Declaration_database declaration_database;
        std::pmr::vector<iris::Module> dependency_core_modules;
        iris::Module core_module;
    };

    export Parsed_module_context parse_module_context(
        std::string_view const input_text,
        std::span<std::string_view const> const input_dependencies_text
    )
    {
        Parsed_module_context context
        {
            .declaration_database = create_declaration_database(),
            .dependency_core_modules = {},
            .core_module = {},
        };

        context.dependency_core_modules.reserve(input_dependencies_text.size());

        for (std::size_t index = 0; index < input_dependencies_text.size(); ++index)
        {
            std::string_view const dependency_text = input_dependencies_text[index];

            std::optional<iris::Module> dependency_module = iris::parser::parse_and_convert_to_module(
                dependency_text,
                std::nullopt,
                {},
                {}
            );
            if (!dependency_module.has_value())
                throw std::runtime_error{"Could not parse dependency module text."};

            add_declarations(context.declaration_database, dependency_module.value());
            context.dependency_core_modules.push_back(std::move(dependency_module.value()));
        }

        std::optional<iris::Module> core_module = iris::parser::parse_and_convert_to_module(
            input_text,
            std::nullopt,
            {},
            {}
        );
        if (!core_module.has_value())
            throw std::runtime_error{"Could not parse input module text."};

        context.core_module = std::move(core_module.value());

        add_declarations(context.declaration_database, context.core_module);

        return context;
    }

    export iris::Function_declaration* find_mutable_function_declaration(
        iris::Module& core_module,
        std::string_view const function_name
    )
    {
        auto const find_in_collection = [function_name]<typename Collection>(Collection& collection) -> iris::Function_declaration*
        {
            auto const location = std::find_if(
                collection.begin(),
                collection.end(),
                [function_name](iris::Function_declaration const& declaration) -> bool
                {
                    return declaration.name == function_name;
                }
            );

            if (location == collection.end())
                return nullptr;

            return &(*location);
        };

        if (iris::Function_declaration* declaration = find_in_collection(core_module.export_declarations.function_declarations); declaration != nullptr)
            return declaration;

        if (iris::Function_declaration* declaration = find_in_collection(core_module.internal_declarations.function_declarations); declaration != nullptr)
            return declaration;

        if (iris::Function_declaration* declaration = find_in_collection(core_module.instanced_declarations.function_declarations); declaration != nullptr)
            return declaration;

        return nullptr;
    }

    export iris::Function_definition* find_mutable_function_definition(
        iris::Module& core_module,
        std::string_view const function_name
    )
    {
        auto const location = std::find_if(
            core_module.definitions.function_definitions.begin(),
            core_module.definitions.function_definitions.end(),
            [function_name](iris::Function_definition const& definition) -> bool
            {
                return definition.name == function_name;
            }
        );
        if (location == core_module.definitions.function_definitions.end())
            return nullptr;

        return &(*location);
    }

    export std::pmr::string format_core_module_to_text(
        iris::Module const& core_module
    )
    {
        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        Format_options const format_options =
        {
            .output_allocator = output_allocator,
            .temporaries_allocator = temporaries_allocator,
        };

        return iris::format_module(core_module, format_options);
    }
}