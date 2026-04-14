module;

#include <xxhash.h>

export module h.core.hash;

import std;

import h.core;

namespace h
{
    void update_hash_with_declaration(
        XXH64_state_t* const state,
        h::Module const& core_module,
        std::string_view const declaration_name
    );

    void update_hash(
        XXH64_state_t* const state,
        std::string_view const string
    );

    void update_hash(
        XXH64_state_t* const state,
        h::Function_type const& function_type
    );

    void update_hash(
        XXH64_state_t* const state,
        h::Function_pointer_type const& function_pointer_type
    );

    void update_hash(
        XXH64_state_t* const state,
        h::Type_reference const& type_reference
    );

    void update_hash(
        XXH64_state_t* const state,
        h::Statement const& statement,
        h::Expression const& expression
    );

    void update_hash(
        XXH64_state_t* const state,
        h::Statement const& statement,
        Expression_index const expression
    );

    void update_hash(
        XXH64_state_t* const state,
        h::Statement const& statement
    );

    void update_hash(
        XXH64_state_t* const state,
        h::Alias_type_declaration const& declaration
    );

    void update_hash(
        XXH64_state_t* const state,
        h::Enum_declaration const& declaration
    );

    void update_hash(
        XXH64_state_t* const state,
        h::Struct_declaration const& declaration
    );

    void update_hash(
        XXH64_state_t* const state,
        h::Union_declaration const& declaration
    );

    void update_hash(
        XXH64_state_t* const state,
        h::Function_declaration const& declaration
    );

    void update_hash(
        XXH64_state_t* const state,
        Type_instance const& type_instance
    );

    export XXH64_hash_t hash_alias_type_declaration(
        XXH64_state_t* const state,
        h::Alias_type_declaration const& declaration
    );

    export XXH64_hash_t hash_enum_declaration(
        XXH64_state_t* const state,
        h::Enum_declaration const& declaration
    );

    export XXH64_hash_t hash_struct_declaration(
        XXH64_state_t* const state,
        h::Struct_declaration const& declaration
    );

    export XXH64_hash_t hash_union_declaration(
        XXH64_state_t* const state,
        h::Union_declaration const& declaration
    );

    export XXH64_hash_t hash_function_declaration(
        XXH64_state_t* const state,
        h::Function_declaration const& declaration
    );

    export XXH64_hash_t hash_type_instance(
        XXH64_state_t* const state,
        h::Type_instance const& type_instance
    );

    export XXH64_hash_t hash_instance_call_key(
        XXH64_state_t* const state,
        h::Instance_call_key const& instance_call_key
    );

    export using Symbol_name_to_hash = std::pmr::unordered_map<std::pmr::string, std::uint64_t>;

    export Symbol_name_to_hash hash_module_declarations(
        h::Module const& core_module,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export struct Type_instance_hash
    {
        using is_transparent = void;
        
        std::size_t operator()(Type_instance const& value) const noexcept
        {
            XXH64_state_t* const state = XXH64_createState();
            if (state == nullptr)
                return 0;

            hash_type_instance(state, value);
        
            XXH64_hash_t const hash = XXH64_digest(state);
            XXH64_freeState(state);

            return static_cast<std::size_t>(hash);
        }
    };

    export struct Instance_call_key_hash
    {
        using is_transparent = void;
        
        std::size_t operator()(Instance_call_key const& value) const noexcept
        {
            XXH64_state_t* const state = XXH64_createState();
            if (state == nullptr)
                return 0;

            hash_instance_call_key(state, value);
        
            XXH64_hash_t const hash = XXH64_digest(state);
            XXH64_freeState(state);

            return static_cast<std::size_t>(hash);
        }
    };
}
