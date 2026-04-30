module;

#include <cassert>

module iris.compiler.instantiate_pass;

import std;

import iris.core;
import iris.core.declarations;
import iris.core.expressions;
import iris.core.types;
import iris.compiler;
import iris.compiler.all_passes;
import iris.compiler.analysis;
import iris.compiler.clang_code_generation;
import iris.compiler.clang_data;
import iris.compiler.types;

namespace iris::compiler
{
    static void add_instantiated_type_to_module(
        Declaration_database& declaration_database,
        Type_instance const& type_instance,
        Module_instanced_declarations& instanced_declarations
    )
    {
        std::pmr::string const mangled_name = mangle_type_instance_name(type_instance);

        std::optional<Declaration> const existing = find_declaration_in_instanced_module_declarations(
            instanced_declarations,
            type_instance.type_constructor.module_reference.name,
            mangled_name
        );
        if (existing.has_value())
            return;

        Declaration_instance_storage storage = instantiate_type_instance(declaration_database, type_instance);

        if (std::holds_alternative<Struct_declaration>(storage.data))
        {
            Struct_declaration& struct_declaration = std::get<Struct_declaration>(storage.data);
            instanced_declarations.struct_declarations.push_back(std::move(struct_declaration));
            
            add_struct_declaration(declaration_database, type_instance.type_constructor.module_reference.name, false, instanced_declarations.struct_declarations.back());
        }
    }

    static void instantiate_type(
        Declaration_database& declaration_database,
        iris::Type_reference& mutable_type_reference,
        Type_instance const& type_instance,
        Module_instanced_declarations& instanced_declarations
    )
    {
        add_instantiated_type_to_module(declaration_database, type_instance, instanced_declarations);

        // Replace `iris::Type_instance` by the actual type
        {
            std::pmr::string const mangled_name = mangle_type_instance_name(type_instance);
        
            mutable_type_reference = iris::create_custom_type_reference(type_instance.type_constructor.module_reference.name, mangled_name);
        }
    }

    static void instantiate_all_types(
        iris::Module const& core_module,
        Declaration_database& declaration_database,
        Module_instanced_declarations& instanced_declarations
    )
    {
        auto const instantiate_all = [&](std::string_view const declaration_name, iris::Type_reference const& type_reference) -> bool {

            if (std::holds_alternative<Type_instance>(type_reference.data))
            {
                iris::Type_reference& mutable_type_reference = const_cast<iris::Type_reference&>(type_reference);
                
                Type_instance const& type_instance = std::get<Type_instance>(type_reference.data);
                instantiate_type(declaration_database, mutable_type_reference, type_instance, instanced_declarations);
            }

            return false;
        };

        iris::visit_type_references_recursively_with_declaration_name(
            core_module,
            instantiate_all
        );
    }

    static void instantiate_types_in_function(
        Declaration_database& declaration_database,
        iris::Function_declaration& function_declaration,
        iris::Function_definition& function_definition,
        Module_instanced_declarations& instanced_declarations
    )
    {
        auto const instantiate_all = [&](iris::Type_reference const& type_reference) -> bool {

            if (std::holds_alternative<Type_instance>(type_reference.data))
            {
                iris::Type_reference& mutable_type_reference = const_cast<iris::Type_reference&>(type_reference);

                Type_instance const& type_instance = std::get<Type_instance>(type_reference.data);
                instantiate_type(declaration_database, mutable_type_reference, type_instance, instanced_declarations);
            }

            return false;
        };

        iris::visit_type_references_recursively(
            function_declaration,
            instantiate_all
        );

        iris::visit_type_references_recursively(
            function_definition,
            instantiate_all
        );
    }

    static bool is_builtin_instance_call(
        iris::Statement const& statement,
        Instance_call_expression const& expression
    )
    {
        iris::Expression const& left_hand_side = statement.expressions[expression.left_hand_side.expression_index];
        if (std::holds_alternative<iris::Variable_expression>(left_hand_side.data))
        {
            iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(left_hand_side.data);
            return is_builtin_function_name(variable_expression.name);
        }

        return false;
    }

    static std::optional<std::size_t> find_parent_call_expression_index(
        iris::Statement const& statement,
        std::size_t const callee_expression_index
    );

    static void rewrite_expressions_for_cross_module_context(
        iris::Function_definition& function_definition,
        std::string_view const source_module_name,
        std::string_view const target_module_name,
        iris::Module_dependencies& target_dependencies,
        iris::Declaration_database& declaration_database,
        iris::Module_instanced_declarations& target_instanced_declarations,
        iris::Module_definitions& target_definitions,
        std::span<iris::Module const* const> const sorted_core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::unordered_set<std::pmr::string>& duplicated_private_functions
    );

    static Module const* find_module_by_name(
        std::span<iris::Module const* const> const sorted_core_modules,
        std::string_view const module_name
    )
    {
        auto const location = std::find_if(
            sorted_core_modules.begin(),
            sorted_core_modules.end(),
            [module_name](iris::Module const* const module) -> bool
            {
                return module != nullptr && module->name == module_name;
            }
        );
        if (location == sorted_core_modules.end())
            return nullptr;

        return *location;
    }

    static std::pmr::string create_private_dependency_name(
        std::string_view const source_module_name,
        std::string_view const function_name,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::string name{source_module_name, output_allocator};
        name += ".";
        name += function_name;
        return name;
    }

    static std::pmr::string create_private_dependency_key(
        std::string_view const source_module_name,
        std::string_view const function_name,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::string key{source_module_name, output_allocator};
        key += "::";
        key += function_name;
        return key;
    }

    static bool ensure_private_function_is_duplicated(
        std::string_view const source_module_name,
        std::string_view const target_module_name,
        std::string_view const function_name,
        iris::Module_dependencies& target_dependencies,
        iris::Declaration_database& declaration_database,
        iris::Module_instanced_declarations& target_instanced_declarations,
        iris::Module_definitions& target_definitions,
        std::span<iris::Module const* const> const sorted_core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::unordered_set<std::pmr::string>& duplicated_private_functions
    )
    {
        std::pmr::string const duplicate_key = create_private_dependency_key(source_module_name, function_name, output_allocator);
        if (duplicated_private_functions.contains(duplicate_key))
            return true;

        std::pmr::string const duplicated_name = create_private_dependency_name(source_module_name, function_name, output_allocator);
        if (find_declaration(declaration_database, target_module_name, duplicated_name).has_value())
        {
            duplicated_private_functions.insert(duplicate_key);
            return true;
        }

        Module const* const source_module = find_module_by_name(sorted_core_modules, source_module_name);
        if (source_module == nullptr)
            return false;

        std::optional<Function_declaration const*> const source_declaration = find_function_declaration(*source_module, function_name);
        std::optional<Function_definition const*> const source_definition = find_function_definition(*source_module, function_name);
        if (!source_declaration.has_value() || !source_definition.has_value())
            return false;

        duplicated_private_functions.insert(duplicate_key);

        Function_declaration duplicated_declaration = *source_declaration.value();
        duplicated_declaration.name = duplicated_name;
        duplicated_declaration.unique_name = std::pmr::string{duplicated_name, output_allocator};
        duplicated_declaration.linkage = Linkage::Private;

        Function_definition duplicated_definition = *source_definition.value();
        duplicated_definition.name = duplicated_name;

        target_instanced_declarations.function_declarations.push_back(std::move(duplicated_declaration));
        target_definitions.function_definitions.push_back(std::move(duplicated_definition));

        add_function_declaration(
            declaration_database,
            target_module_name,
            false,
            target_instanced_declarations.function_declarations.back()
        );

        rewrite_expressions_for_cross_module_context(
            target_definitions.function_definitions.back(),
            source_module_name,
            target_module_name,
            target_dependencies,
            declaration_database,
            target_instanced_declarations,
            target_definitions,
            sorted_core_modules,
            output_allocator,
            duplicated_private_functions
        );

        return true;
    }

    static Import_module_with_alias const& ensure_import_module_with_alias(
        iris::Module_dependencies& dependencies,
        std::string_view const module_name,
        std::string_view const alias,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        Import_module_with_alias const* const existing_import = find_import_module_with_module_name(dependencies, module_name);
        if (existing_import != nullptr)
            return *existing_import;

        Import_module_with_alias new_import
        {
            .module_name = std::pmr::string{module_name, output_allocator},
            .alias = std::pmr::string{alias, output_allocator},
            .usages = std::pmr::vector<std::pmr::string>{output_allocator},
            .source_range = std::nullopt,
        };

        dependencies.alias_imports.push_back(std::move(new_import));
        return dependencies.alias_imports.back();
    }

    static bool is_name_mangled(std::string_view const name)
    {
        return name.find('@') != std::string_view::npos;
    }

    static std::pmr::string create_unique_import_alias(
        iris::Module_dependencies const& dependencies,
        std::string_view const preferred_alias,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        Import_module_with_alias const* const existing_alias = find_import_module_with_alias(dependencies, preferred_alias);
        if (existing_alias == nullptr)
            return std::pmr::string{preferred_alias, output_allocator};

        for (std::size_t index = 1; ; ++index)
        {
            std::pmr::string candidate{preferred_alias, output_allocator};
            candidate += "_";
            candidate += std::to_string(index);

            if (find_import_module_with_alias(dependencies, candidate) == nullptr)
                return candidate;
        }
    }

    static std::string_view ensure_target_alias_for_source_import(
        iris::Module_dependencies const& source_dependencies,
        iris::Module_dependencies& target_dependencies,
        std::string_view const source_alias,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        Import_module_with_alias const* const source_import = find_import_module_with_alias(source_dependencies, source_alias);
        if (source_import == nullptr)
            return {};

        Import_module_with_alias const* const existing_target_import = find_import_module_with_module_name(target_dependencies, source_import->module_name);
        if (existing_target_import != nullptr)
            return existing_target_import->alias;

        std::pmr::string const target_alias = create_unique_import_alias(
            target_dependencies,
            source_alias,
            output_allocator
        );

        return ensure_import_module_with_alias(
            target_dependencies,
            source_import->module_name,
            target_alias,
            output_allocator
        ).alias;
    }

    static void add_usage_to_target_import_alias(
        iris::Module_dependencies& target_dependencies,
        std::string_view const target_alias,
        std::string_view const usage,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        if (target_alias.empty() || usage.empty())
            return;

        Import_module_with_alias* const import_alias = find_import_module_with_alias(target_dependencies, target_alias);
        if (import_alias == nullptr)
            return;

        auto const existing_usage = std::find(import_alias->usages.begin(), import_alias->usages.end(), usage);
        if (existing_usage != import_alias->usages.end())
            return;

        import_alias->usages.push_back(std::pmr::string{usage, output_allocator});
    }

    static void rewrite_statement_for_cross_module_context(
        iris::Statement& statement,
        std::string_view const source_module_name,
        std::string_view const target_module_name,
        iris::Module_dependencies const& source_dependencies,
        iris::Module_dependencies& target_dependencies,
        iris::Declaration_database& declaration_database,
        iris::Module_instanced_declarations& target_instanced_declarations,
        iris::Module_definitions& target_definitions,
        std::span<iris::Module const* const> const sorted_core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::unordered_set<std::pmr::string>& duplicated_private_functions
    )
    {
        if (source_module_name == target_module_name)
            return;

        Import_module_with_alias const* const source_module_import = find_import_module_with_module_name(target_dependencies, source_module_name);
        std::pmr::string const source_module_alias = source_module_import != nullptr ? source_module_import->alias : std::pmr::string{};

        struct Direct_call_rewrite
        {
            iris::Statement* statement = nullptr;
            std::size_t expression_index = 0;
        };

        struct Variable_reference_rewrite
        {
            iris::Statement* statement = nullptr;
            std::size_t expression_index = 0;
        };

        struct Private_function_rewrite
        {
            iris::Statement* statement = nullptr;
            std::size_t expression_index = 0;
            std::pmr::string function_name;
        };

        struct Alias_rewrite
        {
            iris::Statement* statement = nullptr;
            std::size_t expression_index = 0;
            std::pmr::string member_name;
            std::pmr::string replacement_alias;
        };

        std::pmr::vector<Direct_call_rewrite> direct_call_rewrites;
        std::pmr::vector<Variable_reference_rewrite> variable_reference_rewrites;
        std::pmr::vector<Private_function_rewrite> private_function_rewrites;
        std::pmr::vector<Alias_rewrite> alias_rewrites;

        auto const gather_rewrites = [&](iris::Expression const& expression, iris::Statement const& current_statement) -> bool
        {
            iris::Statement& owning_statement = const_cast<iris::Statement&>(current_statement);
            std::size_t const expression_index = find_expression_index(owning_statement, expression);

            if (std::holds_alternative<iris::Variable_expression>(expression.data))
            {
                iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(expression.data);

                if (!is_name_mangled(variable_expression.name))
                {
                    std::optional<std::size_t> const parent_call_expression_index = find_parent_call_expression_index(owning_statement, expression_index);
                    if (parent_call_expression_index.has_value())
                    {
                        iris::Call_expression const& parent_call = std::get<iris::Call_expression>(
                            owning_statement.expressions[parent_call_expression_index.value()].data
                        );

                        if (parent_call.expression.expression_index == expression_index)
                        {
                            std::optional<Declaration> const declaration = find_declaration(
                                declaration_database,
                                source_module_name,
                                variable_expression.name
                            );
                            if (declaration.has_value() && std::holds_alternative<Function_declaration const*>(declaration->data) && !declaration->is_export)
                            {
                                private_function_rewrites.push_back({
                                    .statement = &owning_statement,
                                    .expression_index = expression_index,
                                    .function_name = std::pmr::string{variable_expression.name, output_allocator}
                                });
                            }
                            else if (declaration.has_value() && !source_module_alias.empty())
                            {
                                direct_call_rewrites.push_back({
                                    .statement = &owning_statement,
                                    .expression_index = expression_index,
                                });
                            }

                            return false;
                        }
                    }

                    bool const is_access_left_hand_side = std::ranges::any_of(
                        owning_statement.expressions,
                        [&](iris::Expression const& candidate_expression)
                        {
                            if (!std::holds_alternative<iris::Access_expression>(candidate_expression.data))
                                return false;

                            iris::Access_expression const& access_expression = std::get<iris::Access_expression>(candidate_expression.data);
                            return access_expression.expression.expression_index == expression_index;
                        }
                    );
                    if (is_access_left_hand_side)
                        return false;

                    std::optional<Declaration> const declaration = find_declaration(
                        declaration_database,
                        source_module_name,
                        variable_expression.name
                    );
                    if (declaration.has_value() && std::holds_alternative<Function_declaration const*>(declaration->data) && !declaration->is_export)
                    {
                        private_function_rewrites.push_back({
                            .statement = &owning_statement,
                            .expression_index = expression_index,
                            .function_name = std::pmr::string{variable_expression.name, output_allocator}
                        });
                    }
                    else if (declaration.has_value() && !source_module_alias.empty())
                    {
                        variable_reference_rewrites.push_back({
                            .statement = &owning_statement,
                            .expression_index = expression_index,
                        });
                    }
                }
            }
            else if (std::holds_alternative<iris::Access_expression>(expression.data))
            {
                iris::Access_expression const& access_expression = std::get<iris::Access_expression>(expression.data);
                iris::Expression const& left_hand_side = owning_statement.expressions[access_expression.expression.expression_index];
                if (std::holds_alternative<iris::Variable_expression>(left_hand_side.data))
                {
                    iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(left_hand_side.data);
                    std::string_view const replacement_alias = ensure_target_alias_for_source_import(
                        source_dependencies,
                        target_dependencies,
                        variable_expression.name,
                        output_allocator
                    );
                    if (!replacement_alias.empty())
                    {
                        alias_rewrites.push_back({
                            .statement = &owning_statement,
                            .expression_index = access_expression.expression.expression_index,
                            .member_name = std::pmr::string{access_expression.member_name, output_allocator},
                            .replacement_alias = std::pmr::string{replacement_alias, output_allocator}
                        });
                    }
                }
            }

            return false;
        };

        iris::visit_expressions(statement, gather_rewrites);

        for (Private_function_rewrite const& rewrite : private_function_rewrites)
        {
            bool const duplicated = ensure_private_function_is_duplicated(
                source_module_name,
                target_module_name,
                rewrite.function_name,
                target_dependencies,
                declaration_database,
                target_instanced_declarations,
                target_definitions,
                sorted_core_modules,
                output_allocator,
                duplicated_private_functions
            );
            if (!duplicated)
                continue;

            iris::Expression& expression = rewrite.statement->expressions[rewrite.expression_index];
            iris::Variable_expression& variable_expression = std::get<iris::Variable_expression>(expression.data);
            variable_expression.name = create_private_dependency_name(source_module_name, rewrite.function_name, output_allocator);
        }

        for (Alias_rewrite const& replacement : alias_rewrites)
        {
            iris::Expression& left_hand_side = replacement.statement->expressions[replacement.expression_index];
            iris::Variable_expression& variable_expression = std::get<iris::Variable_expression>(left_hand_side.data);
            variable_expression.name = replacement.replacement_alias;

            add_usage_to_target_import_alias(
                target_dependencies,
                replacement.replacement_alias,
                replacement.member_name,
                output_allocator
            );
        }

        for (Variable_reference_rewrite const& rewrite : variable_reference_rewrites)
        {
            iris::Variable_expression const variable_expression = std::get<iris::Variable_expression>(
                rewrite.statement->expressions[rewrite.expression_index].data
            );

            rewrite.statement->expressions.push_back(
                iris::create_variable_expression(std::pmr::string{source_module_alias, output_allocator})
            );
            std::size_t const alias_expression_index = rewrite.statement->expressions.size() - 1;

            rewrite.statement->expressions[rewrite.expression_index] = {
                .data = iris::Access_expression{
                    .expression = iris::Expression_index{ .expression_index = alias_expression_index },
                    .member_name = variable_expression.name,
                }
            };

            add_usage_to_target_import_alias(
                target_dependencies,
                source_module_alias,
                variable_expression.name,
                output_allocator
            );
        }

        for (Direct_call_rewrite const& rewrite : direct_call_rewrites)
        {
            iris::Variable_expression const variable_expression = std::get<iris::Variable_expression>(
                rewrite.statement->expressions[rewrite.expression_index].data
            );

            rewrite.statement->expressions.push_back(
                iris::create_variable_expression(std::pmr::string{source_module_alias, output_allocator})
            );
            std::size_t const alias_expression_index = rewrite.statement->expressions.size() - 1;

            rewrite.statement->expressions[rewrite.expression_index] = {
                .data = iris::Access_expression{
                    .expression = iris::Expression_index{ .expression_index = alias_expression_index },
                    .member_name = variable_expression.name,
                }
            };

            add_usage_to_target_import_alias(
                target_dependencies,
                source_module_alias,
                variable_expression.name,
                output_allocator
            );
        }
    }

    static void rewrite_expressions_for_cross_module_context(
        iris::Function_definition& function_definition,
        std::string_view const source_module_name,
        std::string_view const target_module_name,
        iris::Module_dependencies& target_dependencies,
        iris::Declaration_database& declaration_database,
        iris::Module_instanced_declarations& target_instanced_declarations,
        iris::Module_definitions& target_definitions,
        std::span<iris::Module const* const> const sorted_core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::unordered_set<std::pmr::string>& duplicated_private_functions
    )
    {
        if (source_module_name == target_module_name)
            return;

        iris::Module_dependencies const& source_dependencies = get_module_dependencies(declaration_database, source_module_name);

        for (iris::Statement& statement : function_definition.statements)
        {
            rewrite_statement_for_cross_module_context(
                statement,
                source_module_name,
                target_module_name,
                source_dependencies,
                target_dependencies,
                declaration_database,
                target_instanced_declarations,
                target_definitions,
                sorted_core_modules,
                output_allocator,
                duplicated_private_functions
            );
        }
    }

    static void instantiate_function(
        std::string_view const module_name,
        iris::Statement& statement,
        iris::Expression const& expression,
        Instance_call_key const key,
        All_passes_parameters const& parameters
    )
    {
        std::pmr::string const mangled_name = std::pmr::string{mangle_instance_call_name(key)};

        std::optional<Declaration> const existing = find_declaration_in_instanced_module_declarations(
            parameters.instanced_declarations,
            key.module_name,
            mangled_name
        );
        if (existing.has_value())
            return;
        
        iris::Function_constructor const* function_constructor = get_function_constructor(
            parameters.declaration_database,
            key.module_name,
            key.function_constructor_name
        );
        if (function_constructor == nullptr)
            throw std::runtime_error{ "Could not find function constructor!" };

        iris::Function_expression function_expression = create_instance_call_expression_value(
            *function_constructor,
            key.arguments,
            key
        );

        run_all_passes_on_function(
            key.module_name,
            function_expression.declaration,
            function_expression.definition,
            parameters
        );

        std::pmr::unordered_set<std::pmr::string> duplicated_private_functions;

        rewrite_expressions_for_cross_module_context(
            function_expression.definition,
            key.module_name,
            parameters.target_module_name,
            parameters.dependencies,
            parameters.declaration_database,
            parameters.instanced_declarations,
            parameters.definitions,
            parameters.sorted_core_modules,
            parameters.output_allocator,
            duplicated_private_functions
        );

        parameters.instanced_declarations.function_declarations.push_back(function_expression.declaration);
        parameters.definitions.function_definitions.push_back(function_expression.definition);

        add_function_declaration(parameters.declaration_database, parameters.target_module_name, false, parameters.instanced_declarations.function_declarations.back());
    }

    struct Replace_instantiate_function_parameters
    {
        iris::Statement& statement;
        std::size_t expression_index;
        Instance_call_key const key;
    };

    static std::optional<std::size_t> find_parent_call_expression_index(
        iris::Statement const& statement,
        std::size_t const callee_expression_index
    )
    {
        for (std::size_t index = 0; index < statement.expressions.size(); ++index)
        {
            iris::Expression const& expression = statement.expressions[index];
            if (!std::holds_alternative<iris::Call_expression>(expression.data))
                continue;

            iris::Call_expression const& call_expression = std::get<iris::Call_expression>(expression.data);
            if (call_expression.expression.expression_index == callee_expression_index)
                return index;
        }

        return std::nullopt;
    }

    static std::optional<iris::Expression_index> create_implicit_receiver_argument(
        iris::Statement& statement,
        iris::Expression const& callee_expression
    )
    {
        if (std::holds_alternative<iris::Access_expression>(callee_expression.data))
        {
            iris::Access_expression const& access_expression = std::get<iris::Access_expression>(callee_expression.data);

            iris::Expression_index const copied_receiver_expression = copy_expressions_to_new_statement(
                statement,
                statement,
                access_expression.expression
            );

            iris::Expression receiver_expression = {
                .data = iris::Unary_expression{
                    .expression = copied_receiver_expression,
                    .operation = iris::Unary_operation::Address_of
                }
            };

            statement.expressions.push_back(std::move(receiver_expression));
            return iris::Expression_index{ .expression_index = statement.expressions.size() - 1 };
        }

        if (std::holds_alternative<iris::Dereference_and_access_expression>(callee_expression.data))
        {
            iris::Dereference_and_access_expression const& access_expression = std::get<iris::Dereference_and_access_expression>(callee_expression.data);

            iris::Expression_index const copied_receiver_expression = copy_expressions_to_new_statement(
                statement,
                statement,
                access_expression.expression
            );

            return copied_receiver_expression;
        }

        return std::nullopt;
    }

    static void replace_instantiate_function(
        Replace_instantiate_function_parameters const& instantiate_parameters,
        All_passes_parameters const& parameters
    )
    {
        std::pmr::string const mangled_name = std::pmr::string{mangle_instance_call_name(instantiate_parameters.key)};

        std::size_t const callee_expression_index = instantiate_parameters.expression_index;
        iris::Expression const& callee_expression = instantiate_parameters.statement.expressions[callee_expression_index];

        std::optional<std::size_t> const parent_call_expression_index = find_parent_call_expression_index(
            instantiate_parameters.statement,
            callee_expression_index
        );

        if (parent_call_expression_index.has_value())
        {
            std::optional<iris::Expression_index> const receiver_argument = create_implicit_receiver_argument(
                instantiate_parameters.statement,
                callee_expression
            );

            if (receiver_argument.has_value())
            {
                iris::Expression& parent_call_expression = instantiate_parameters.statement.expressions[parent_call_expression_index.value()];
                iris::Call_expression& call_expression = std::get<iris::Call_expression>(parent_call_expression.data);
                call_expression.arguments.insert(call_expression.arguments.begin(), receiver_argument.value());
            }
        }

        iris::Expression const& expression_to_replace = instantiate_parameters.statement.expressions[callee_expression_index];

        std::pmr::vector<iris::Expression> new_expressions
        {
            iris::create_variable_expression(mangled_name)
        };

        iris::Statement const new_statement = iris::create_statement(std::move(new_expressions));

        replace_expression(
            instantiate_parameters.statement,
            expression_to_replace,
            new_statement,
            parameters.temporaries_allocator
        );
    }

    static void visit_deduced_instance_calls(
        std::string_view const module_name,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition& function_definition,
        All_passes_parameters const& parameters,
        std::function<void(std::string_view, iris::Statement&, iris::Expression const&, Instance_call_key const&, All_passes_parameters const&)> const& callback
    )
    {
        auto const scope_callback = [&](iris::Statement const& statement, iris::compiler::Scope const& scope) -> bool
        {
            auto const instantiate_all = [&](iris::Expression const& expression, iris::Statement const& statement) -> bool
            {
                if (std::holds_alternative<iris::Call_expression>(expression.data))
                {
                    iris::Call_expression const& call_expression = std::get<iris::Call_expression>(expression.data);
                    std::optional<Deduced_instance_call> const deduced_instance_call = deduce_instance_call_arguments(
                        parameters.declaration_database,
                        module_name,
                        scope,
                        statement,
                        call_expression,
                        parameters.temporaries_allocator
                    );
                    if (deduced_instance_call.has_value())
                    {
                        Instance_call_key const key = {
                            .module_name = deduced_instance_call->custom_type_reference.module_reference.name,
                            .function_constructor_name = deduced_instance_call->custom_type_reference.name,
                            .arguments = deduced_instance_call->arguments
                        };

                        iris::Statement& mutable_statement = const_cast<iris::Statement&>(statement);

                        callback(
                            module_name,
                            mutable_statement,
                            statement.expressions[call_expression.expression.expression_index],
                            key,
                            parameters
                        );
                    }
                }
                else if (std::holds_alternative<Instance_call_expression>(expression.data))
                {
                    Instance_call_expression const& instance_call_expression = std::get<Instance_call_expression>(expression.data);

                    if (is_builtin_instance_call(statement, instance_call_expression))
                        return false;

                    Instance_call_key const key = create_instance_call_key(
                        parameters.declaration_database,
                        module_name,
                        instance_call_expression,
                        statement
                    );

                    iris::Statement& mutable_statement = const_cast<iris::Statement&>(statement);

                    callback(
                        module_name,
                        mutable_statement,
                        expression,
                        key,
                        parameters
                    );
                }

                return false;
            };

            iris::visit_expressions(
                statement,
                instantiate_all
            );

            return false;
        };

        Scope scope;
        add_parameters_to_scope(scope, function_declaration.input_parameter_names, function_declaration.type.input_parameter_types, function_declaration.input_parameter_source_positions);

        visit_statements_using_scope(
            module_name,
            &function_declaration,
            scope,
            function_definition.statements,
            parameters.declaration_database,
            scope_callback
        );
    }

    static void instantiate_functions_in_function(
        std::string_view const module_name,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition& function_definition,
        All_passes_parameters const& parameters
    )
    {
        visit_deduced_instance_calls(
            module_name,
            function_declaration,
            function_definition,
            parameters,
            instantiate_function
        );

        if (function_definition.name == "run")
        {
            int i = 0;
        }

        std::pmr::vector<Replace_instantiate_function_parameters> replace_parameters;
        auto const gather_replacements = [&](std::string_view const module_name, iris::Statement& statement, iris::Expression const& expression, Instance_call_key const key, All_passes_parameters const& parameters)
        {
            replace_parameters.push_back({
                statement,
                find_expression_index(statement, expression),
                key
            });
        };

        visit_deduced_instance_calls(
            module_name,
            function_declaration,
            function_definition,
            parameters,
            gather_replacements
        );

        for (Replace_instantiate_function_parameters const value : replace_parameters)
        {
            replace_instantiate_function(value, parameters);
        }
    }

    static void instantiate_all_functions(
        iris::Module& core_module,
        All_passes_parameters const& parameters
    )
    {
        for (iris::Function_definition& function_definition : core_module.definitions.function_definitions)
        {
            std::optional<Function_declaration const*> const function_declaration = find_function_declaration(core_module, function_definition.name);

            if (!function_declaration.has_value())
                continue;

            if (function_declaration.value()->is_test && !parameters.is_test_mode)
                continue;

            instantiate_functions_in_function(
                core_module.name,
                *function_declaration.value(),
                function_definition,
                parameters
            );
        }
    }

    static void verify_no_instance_calls(
        std::string_view const module_name,
        iris::Function_declaration const& function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& expression,
        iris::Declaration_database const& declaration_database
    )
    {
        if (std::holds_alternative<iris::Access_expression>(expression.data))
        {
            iris::Access_expression const& access_expression = std::get<iris::Access_expression>(expression.data);

            std::optional<iris::Type_reference> const expression_type = get_expression_type(
                module_name,
                &function_declaration,
                scope,
                statement,
                expression,
                std::nullopt,
                declaration_database
            );
            if (expression_type.has_value())
            {
                std::optional<Declaration> const declaration = find_underlying_declaration(declaration_database, expression_type.value());
                if (declaration.has_value())
                {
                    assert(!std::holds_alternative<Function_constructor const*>(declaration.value().data));
                }
            }
        }
        else if (std::holds_alternative<iris::Instance_call_expression>(expression.data))
        {
            iris::Instance_call_expression const& instance_call_expression = std::get<iris::Instance_call_expression>(expression.data);
            iris::Expression const& expression = statement.expressions[instance_call_expression.left_hand_side.expression_index];
            assert(std::holds_alternative<iris::Variable_expression>(expression.data));
            iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(expression.data);
            assert(iris::is_builtin_function_name(variable_expression.name));
        }
    }

    static void verify_no_instance_calls_in_function(
        std::string_view const module_name,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition const& function_definition,
        iris::Declaration_database const& declaration_database
    )
    {
        auto const scope_callback = [&](iris::Statement const& statement, iris::compiler::Scope const& scope) -> bool
        {
            auto const verify_all = [&](iris::Expression const& expression, iris::Statement const& statement) -> bool
            {
                verify_no_instance_calls(module_name, function_declaration, scope, statement, expression, declaration_database);
                return false;
            };

            iris::visit_expressions(
                statement,
                verify_all
            );

            return false;
        };

        Scope scope;
        add_parameters_to_scope(scope, function_declaration.input_parameter_names, function_declaration.type.input_parameter_types, function_declaration.input_parameter_source_positions);

        visit_statements_using_scope(
            module_name,
            &function_declaration,
            scope,
            function_definition.statements,
            declaration_database,
            scope_callback
        );
    }

    static void verify_no_instance_calls_in_module(
        iris::Module const& core_module,
        iris::Declaration_database const& declaration_database,
        bool const is_test_mode
    )
    {
        for (iris::Function_definition const& definition : core_module.definitions.function_definitions)
        {
            std::optional<Function_declaration const*> const declaration = find_function_declaration(core_module, definition.name);
            if (!declaration.has_value())
                continue;

            if (declaration.value()->is_test && !is_test_mode)
                continue;

            verify_no_instance_calls_in_function(
                core_module.name,
                *declaration.value(),
                definition,
                declaration_database
            );
        }
    }

    static void verify_no_type_instances(
        iris::Type_reference const& type_reference,
        iris::Declaration_database const& declaration_database
    )
    {
        assert(!std::holds_alternative<Type_instance>(type_reference.data));

        if (std::holds_alternative<Custom_type_reference>(type_reference.data))
        {
            Custom_type_reference const& custom_type_reference = std::get<Custom_type_reference>(type_reference.data);
            std::optional<Declaration> const declaration = find_declaration(
                declaration_database,
                custom_type_reference.module_reference.name,
                custom_type_reference.name
            );
            if (declaration.has_value())
            {
                assert(!std::holds_alternative<Function_constructor const*>(declaration->data));
                assert(!std::holds_alternative<Type_constructor const*>(declaration->data));
            }
        }
    }

    static void verify_no_type_instances_in_function(
        iris::Function_declaration const& function_declaration,
        iris::Function_definition const& function_definition,
        iris::Declaration_database const& declaration_database
    )
    {
        auto const visitor = [&](iris::Type_reference const& type_reference) -> bool
        {
            verify_no_type_instances(type_reference, declaration_database);
            return false;
        };

        iris::visit_type_references_recursively(
            function_declaration,
            visitor
        );

        iris::visit_type_references_recursively(
            function_definition,
            visitor
        );
    }

    static void verify_no_type_instances_in_module(
        iris::Module const& core_module,
        iris::Declaration_database const& declaration_database
    )
    {
        auto const visitor = [&](std::string_view const declaration_name, iris::Type_reference const& type_reference) -> bool
        {
            verify_no_type_instances(type_reference, declaration_database);
            return false;
        };

        iris::visit_type_references_recursively_with_declaration_name(
            core_module,
            visitor
        );
    }

    void run_instantiate_pass_on_function(
        std::string_view const module_name,
        iris::Function_declaration& function_declaration,
        iris::Function_definition& function_definition,
        All_passes_parameters const& parameters
    )
    {
        instantiate_functions_in_function(module_name, function_declaration, function_definition, parameters);
        instantiate_types_in_function(parameters.declaration_database, function_declaration, function_definition, parameters.instanced_declarations);

        verify_no_instance_calls_in_function(module_name, function_declaration, function_definition, parameters.declaration_database);
        verify_no_type_instances_in_function(function_declaration, function_definition, parameters.declaration_database);
    }

    void run_instantiate_pass_on_module(
        iris::Module& core_module,
        All_passes_parameters const& parameters
    )
    {
        instantiate_all_functions(core_module, parameters);
        instantiate_all_types(core_module, parameters.declaration_database, parameters.instanced_declarations);

        verify_no_instance_calls_in_module(core_module, parameters.declaration_database, parameters.is_test_mode);
        verify_no_type_instances_in_module(core_module, parameters.declaration_database);
    }
}
