module;

#include <filesystem>
#include <format>
#include <iostream>
#include <memory_resource>
#include <optional>
#include <variant>
#include <vector>

export module h.json_serializer.read_json;

import h.core;

namespace h::json
{
    export struct Stack_state
    {
        void* pointer;
        std::pmr::string type;
        std::optional<Stack_state>(*get_next_state)(Stack_state* state, std::string_view key);

        void (*set_vector_size)(Stack_state const* state, std::size_t size);
        void* (*get_element)(Stack_state const* state, std::size_t index);
        std::optional<Stack_state>(*get_next_state_element)(Stack_state* state, std::string_view key);

        void (*set_variant_type)(Stack_state* state, std::string_view type);
    };

    export template<typename Enum_type, typename Event_value>
        bool read_enum(Enum_type& output, Event_value const value)
    {
        return false;
    };

    export template<>
        bool read_enum(Fundamental_type& output, std::string_view const value)
    {
        if (value == "Bool")
        {
            output = Fundamental_type::Bool;
            return true;
        }
        else if (value == "Byte")
        {
            output = Fundamental_type::Byte;
            return true;
        }
        else if (value == "Float16")
        {
            output = Fundamental_type::Float16;
            return true;
        }
        else if (value == "Float32")
        {
            output = Fundamental_type::Float32;
            return true;
        }
        else if (value == "Float64")
        {
            output = Fundamental_type::Float64;
            return true;
        }
        else if (value == "String")
        {
            output = Fundamental_type::String;
            return true;
        }
        else if (value == "Any_type")
        {
            output = Fundamental_type::Any_type;
            return true;
        }
        else if (value == "C_bool")
        {
            output = Fundamental_type::C_bool;
            return true;
        }
        else if (value == "C_char")
        {
            output = Fundamental_type::C_char;
            return true;
        }
        else if (value == "C_schar")
        {
            output = Fundamental_type::C_schar;
            return true;
        }
        else if (value == "C_uchar")
        {
            output = Fundamental_type::C_uchar;
            return true;
        }
        else if (value == "C_short")
        {
            output = Fundamental_type::C_short;
            return true;
        }
        else if (value == "C_ushort")
        {
            output = Fundamental_type::C_ushort;
            return true;
        }
        else if (value == "C_int")
        {
            output = Fundamental_type::C_int;
            return true;
        }
        else if (value == "C_uint")
        {
            output = Fundamental_type::C_uint;
            return true;
        }
        else if (value == "C_long")
        {
            output = Fundamental_type::C_long;
            return true;
        }
        else if (value == "C_ulong")
        {
            output = Fundamental_type::C_ulong;
            return true;
        }
        else if (value == "C_longlong")
        {
            output = Fundamental_type::C_longlong;
            return true;
        }
        else if (value == "C_ulonglong")
        {
            output = Fundamental_type::C_ulonglong;
            return true;
        }
        else if (value == "C_longdouble")
        {
            output = Fundamental_type::C_longdouble;
            return true;
        }

        std::cerr << std::format("Failed to read enum 'Fundamental_type' with value '{}'\n", value);
        return false;
    }

    export template<>
        bool read_enum(Global_variable_type& output, std::string_view const value)
    {
        if (value == "Constant")
        {
            output = Global_variable_type::Constant;
            return true;
        }
        else if (value == "Mutable")
        {
            output = Global_variable_type::Mutable;
            return true;
        }
        else if (value == "Macro")
        {
            output = Global_variable_type::Macro;
            return true;
        }

        std::cerr << std::format("Failed to read enum 'Global_variable_type' with value '{}'\n", value);
        return false;
    }

    export template<>
        bool read_enum(Linkage& output, std::string_view const value)
    {
        if (value == "External")
        {
            output = Linkage::External;
            return true;
        }
        else if (value == "Private")
        {
            output = Linkage::Private;
            return true;
        }

        std::cerr << std::format("Failed to read enum 'Linkage' with value '{}'\n", value);
        return false;
    }

    export template<>
        bool read_enum(Binary_operation& output, std::string_view const value)
    {
        if (value == "Add")
        {
            output = Binary_operation::Add;
            return true;
        }
        else if (value == "Subtract")
        {
            output = Binary_operation::Subtract;
            return true;
        }
        else if (value == "Multiply")
        {
            output = Binary_operation::Multiply;
            return true;
        }
        else if (value == "Divide")
        {
            output = Binary_operation::Divide;
            return true;
        }
        else if (value == "Modulus")
        {
            output = Binary_operation::Modulus;
            return true;
        }
        else if (value == "Equal")
        {
            output = Binary_operation::Equal;
            return true;
        }
        else if (value == "Not_equal")
        {
            output = Binary_operation::Not_equal;
            return true;
        }
        else if (value == "Less_than")
        {
            output = Binary_operation::Less_than;
            return true;
        }
        else if (value == "Less_than_or_equal_to")
        {
            output = Binary_operation::Less_than_or_equal_to;
            return true;
        }
        else if (value == "Greater_than")
        {
            output = Binary_operation::Greater_than;
            return true;
        }
        else if (value == "Greater_than_or_equal_to")
        {
            output = Binary_operation::Greater_than_or_equal_to;
            return true;
        }
        else if (value == "Logical_and")
        {
            output = Binary_operation::Logical_and;
            return true;
        }
        else if (value == "Logical_or")
        {
            output = Binary_operation::Logical_or;
            return true;
        }
        else if (value == "Bitwise_and")
        {
            output = Binary_operation::Bitwise_and;
            return true;
        }
        else if (value == "Bitwise_or")
        {
            output = Binary_operation::Bitwise_or;
            return true;
        }
        else if (value == "Bitwise_xor")
        {
            output = Binary_operation::Bitwise_xor;
            return true;
        }
        else if (value == "Bit_shift_left")
        {
            output = Binary_operation::Bit_shift_left;
            return true;
        }
        else if (value == "Bit_shift_right")
        {
            output = Binary_operation::Bit_shift_right;
            return true;
        }
        else if (value == "Has")
        {
            output = Binary_operation::Has;
            return true;
        }

        std::cerr << std::format("Failed to read enum 'Binary_operation' with value '{}'\n", value);
        return false;
    }

    export template<>
        bool read_enum(Cast_type& output, std::string_view const value)
    {
        if (value == "Numeric")
        {
            output = Cast_type::Numeric;
            return true;
        }
        else if (value == "BitCast")
        {
            output = Cast_type::BitCast;
            return true;
        }

        std::cerr << std::format("Failed to read enum 'Cast_type' with value '{}'\n", value);
        return false;
    }

    export template<>
        bool read_enum(Instantiate_expression_type& output, std::string_view const value)
    {
        if (value == "Default")
        {
            output = Instantiate_expression_type::Default;
            return true;
        }
        else if (value == "Explicit")
        {
            output = Instantiate_expression_type::Explicit;
            return true;
        }
        else if (value == "Uninitialized")
        {
            output = Instantiate_expression_type::Uninitialized;
            return true;
        }
        else if (value == "Zero_initialized")
        {
            output = Instantiate_expression_type::Zero_initialized;
            return true;
        }

        std::cerr << std::format("Failed to read enum 'Instantiate_expression_type' with value '{}'\n", value);
        return false;
    }

    export template<>
        bool read_enum(Unary_operation& output, std::string_view const value)
    {
        if (value == "Not")
        {
            output = Unary_operation::Not;
            return true;
        }
        else if (value == "Bitwise_not")
        {
            output = Unary_operation::Bitwise_not;
            return true;
        }
        else if (value == "Minus")
        {
            output = Unary_operation::Minus;
            return true;
        }
        else if (value == "Pre_increment")
        {
            output = Unary_operation::Pre_increment;
            return true;
        }
        else if (value == "Post_increment")
        {
            output = Unary_operation::Post_increment;
            return true;
        }
        else if (value == "Pre_decrement")
        {
            output = Unary_operation::Pre_decrement;
            return true;
        }
        else if (value == "Post_decrement")
        {
            output = Unary_operation::Post_decrement;
            return true;
        }
        else if (value == "Indirection")
        {
            output = Unary_operation::Indirection;
            return true;
        }
        else if (value == "Address_of")
        {
            output = Unary_operation::Address_of;
            return true;
        }

        std::cerr << std::format("Failed to read enum 'Unary_operation' with value '{}'\n", value);
        return false;
    }

    export std::optional<int> get_enum_value(std::string_view const type, std::string_view const value)
    {
        if (type == "Fundamental_type")
        {
            Fundamental_type enum_value;
            read_enum(enum_value, value);
            return static_cast<int>(enum_value);
        }

        if (type == "Global_variable_type")
        {
            Global_variable_type enum_value;
            read_enum(enum_value, value);
            return static_cast<int>(enum_value);
        }

        if (type == "Linkage")
        {
            Linkage enum_value;
            read_enum(enum_value, value);
            return static_cast<int>(enum_value);
        }

        if (type == "Binary_operation")
        {
            Binary_operation enum_value;
            read_enum(enum_value, value);
            return static_cast<int>(enum_value);
        }

        if (type == "Cast_type")
        {
            Cast_type enum_value;
            read_enum(enum_value, value);
            return static_cast<int>(enum_value);
        }

        if (type == "Instantiate_expression_type")
        {
            Instantiate_expression_type enum_value;
            read_enum(enum_value, value);
            return static_cast<int>(enum_value);
        }

        if (type == "Unary_operation")
        {
            Unary_operation enum_value;
            read_enum(enum_value, value);
            return static_cast<int>(enum_value);
        }

        return {};
    }

    std::optional<Stack_state> get_next_state_vector(Stack_state* state, std::string_view const key)
    {
        if (key == "size")
        {
            return Stack_state
            {
                .pointer = state->pointer,
                .type = "vector_size",
                .get_next_state = nullptr,
            };
        }
        else if (key == "elements")
        {
            return Stack_state
            {
                .pointer = state->pointer,
                .type = "vector_elements",
                .get_next_state = nullptr
            };
        }
        else
        {
            return {};
        }
    }

    export std::optional<Stack_state> get_next_state_source_location(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_source_position(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_source_range(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_source_range_location(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_integer_type(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_array_slice_type(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_builtin_type_reference(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_function_type(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_function_pointer_type(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_null_pointer_type(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_pointer_type(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_module_reference(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_constant_array_type(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_custom_type_reference(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_type_instance(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_parameter_type(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_type_reference(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_indexed_comment(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_statement(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_global_variable_declaration(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_alias_type_declaration(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_enum_value(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_enum_declaration(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_forward_declaration(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_struct_declaration(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_union_declaration(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_function_condition(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_function_declaration(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_function_definition(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_variable_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_expression_index(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_access_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_access_array_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_assert_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_assignment_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_binary_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_block_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_break_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_call_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_cast_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_comment_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_compile_time_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_constant_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_constant_array_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_continue_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_defer_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_dereference_and_access_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_for_loop_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_function_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_instance_call_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_instance_call_key(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_condition_statement_pair(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_if_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_instantiate_member_value_pair(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_instantiate_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_invalid_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_null_pointer_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_parenthesis_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_reflection_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_return_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_struct_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_switch_case_expression_pair(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_switch_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_ternary_condition_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_type_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_unary_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_union_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_variable_declaration_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_variable_declaration_with_type_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_while_loop_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_expression(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_type_constructor_parameter(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_type_constructor(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_function_constructor_parameter(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_function_constructor(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_language_version(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_import_module_with_alias(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_module_dependencies(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_module_declarations(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_module_definitions(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_module(Stack_state* state, std::string_view const key);
    export std::optional<Stack_state> get_next_state_source_location(Stack_state* state, std::string_view const key)
    {
        h::Source_location* parent = static_cast<h::Source_location*>(state->pointer);

        if (key == "file_path")
        {
            parent->file_path = std::filesystem::path{};
            return Stack_state
            {
                .pointer = &parent->file_path.value(),
                .type = "std::filesystem::path",
                .get_next_state = nullptr,
            };
        }

        if (key == "line")
        {

            return Stack_state
            {
                .pointer = &parent->line,
                .type = "std::uint32_t",
                .get_next_state = nullptr,
            };
        }

        if (key == "column")
        {

            return Stack_state
            {
                .pointer = &parent->column,
                .type = "std::uint32_t",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_source_position(Stack_state* state, std::string_view const key)
    {
        h::Source_position* parent = static_cast<h::Source_position*>(state->pointer);

        if (key == "line")
        {

            return Stack_state
            {
                .pointer = &parent->line,
                .type = "std::uint32_t",
                .get_next_state = nullptr,
            };
        }

        if (key == "column")
        {

            return Stack_state
            {
                .pointer = &parent->column,
                .type = "std::uint32_t",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_source_range(Stack_state* state, std::string_view const key)
    {
        h::Source_range* parent = static_cast<h::Source_range*>(state->pointer);

        if (key == "start")
        {

            return Stack_state
            {
                .pointer = &parent->start,
                .type = "Source_position",
                .get_next_state = get_next_state_source_position,
            };
        }

        if (key == "end")
        {

            return Stack_state
            {
                .pointer = &parent->end,
                .type = "Source_position",
                .get_next_state = get_next_state_source_position,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_source_range_location(Stack_state* state, std::string_view const key)
    {
        h::Source_range_location* parent = static_cast<h::Source_range_location*>(state->pointer);

        if (key == "file_path")
        {
            parent->file_path = std::filesystem::path{};
            return Stack_state
            {
                .pointer = &parent->file_path.value(),
                .type = "std::filesystem::path",
                .get_next_state = nullptr,
            };
        }

        if (key == "range")
        {

            return Stack_state
            {
                .pointer = &parent->range,
                .type = "Source_range",
                .get_next_state = get_next_state_source_range,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_integer_type(Stack_state* state, std::string_view const key)
    {
        h::Integer_type* parent = static_cast<h::Integer_type*>(state->pointer);

        if (key == "number_of_bits")
        {

            return Stack_state
            {
                .pointer = &parent->number_of_bits,
                .type = "std::uint32_t",
                .get_next_state = nullptr,
            };
        }

        if (key == "is_signed")
        {

            return Stack_state
            {
                .pointer = &parent->is_signed,
                .type = "bool",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_array_slice_type(Stack_state* state, std::string_view const key)
    {
        h::Array_slice_type* parent = static_cast<h::Array_slice_type*>(state->pointer);

        if (key == "element_type")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->element_type,
                .type = "std::pmr::vector<Type_reference>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_type_reference
            };
        }

        if (key == "is_mutable")
        {

            return Stack_state
            {
                .pointer = &parent->is_mutable,
                .type = "bool",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_builtin_type_reference(Stack_state* state, std::string_view const key)
    {
        h::Builtin_type_reference* parent = static_cast<h::Builtin_type_reference*>(state->pointer);

        if (key == "value")
        {

            return Stack_state
            {
                .pointer = &parent->value,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_function_type(Stack_state* state, std::string_view const key)
    {
        h::Function_type* parent = static_cast<h::Function_type*>(state->pointer);

        if (key == "input_parameter_types")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->input_parameter_types,
                .type = "std::pmr::vector<Type_reference>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_type_reference
            };
        }

        if (key == "output_parameter_types")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->output_parameter_types,
                .type = "std::pmr::vector<Type_reference>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_type_reference
            };
        }

        if (key == "is_variadic")
        {

            return Stack_state
            {
                .pointer = &parent->is_variadic,
                .type = "bool",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_function_pointer_type(Stack_state* state, std::string_view const key)
    {
        h::Function_pointer_type* parent = static_cast<h::Function_pointer_type*>(state->pointer);

        if (key == "type")
        {

            return Stack_state
            {
                .pointer = &parent->type,
                .type = "Function_type",
                .get_next_state = get_next_state_function_type,
            };
        }

        if (key == "input_parameter_names")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<std::pmr::string>* parent = static_cast<std::pmr::vector<std::pmr::string>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<std::pmr::string>* parent = static_cast<std::pmr::vector<std::pmr::string>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->input_parameter_names,
                .type = "std::pmr::vector<std::pmr::string>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = nullptr
            };
        }

        if (key == "output_parameter_names")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<std::pmr::string>* parent = static_cast<std::pmr::vector<std::pmr::string>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<std::pmr::string>* parent = static_cast<std::pmr::vector<std::pmr::string>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->output_parameter_names,
                .type = "std::pmr::vector<std::pmr::string>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = nullptr
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_null_pointer_type(Stack_state* state, std::string_view const key)
    {
        h::Null_pointer_type* parent = static_cast<h::Null_pointer_type*>(state->pointer);

        return {};
    }

    export std::optional<Stack_state> get_next_state_pointer_type(Stack_state* state, std::string_view const key)
    {
        h::Pointer_type* parent = static_cast<h::Pointer_type*>(state->pointer);

        if (key == "element_type")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->element_type,
                .type = "std::pmr::vector<Type_reference>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_type_reference
            };
        }

        if (key == "is_mutable")
        {

            return Stack_state
            {
                .pointer = &parent->is_mutable,
                .type = "bool",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_module_reference(Stack_state* state, std::string_view const key)
    {
        h::Module_reference* parent = static_cast<h::Module_reference*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_constant_array_type(Stack_state* state, std::string_view const key)
    {
        h::Constant_array_type* parent = static_cast<h::Constant_array_type*>(state->pointer);

        if (key == "value_type")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->value_type,
                .type = "std::pmr::vector<Type_reference>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_type_reference
            };
        }

        if (key == "size")
        {

            return Stack_state
            {
                .pointer = &parent->size,
                .type = "std::uint64_t",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_custom_type_reference(Stack_state* state, std::string_view const key)
    {
        h::Custom_type_reference* parent = static_cast<h::Custom_type_reference*>(state->pointer);

        if (key == "module_reference")
        {

            return Stack_state
            {
                .pointer = &parent->module_reference,
                .type = "Module_reference",
                .get_next_state = get_next_state_module_reference,
            };
        }

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_type_instance(Stack_state* state, std::string_view const key)
    {
        h::Type_instance* parent = static_cast<h::Type_instance*>(state->pointer);

        if (key == "type_constructor")
        {

            return Stack_state
            {
                .pointer = &parent->type_constructor,
                .type = "Custom_type_reference",
                .get_next_state = get_next_state_custom_type_reference,
            };
        }

        if (key == "arguments")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->arguments,
                .type = "std::pmr::vector<Statement>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_statement
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_parameter_type(Stack_state* state, std::string_view const key)
    {
        h::Parameter_type* parent = static_cast<h::Parameter_type*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_type_reference(Stack_state* state, std::string_view const key)
    {
        h::Type_reference* parent = static_cast<h::Type_reference*>(state->pointer);

        if (key == "data")
        {
            auto const set_variant_type = [](Stack_state* state, std::string_view const type) -> void
            {
                using Variant_type = std::variant<h::Array_slice_type, h::Builtin_type_reference, h::Constant_array_type, h::Custom_type_reference, h::Fundamental_type, h::Function_pointer_type, h::Integer_type, h::Null_pointer_type, h::Parameter_type, h::Pointer_type, h::Type_instance>;
                Variant_type* pointer = static_cast<Variant_type*>(state->pointer);

                if (type == "Array_slice_type")
                {
                    *pointer = Array_slice_type{};
                    state->type = "Array_slice_type";
                    return;
                }
                if (type == "Builtin_type_reference")
                {
                    *pointer = Builtin_type_reference{};
                    state->type = "Builtin_type_reference";
                    return;
                }
                if (type == "Constant_array_type")
                {
                    *pointer = Constant_array_type{};
                    state->type = "Constant_array_type";
                    return;
                }
                if (type == "Custom_type_reference")
                {
                    *pointer = Custom_type_reference{};
                    state->type = "Custom_type_reference";
                    return;
                }
                if (type == "Fundamental_type")
                {
                    *pointer = Fundamental_type{};
                    state->type = "Fundamental_type";
                    return;
                }
                if (type == "Function_pointer_type")
                {
                    *pointer = Function_pointer_type{};
                    state->type = "Function_pointer_type";
                    return;
                }
                if (type == "Integer_type")
                {
                    *pointer = Integer_type{};
                    state->type = "Integer_type";
                    return;
                }
                if (type == "Null_pointer_type")
                {
                    *pointer = Null_pointer_type{};
                    state->type = "Null_pointer_type";
                    return;
                }
                if (type == "Parameter_type")
                {
                    *pointer = Parameter_type{};
                    state->type = "Parameter_type";
                    return;
                }
                if (type == "Pointer_type")
                {
                    *pointer = Pointer_type{};
                    state->type = "Pointer_type";
                    return;
                }
                if (type == "Type_instance")
                {
                    *pointer = Type_instance{};
                    state->type = "Type_instance";
                    return;
                }
            };

            auto const get_next_state = [](Stack_state* state, std::string_view const key) -> std::optional<Stack_state>
            {
                if (key == "type")
                {
                    return Stack_state
                    {
                        .pointer = state->pointer,
                        .type = "variant_type",
                        .get_next_state = nullptr
                    };
                }

                if (key == "value")
                {
                    auto const get_next_state_function = [&]() -> std::optional<Stack_state>(*)(Stack_state* state, std::string_view key)
                    {
                        if (state->type == "Array_slice_type")
                        {
                            return get_next_state_array_slice_type;
                        }

                        if (state->type == "Builtin_type_reference")
                        {
                            return get_next_state_builtin_type_reference;
                        }

                        if (state->type == "Constant_array_type")
                        {
                            return get_next_state_constant_array_type;
                        }

                        if (state->type == "Custom_type_reference")
                        {
                            return get_next_state_custom_type_reference;
                        }

                        if (state->type == "Fundamental_type")
                        {
                            return nullptr;
                        }

                        if (state->type == "Function_pointer_type")
                        {
                            return get_next_state_function_pointer_type;
                        }

                        if (state->type == "Integer_type")
                        {
                            return get_next_state_integer_type;
                        }

                        if (state->type == "Null_pointer_type")
                        {
                            return get_next_state_null_pointer_type;
                        }

                        if (state->type == "Parameter_type")
                        {
                            return get_next_state_parameter_type;
                        }

                        if (state->type == "Pointer_type")
                        {
                            return get_next_state_pointer_type;
                        }

                        if (state->type == "Type_instance")
                        {
                            return get_next_state_type_instance;
                        }

                        return nullptr;
                    };

                    return Stack_state
                    {
                        .pointer = state->pointer,
                        .type = "variant_value",
                        .get_next_state = get_next_state_function()
                    };
                }

                return {};
            };


            return Stack_state
            {
                .pointer = &parent->data,
                .type = "std::variant<Array_slice_type,Builtin_type_reference,Constant_array_type,Custom_type_reference,Fundamental_type,Function_pointer_type,Integer_type,Null_pointer_type,Parameter_type,Pointer_type,Type_instance>",
                .get_next_state = get_next_state,
                .set_variant_type = set_variant_type,
            };
        }

        if (key == "source_range")
        {
            parent->source_range = Source_range{};
            return Stack_state
            {
                .pointer = &parent->source_range.value(),
                .type = "Source_range",
                .get_next_state = get_next_state_source_range
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_indexed_comment(Stack_state* state, std::string_view const key)
    {
        h::Indexed_comment* parent = static_cast<h::Indexed_comment*>(state->pointer);

        if (key == "index")
        {

            return Stack_state
            {
                .pointer = &parent->index,
                .type = "std::uint64_t",
                .get_next_state = nullptr,
            };
        }

        if (key == "comment")
        {

            return Stack_state
            {
                .pointer = &parent->comment,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_statement(Stack_state* state, std::string_view const key)
    {
        h::Statement* parent = static_cast<h::Statement*>(state->pointer);

        if (key == "expressions")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Expression>* parent = static_cast<std::pmr::vector<Expression>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Expression>* parent = static_cast<std::pmr::vector<Expression>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->expressions,
                .type = "std::pmr::vector<Expression>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_expression
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_global_variable_declaration(Stack_state* state, std::string_view const key)
    {
        h::Global_variable_declaration* parent = static_cast<h::Global_variable_declaration*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "unique_name")
        {
            parent->unique_name = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->unique_name.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "type")
        {
            parent->type = Type_reference{};
            return Stack_state
            {
                .pointer = &parent->type.value(),
                .type = "Type_reference",
                .get_next_state = get_next_state_type_reference
            };
        }

        if (key == "initial_value")
        {

            return Stack_state
            {
                .pointer = &parent->initial_value,
                .type = "Statement",
                .get_next_state = get_next_state_statement,
            };
        }

        if (key == "global_type")
        {

            return Stack_state
            {
                .pointer = &parent->global_type,
                .type = "Global_variable_type",
                .get_next_state = nullptr,
            };
        }

        if (key == "comment")
        {
            parent->comment = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->comment.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "source_location")
        {
            parent->source_location = Source_range_location{};
            return Stack_state
            {
                .pointer = &parent->source_location.value(),
                .type = "Source_range_location",
                .get_next_state = get_next_state_source_range_location
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_alias_type_declaration(Stack_state* state, std::string_view const key)
    {
        h::Alias_type_declaration* parent = static_cast<h::Alias_type_declaration*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "unique_name")
        {
            parent->unique_name = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->unique_name.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "type")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->type,
                .type = "std::pmr::vector<Type_reference>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_type_reference
            };
        }

        if (key == "comment")
        {
            parent->comment = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->comment.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "source_location")
        {
            parent->source_location = Source_range_location{};
            return Stack_state
            {
                .pointer = &parent->source_location.value(),
                .type = "Source_range_location",
                .get_next_state = get_next_state_source_range_location
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_enum_value(Stack_state* state, std::string_view const key)
    {
        h::Enum_value* parent = static_cast<h::Enum_value*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "value")
        {
            parent->value = Statement{};
            return Stack_state
            {
                .pointer = &parent->value.value(),
                .type = "Statement",
                .get_next_state = get_next_state_statement
            };
        }

        if (key == "comment")
        {
            parent->comment = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->comment.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "source_location")
        {
            parent->source_location = Source_location{};
            return Stack_state
            {
                .pointer = &parent->source_location.value(),
                .type = "Source_location",
                .get_next_state = get_next_state_source_location
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_enum_declaration(Stack_state* state, std::string_view const key)
    {
        h::Enum_declaration* parent = static_cast<h::Enum_declaration*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "unique_name")
        {
            parent->unique_name = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->unique_name.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "values")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Enum_value>* parent = static_cast<std::pmr::vector<Enum_value>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Enum_value>* parent = static_cast<std::pmr::vector<Enum_value>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->values,
                .type = "std::pmr::vector<Enum_value>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_enum_value
            };
        }

        if (key == "comment")
        {
            parent->comment = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->comment.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "source_location")
        {
            parent->source_location = Source_range_location{};
            return Stack_state
            {
                .pointer = &parent->source_location.value(),
                .type = "Source_range_location",
                .get_next_state = get_next_state_source_range_location
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_forward_declaration(Stack_state* state, std::string_view const key)
    {
        h::Forward_declaration* parent = static_cast<h::Forward_declaration*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "unique_name")
        {
            parent->unique_name = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->unique_name.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "source_location")
        {
            parent->source_location = Source_range_location{};
            return Stack_state
            {
                .pointer = &parent->source_location.value(),
                .type = "Source_range_location",
                .get_next_state = get_next_state_source_range_location
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_struct_declaration(Stack_state* state, std::string_view const key)
    {
        h::Struct_declaration* parent = static_cast<h::Struct_declaration*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "unique_name")
        {
            parent->unique_name = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->unique_name.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "member_types")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->member_types,
                .type = "std::pmr::vector<Type_reference>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_type_reference
            };
        }

        if (key == "member_names")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<std::pmr::string>* parent = static_cast<std::pmr::vector<std::pmr::string>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<std::pmr::string>* parent = static_cast<std::pmr::vector<std::pmr::string>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->member_names,
                .type = "std::pmr::vector<std::pmr::string>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = nullptr
            };
        }

        if (key == "member_bit_fields")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<std::optional<std::uint32_t>>* parent = static_cast<std::pmr::vector<std::optional<std::uint32_t>>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<std::optional<std::uint32_t>>* parent = static_cast<std::pmr::vector<std::optional<std::uint32_t>>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->member_bit_fields,
                .type = "std::pmr::vector<std::optional<std::uint32_t>>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = nullptr,
            };
        }

        if (key == "member_default_values")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->member_default_values,
                .type = "std::pmr::vector<Statement>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_statement
            };
        }

        if (key == "is_packed")
        {

            return Stack_state
            {
                .pointer = &parent->is_packed,
                .type = "bool",
                .get_next_state = nullptr,
            };
        }

        if (key == "is_literal")
        {

            return Stack_state
            {
                .pointer = &parent->is_literal,
                .type = "bool",
                .get_next_state = nullptr,
            };
        }

        if (key == "comment")
        {
            parent->comment = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->comment.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "member_comments")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Indexed_comment>* parent = static_cast<std::pmr::vector<Indexed_comment>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Indexed_comment>* parent = static_cast<std::pmr::vector<Indexed_comment>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->member_comments,
                .type = "std::pmr::vector<Indexed_comment>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_indexed_comment
            };
        }

        if (key == "source_location")
        {
            parent->source_location = Source_range_location{};
            return Stack_state
            {
                .pointer = &parent->source_location.value(),
                .type = "Source_range_location",
                .get_next_state = get_next_state_source_range_location
            };
        }

        if (key == "member_source_positions")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Source_position>* parent = static_cast<std::pmr::vector<Source_position>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Source_position>* parent = static_cast<std::pmr::vector<Source_position>*>(state->pointer);
                return &((*parent)[index]);
            };
            parent->member_source_positions = std::pmr::vector<Source_position>{};
            return Stack_state
            {
                .pointer = &parent->member_source_positions.value(),
                .type = "std::pmr::vector<Source_position>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_source_position
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_union_declaration(Stack_state* state, std::string_view const key)
    {
        h::Union_declaration* parent = static_cast<h::Union_declaration*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "unique_name")
        {
            parent->unique_name = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->unique_name.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "member_types")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->member_types,
                .type = "std::pmr::vector<Type_reference>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_type_reference
            };
        }

        if (key == "member_names")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<std::pmr::string>* parent = static_cast<std::pmr::vector<std::pmr::string>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<std::pmr::string>* parent = static_cast<std::pmr::vector<std::pmr::string>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->member_names,
                .type = "std::pmr::vector<std::pmr::string>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = nullptr
            };
        }

        if (key == "comment")
        {
            parent->comment = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->comment.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "member_comments")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Indexed_comment>* parent = static_cast<std::pmr::vector<Indexed_comment>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Indexed_comment>* parent = static_cast<std::pmr::vector<Indexed_comment>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->member_comments,
                .type = "std::pmr::vector<Indexed_comment>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_indexed_comment
            };
        }

        if (key == "source_location")
        {
            parent->source_location = Source_range_location{};
            return Stack_state
            {
                .pointer = &parent->source_location.value(),
                .type = "Source_range_location",
                .get_next_state = get_next_state_source_range_location
            };
        }

        if (key == "member_source_positions")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Source_position>* parent = static_cast<std::pmr::vector<Source_position>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Source_position>* parent = static_cast<std::pmr::vector<Source_position>*>(state->pointer);
                return &((*parent)[index]);
            };
            parent->member_source_positions = std::pmr::vector<Source_position>{};
            return Stack_state
            {
                .pointer = &parent->member_source_positions.value(),
                .type = "std::pmr::vector<Source_position>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_source_position
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_function_condition(Stack_state* state, std::string_view const key)
    {
        h::Function_condition* parent = static_cast<h::Function_condition*>(state->pointer);

        if (key == "description")
        {

            return Stack_state
            {
                .pointer = &parent->description,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "condition")
        {

            return Stack_state
            {
                .pointer = &parent->condition,
                .type = "Statement",
                .get_next_state = get_next_state_statement,
            };
        }

        if (key == "source_range")
        {
            parent->source_range = Source_range{};
            return Stack_state
            {
                .pointer = &parent->source_range.value(),
                .type = "Source_range",
                .get_next_state = get_next_state_source_range
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_function_declaration(Stack_state* state, std::string_view const key)
    {
        h::Function_declaration* parent = static_cast<h::Function_declaration*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "unique_name")
        {
            parent->unique_name = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->unique_name.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "type")
        {

            return Stack_state
            {
                .pointer = &parent->type,
                .type = "Function_type",
                .get_next_state = get_next_state_function_type,
            };
        }

        if (key == "input_parameter_names")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<std::pmr::string>* parent = static_cast<std::pmr::vector<std::pmr::string>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<std::pmr::string>* parent = static_cast<std::pmr::vector<std::pmr::string>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->input_parameter_names,
                .type = "std::pmr::vector<std::pmr::string>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = nullptr
            };
        }

        if (key == "output_parameter_names")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<std::pmr::string>* parent = static_cast<std::pmr::vector<std::pmr::string>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<std::pmr::string>* parent = static_cast<std::pmr::vector<std::pmr::string>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->output_parameter_names,
                .type = "std::pmr::vector<std::pmr::string>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = nullptr
            };
        }

        if (key == "linkage")
        {

            return Stack_state
            {
                .pointer = &parent->linkage,
                .type = "Linkage",
                .get_next_state = nullptr,
            };
        }

        if (key == "is_test")
        {

            return Stack_state
            {
                .pointer = &parent->is_test,
                .type = "bool",
                .get_next_state = nullptr,
            };
        }

        if (key == "preconditions")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Function_condition>* parent = static_cast<std::pmr::vector<Function_condition>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Function_condition>* parent = static_cast<std::pmr::vector<Function_condition>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->preconditions,
                .type = "std::pmr::vector<Function_condition>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_function_condition
            };
        }

        if (key == "postconditions")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Function_condition>* parent = static_cast<std::pmr::vector<Function_condition>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Function_condition>* parent = static_cast<std::pmr::vector<Function_condition>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->postconditions,
                .type = "std::pmr::vector<Function_condition>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_function_condition
            };
        }

        if (key == "comment")
        {
            parent->comment = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->comment.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "source_location")
        {
            parent->source_location = Source_range_location{};
            return Stack_state
            {
                .pointer = &parent->source_location.value(),
                .type = "Source_range_location",
                .get_next_state = get_next_state_source_range_location
            };
        }

        if (key == "input_parameter_source_positions")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Source_position>* parent = static_cast<std::pmr::vector<Source_position>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Source_position>* parent = static_cast<std::pmr::vector<Source_position>*>(state->pointer);
                return &((*parent)[index]);
            };
            parent->input_parameter_source_positions = std::pmr::vector<Source_position>{};
            return Stack_state
            {
                .pointer = &parent->input_parameter_source_positions.value(),
                .type = "std::pmr::vector<Source_position>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_source_position
            };
        }

        if (key == "output_parameter_source_positions")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Source_position>* parent = static_cast<std::pmr::vector<Source_position>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Source_position>* parent = static_cast<std::pmr::vector<Source_position>*>(state->pointer);
                return &((*parent)[index]);
            };
            parent->output_parameter_source_positions = std::pmr::vector<Source_position>{};
            return Stack_state
            {
                .pointer = &parent->output_parameter_source_positions.value(),
                .type = "std::pmr::vector<Source_position>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_source_position
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_function_definition(Stack_state* state, std::string_view const key)
    {
        h::Function_definition* parent = static_cast<h::Function_definition*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "statements")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->statements,
                .type = "std::pmr::vector<Statement>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_statement
            };
        }

        if (key == "source_location")
        {
            parent->source_location = Source_range_location{};
            return Stack_state
            {
                .pointer = &parent->source_location.value(),
                .type = "Source_range_location",
                .get_next_state = get_next_state_source_range_location
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_variable_expression(Stack_state* state, std::string_view const key)
    {
        h::Variable_expression* parent = static_cast<h::Variable_expression*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_expression_index(Stack_state* state, std::string_view const key)
    {
        h::Expression_index* parent = static_cast<h::Expression_index*>(state->pointer);

        if (key == "expression_index")
        {

            return Stack_state
            {
                .pointer = &parent->expression_index,
                .type = "std::uint64_t",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_access_expression(Stack_state* state, std::string_view const key)
    {
        h::Access_expression* parent = static_cast<h::Access_expression*>(state->pointer);

        if (key == "expression")
        {

            return Stack_state
            {
                .pointer = &parent->expression,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        if (key == "member_name")
        {

            return Stack_state
            {
                .pointer = &parent->member_name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_access_array_expression(Stack_state* state, std::string_view const key)
    {
        h::Access_array_expression* parent = static_cast<h::Access_array_expression*>(state->pointer);

        if (key == "expression")
        {

            return Stack_state
            {
                .pointer = &parent->expression,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        if (key == "index")
        {

            return Stack_state
            {
                .pointer = &parent->index,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_assert_expression(Stack_state* state, std::string_view const key)
    {
        h::Assert_expression* parent = static_cast<h::Assert_expression*>(state->pointer);

        if (key == "message")
        {
            parent->message = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->message.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "statement")
        {

            return Stack_state
            {
                .pointer = &parent->statement,
                .type = "Statement",
                .get_next_state = get_next_state_statement,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_assignment_expression(Stack_state* state, std::string_view const key)
    {
        h::Assignment_expression* parent = static_cast<h::Assignment_expression*>(state->pointer);

        if (key == "left_hand_side")
        {

            return Stack_state
            {
                .pointer = &parent->left_hand_side,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        if (key == "right_hand_side")
        {

            return Stack_state
            {
                .pointer = &parent->right_hand_side,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        if (key == "additional_operation")
        {
            parent->additional_operation = Binary_operation{};
            return Stack_state
            {
                .pointer = &parent->additional_operation.value(),
                .type = "Binary_operation",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_binary_expression(Stack_state* state, std::string_view const key)
    {
        h::Binary_expression* parent = static_cast<h::Binary_expression*>(state->pointer);

        if (key == "left_hand_side")
        {

            return Stack_state
            {
                .pointer = &parent->left_hand_side,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        if (key == "right_hand_side")
        {

            return Stack_state
            {
                .pointer = &parent->right_hand_side,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        if (key == "operation")
        {

            return Stack_state
            {
                .pointer = &parent->operation,
                .type = "Binary_operation",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_block_expression(Stack_state* state, std::string_view const key)
    {
        h::Block_expression* parent = static_cast<h::Block_expression*>(state->pointer);

        if (key == "statements")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->statements,
                .type = "std::pmr::vector<Statement>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_statement
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_break_expression(Stack_state* state, std::string_view const key)
    {
        h::Break_expression* parent = static_cast<h::Break_expression*>(state->pointer);

        if (key == "loop_count")
        {

            return Stack_state
            {
                .pointer = &parent->loop_count,
                .type = "std::uint64_t",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_call_expression(Stack_state* state, std::string_view const key)
    {
        h::Call_expression* parent = static_cast<h::Call_expression*>(state->pointer);

        if (key == "expression")
        {

            return Stack_state
            {
                .pointer = &parent->expression,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        if (key == "arguments")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Expression_index>* parent = static_cast<std::pmr::vector<Expression_index>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Expression_index>* parent = static_cast<std::pmr::vector<Expression_index>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->arguments,
                .type = "std::pmr::vector<Expression_index>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_expression_index
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_cast_expression(Stack_state* state, std::string_view const key)
    {
        h::Cast_expression* parent = static_cast<h::Cast_expression*>(state->pointer);

        if (key == "source")
        {

            return Stack_state
            {
                .pointer = &parent->source,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        if (key == "destination_type")
        {

            return Stack_state
            {
                .pointer = &parent->destination_type,
                .type = "Type_reference",
                .get_next_state = get_next_state_type_reference,
            };
        }

        if (key == "cast_type")
        {

            return Stack_state
            {
                .pointer = &parent->cast_type,
                .type = "Cast_type",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_comment_expression(Stack_state* state, std::string_view const key)
    {
        h::Comment_expression* parent = static_cast<h::Comment_expression*>(state->pointer);

        if (key == "comment")
        {

            return Stack_state
            {
                .pointer = &parent->comment,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_compile_time_expression(Stack_state* state, std::string_view const key)
    {
        h::Compile_time_expression* parent = static_cast<h::Compile_time_expression*>(state->pointer);

        if (key == "expression")
        {

            return Stack_state
            {
                .pointer = &parent->expression,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_constant_expression(Stack_state* state, std::string_view const key)
    {
        h::Constant_expression* parent = static_cast<h::Constant_expression*>(state->pointer);

        if (key == "type")
        {

            return Stack_state
            {
                .pointer = &parent->type,
                .type = "Type_reference",
                .get_next_state = get_next_state_type_reference,
            };
        }

        if (key == "data")
        {

            return Stack_state
            {
                .pointer = &parent->data,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_constant_array_expression(Stack_state* state, std::string_view const key)
    {
        h::Constant_array_expression* parent = static_cast<h::Constant_array_expression*>(state->pointer);

        if (key == "array_data")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->array_data,
                .type = "std::pmr::vector<Statement>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_statement
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_continue_expression(Stack_state* state, std::string_view const key)
    {
        h::Continue_expression* parent = static_cast<h::Continue_expression*>(state->pointer);

        return {};
    }

    export std::optional<Stack_state> get_next_state_defer_expression(Stack_state* state, std::string_view const key)
    {
        h::Defer_expression* parent = static_cast<h::Defer_expression*>(state->pointer);

        if (key == "expression_to_defer")
        {

            return Stack_state
            {
                .pointer = &parent->expression_to_defer,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_dereference_and_access_expression(Stack_state* state, std::string_view const key)
    {
        h::Dereference_and_access_expression* parent = static_cast<h::Dereference_and_access_expression*>(state->pointer);

        if (key == "expression")
        {

            return Stack_state
            {
                .pointer = &parent->expression,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        if (key == "member_name")
        {

            return Stack_state
            {
                .pointer = &parent->member_name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_for_loop_expression(Stack_state* state, std::string_view const key)
    {
        h::For_loop_expression* parent = static_cast<h::For_loop_expression*>(state->pointer);

        if (key == "variable_name")
        {

            return Stack_state
            {
                .pointer = &parent->variable_name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "range_begin")
        {

            return Stack_state
            {
                .pointer = &parent->range_begin,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        if (key == "range_end")
        {

            return Stack_state
            {
                .pointer = &parent->range_end,
                .type = "Statement",
                .get_next_state = get_next_state_statement,
            };
        }

        if (key == "range_comparison_operation")
        {

            return Stack_state
            {
                .pointer = &parent->range_comparison_operation,
                .type = "Binary_operation",
                .get_next_state = nullptr,
            };
        }

        if (key == "step_by")
        {
            parent->step_by = Expression_index{};
            return Stack_state
            {
                .pointer = &parent->step_by.value(),
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index
            };
        }

        if (key == "then_statements")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->then_statements,
                .type = "std::pmr::vector<Statement>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_statement
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_function_expression(Stack_state* state, std::string_view const key)
    {
        h::Function_expression* parent = static_cast<h::Function_expression*>(state->pointer);

        if (key == "declaration")
        {

            return Stack_state
            {
                .pointer = &parent->declaration,
                .type = "Function_declaration",
                .get_next_state = get_next_state_function_declaration,
            };
        }

        if (key == "definition")
        {

            return Stack_state
            {
                .pointer = &parent->definition,
                .type = "Function_definition",
                .get_next_state = get_next_state_function_definition,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_instance_call_expression(Stack_state* state, std::string_view const key)
    {
        h::Instance_call_expression* parent = static_cast<h::Instance_call_expression*>(state->pointer);

        if (key == "left_hand_side")
        {

            return Stack_state
            {
                .pointer = &parent->left_hand_side,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        if (key == "arguments")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->arguments,
                .type = "std::pmr::vector<Statement>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_statement
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_instance_call_key(Stack_state* state, std::string_view const key)
    {
        h::Instance_call_key* parent = static_cast<h::Instance_call_key*>(state->pointer);

        if (key == "module_name")
        {

            return Stack_state
            {
                .pointer = &parent->module_name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "function_constructor_name")
        {

            return Stack_state
            {
                .pointer = &parent->function_constructor_name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "arguments")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->arguments,
                .type = "std::pmr::vector<Statement>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_statement
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_condition_statement_pair(Stack_state* state, std::string_view const key)
    {
        h::Condition_statement_pair* parent = static_cast<h::Condition_statement_pair*>(state->pointer);

        if (key == "condition")
        {
            parent->condition = Statement{};
            return Stack_state
            {
                .pointer = &parent->condition.value(),
                .type = "Statement",
                .get_next_state = get_next_state_statement
            };
        }

        if (key == "then_statements")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->then_statements,
                .type = "std::pmr::vector<Statement>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_statement
            };
        }

        if (key == "block_source_range")
        {
            parent->block_source_range = Source_range{};
            return Stack_state
            {
                .pointer = &parent->block_source_range.value(),
                .type = "Source_range",
                .get_next_state = get_next_state_source_range
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_if_expression(Stack_state* state, std::string_view const key)
    {
        h::If_expression* parent = static_cast<h::If_expression*>(state->pointer);

        if (key == "series")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Condition_statement_pair>* parent = static_cast<std::pmr::vector<Condition_statement_pair>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Condition_statement_pair>* parent = static_cast<std::pmr::vector<Condition_statement_pair>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->series,
                .type = "std::pmr::vector<Condition_statement_pair>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_condition_statement_pair
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_instantiate_member_value_pair(Stack_state* state, std::string_view const key)
    {
        h::Instantiate_member_value_pair* parent = static_cast<h::Instantiate_member_value_pair*>(state->pointer);

        if (key == "member_name")
        {

            return Stack_state
            {
                .pointer = &parent->member_name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "value")
        {

            return Stack_state
            {
                .pointer = &parent->value,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        if (key == "source_range")
        {
            parent->source_range = Source_range{};
            return Stack_state
            {
                .pointer = &parent->source_range.value(),
                .type = "Source_range",
                .get_next_state = get_next_state_source_range
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_instantiate_expression(Stack_state* state, std::string_view const key)
    {
        h::Instantiate_expression* parent = static_cast<h::Instantiate_expression*>(state->pointer);

        if (key == "type")
        {

            return Stack_state
            {
                .pointer = &parent->type,
                .type = "Instantiate_expression_type",
                .get_next_state = nullptr,
            };
        }

        if (key == "members")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Instantiate_member_value_pair>* parent = static_cast<std::pmr::vector<Instantiate_member_value_pair>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Instantiate_member_value_pair>* parent = static_cast<std::pmr::vector<Instantiate_member_value_pair>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->members,
                .type = "std::pmr::vector<Instantiate_member_value_pair>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_instantiate_member_value_pair
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_invalid_expression(Stack_state* state, std::string_view const key)
    {
        h::Invalid_expression* parent = static_cast<h::Invalid_expression*>(state->pointer);

        if (key == "value")
        {

            return Stack_state
            {
                .pointer = &parent->value,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_null_pointer_expression(Stack_state* state, std::string_view const key)
    {
        h::Null_pointer_expression* parent = static_cast<h::Null_pointer_expression*>(state->pointer);

        return {};
    }

    export std::optional<Stack_state> get_next_state_parenthesis_expression(Stack_state* state, std::string_view const key)
    {
        h::Parenthesis_expression* parent = static_cast<h::Parenthesis_expression*>(state->pointer);

        if (key == "expression")
        {

            return Stack_state
            {
                .pointer = &parent->expression,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_reflection_expression(Stack_state* state, std::string_view const key)
    {
        h::Reflection_expression* parent = static_cast<h::Reflection_expression*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "type_arguments")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Type_reference>* parent = static_cast<std::pmr::vector<Type_reference>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->type_arguments,
                .type = "std::pmr::vector<Type_reference>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_type_reference
            };
        }

        if (key == "arguments")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Expression_index>* parent = static_cast<std::pmr::vector<Expression_index>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Expression_index>* parent = static_cast<std::pmr::vector<Expression_index>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->arguments,
                .type = "std::pmr::vector<Expression_index>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_expression_index
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_return_expression(Stack_state* state, std::string_view const key)
    {
        h::Return_expression* parent = static_cast<h::Return_expression*>(state->pointer);

        if (key == "expression")
        {
            parent->expression = Expression_index{};
            return Stack_state
            {
                .pointer = &parent->expression.value(),
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_struct_expression(Stack_state* state, std::string_view const key)
    {
        h::Struct_expression* parent = static_cast<h::Struct_expression*>(state->pointer);

        if (key == "declaration")
        {

            return Stack_state
            {
                .pointer = &parent->declaration,
                .type = "Struct_declaration",
                .get_next_state = get_next_state_struct_declaration,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_switch_case_expression_pair(Stack_state* state, std::string_view const key)
    {
        h::Switch_case_expression_pair* parent = static_cast<h::Switch_case_expression_pair*>(state->pointer);

        if (key == "case_value")
        {
            parent->case_value = Expression_index{};
            return Stack_state
            {
                .pointer = &parent->case_value.value(),
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index
            };
        }

        if (key == "statements")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->statements,
                .type = "std::pmr::vector<Statement>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_statement
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_switch_expression(Stack_state* state, std::string_view const key)
    {
        h::Switch_expression* parent = static_cast<h::Switch_expression*>(state->pointer);

        if (key == "value")
        {

            return Stack_state
            {
                .pointer = &parent->value,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        if (key == "cases")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Switch_case_expression_pair>* parent = static_cast<std::pmr::vector<Switch_case_expression_pair>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Switch_case_expression_pair>* parent = static_cast<std::pmr::vector<Switch_case_expression_pair>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->cases,
                .type = "std::pmr::vector<Switch_case_expression_pair>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_switch_case_expression_pair
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_ternary_condition_expression(Stack_state* state, std::string_view const key)
    {
        h::Ternary_condition_expression* parent = static_cast<h::Ternary_condition_expression*>(state->pointer);

        if (key == "condition")
        {

            return Stack_state
            {
                .pointer = &parent->condition,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        if (key == "then_statement")
        {

            return Stack_state
            {
                .pointer = &parent->then_statement,
                .type = "Statement",
                .get_next_state = get_next_state_statement,
            };
        }

        if (key == "else_statement")
        {

            return Stack_state
            {
                .pointer = &parent->else_statement,
                .type = "Statement",
                .get_next_state = get_next_state_statement,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_type_expression(Stack_state* state, std::string_view const key)
    {
        h::Type_expression* parent = static_cast<h::Type_expression*>(state->pointer);

        if (key == "type")
        {

            return Stack_state
            {
                .pointer = &parent->type,
                .type = "Type_reference",
                .get_next_state = get_next_state_type_reference,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_unary_expression(Stack_state* state, std::string_view const key)
    {
        h::Unary_expression* parent = static_cast<h::Unary_expression*>(state->pointer);

        if (key == "expression")
        {

            return Stack_state
            {
                .pointer = &parent->expression,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        if (key == "operation")
        {

            return Stack_state
            {
                .pointer = &parent->operation,
                .type = "Unary_operation",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_union_expression(Stack_state* state, std::string_view const key)
    {
        h::Union_expression* parent = static_cast<h::Union_expression*>(state->pointer);

        if (key == "declaration")
        {

            return Stack_state
            {
                .pointer = &parent->declaration,
                .type = "Union_declaration",
                .get_next_state = get_next_state_union_declaration,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_variable_declaration_expression(Stack_state* state, std::string_view const key)
    {
        h::Variable_declaration_expression* parent = static_cast<h::Variable_declaration_expression*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "is_mutable")
        {

            return Stack_state
            {
                .pointer = &parent->is_mutable,
                .type = "bool",
                .get_next_state = nullptr,
            };
        }

        if (key == "right_hand_side")
        {

            return Stack_state
            {
                .pointer = &parent->right_hand_side,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_variable_declaration_with_type_expression(Stack_state* state, std::string_view const key)
    {
        h::Variable_declaration_with_type_expression* parent = static_cast<h::Variable_declaration_with_type_expression*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "is_mutable")
        {

            return Stack_state
            {
                .pointer = &parent->is_mutable,
                .type = "bool",
                .get_next_state = nullptr,
            };
        }

        if (key == "type")
        {

            return Stack_state
            {
                .pointer = &parent->type,
                .type = "Type_reference",
                .get_next_state = get_next_state_type_reference,
            };
        }

        if (key == "right_hand_side")
        {

            return Stack_state
            {
                .pointer = &parent->right_hand_side,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_while_loop_expression(Stack_state* state, std::string_view const key)
    {
        h::While_loop_expression* parent = static_cast<h::While_loop_expression*>(state->pointer);

        if (key == "condition")
        {

            return Stack_state
            {
                .pointer = &parent->condition,
                .type = "Statement",
                .get_next_state = get_next_state_statement,
            };
        }

        if (key == "then_statements")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->then_statements,
                .type = "std::pmr::vector<Statement>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_statement
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_expression(Stack_state* state, std::string_view const key)
    {
        h::Expression* parent = static_cast<h::Expression*>(state->pointer);

        if (key == "data")
        {
            auto const set_variant_type = [](Stack_state* state, std::string_view const type) -> void
            {
                using Variant_type = std::variant<h::Access_expression, h::Access_array_expression, h::Assert_expression, h::Assignment_expression, h::Binary_expression, h::Block_expression, h::Break_expression, h::Call_expression, h::Cast_expression, h::Comment_expression, h::Compile_time_expression, h::Constant_expression, h::Constant_array_expression, h::Continue_expression, h::Defer_expression, h::Dereference_and_access_expression, h::For_loop_expression, h::Function_expression, h::Instance_call_expression, h::If_expression, h::Instantiate_expression, h::Invalid_expression, h::Null_pointer_expression, h::Parenthesis_expression, h::Reflection_expression, h::Return_expression, h::Struct_expression, h::Switch_expression, h::Ternary_condition_expression, h::Type_expression, h::Unary_expression, h::Union_expression, h::Variable_declaration_expression, h::Variable_declaration_with_type_expression, h::Variable_expression, h::While_loop_expression>;
                Variant_type* pointer = static_cast<Variant_type*>(state->pointer);

                if (type == "Access_expression")
                {
                    *pointer = Access_expression{};
                    state->type = "Access_expression";
                    return;
                }
                if (type == "Access_array_expression")
                {
                    *pointer = Access_array_expression{};
                    state->type = "Access_array_expression";
                    return;
                }
                if (type == "Assert_expression")
                {
                    *pointer = Assert_expression{};
                    state->type = "Assert_expression";
                    return;
                }
                if (type == "Assignment_expression")
                {
                    *pointer = Assignment_expression{};
                    state->type = "Assignment_expression";
                    return;
                }
                if (type == "Binary_expression")
                {
                    *pointer = Binary_expression{};
                    state->type = "Binary_expression";
                    return;
                }
                if (type == "Block_expression")
                {
                    *pointer = Block_expression{};
                    state->type = "Block_expression";
                    return;
                }
                if (type == "Break_expression")
                {
                    *pointer = Break_expression{};
                    state->type = "Break_expression";
                    return;
                }
                if (type == "Call_expression")
                {
                    *pointer = Call_expression{};
                    state->type = "Call_expression";
                    return;
                }
                if (type == "Cast_expression")
                {
                    *pointer = Cast_expression{};
                    state->type = "Cast_expression";
                    return;
                }
                if (type == "Comment_expression")
                {
                    *pointer = Comment_expression{};
                    state->type = "Comment_expression";
                    return;
                }
                if (type == "Compile_time_expression")
                {
                    *pointer = Compile_time_expression{};
                    state->type = "Compile_time_expression";
                    return;
                }
                if (type == "Constant_expression")
                {
                    *pointer = Constant_expression{};
                    state->type = "Constant_expression";
                    return;
                }
                if (type == "Constant_array_expression")
                {
                    *pointer = Constant_array_expression{};
                    state->type = "Constant_array_expression";
                    return;
                }
                if (type == "Continue_expression")
                {
                    *pointer = Continue_expression{};
                    state->type = "Continue_expression";
                    return;
                }
                if (type == "Defer_expression")
                {
                    *pointer = Defer_expression{};
                    state->type = "Defer_expression";
                    return;
                }
                if (type == "Dereference_and_access_expression")
                {
                    *pointer = Dereference_and_access_expression{};
                    state->type = "Dereference_and_access_expression";
                    return;
                }
                if (type == "For_loop_expression")
                {
                    *pointer = For_loop_expression{};
                    state->type = "For_loop_expression";
                    return;
                }
                if (type == "Function_expression")
                {
                    *pointer = Function_expression{};
                    state->type = "Function_expression";
                    return;
                }
                if (type == "Instance_call_expression")
                {
                    *pointer = Instance_call_expression{};
                    state->type = "Instance_call_expression";
                    return;
                }
                if (type == "If_expression")
                {
                    *pointer = If_expression{};
                    state->type = "If_expression";
                    return;
                }
                if (type == "Instantiate_expression")
                {
                    *pointer = Instantiate_expression{};
                    state->type = "Instantiate_expression";
                    return;
                }
                if (type == "Invalid_expression")
                {
                    *pointer = Invalid_expression{};
                    state->type = "Invalid_expression";
                    return;
                }
                if (type == "Null_pointer_expression")
                {
                    *pointer = Null_pointer_expression{};
                    state->type = "Null_pointer_expression";
                    return;
                }
                if (type == "Parenthesis_expression")
                {
                    *pointer = Parenthesis_expression{};
                    state->type = "Parenthesis_expression";
                    return;
                }
                if (type == "Reflection_expression")
                {
                    *pointer = Reflection_expression{};
                    state->type = "Reflection_expression";
                    return;
                }
                if (type == "Return_expression")
                {
                    *pointer = Return_expression{};
                    state->type = "Return_expression";
                    return;
                }
                if (type == "Struct_expression")
                {
                    *pointer = Struct_expression{};
                    state->type = "Struct_expression";
                    return;
                }
                if (type == "Switch_expression")
                {
                    *pointer = Switch_expression{};
                    state->type = "Switch_expression";
                    return;
                }
                if (type == "Ternary_condition_expression")
                {
                    *pointer = Ternary_condition_expression{};
                    state->type = "Ternary_condition_expression";
                    return;
                }
                if (type == "Type_expression")
                {
                    *pointer = Type_expression{};
                    state->type = "Type_expression";
                    return;
                }
                if (type == "Unary_expression")
                {
                    *pointer = Unary_expression{};
                    state->type = "Unary_expression";
                    return;
                }
                if (type == "Union_expression")
                {
                    *pointer = Union_expression{};
                    state->type = "Union_expression";
                    return;
                }
                if (type == "Variable_declaration_expression")
                {
                    *pointer = Variable_declaration_expression{};
                    state->type = "Variable_declaration_expression";
                    return;
                }
                if (type == "Variable_declaration_with_type_expression")
                {
                    *pointer = Variable_declaration_with_type_expression{};
                    state->type = "Variable_declaration_with_type_expression";
                    return;
                }
                if (type == "Variable_expression")
                {
                    *pointer = Variable_expression{};
                    state->type = "Variable_expression";
                    return;
                }
                if (type == "While_loop_expression")
                {
                    *pointer = While_loop_expression{};
                    state->type = "While_loop_expression";
                    return;
                }
            };

            auto const get_next_state = [](Stack_state* state, std::string_view const key) -> std::optional<Stack_state>
            {
                if (key == "type")
                {
                    return Stack_state
                    {
                        .pointer = state->pointer,
                        .type = "variant_type",
                        .get_next_state = nullptr
                    };
                }

                if (key == "value")
                {
                    auto const get_next_state_function = [&]() -> std::optional<Stack_state>(*)(Stack_state* state, std::string_view key)
                    {
                        if (state->type == "Access_expression")
                        {
                            return get_next_state_access_expression;
                        }

                        if (state->type == "Access_array_expression")
                        {
                            return get_next_state_access_array_expression;
                        }

                        if (state->type == "Assert_expression")
                        {
                            return get_next_state_assert_expression;
                        }

                        if (state->type == "Assignment_expression")
                        {
                            return get_next_state_assignment_expression;
                        }

                        if (state->type == "Binary_expression")
                        {
                            return get_next_state_binary_expression;
                        }

                        if (state->type == "Block_expression")
                        {
                            return get_next_state_block_expression;
                        }

                        if (state->type == "Break_expression")
                        {
                            return get_next_state_break_expression;
                        }

                        if (state->type == "Call_expression")
                        {
                            return get_next_state_call_expression;
                        }

                        if (state->type == "Cast_expression")
                        {
                            return get_next_state_cast_expression;
                        }

                        if (state->type == "Comment_expression")
                        {
                            return get_next_state_comment_expression;
                        }

                        if (state->type == "Compile_time_expression")
                        {
                            return get_next_state_compile_time_expression;
                        }

                        if (state->type == "Constant_expression")
                        {
                            return get_next_state_constant_expression;
                        }

                        if (state->type == "Constant_array_expression")
                        {
                            return get_next_state_constant_array_expression;
                        }

                        if (state->type == "Continue_expression")
                        {
                            return get_next_state_continue_expression;
                        }

                        if (state->type == "Defer_expression")
                        {
                            return get_next_state_defer_expression;
                        }

                        if (state->type == "Dereference_and_access_expression")
                        {
                            return get_next_state_dereference_and_access_expression;
                        }

                        if (state->type == "For_loop_expression")
                        {
                            return get_next_state_for_loop_expression;
                        }

                        if (state->type == "Function_expression")
                        {
                            return get_next_state_function_expression;
                        }

                        if (state->type == "Instance_call_expression")
                        {
                            return get_next_state_instance_call_expression;
                        }

                        if (state->type == "If_expression")
                        {
                            return get_next_state_if_expression;
                        }

                        if (state->type == "Instantiate_expression")
                        {
                            return get_next_state_instantiate_expression;
                        }

                        if (state->type == "Invalid_expression")
                        {
                            return get_next_state_invalid_expression;
                        }

                        if (state->type == "Null_pointer_expression")
                        {
                            return get_next_state_null_pointer_expression;
                        }

                        if (state->type == "Parenthesis_expression")
                        {
                            return get_next_state_parenthesis_expression;
                        }

                        if (state->type == "Reflection_expression")
                        {
                            return get_next_state_reflection_expression;
                        }

                        if (state->type == "Return_expression")
                        {
                            return get_next_state_return_expression;
                        }

                        if (state->type == "Struct_expression")
                        {
                            return get_next_state_struct_expression;
                        }

                        if (state->type == "Switch_expression")
                        {
                            return get_next_state_switch_expression;
                        }

                        if (state->type == "Ternary_condition_expression")
                        {
                            return get_next_state_ternary_condition_expression;
                        }

                        if (state->type == "Type_expression")
                        {
                            return get_next_state_type_expression;
                        }

                        if (state->type == "Unary_expression")
                        {
                            return get_next_state_unary_expression;
                        }

                        if (state->type == "Union_expression")
                        {
                            return get_next_state_union_expression;
                        }

                        if (state->type == "Variable_declaration_expression")
                        {
                            return get_next_state_variable_declaration_expression;
                        }

                        if (state->type == "Variable_declaration_with_type_expression")
                        {
                            return get_next_state_variable_declaration_with_type_expression;
                        }

                        if (state->type == "Variable_expression")
                        {
                            return get_next_state_variable_expression;
                        }

                        if (state->type == "While_loop_expression")
                        {
                            return get_next_state_while_loop_expression;
                        }

                        return nullptr;
                    };

                    return Stack_state
                    {
                        .pointer = state->pointer,
                        .type = "variant_value",
                        .get_next_state = get_next_state_function()
                    };
                }

                return {};
            };


            return Stack_state
            {
                .pointer = &parent->data,
                .type = "std::variant<Access_expression,Access_array_expression,Assert_expression,Assignment_expression,Binary_expression,Block_expression,Break_expression,Call_expression,Cast_expression,Comment_expression,Compile_time_expression,Constant_expression,Constant_array_expression,Continue_expression,Defer_expression,Dereference_and_access_expression,For_loop_expression,Function_expression,Instance_call_expression,If_expression,Instantiate_expression,Invalid_expression,Null_pointer_expression,Parenthesis_expression,Reflection_expression,Return_expression,Struct_expression,Switch_expression,Ternary_condition_expression,Type_expression,Unary_expression,Union_expression,Variable_declaration_expression,Variable_declaration_with_type_expression,Variable_expression,While_loop_expression>",
                .get_next_state = get_next_state,
                .set_variant_type = set_variant_type,
            };
        }

        if (key == "source_range")
        {
            parent->source_range = Source_range{};
            return Stack_state
            {
                .pointer = &parent->source_range.value(),
                .type = "Source_range",
                .get_next_state = get_next_state_source_range
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_type_constructor_parameter(Stack_state* state, std::string_view const key)
    {
        h::Type_constructor_parameter* parent = static_cast<h::Type_constructor_parameter*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "type")
        {

            return Stack_state
            {
                .pointer = &parent->type,
                .type = "Type_reference",
                .get_next_state = get_next_state_type_reference,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_type_constructor(Stack_state* state, std::string_view const key)
    {
        h::Type_constructor* parent = static_cast<h::Type_constructor*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "parameters")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Type_constructor_parameter>* parent = static_cast<std::pmr::vector<Type_constructor_parameter>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Type_constructor_parameter>* parent = static_cast<std::pmr::vector<Type_constructor_parameter>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->parameters,
                .type = "std::pmr::vector<Type_constructor_parameter>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_type_constructor_parameter
            };
        }

        if (key == "statements")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->statements,
                .type = "std::pmr::vector<Statement>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_statement
            };
        }

        if (key == "comment")
        {
            parent->comment = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->comment.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "source_location")
        {
            parent->source_location = Source_range_location{};
            return Stack_state
            {
                .pointer = &parent->source_location.value(),
                .type = "Source_range_location",
                .get_next_state = get_next_state_source_range_location
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_function_constructor_parameter(Stack_state* state, std::string_view const key)
    {
        h::Function_constructor_parameter* parent = static_cast<h::Function_constructor_parameter*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "type")
        {

            return Stack_state
            {
                .pointer = &parent->type,
                .type = "Type_reference",
                .get_next_state = get_next_state_type_reference,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_function_constructor(Stack_state* state, std::string_view const key)
    {
        h::Function_constructor* parent = static_cast<h::Function_constructor*>(state->pointer);

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "parameters")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Function_constructor_parameter>* parent = static_cast<std::pmr::vector<Function_constructor_parameter>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Function_constructor_parameter>* parent = static_cast<std::pmr::vector<Function_constructor_parameter>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->parameters,
                .type = "std::pmr::vector<Function_constructor_parameter>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_function_constructor_parameter
            };
        }

        if (key == "statements")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Statement>* parent = static_cast<std::pmr::vector<Statement>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->statements,
                .type = "std::pmr::vector<Statement>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_statement
            };
        }

        if (key == "comment")
        {
            parent->comment = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->comment.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "source_location")
        {
            parent->source_location = Source_range_location{};
            return Stack_state
            {
                .pointer = &parent->source_location.value(),
                .type = "Source_range_location",
                .get_next_state = get_next_state_source_range_location
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_language_version(Stack_state* state, std::string_view const key)
    {
        h::Language_version* parent = static_cast<h::Language_version*>(state->pointer);

        if (key == "major")
        {

            return Stack_state
            {
                .pointer = &parent->major,
                .type = "std::uint32_t",
                .get_next_state = nullptr,
            };
        }

        if (key == "minor")
        {

            return Stack_state
            {
                .pointer = &parent->minor,
                .type = "std::uint32_t",
                .get_next_state = nullptr,
            };
        }

        if (key == "patch")
        {

            return Stack_state
            {
                .pointer = &parent->patch,
                .type = "std::uint32_t",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_import_module_with_alias(Stack_state* state, std::string_view const key)
    {
        h::Import_module_with_alias* parent = static_cast<h::Import_module_with_alias*>(state->pointer);

        if (key == "module_name")
        {

            return Stack_state
            {
                .pointer = &parent->module_name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "alias")
        {

            return Stack_state
            {
                .pointer = &parent->alias,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "usages")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<std::pmr::string>* parent = static_cast<std::pmr::vector<std::pmr::string>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<std::pmr::string>* parent = static_cast<std::pmr::vector<std::pmr::string>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->usages,
                .type = "std::pmr::vector<std::pmr::string>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = nullptr
            };
        }

        if (key == "source_range")
        {
            parent->source_range = Source_range{};
            return Stack_state
            {
                .pointer = &parent->source_range.value(),
                .type = "Source_range",
                .get_next_state = get_next_state_source_range
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_module_dependencies(Stack_state* state, std::string_view const key)
    {
        h::Module_dependencies* parent = static_cast<h::Module_dependencies*>(state->pointer);

        if (key == "alias_imports")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Import_module_with_alias>* parent = static_cast<std::pmr::vector<Import_module_with_alias>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Import_module_with_alias>* parent = static_cast<std::pmr::vector<Import_module_with_alias>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->alias_imports,
                .type = "std::pmr::vector<Import_module_with_alias>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_import_module_with_alias
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_module_declarations(Stack_state* state, std::string_view const key)
    {
        h::Module_declarations* parent = static_cast<h::Module_declarations*>(state->pointer);

        if (key == "alias_type_declarations")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Alias_type_declaration>* parent = static_cast<std::pmr::vector<Alias_type_declaration>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Alias_type_declaration>* parent = static_cast<std::pmr::vector<Alias_type_declaration>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->alias_type_declarations,
                .type = "std::pmr::vector<Alias_type_declaration>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_alias_type_declaration
            };
        }

        if (key == "enum_declarations")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Enum_declaration>* parent = static_cast<std::pmr::vector<Enum_declaration>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Enum_declaration>* parent = static_cast<std::pmr::vector<Enum_declaration>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->enum_declarations,
                .type = "std::pmr::vector<Enum_declaration>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_enum_declaration
            };
        }

        if (key == "forward_declarations")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Forward_declaration>* parent = static_cast<std::pmr::vector<Forward_declaration>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Forward_declaration>* parent = static_cast<std::pmr::vector<Forward_declaration>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->forward_declarations,
                .type = "std::pmr::vector<Forward_declaration>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_forward_declaration
            };
        }

        if (key == "global_variable_declarations")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Global_variable_declaration>* parent = static_cast<std::pmr::vector<Global_variable_declaration>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Global_variable_declaration>* parent = static_cast<std::pmr::vector<Global_variable_declaration>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->global_variable_declarations,
                .type = "std::pmr::vector<Global_variable_declaration>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_global_variable_declaration
            };
        }

        if (key == "struct_declarations")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Struct_declaration>* parent = static_cast<std::pmr::vector<Struct_declaration>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Struct_declaration>* parent = static_cast<std::pmr::vector<Struct_declaration>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->struct_declarations,
                .type = "std::pmr::vector<Struct_declaration>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_struct_declaration
            };
        }

        if (key == "union_declarations")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Union_declaration>* parent = static_cast<std::pmr::vector<Union_declaration>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Union_declaration>* parent = static_cast<std::pmr::vector<Union_declaration>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->union_declarations,
                .type = "std::pmr::vector<Union_declaration>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_union_declaration
            };
        }

        if (key == "function_declarations")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Function_declaration>* parent = static_cast<std::pmr::vector<Function_declaration>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Function_declaration>* parent = static_cast<std::pmr::vector<Function_declaration>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->function_declarations,
                .type = "std::pmr::vector<Function_declaration>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_function_declaration
            };
        }

        if (key == "function_constructors")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Function_constructor>* parent = static_cast<std::pmr::vector<Function_constructor>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Function_constructor>* parent = static_cast<std::pmr::vector<Function_constructor>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->function_constructors,
                .type = "std::pmr::vector<Function_constructor>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_function_constructor
            };
        }

        if (key == "type_constructors")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Type_constructor>* parent = static_cast<std::pmr::vector<Type_constructor>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Type_constructor>* parent = static_cast<std::pmr::vector<Type_constructor>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->type_constructors,
                .type = "std::pmr::vector<Type_constructor>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_type_constructor
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_module_definitions(Stack_state* state, std::string_view const key)
    {
        h::Module_definitions* parent = static_cast<h::Module_definitions*>(state->pointer);

        if (key == "function_definitions")
        {
            auto const set_vector_size = [](Stack_state const* const state, std::size_t const size) -> void
            {
                std::pmr::vector<Function_definition>* parent = static_cast<std::pmr::vector<Function_definition>*>(state->pointer);
                parent->resize(size);
            };

            auto const get_element = [](Stack_state const* const state, std::size_t const index) -> void*
            {
                std::pmr::vector<Function_definition>* parent = static_cast<std::pmr::vector<Function_definition>*>(state->pointer);
                return &((*parent)[index]);
            };

            return Stack_state
            {
                .pointer = &parent->function_definitions,
                .type = "std::pmr::vector<Function_definition>",
                .get_next_state = get_next_state_vector,
                .set_vector_size = set_vector_size,
                .get_element = get_element,
                .get_next_state_element = get_next_state_function_definition
            };
        }

        return {};
    }

    export std::optional<Stack_state> get_next_state_module(Stack_state* state, std::string_view const key)
    {
        h::Module* parent = static_cast<h::Module*>(state->pointer);

        if (key == "language_version")
        {

            return Stack_state
            {
                .pointer = &parent->language_version,
                .type = "Language_version",
                .get_next_state = get_next_state_language_version,
            };
        }

        if (key == "name")
        {

            return Stack_state
            {
                .pointer = &parent->name,
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "content_hash")
        {
            parent->content_hash = std::uint64_t{};
            return Stack_state
            {
                .pointer = &parent->content_hash.value(),
                .type = "std::uint64_t",
                .get_next_state = nullptr,
            };
        }

        if (key == "dependencies")
        {

            return Stack_state
            {
                .pointer = &parent->dependencies,
                .type = "Module_dependencies",
                .get_next_state = get_next_state_module_dependencies,
            };
        }

        if (key == "export_declarations")
        {

            return Stack_state
            {
                .pointer = &parent->export_declarations,
                .type = "Module_declarations",
                .get_next_state = get_next_state_module_declarations,
            };
        }

        if (key == "internal_declarations")
        {

            return Stack_state
            {
                .pointer = &parent->internal_declarations,
                .type = "Module_declarations",
                .get_next_state = get_next_state_module_declarations,
            };
        }

        if (key == "definitions")
        {

            return Stack_state
            {
                .pointer = &parent->definitions,
                .type = "Module_definitions",
                .get_next_state = get_next_state_module_definitions,
            };
        }

        if (key == "comment")
        {
            parent->comment = std::pmr::string{};
            return Stack_state
            {
                .pointer = &parent->comment.value(),
                .type = "std::pmr::string",
                .get_next_state = nullptr,
            };
        }

        if (key == "source_file_path")
        {
            parent->source_file_path = std::filesystem::path{};
            return Stack_state
            {
                .pointer = &parent->source_file_path.value(),
                .type = "std::filesystem::path",
                .get_next_state = nullptr,
            };
        }

        return {};
    }

    export template<typename Struct_type>
        Stack_state get_first_state(Struct_type* output)
    {
        if constexpr (std::is_same_v<Struct_type, h::Source_location>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Source_location",
                .get_next_state = get_next_state_source_location
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Source_position>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Source_position",
                .get_next_state = get_next_state_source_position
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Source_range>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Source_range",
                .get_next_state = get_next_state_source_range
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Source_range_location>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Source_range_location",
                .get_next_state = get_next_state_source_range_location
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Integer_type>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Integer_type",
                .get_next_state = get_next_state_integer_type
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Array_slice_type>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Array_slice_type",
                .get_next_state = get_next_state_array_slice_type
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Builtin_type_reference>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Builtin_type_reference",
                .get_next_state = get_next_state_builtin_type_reference
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Function_type>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Function_type",
                .get_next_state = get_next_state_function_type
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Function_pointer_type>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Function_pointer_type",
                .get_next_state = get_next_state_function_pointer_type
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Null_pointer_type>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Null_pointer_type",
                .get_next_state = get_next_state_null_pointer_type
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Pointer_type>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Pointer_type",
                .get_next_state = get_next_state_pointer_type
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Module_reference>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Module_reference",
                .get_next_state = get_next_state_module_reference
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Constant_array_type>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Constant_array_type",
                .get_next_state = get_next_state_constant_array_type
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Custom_type_reference>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Custom_type_reference",
                .get_next_state = get_next_state_custom_type_reference
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Type_instance>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Type_instance",
                .get_next_state = get_next_state_type_instance
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Parameter_type>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Parameter_type",
                .get_next_state = get_next_state_parameter_type
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Type_reference>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Type_reference",
                .get_next_state = get_next_state_type_reference
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Indexed_comment>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Indexed_comment",
                .get_next_state = get_next_state_indexed_comment
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Statement>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Statement",
                .get_next_state = get_next_state_statement
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Global_variable_declaration>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Global_variable_declaration",
                .get_next_state = get_next_state_global_variable_declaration
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Alias_type_declaration>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Alias_type_declaration",
                .get_next_state = get_next_state_alias_type_declaration
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Enum_value>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Enum_value",
                .get_next_state = get_next_state_enum_value
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Enum_declaration>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Enum_declaration",
                .get_next_state = get_next_state_enum_declaration
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Forward_declaration>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Forward_declaration",
                .get_next_state = get_next_state_forward_declaration
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Struct_declaration>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Struct_declaration",
                .get_next_state = get_next_state_struct_declaration
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Union_declaration>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Union_declaration",
                .get_next_state = get_next_state_union_declaration
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Function_condition>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Function_condition",
                .get_next_state = get_next_state_function_condition
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Function_declaration>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Function_declaration",
                .get_next_state = get_next_state_function_declaration
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Function_definition>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Function_definition",
                .get_next_state = get_next_state_function_definition
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Variable_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Variable_expression",
                .get_next_state = get_next_state_variable_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Expression_index>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Expression_index",
                .get_next_state = get_next_state_expression_index
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Access_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Access_expression",
                .get_next_state = get_next_state_access_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Access_array_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Access_array_expression",
                .get_next_state = get_next_state_access_array_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Assert_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Assert_expression",
                .get_next_state = get_next_state_assert_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Assignment_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Assignment_expression",
                .get_next_state = get_next_state_assignment_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Binary_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Binary_expression",
                .get_next_state = get_next_state_binary_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Block_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Block_expression",
                .get_next_state = get_next_state_block_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Break_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Break_expression",
                .get_next_state = get_next_state_break_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Call_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Call_expression",
                .get_next_state = get_next_state_call_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Cast_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Cast_expression",
                .get_next_state = get_next_state_cast_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Comment_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Comment_expression",
                .get_next_state = get_next_state_comment_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Compile_time_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Compile_time_expression",
                .get_next_state = get_next_state_compile_time_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Constant_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Constant_expression",
                .get_next_state = get_next_state_constant_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Constant_array_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Constant_array_expression",
                .get_next_state = get_next_state_constant_array_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Continue_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Continue_expression",
                .get_next_state = get_next_state_continue_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Defer_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Defer_expression",
                .get_next_state = get_next_state_defer_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Dereference_and_access_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Dereference_and_access_expression",
                .get_next_state = get_next_state_dereference_and_access_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::For_loop_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "For_loop_expression",
                .get_next_state = get_next_state_for_loop_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Function_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Function_expression",
                .get_next_state = get_next_state_function_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Instance_call_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Instance_call_expression",
                .get_next_state = get_next_state_instance_call_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Instance_call_key>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Instance_call_key",
                .get_next_state = get_next_state_instance_call_key
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Condition_statement_pair>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Condition_statement_pair",
                .get_next_state = get_next_state_condition_statement_pair
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::If_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "If_expression",
                .get_next_state = get_next_state_if_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Instantiate_member_value_pair>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Instantiate_member_value_pair",
                .get_next_state = get_next_state_instantiate_member_value_pair
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Instantiate_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Instantiate_expression",
                .get_next_state = get_next_state_instantiate_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Invalid_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Invalid_expression",
                .get_next_state = get_next_state_invalid_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Null_pointer_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Null_pointer_expression",
                .get_next_state = get_next_state_null_pointer_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Parenthesis_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Parenthesis_expression",
                .get_next_state = get_next_state_parenthesis_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Reflection_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Reflection_expression",
                .get_next_state = get_next_state_reflection_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Return_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Return_expression",
                .get_next_state = get_next_state_return_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Struct_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Struct_expression",
                .get_next_state = get_next_state_struct_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Switch_case_expression_pair>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Switch_case_expression_pair",
                .get_next_state = get_next_state_switch_case_expression_pair
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Switch_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Switch_expression",
                .get_next_state = get_next_state_switch_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Ternary_condition_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Ternary_condition_expression",
                .get_next_state = get_next_state_ternary_condition_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Type_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Type_expression",
                .get_next_state = get_next_state_type_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Unary_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Unary_expression",
                .get_next_state = get_next_state_unary_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Union_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Union_expression",
                .get_next_state = get_next_state_union_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Variable_declaration_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Variable_declaration_expression",
                .get_next_state = get_next_state_variable_declaration_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Variable_declaration_with_type_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Variable_declaration_with_type_expression",
                .get_next_state = get_next_state_variable_declaration_with_type_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::While_loop_expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "While_loop_expression",
                .get_next_state = get_next_state_while_loop_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Expression>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Expression",
                .get_next_state = get_next_state_expression
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Type_constructor_parameter>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Type_constructor_parameter",
                .get_next_state = get_next_state_type_constructor_parameter
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Type_constructor>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Type_constructor",
                .get_next_state = get_next_state_type_constructor
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Function_constructor_parameter>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Function_constructor_parameter",
                .get_next_state = get_next_state_function_constructor_parameter
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Function_constructor>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Function_constructor",
                .get_next_state = get_next_state_function_constructor
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Language_version>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Language_version",
                .get_next_state = get_next_state_language_version
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Import_module_with_alias>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Import_module_with_alias",
                .get_next_state = get_next_state_import_module_with_alias
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Module_dependencies>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Module_dependencies",
                .get_next_state = get_next_state_module_dependencies
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Module_declarations>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Module_declarations",
                .get_next_state = get_next_state_module_declarations
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Module_definitions>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Module_definitions",
                .get_next_state = get_next_state_module_definitions
            };
        }

        if constexpr (std::is_same_v<Struct_type, h::Module>)
        {
            return Stack_state
            {
                .pointer = output,
                .type = "Module",
                .get_next_state = get_next_state_module
            };
        }

    }
}
