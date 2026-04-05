module;

#include <format>
#include <functional>
#include <memory_resource>
#include <optional>
#include <string>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <variant>

module h.core.declarations;

import h.common;
import h.core;
import h.core.execution_engine;
import h.core.types;

namespace h
{
    bool are_type_instances_equivalent(Type_instance const& lhs, Type_instance const& rhs)
    {
        if (lhs.type_constructor != rhs.type_constructor)
            return false;

        if (lhs.arguments.size() != rhs.arguments.size())
            return false;

        for (std::size_t argument_index = 0; argument_index < lhs.arguments.size(); ++argument_index)
        {
            Statement const& lhs_statement = lhs.arguments[argument_index];
            Statement const& rhs_statement = rhs.arguments[argument_index];

            if (lhs_statement.expressions.size() != rhs_statement.expressions.size())
                return false;
            
            for (std::size_t expression_index = 0; expression_index < lhs_statement.expressions.size(); ++expression_index)
            {
                Expression const& lhs_expression = lhs_statement.expressions[expression_index];
                Expression const& rhs_expression = rhs_statement.expressions[expression_index];

                if (lhs_expression.data != rhs_expression.data)
                    return false;
            }
        }

        return true;
    }

    Declaration_database create_declaration_database()
    {
        return {};
    }

    void add_declarations(
        Declaration_database& database,
        std::string_view const module_name,
        bool const are_export,
        std::span<h::Alias_type_declaration const> const alias_type_declarations,
        std::span<h::Enum_declaration const> const enum_declarations,
        std::span<h::Forward_declaration const> forward_declarations,
        std::span<h::Global_variable_declaration const> global_variable_declarations,
        std::span<h::Struct_declaration const> const struct_declarations,
        std::span<h::Union_declaration const> const union_declarations,
        std::span<h::Function_declaration const> const function_declarations,
        std::span<h::Function_constructor const> const function_constructors,
        std::span<h::Type_constructor const> const type_constructors
    )
    {
        Declaration_map& map = database.map[module_name.data()];

        for (Forward_declaration const& declaration : forward_declarations)
        {
            map.insert(std::make_pair(declaration.name, Declaration{ .data = &declaration, .module_name = std::pmr::string{ module_name }, .is_export = are_export}));
        }

        for (Alias_type_declaration const& declaration : alias_type_declarations)
        {
            map.insert(std::make_pair(declaration.name, Declaration{ .data = &declaration, .module_name = std::pmr::string{ module_name }, .is_export = are_export}));
        }

        for (Enum_declaration const& declaration : enum_declarations)
        {
            map.insert(std::make_pair(declaration.name, Declaration{ .data = &declaration, .module_name = std::pmr::string{ module_name }, .is_export = are_export}));
        }

        for (Function_constructor const& declaration : function_constructors)
        {
            map.insert(std::make_pair(declaration.name, Declaration{ .data = &declaration, .module_name = std::pmr::string{ module_name }, .is_export = are_export}));
        }

        for (Function_declaration const& declaration : function_declarations)
        {
            map.insert(std::make_pair(declaration.name, Declaration{ .data = &declaration, .module_name = std::pmr::string{ module_name }, .is_export = are_export}));
        }

        for (Global_variable_declaration const& declaration : global_variable_declarations)
        {
            map.insert(std::make_pair(declaration.name, Declaration{ .data = &declaration, .module_name = std::pmr::string{ module_name }, .is_export = are_export}));
        }

        for (Struct_declaration const& declaration : struct_declarations)
        {
            map.insert(std::make_pair(declaration.name, Declaration{ .data = &declaration, .module_name = std::pmr::string{ module_name }, .is_export = are_export}));
        }

        for (Type_constructor const& declaration : type_constructors)
        {
            map.insert(std::make_pair(declaration.name, Declaration{ .data = &declaration, .module_name = std::pmr::string{ module_name }, .is_export = are_export}));
        }

        for (Union_declaration const& declaration : union_declarations)
        {
            map.insert(std::make_pair(declaration.name, Declaration{ .data = &declaration, .module_name = std::pmr::string{ module_name }}));
        }
    }

    void add_declarations(
        Declaration_database& database,
        std::string_view const module_name,
        bool const are_export,
        Module_declarations const& declarations
    )
    {
        add_declarations(
            database,
            module_name,
            are_export,
            declarations.alias_type_declarations,
            declarations.enum_declarations,
            declarations.forward_declarations,
            declarations.global_variable_declarations,
            declarations.struct_declarations,
            declarations.union_declarations,
            declarations.function_declarations,
            declarations.function_constructors,
            declarations.type_constructors
        );
    }

    void add_declarations(
        Declaration_database& database,
        Module const& core_module
    )
    {
        add_declarations(
            database,
            core_module.name,
            true,
            core_module.export_declarations
        );

        add_declarations(
            database,
            core_module.name,
            false,
            core_module.internal_declarations
        );
    }

    void add_struct_declaration(
        Declaration_database& database,
        std::string_view const module_name,
        bool const is_export,
        h::Struct_declaration const& declaration
    )
    {
        Declaration_map& map = database.map[module_name.data()];
        map.insert(std::make_pair(declaration.name, Declaration{ .data = &declaration, .module_name = std::pmr::string{ module_name }, .is_export = is_export}));
    }

    void add_function_declaration(
        Declaration_database& database,
        std::string_view const module_name,
        bool const is_export,
        h::Function_declaration const& declaration
    )
    {
        Declaration_map& map = database.map[module_name.data()];
        map.insert(std::make_pair(declaration.name, Declaration{ .data = &declaration, .module_name = std::pmr::string{ module_name }, .is_export = is_export}));
    }

    std::optional<Declaration> find_declaration(
        Declaration_database const& database,
        std::string_view const module_name,
        std::string_view const declaration_name
    )
    {
        auto const declaration_map_location = database.map.find(module_name);
        if (declaration_map_location == database.map.end())
            return std::nullopt;

        Declaration_map const& declaration_map = declaration_map_location->second;
        auto const declaration_location = declaration_map.find(declaration_name);
        if (declaration_location == declaration_map.end())
            return std::nullopt;

        return declaration_location->second;
    }

    std::optional<Declaration> find_declaration(
        Declaration_database const& database,
        Type_reference const& type_reference
    )
    {
        if (std::holds_alternative<Custom_type_reference>(type_reference.data))
        {
            Custom_type_reference const& custom_type_reference = std::get<Custom_type_reference>(type_reference.data);
            std::string_view const declaration_module_name = custom_type_reference.module_reference.name;
            std::string_view const declaration_name = custom_type_reference.name;
            return find_declaration(database, declaration_module_name, declaration_name);
        }
        else if (std::holds_alternative<Type_instance>(type_reference.data))
        {
            Type_instance const& type_instance = std::get<Type_instance>(type_reference.data);
            std::string_view const declaration_module_name = type_instance.type_constructor.module_reference.name;
            std::pmr::string const mangled_name = mangle_type_instance_name(type_instance);
            std::optional<Declaration> const instanced_declaration = find_declaration(database, declaration_module_name, mangled_name);
            if (instanced_declaration.has_value())
                return instanced_declaration;

            return find_declaration(database, declaration_module_name, type_instance.type_constructor.name);
        }

        return std::nullopt;
    }

    std::optional<Declaration> find_underlying_declaration(
        Declaration_database const& database,
        std::string_view const module_name,
        std::string_view const declaration_name
    )
    {
        std::optional<Declaration> optional_declaration = find_declaration(
            database,
            module_name,
            declaration_name
        );
        if (!optional_declaration.has_value())
            return std::nullopt;

        return get_underlying_declaration(
            database,
            optional_declaration.value()
        );
    }

    std::optional<Declaration> find_underlying_declaration(
        Declaration_database const& database,
        Type_reference const& type_reference
    )
    {
        std::optional<Declaration> optional_declaration = find_declaration(
            database,
            type_reference
        );
        if (!optional_declaration.has_value())
            return std::nullopt;

        return get_underlying_declaration(
            database,
            optional_declaration.value()
        );
    }

    std::optional<Declaration> find_declaration_using_import_alias(
        Declaration_database const& database,
        h::Module const& core_module,
        std::string_view const import_alias_name,
        std::string_view const declaration_name
    )
    {
        auto const location = std::find_if(
            core_module.dependencies.alias_imports.begin(),
            core_module.dependencies.alias_imports.end(),
            [import_alias_name](Import_module_with_alias const& alias_import) -> bool { return alias_import.alias == import_alias_name; }
        );
        if (location == core_module.dependencies.alias_imports.end())
            return std::nullopt;

        return find_declaration(
            database,
            location->module_name,
            declaration_name
        );
    }

    std::optional<Declaration> find_underlying_declaration_using_import_alias(
        Declaration_database const& database,
        h::Module const& core_module,
        std::string_view const import_alias_name,
        std::string_view const declaration_name
    )
    {
        std::optional<Declaration> optional_declaration = find_declaration_using_import_alias(
            database,
            core_module,
            import_alias_name,
            declaration_name
        );
        if (!optional_declaration.has_value())
            return std::nullopt;

        return get_underlying_declaration(
            database,
            optional_declaration.value()
        );
    }

    std::optional<Declaration> find_declaration_in_instanced_module_declarations(
        h::Module_instanced_declarations const& declarations,
        std::string_view const module_name,
        std::string_view const declaration_name
    )
    {
        auto const create_declaration = [module_name](auto const& declaration_pointer) -> Declaration
        {
            return Declaration{
                .data = declaration_pointer,
                .module_name = std::pmr::string{ module_name },
                .is_export = false
            };
        };

        auto const find_declaration_in_list = [declaration_name, &create_declaration]<typename T>(std::pmr::deque<T> const& declaration_list) -> std::optional<Declaration>
        {
            auto const location = std::find_if(
                declaration_list.begin(),
                declaration_list.end(),
                [declaration_name](T const& declaration) -> bool { return declaration.name == declaration_name; }
            );
            if (location == declaration_list.end())
                return std::nullopt;

            return create_declaration(&*location);
        };

        if (std::optional<Declaration> declaration = find_declaration_in_list(declarations.function_declarations); declaration.has_value())
            return declaration;

        if (std::optional<Declaration> declaration = find_declaration_in_list(declarations.struct_declarations); declaration.has_value())
            return declaration;

        if (std::optional<Declaration> declaration = find_declaration_in_list(declarations.union_declarations); declaration.has_value())
            return declaration;

        return std::nullopt;
    }

    std::optional<Type_reference> get_underlying_type(
        Declaration_database const& declaration_database,
        Type_reference const& type_reference
    )
    {
        if (std::holds_alternative<Custom_type_reference>(type_reference.data))
        {
            Custom_type_reference const& data = std::get<Custom_type_reference>(type_reference.data);
            std::string_view const module_name = data.module_reference.name;

            std::optional<Declaration> const declaration = find_declaration(declaration_database, module_name, data.name);

            if (declaration.has_value() && std::holds_alternative<Alias_type_declaration const*>(declaration->data))
            {
                Alias_type_declaration const* alias_declaration = std::get<Alias_type_declaration const*>(declaration->data);

                std::optional<Type_reference> alias_type = get_underlying_type(declaration_database, *alias_declaration);
                return alias_type;
            }
            else
            {
                return type_reference;
            }
        }
        if (std::holds_alternative<Pointer_type>(type_reference.data))
        {
            Pointer_type const& data = std::get<Pointer_type>(type_reference.data);
            if (data.element_type.empty())
                return type_reference;

            std::optional<Type_reference> const underlying_element_type = get_underlying_type(declaration_database, data.element_type[0]);
            if (!underlying_element_type.has_value())
                return create_pointer_type_type_reference({}, data.is_mutable);

            return create_pointer_type_type_reference({underlying_element_type.value()}, data.is_mutable);
        }
        else
        {
            return type_reference;
        }
    }

    std::optional<Type_reference> get_underlying_type(
        Declaration_database const& declaration_database,
        std::optional<Type_reference> const& type_reference
    )
    {
        if (!type_reference.has_value())
            return std::nullopt;

        return get_underlying_type(declaration_database, type_reference.value());
    }

    std::optional<Type_reference> get_underlying_type(
        Declaration_database const& declaration_database,
        Alias_type_declaration const& declaration
    )
    {
        if (declaration.type.empty())
            return std::nullopt;

        return get_underlying_type(declaration_database, declaration.type[0]);
    }

    std::optional<Declaration> get_underlying_declaration(
        Declaration_database const& declaration_database,
        Declaration const& declaration
    )
    {
        if (std::holds_alternative<Alias_type_declaration const*>(declaration.data))
        {
            Alias_type_declaration const& alias_type_declaration = *std::get<Alias_type_declaration const*>(declaration.data);
            return get_underlying_declaration(
                declaration_database,
                alias_type_declaration
            );
        }
        else
        {
            return declaration;
        }
    }

    std::optional<Declaration> get_underlying_declaration(
        Declaration_database const& declaration_database,
        Alias_type_declaration const& declaration
    )
    {
        std::optional<Type_reference> const type_reference = get_underlying_type(declaration_database, declaration);
        if (type_reference.has_value())
        {
            if (std::holds_alternative<Custom_type_reference>(type_reference.value().data))
            {
                Custom_type_reference const& data = std::get<Custom_type_reference>(type_reference.value().data);
                std::string_view const module_name = data.module_reference.name;

                std::optional<Declaration> const underlying_declaration = find_declaration(declaration_database, module_name, data.name);
                if (underlying_declaration.has_value())
                {
                    Declaration const& underlying_declaration_value = underlying_declaration.value();
                    if (std::holds_alternative<Alias_type_declaration const*>(underlying_declaration_value.data))
                    {
                        Alias_type_declaration const* underlying_alias = std::get<Alias_type_declaration const*>(underlying_declaration_value.data);
                        return get_underlying_declaration(declaration_database, *underlying_alias);
                    }
                    else
                    {
                        return underlying_declaration;
                    }
                }
            }
            else if (std::holds_alternative<Type_instance>(type_reference.value().data))
            {
                std::optional<Declaration> const underlying_declaration = find_declaration(declaration_database, type_reference.value());
                return underlying_declaration;
            }
        }

        return std::nullopt;
    }

    Declaration_instance_storage instantiate_type_instance(
        Declaration_database const& declaration_database,
        Type_instance const& type_instance
    )
    {
        std::optional<Declaration> const declaration = find_declaration(declaration_database, type_instance.type_constructor.module_reference.name, type_instance.type_constructor.name);

        if (!declaration.has_value())
            throw std::runtime_error{"Could not find declaration when instantiating type!"};

        if (!std::holds_alternative<Type_constructor const*>(declaration.value().data))
            throw std::runtime_error{"Declaration to instantiate is not a type constructor!"};

        Type_constructor const& type_constructor = *std::get<Type_constructor const*>(declaration.value().data);

        std::pmr::polymorphic_allocator<> allocator = {}; // TODO

        h::execution_engine::Execution_engine engine = h::execution_engine::create_execution_engine(
            allocator
        );

        h::execution_engine::Value_storage const created_declaration = h::execution_engine::evaluate_type_constructor(
            engine,
            type_constructor,
            type_instance.arguments
        );

        if (std::holds_alternative<Struct_declaration>(created_declaration.data))
        {
            Struct_declaration struct_declaration = std::get<Struct_declaration>(created_declaration.data);
            struct_declaration.name = mangle_type_instance_name(type_instance);
            struct_declaration.unique_name = struct_declaration.name;

            return Declaration_instance_storage{.data = struct_declaration};
        }

        throw std::runtime_error{"Could not instantiate type instance!"};
    }

    std::pmr::string mangle_type_instance_name(
        Type_instance const& type_instance
    )
    {
        std::size_t const type_instance_hash = Type_instance_hash{}(type_instance);
        return std::pmr::string{std::format("{}@{}@{}", type_instance.type_constructor.module_reference.name, type_instance.type_constructor.name, type_instance_hash)};
    }

    std::optional<h::Custom_type_reference> unmangle_type_instance_name(
        std::string_view const name
    )
    {
        auto const first_location = std::find(name.begin(), name.end(), '@');
        if (first_location == name.end())
            return std::nullopt;

        auto const second_location = std::find(first_location + 1, name.end(), '@');
        if (second_location == name.end())
            return std::nullopt;

        std::size_t const module_name_length = std::distance(name.begin(), first_location);
        std::string_view const module_name = name.substr(0, module_name_length);

        std::size_t const type_constructor_name_length = std::distance(first_location + 1, second_location);
        std::string_view const type_constructor_name = name.substr(module_name_length + 1, type_constructor_name_length);

        return h::Custom_type_reference{
            .module_reference { .name = std::pmr::string{module_name} },
            .name = std::pmr::string{type_constructor_name}
        };
    }

    std::optional<Custom_type_reference> get_function_constructor_type_reference(
        Declaration_database const& declaration_database,
        h::Module const& core_module,
        Expression const& expression,
        Statement const& statement
    )
    {
        if (std::holds_alternative<Variable_expression>(expression.data))
        {
            Variable_expression const& variable_expression = std::get<Variable_expression>(expression.data);
            std::optional<Declaration> const declaration = find_declaration(declaration_database, core_module.name, variable_expression.name);
            if (!declaration.has_value() || !std::holds_alternative<Function_constructor const*>(declaration.value().data))
                return std::nullopt;

            return Custom_type_reference
            {
                .module_reference = {
                    .name = core_module.name
                },
                .name = variable_expression.name
            };
        }
        else if (std::holds_alternative<Access_expression>(expression.data))
        {
            Access_expression const& access_expression = std::get<Access_expression>(expression.data);
            Expression const& left_hand_side_expression = statement.expressions[access_expression.expression.expression_index];
            if (!std::holds_alternative<Variable_expression>(left_hand_side_expression.data))
                return std::nullopt;

            Variable_expression const& variable_expression = std::get<Variable_expression>(left_hand_side_expression.data);
            Import_module_with_alias const* const import_module = find_import_module_with_alias(core_module, variable_expression.name);
            if (import_module == nullptr)
                return std::nullopt;

            return Custom_type_reference
            {
                .module_reference = {
                    .name = import_module->module_name
                },
                .name = access_expression.member_name
            };
        }
        else if (std::holds_alternative<h::Instance_call_expression>(expression.data))
        {
            Instance_call_expression const& data = std::get<h::Instance_call_expression>(expression.data);

            return get_function_constructor_type_reference(
                declaration_database,
                core_module,
                statement.expressions[data.left_hand_side.expression_index],
                statement
            );
        }

        return std::nullopt;
    }

    Instance_call_key create_instance_call_key(
        Declaration_database const& declaration_database,
        h::Module const& core_module,
        Instance_call_expression const& expression,
        Statement const& statement
    )
    {
        std::optional<Custom_type_reference> const custom_type_reference = get_function_constructor_type_reference(
            declaration_database,
            core_module,
            statement.expressions[expression.left_hand_side.expression_index],
            statement
        );
        if (!custom_type_reference.has_value())
            throw std::runtime_error("Could not find function constructor for instance call");

        return Instance_call_key
        {
            .module_name = std::pmr::string{ custom_type_reference->module_reference.name },
            .function_constructor_name = custom_type_reference->name,
            .arguments = expression.arguments
        };
    }

    Function_constructor const* get_function_constructor(
        Declaration_database const& declaration_database,
        std::string_view const module_name,
        std::string_view const declaration_name
    )
    {
        std::optional<Declaration> const declaration = find_declaration(declaration_database, module_name, declaration_name);
        if (!declaration.has_value() || !std::holds_alternative<Function_constructor const*>(declaration.value().data))
            return nullptr;

        return std::get<Function_constructor const*>(declaration.value().data);
    }

    Function_constructor const* get_function_constructor(
        Declaration_database const& declaration_database,
        Custom_type_reference const& custom_type_reference
    )
    {
        return get_function_constructor(declaration_database, custom_type_reference.module_reference.name, custom_type_reference.name);
    }

    Function_constructor const* get_function_constructor(
        Declaration_database const& declaration_database,
        h::Module const& core_module,
        Expression const& expression,
        Statement const& statement
    )
    {
        std::optional<Custom_type_reference> const custom_type_reference = get_function_constructor_type_reference(
            declaration_database,
            core_module,
            expression,
            statement
        );
        if (!custom_type_reference.has_value())
            return nullptr;

        return get_function_constructor(declaration_database, *custom_type_reference);
    }

    std::optional<Function_expression> get_instance_call_function_expression(
        Declaration_database const& declaration_database,
        Module const& core_module,
        Instance_call_key const& key
    )
    {
        std::string const mangled_name = mangle_instance_call_name(key);
        std::optional<Declaration> const declaration = find_declaration(declaration_database, key.module_name, mangled_name);
        if (!declaration.has_value() || !std::holds_alternative<Function_declaration const*>(declaration->data))
            return std::nullopt;

        Function_declaration const& function_declaration = *std::get<Function_declaration const*>(declaration->data);

        for (Function_definition const& definition : core_module.definitions.function_definitions)
        {
            if (definition.name == function_declaration.name)
            {
                return Function_expression{ .declaration = function_declaration, .definition = definition };
            }
        }

        return std::nullopt;
    }

    std::string mangle_instance_call_name(
        Instance_call_key const& key
    )
    {
        std::size_t const instance_call_hash = Instance_call_key_hash{}(key);
        return std::format("{}@{}@{}", key.module_name, key.function_constructor_name, instance_call_hash);
    }

    std::optional<h::Custom_type_reference> unmangle_instance_call_name(
        std::string_view const name
    )
    {
        auto const first_location = std::find(name.begin(), name.end(), '@');
        if (first_location == name.end())
            return std::nullopt;

        auto const second_location = std::find(first_location + 1, name.end(), '@');
        if (second_location == name.end())
            return std::nullopt;

        std::size_t const module_name_length = std::distance(name.begin(), first_location);
        std::string_view const module_name = name.substr(0, module_name_length);

        std::size_t const function_constructor_name_length = std::distance(first_location + 1, second_location);
        std::string_view const function_constructor_name = name.substr(module_name_length + 1, function_constructor_name_length);

        return h::Custom_type_reference{
            .module_reference { .name = std::pmr::string{module_name} },
            .name = std::pmr::string{function_constructor_name}
        };
    }

    Function_expression create_instance_call_expression_value(
        Function_constructor const& function_constructor,
        std::span<Statement const> const arguments,
        Instance_call_key const& key
    )
    {
        std::pmr::polymorphic_allocator<> allocator = {}; // TODO

        h::execution_engine::Execution_engine engine = h::execution_engine::create_execution_engine(
            allocator
        );

        Function_expression function_expression = h::execution_engine::evaluate_function_constructor(
            engine,
            function_constructor,
            arguments
        );

        Function_declaration& function_declaration = function_expression.declaration;

        std::string const mangled_name = mangle_instance_call_name(key);
        function_declaration.name = std::pmr::string{mangled_name};
        function_declaration.unique_name = function_declaration.name;
        function_expression.definition.name = function_declaration.name;

        return function_expression;
    }

    std::pair<Instance_call_key, Function_expression> create_instance_call_expression_value(
        Declaration_database const& declaration_database,
        h::Module const& core_module,
        Instance_call_expression const& expression,
        Statement const& statement
    )
    {
        Function_constructor const* function_constructor = get_function_constructor(
            declaration_database,
            core_module,
            statement.expressions[expression.left_hand_side.expression_index],
            statement
        );
        if (function_constructor == nullptr)
            throw std::runtime_error{ "Could not find function constructor!" };

        Instance_call_key const key = create_instance_call_key(
            declaration_database,
            core_module,
            expression,
            statement
        );

        Function_expression const function_expression = create_instance_call_expression_value(
            *function_constructor,
            expression.arguments,
            key
        );
        return std::make_pair(key, function_expression);
    }

    std::optional<std::string_view> get_declaration_unique_name(
        Declaration const& declaration
    )
    {
        if (std::holds_alternative<Alias_type_declaration const*>(declaration.data))
        {
            Alias_type_declaration const& data = *std::get<Alias_type_declaration const*>(declaration.data);
            if (data.unique_name.has_value())
                return data.unique_name.value();
        }
        else if (std::holds_alternative<Enum_declaration const*>(declaration.data))
        {
            Enum_declaration const& data = *std::get<Enum_declaration const*>(declaration.data);
            if (data.unique_name.has_value())
                return data.unique_name.value();
        }
        else if (std::holds_alternative<Forward_declaration const*>(declaration.data))
        {
            Forward_declaration const& data = *std::get<Forward_declaration const*>(declaration.data);
            if (data.unique_name.has_value())
                return data.unique_name.value();
        }
        else if (std::holds_alternative<Function_declaration const*>(declaration.data))
        {
            Function_declaration const& data = *std::get<Function_declaration const*>(declaration.data);
            if (data.unique_name.has_value())
                return data.unique_name.value();
        }
        else if (std::holds_alternative<Global_variable_declaration const*>(declaration.data))
        {
            Global_variable_declaration const& data = *std::get<Global_variable_declaration const*>(declaration.data);
            if (data.unique_name.has_value())
                return data.unique_name.value();
        }
        else if (std::holds_alternative<Struct_declaration const*>(declaration.data))
        {
            Struct_declaration const& data = *std::get<Struct_declaration const*>(declaration.data);
            if (data.unique_name.has_value())
                return data.unique_name.value();
        }
        else if (std::holds_alternative<Union_declaration const*>(declaration.data))
        {
            Union_declaration const& data = *std::get<Union_declaration const*>(declaration.data);
            if (data.unique_name.has_value())
                return data.unique_name.value();
        }

        return std::nullopt;
    }

    std::string_view get_declaration_name(
        Declaration const& declaration
    )
    {
        std::string_view name;

        std::visit([&](auto const& data) -> void {
            name = data->name;
        }, declaration.data);

        return name;
    }

    std::optional<h::Source_range_location> get_declaration_source_location(
        Declaration const& declaration
    )
    {
       std::optional<h::Source_range_location> source_location;

        std::visit([&](auto const& data) -> void {
            source_location = data->source_location;
        }, declaration.data);

        return source_location;
    }

    void visit_declarations(
        Declaration_database const& database,
        std::string_view const module_name,
        std::function<bool(Declaration const& declaration)> const& visitor
    )
    {
        auto const location = database.map.find(module_name);
        if (location == database.map.end())
            return;

        for (auto const& pair : location->second)
        {
            bool const done = visitor(pair.second);
            if (done)
                return;
        }
    }

    bool is_enum_type(
        Declaration_database const& declaration_database,
        Type_reference const& type
    )
    {
        std::optional<Declaration> const declaration = find_underlying_declaration(
            declaration_database,
            type
        );
        if (!declaration.has_value())
            return false;
        
        return std::holds_alternative<Enum_declaration const*>(declaration->data);
    }
}
