module;

#include <xxhash.h>

module h.core.hash;

import std;

import h.common;
import h.core;

namespace h
{
    static inline void update_hash(
        XXH64_state_t* const state,
        XXH_NOESCAPE void const* const input,
        size_t const size
    );

    void update_hash_with_declaration(
        XXH64_state_t* const state,
        h::Module const& core_module,
        std::string_view const declaration_name
    )
    {
        {
            std::optional<h::Function_declaration const*> const declaration = find_function_declaration(core_module, declaration_name);
            if (declaration)
            {
                update_hash(state, *declaration.value());
                return;
            }
        }

        {
            std::optional<h::Alias_type_declaration const*> const declaration = find_alias_type_declaration(core_module, declaration_name);
            if (declaration)
            {
                update_hash(state, *declaration.value());
                return;
            }
        }

        {
            std::optional<h::Enum_declaration const*> const declaration = find_enum_declaration(core_module, declaration_name);
            if (declaration)
            {
                update_hash(state, *declaration.value());
                return;
            }
        }

        {
            std::optional<h::Struct_declaration const*> const declaration = find_struct_declaration(core_module, declaration_name);
            if (declaration)
            {
                update_hash(state, *declaration.value());
                return;
            }
        }

        {
            std::optional<h::Union_declaration const*> const declaration = find_union_declaration(core_module, declaration_name);
            if (declaration)
            {
                update_hash(state, *declaration.value());
                return;
            }
        }
    }

    static inline void update_hash(
        XXH64_state_t* const state,
        XXH_NOESCAPE void const* const input,
        size_t const size
    )
    {
        if (XXH64_update(state, input, size) == XXH_ERROR)
            h::common::print_message_and_exit("Could not update xxhash state!");
    }

    void update_hash(
        XXH64_state_t* const state,
        std::string_view const string
    )
    {
        update_hash(state, string.data(), string.size());
    }

    void update_hash(
        XXH64_state_t* const state,
        h::Function_type const& function_type
    )
    {
        for (h::Type_reference const& parameter_type : function_type.input_parameter_types)
        {
            update_hash(state, parameter_type);
        }

        for (h::Type_reference const& parameter_type : function_type.output_parameter_types)
        {
            update_hash(state, parameter_type);
        }

        update_hash(state, &function_type.is_variadic, sizeof(function_type.is_variadic));
    }

    void update_hash(
        XXH64_state_t* const state,
        h::Function_pointer_type const& function_pointer_type
    )
    {
        update_hash(state, function_pointer_type.type);
    }

    void update_hash(
        XXH64_state_t* const state,
        h::Type_reference const& type_reference
    )
    {
        if (std::holds_alternative<h::Array_slice_type>(type_reference.data))
        {
            h::Array_slice_type const& data = std::get<h::Array_slice_type>(type_reference.data);

            for (h::Type_reference const& element_type : data.element_type)
            {
                update_hash(state, element_type);
            }

            update_hash(state, &data.is_mutable, sizeof(data.is_mutable));
        }
        else if (std::holds_alternative<h::Builtin_type_reference>(type_reference.data))
        {
            h::Builtin_type_reference const& data = std::get<h::Builtin_type_reference>(type_reference.data);
            update_hash(state, data.value);
        }
        else if (std::holds_alternative<h::Constant_array_type>(type_reference.data))
        {
            h::Constant_array_type const& data = std::get<h::Constant_array_type>(type_reference.data);

            for (h::Type_reference const& value_type : data.value_type)
            {
                update_hash(state, value_type);
            }

            update_hash(state, &data.size, sizeof(data.size));
        }
        else if (std::holds_alternative<h::Custom_type_reference>(type_reference.data))
        {
            h::Custom_type_reference const& data = std::get<h::Custom_type_reference>(type_reference.data);
            update_hash(state, data.module_reference.name);
            update_hash(state, data.name);
        }
        else if (std::holds_alternative<h::Fundamental_type>(type_reference.data))
        {
            h::Fundamental_type const data = std::get<h::Fundamental_type>(type_reference.data);
            update_hash(state, &data, sizeof(data));
        }
        else if (std::holds_alternative<h::Function_pointer_type>(type_reference.data))
        {
            h::Function_pointer_type const& data = std::get<h::Function_pointer_type>(type_reference.data);
            update_hash(state, data);
        }
        else if (std::holds_alternative<h::Integer_type>(type_reference.data))
        {
            h::Integer_type const& data = std::get<h::Integer_type>(type_reference.data);
            update_hash(state, &data.number_of_bits, sizeof(data.number_of_bits));
            update_hash(state, &data.is_signed, sizeof(data.is_signed));
        }
        else if (std::holds_alternative<h::Pointer_type>(type_reference.data))
        {
            h::Pointer_type const& data = std::get<h::Pointer_type>(type_reference.data);

            for (h::Type_reference const& element_type : data.element_type)
            {
                update_hash(state, element_type);
            }

            update_hash(state, &data.is_mutable, sizeof(data.is_mutable));
        }
        else if (std::holds_alternative<h::Soa_array_type>(type_reference.data))
        {
            h::Soa_array_type const& data = std::get<h::Soa_array_type>(type_reference.data);

            for (h::Type_reference const& value_type : data.value_type)
            {
                update_hash(state, value_type);
            }

            update_hash(state, &data.size, sizeof(data.size));
        }
        else
        {
            h::common::print_message_and_exit("Hash of type reference data is not implemented!");
        }
    }

    void update_hash(
        XXH64_state_t* const state,
        h::Statement const& statement,
        h::Expression const& expression
    )
    {
        {
            std::size_t const index = expression.data.index();
            update_hash(state, &index, sizeof(index));
        }

        if (std::holds_alternative<h::Access_expression>(expression.data))
        {
            h::Access_expression const& data = std::get<h::Access_expression>(expression.data);
            update_hash(state, statement, data.expression);
            update_hash(state, data.member_name);
        }
        else if (std::holds_alternative<h::Binary_expression>(expression.data))
        {
            h::Binary_expression const& data = std::get<h::Binary_expression>(expression.data);
            update_hash(state, statement, data.left_hand_side);
            update_hash(state, statement, data.right_hand_side);
            update_hash(state, &data.operation, sizeof(data.operation));
        }
        else if (std::holds_alternative<h::Cast_expression>(expression.data))
        {
            h::Cast_expression const& data = std::get<h::Cast_expression>(expression.data);
            update_hash(state, statement, data.source);
            update_hash(state, data.destination_type);
            update_hash(state, &data.cast_type, sizeof(data.cast_type));
        }
        else if (std::holds_alternative<h::Constant_expression>(expression.data))
        {
            h::Constant_expression const& data = std::get<h::Constant_expression>(expression.data);
            update_hash(state, data.type);
            update_hash(state, data.data);
        }
        else if (std::holds_alternative<h::Constant_array_expression>(expression.data))
        {
            h::Constant_array_expression const& data = std::get<h::Constant_array_expression>(expression.data);

            for (h::Statement const& element_statement : data.array_data)
            {
                update_hash(state, element_statement);
            }
        }
        else if (std::holds_alternative<h::Instantiate_expression>(expression.data))
        {
            h::Instantiate_expression const& data = std::get<h::Instantiate_expression>(expression.data);
            update_hash(state, &data.type, sizeof(data.type));

            for (h::Instantiate_member_value_pair const& pair : data.members)
            {
                update_hash(state, pair.member_name);
                update_hash(state, statement, pair.value);
            }
        }
        else if (std::holds_alternative<h::Null_pointer_expression>(expression.data))
        {
            std::uint8_t const null_value = 0;
            update_hash(state, &null_value, sizeof(null_value));
        }
        else if (std::holds_alternative<h::Parenthesis_expression>(expression.data))
        {
            h::Parenthesis_expression const& data = std::get<h::Parenthesis_expression>(expression.data);
            update_hash(state, statement, data.expression);
        }
        else if (std::holds_alternative<h::Type_expression>(expression.data))
        {
            h::Type_expression const& data = std::get<h::Type_expression>(expression.data);
            update_hash(state, data.type);
        }
        else if (std::holds_alternative<h::Unary_expression>(expression.data))
        {
            h::Unary_expression const& data = std::get<h::Unary_expression>(expression.data);
            update_hash(state, statement, data.expression);
            update_hash(state, &data.operation, sizeof(data.operation));
        }
        else if (std::holds_alternative<h::Variable_expression>(expression.data))
        {
            h::Variable_expression const& data = std::get<h::Variable_expression>(expression.data);
            update_hash(state, data.name);
        }
        else
        {
            h::common::print_message_and_exit("Hash of expression type is not implemented!");
        }
    }

    inline void update_hash(
        XXH64_state_t* const state,
        h::Statement const& statement,
        Expression_index const expression
    )
    {
        update_hash(state, statement, statement.expressions[expression.expression_index]);
    }

    void update_hash(
        XXH64_state_t* const state,
        h::Statement const& statement
    )
    {
        for (h::Expression const& expression : statement.expressions)
        {
            update_hash(state, statement, expression);
        }
    }

    void update_hash(
        XXH64_state_t* const state,
        h::Alias_type_declaration const& declaration
    )
    {
        update_hash(state, declaration.name);

        if (declaration.unique_name.has_value())
            update_hash(state, *declaration.unique_name);

        for (h::Type_reference const& type_reference : declaration.type)
            update_hash(state, type_reference);
    }

    void update_hash(
        XXH64_state_t* const state,
        h::Enum_declaration const& declaration
    )
    {
        update_hash(state, declaration.name);

        if (declaration.unique_name.has_value())
            update_hash(state, *declaration.unique_name);

        for (h::Enum_value const& enum_value : declaration.values)
        {
            update_hash(state, enum_value.name);
            if (enum_value.value)
                update_hash(state, *enum_value.value);
        }
    }

    void update_hash(
        XXH64_state_t* const state,
        h::Struct_declaration const& declaration
    )
    {
        update_hash(state, declaration.name);

        if (declaration.unique_name.has_value())
            update_hash(state, *declaration.unique_name);

        for (h::Type_reference const& type_reference : declaration.member_types)
            update_hash(state, type_reference);

        for (std::pmr::string const& member_name : declaration.member_names)
            update_hash(state, member_name);

        for (h::Statement const& member_default_value : declaration.member_default_values)
            update_hash(state, member_default_value);

        update_hash(state, &declaration.is_packed, sizeof(declaration.is_packed));
        update_hash(state, &declaration.is_literal, sizeof(declaration.is_literal));
    }

    void update_hash(
        XXH64_state_t* const state,
        h::Union_declaration const& declaration
    )
    {
        update_hash(state, declaration.name);

        if (declaration.unique_name.has_value())
            update_hash(state, *declaration.unique_name);

        for (h::Type_reference const& type_reference : declaration.member_types)
            update_hash(state, type_reference);

        for (std::pmr::string const& member_name : declaration.member_names)
            update_hash(state, member_name);
    }

    void update_hash(
        XXH64_state_t* const state,
        h::Function_declaration const& declaration
    )
    {
        update_hash(state, declaration.name);

        if (declaration.unique_name.has_value())
            update_hash(state, *declaration.unique_name);

        update_hash(state, declaration.type);
        update_hash(state, &declaration.linkage, sizeof(declaration.linkage));
    }

    void update_hash(
        XXH64_state_t* const state,
        Type_instance const& type_instance
    )
    {
        update_hash(state, type_instance.type_constructor.module_reference.name);
        update_hash(state, type_instance.type_constructor.name);

        for (Statement const& statement : type_instance.arguments)
        {
            update_hash(state, statement);
        }
    }

    XXH64_hash_t hash_alias_type_declaration(
        XXH64_state_t* const state,
        h::Alias_type_declaration const& declaration
    )
    {
        XXH64_hash_t const seed = 0;
        if (XXH64_reset(state, seed) == XXH_ERROR)
            h::common::print_message_and_exit("Could not reset xxhash state!");

        update_hash(state, declaration);

        XXH64_hash_t const hash = XXH64_digest(state);
        return hash;
    }

    XXH64_hash_t hash_enum_declaration(
        XXH64_state_t* const state,
        h::Enum_declaration const& declaration
    )
    {
        XXH64_hash_t const seed = 0;
        if (XXH64_reset(state, seed) == XXH_ERROR)
            h::common::print_message_and_exit("Could not reset xxhash state!");

        update_hash(state, declaration);

        XXH64_hash_t const hash = XXH64_digest(state);
        return hash;
    }

    XXH64_hash_t hash_struct_declaration(
        XXH64_state_t* const state,
        h::Struct_declaration const& declaration
    )
    {
        XXH64_hash_t const seed = 0;
        if (XXH64_reset(state, seed) == XXH_ERROR)
            h::common::print_message_and_exit("Could not reset xxhash state!");

        update_hash(state, declaration);

        XXH64_hash_t const hash = XXH64_digest(state);
        return hash;
    }

    XXH64_hash_t hash_union_declaration(
        XXH64_state_t* const state,
        h::Union_declaration const& declaration
    )
    {
        XXH64_hash_t const seed = 0;
        if (XXH64_reset(state, seed) == XXH_ERROR)
            h::common::print_message_and_exit("Could not reset xxhash state!");

        update_hash(state, declaration);

        XXH64_hash_t const hash = XXH64_digest(state);
        return hash;
    }

    XXH64_hash_t hash_function_declaration(
        XXH64_state_t* const state,
        h::Function_declaration const& declaration
    )
    {
        XXH64_hash_t const seed = 0;
        if (XXH64_reset(state, seed) == XXH_ERROR)
            h::common::print_message_and_exit("Could not reset xxhash state!");

        update_hash(state, declaration);

        XXH64_hash_t const hash = XXH64_digest(state);
        return hash;
    }

    XXH64_hash_t hash_type_instance(
        XXH64_state_t* const state,
        h::Type_instance const& type_instance
    )
    {
        XXH64_hash_t const seed = 0;
        if (XXH64_reset(state, seed) == XXH_ERROR)
            h::common::print_message_and_exit("Could not reset xxhash state!");

        update_hash(state, type_instance);

        XXH64_hash_t const hash = XXH64_digest(state);
        return hash;
    }

    XXH64_hash_t hash_instance_call_key(
        XXH64_state_t* const state,
        h::Instance_call_key const& instance_call_key
    )
    {
        XXH64_hash_t const seed = 0;
        if (XXH64_reset(state, seed) == XXH_ERROR)
            h::common::print_message_and_exit("Could not reset xxhash state!");

        update_hash(state, instance_call_key.module_name);
        update_hash(state, instance_call_key.function_constructor_name);

        for (Statement const& statement : instance_call_key.arguments)
        {
            update_hash(state, statement);
        }

        XXH64_hash_t const hash = XXH64_digest(state);
        return hash;
    }

    Symbol_name_to_hash hash_module_declarations(
        h::Module const& core_module,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        XXH64_state_t* const state = XXH64_createState();
        if (state == nullptr)
            h::common::print_message_and_exit("Could not initialize xxhash state!");

        std::pmr::unordered_map<std::pmr::string, std::uint64_t> map{ output_allocator };

        for (Alias_type_declaration const& declaration : core_module.export_declarations.alias_type_declarations)
        {
            XXH64_hash_t const hash = hash_alias_type_declaration(state, declaration);
            map.insert(std::make_pair(declaration.name, hash));
        }

        for (Alias_type_declaration const& declaration : core_module.internal_declarations.alias_type_declarations)
        {
            XXH64_hash_t const hash = hash_alias_type_declaration(state, declaration);
            map.insert(std::make_pair(declaration.name, hash));
        }

        for (Enum_declaration const& declaration : core_module.export_declarations.enum_declarations)
        {
            XXH64_hash_t const hash = hash_enum_declaration(state, declaration);
            map.insert(std::make_pair(declaration.name, hash));
        }

        for (Enum_declaration const& declaration : core_module.internal_declarations.enum_declarations)
        {
            XXH64_hash_t const hash = hash_enum_declaration(state, declaration);
            map.insert(std::make_pair(declaration.name, hash));
        }

        for (Struct_declaration const& declaration : core_module.export_declarations.struct_declarations)
        {
            XXH64_hash_t const hash = hash_struct_declaration(state, declaration);
            map.insert(std::make_pair(declaration.name, hash));
        }

        for (Struct_declaration const& declaration : core_module.internal_declarations.struct_declarations)
        {
            XXH64_hash_t const hash = hash_struct_declaration(state, declaration);
            map.insert(std::make_pair(declaration.name, hash));
        }

        for (Union_declaration const& declaration : core_module.export_declarations.union_declarations)
        {
            XXH64_hash_t const hash = hash_union_declaration(state, declaration);
            map.insert(std::make_pair(declaration.name, hash));
        }

        for (Union_declaration const& declaration : core_module.internal_declarations.union_declarations)
        {
            XXH64_hash_t const hash = hash_union_declaration(state, declaration);
            map.insert(std::make_pair(declaration.name, hash));
        }

        for (Function_declaration const& declaration : core_module.export_declarations.function_declarations)
        {
            XXH64_hash_t const hash = hash_function_declaration(state, declaration);
            map.insert(std::make_pair(declaration.name, hash));
        }

        for (Function_declaration const& declaration : core_module.internal_declarations.function_declarations)
        {
            XXH64_hash_t const hash = hash_function_declaration(state, declaration);
            map.insert(std::make_pair(declaration.name, hash));
        }

        XXH64_freeState(state);

        return map;
    }
}
