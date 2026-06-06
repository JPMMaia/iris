module iris.compiler.import_usages_pass;

import std;

import iris.core;
import iris.core.types;

namespace iris::compiler
{
    void add_import_usages(
        iris::Module& core_module,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        for (Import_module_with_alias& alias_import : core_module.dependencies.alias_imports)
            alias_import.usages.clear();

        auto const add_unique_usage = [&](std::string_view const module_name, std::string_view const usage) -> void
        {
            if (module_name.empty())
                return;
    
            auto const location = std::find_if(
                core_module.dependencies.alias_imports.begin(),
                core_module.dependencies.alias_imports.end(),
                [&](Import_module_with_alias const& import_alias) -> bool { return import_alias.module_name == module_name; }
            );
            if (location != core_module.dependencies.alias_imports.end())
            {
                Import_module_with_alias& import_alias = *location;

                auto const usage_location = std::find(
                    import_alias.usages.begin(),
                    import_alias.usages.end(),
                    usage
                );
                if (usage_location == import_alias.usages.end())
                    import_alias.usages.push_back(std::pmr::string{ usage, std::move(output_allocator) });
            }
        };

        auto const process_type = [&](iris::Type_reference const& type_reference) -> bool
        {
            if (std::holds_alternative<iris::Custom_type_reference>(type_reference.data))
            {
                iris::Custom_type_reference const& custom_type_reference = std::get<iris::Custom_type_reference>(type_reference.data);
                add_unique_usage(custom_type_reference.module_reference.name, custom_type_reference.name);
            }

            return false;
        };

        iris::visit_type_references_recursively(
            core_module,
            process_type
        );

        auto const process_expression = [&](iris::Expression const& expression, iris::Statement const& statement) -> bool
        {
            if (std::holds_alternative<iris::Access_expression>(expression.data))
            {
                iris::Access_expression const& access_expression = std::get<iris::Access_expression>(expression.data);
                
                iris::Expression const& left_hand_side = statement.expressions[access_expression.expression.expression_index];
                if (std::holds_alternative<iris::Variable_expression>(left_hand_side.data))
                {
                    iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(left_hand_side.data);

                    std::string_view const left_hand_side_name = variable_expression.name;

                    auto const location = std::find_if(
                        core_module.dependencies.alias_imports.begin(),
                        core_module.dependencies.alias_imports.end(),
                        [&](Import_module_with_alias const& import_alias) -> bool { return import_alias.alias == left_hand_side_name; }
                    );
                    if (location != core_module.dependencies.alias_imports.end())
                    {
                        Import_module_with_alias& import_alias = *location;
                        add_unique_usage(import_alias.module_name, access_expression.member_name);
                    }
                }
            }

            return false;
        };

        iris::visit_expressions(
            core_module,
            process_expression
        );

        for (Import_module_with_alias& alias_import : core_module.dependencies.alias_imports)
            std::sort(alias_import.usages.begin(), alias_import.usages.end());
    }
}
