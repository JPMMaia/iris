module;

#include <filesystem>
#include <iostream>
#include <memory_resource>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

export module h.json_serializer.write_json;

import h.core;

namespace h::json
{
    export std::string_view write_enum(Fundamental_type const value)
    {
        if (value == Fundamental_type::Bool)
        {
            return "Bool";
        }
        else if (value == Fundamental_type::Byte)
        {
            return "Byte";
        }
        else if (value == Fundamental_type::Float16)
        {
            return "Float16";
        }
        else if (value == Fundamental_type::Float32)
        {
            return "Float32";
        }
        else if (value == Fundamental_type::Float64)
        {
            return "Float64";
        }
        else if (value == Fundamental_type::String)
        {
            return "String";
        }
        else if (value == Fundamental_type::Any_type)
        {
            return "Any_type";
        }
        else if (value == Fundamental_type::C_bool)
        {
            return "C_bool";
        }
        else if (value == Fundamental_type::C_char)
        {
            return "C_char";
        }
        else if (value == Fundamental_type::C_schar)
        {
            return "C_schar";
        }
        else if (value == Fundamental_type::C_uchar)
        {
            return "C_uchar";
        }
        else if (value == Fundamental_type::C_short)
        {
            return "C_short";
        }
        else if (value == Fundamental_type::C_ushort)
        {
            return "C_ushort";
        }
        else if (value == Fundamental_type::C_int)
        {
            return "C_int";
        }
        else if (value == Fundamental_type::C_uint)
        {
            return "C_uint";
        }
        else if (value == Fundamental_type::C_long)
        {
            return "C_long";
        }
        else if (value == Fundamental_type::C_ulong)
        {
            return "C_ulong";
        }
        else if (value == Fundamental_type::C_longlong)
        {
            return "C_longlong";
        }
        else if (value == Fundamental_type::C_ulonglong)
        {
            return "C_ulonglong";
        }
        else if (value == Fundamental_type::C_longdouble)
        {
            return "C_longdouble";
        }

        throw std::runtime_error{ "Failed to write enum 'Fundamental_type'!\n" };
    }

    export std::string_view write_enum(Global_variable_type const value)
    {
        if (value == Global_variable_type::Constant)
        {
            return "Constant";
        }
        else if (value == Global_variable_type::Mutable)
        {
            return "Mutable";
        }
        else if (value == Global_variable_type::Macro)
        {
            return "Macro";
        }

        throw std::runtime_error{ "Failed to write enum 'Global_variable_type'!\n" };
    }

    export std::string_view write_enum(Linkage const value)
    {
        if (value == Linkage::External)
        {
            return "External";
        }
        else if (value == Linkage::Private)
        {
            return "Private";
        }

        throw std::runtime_error{ "Failed to write enum 'Linkage'!\n" };
    }

    export std::string_view write_enum(Binary_operation const value)
    {
        if (value == Binary_operation::Add)
        {
            return "Add";
        }
        else if (value == Binary_operation::Subtract)
        {
            return "Subtract";
        }
        else if (value == Binary_operation::Multiply)
        {
            return "Multiply";
        }
        else if (value == Binary_operation::Divide)
        {
            return "Divide";
        }
        else if (value == Binary_operation::Modulus)
        {
            return "Modulus";
        }
        else if (value == Binary_operation::Equal)
        {
            return "Equal";
        }
        else if (value == Binary_operation::Not_equal)
        {
            return "Not_equal";
        }
        else if (value == Binary_operation::Less_than)
        {
            return "Less_than";
        }
        else if (value == Binary_operation::Less_than_or_equal_to)
        {
            return "Less_than_or_equal_to";
        }
        else if (value == Binary_operation::Greater_than)
        {
            return "Greater_than";
        }
        else if (value == Binary_operation::Greater_than_or_equal_to)
        {
            return "Greater_than_or_equal_to";
        }
        else if (value == Binary_operation::Logical_and)
        {
            return "Logical_and";
        }
        else if (value == Binary_operation::Logical_or)
        {
            return "Logical_or";
        }
        else if (value == Binary_operation::Bitwise_and)
        {
            return "Bitwise_and";
        }
        else if (value == Binary_operation::Bitwise_or)
        {
            return "Bitwise_or";
        }
        else if (value == Binary_operation::Bitwise_xor)
        {
            return "Bitwise_xor";
        }
        else if (value == Binary_operation::Bit_shift_left)
        {
            return "Bit_shift_left";
        }
        else if (value == Binary_operation::Bit_shift_right)
        {
            return "Bit_shift_right";
        }
        else if (value == Binary_operation::Has)
        {
            return "Has";
        }

        throw std::runtime_error{ "Failed to write enum 'Binary_operation'!\n" };
    }

    export std::string_view write_enum(Cast_type const value)
    {
        if (value == Cast_type::Numeric)
        {
            return "Numeric";
        }
        else if (value == Cast_type::BitCast)
        {
            return "BitCast";
        }

        throw std::runtime_error{ "Failed to write enum 'Cast_type'!\n" };
    }

    export std::string_view write_enum(Instantiate_expression_type const value)
    {
        if (value == Instantiate_expression_type::Default)
        {
            return "Default";
        }
        else if (value == Instantiate_expression_type::Explicit)
        {
            return "Explicit";
        }
        else if (value == Instantiate_expression_type::Uninitialized)
        {
            return "Uninitialized";
        }
        else if (value == Instantiate_expression_type::Zero_initialized)
        {
            return "Zero_initialized";
        }

        throw std::runtime_error{ "Failed to write enum 'Instantiate_expression_type'!\n" };
    }

    export std::string_view write_enum(Unary_operation const value)
    {
        if (value == Unary_operation::Not)
        {
            return "Not";
        }
        else if (value == Unary_operation::Bitwise_not)
        {
            return "Bitwise_not";
        }
        else if (value == Unary_operation::Minus)
        {
            return "Minus";
        }
        else if (value == Unary_operation::Pre_increment)
        {
            return "Pre_increment";
        }
        else if (value == Unary_operation::Post_increment)
        {
            return "Post_increment";
        }
        else if (value == Unary_operation::Pre_decrement)
        {
            return "Pre_decrement";
        }
        else if (value == Unary_operation::Post_decrement)
        {
            return "Post_decrement";
        }
        else if (value == Unary_operation::Indirection)
        {
            return "Indirection";
        }
        else if (value == Unary_operation::Address_of)
        {
            return "Address_of";
        }

        throw std::runtime_error{ "Failed to write enum 'Unary_operation'!\n" };
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Source_location const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Source_position const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Source_range const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Source_range_location const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Integer_type const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Array_slice_type const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Builtin_type_reference const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_type const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_pointer_type const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Null_pointer_type const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Pointer_type const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Module_reference const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Constant_array_type const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Custom_type_reference const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Type_instance const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Parameter_type const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Type_reference const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Indexed_comment const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Statement const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Global_variable_declaration const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Alias_type_declaration const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Enum_value const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Enum_declaration const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Forward_declaration const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Struct_declaration const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Union_declaration const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_condition const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_declaration const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_definition const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Variable_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Expression_index const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Access_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Access_array_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Assert_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Assignment_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Binary_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Block_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Break_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Call_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Cast_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Comment_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Compile_time_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Constant_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Constant_array_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Continue_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Defer_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Dereference_and_access_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            For_loop_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Instance_call_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Instance_call_key const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Condition_statement_pair const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            If_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Instantiate_member_value_pair const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Instantiate_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Invalid_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Null_pointer_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Parenthesis_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Reflection_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Return_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Struct_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Switch_case_expression_pair const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Switch_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Ternary_condition_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Type_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Unary_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Union_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Variable_declaration_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Variable_declaration_with_type_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            While_loop_expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Expression const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Type_constructor_parameter const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Type_constructor const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_constructor_parameter const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_constructor const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Language_version const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Import_module_with_alias const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Module_dependencies const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Module_declarations const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Module_definitions const& input
        );

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Module const& input
        );

    template <typename C> struct Is_optional : std::false_type {};
    template <typename T> struct Is_optional< std::optional<T> > : std::true_type {};
    template <typename C> inline constexpr bool Is_optional_v = Is_optional<C>::value;

    export template <typename Writer_type, typename Value_type>
        void write_object(
            Writer_type& writer,
            std::pmr::vector<Value_type> const& values
        );

    export template <typename Writer_type, typename Value_type>
        void write_value(
            Writer_type& writer,
            Value_type const& value
        )
    {
        if constexpr (std::is_unsigned_v<Value_type> && sizeof(Value_type) <= 4)
        {
            writer.Uint(value);
        }
        else if constexpr (std::is_unsigned_v<Value_type>)
        {
            writer.Uint64(value);
        }
        else if constexpr (std::is_signed_v<Value_type> && sizeof(Value_type) <= 4)
        {
            writer.Int(value);
        }
        else if constexpr (std::is_signed_v<Value_type>)
        {
            writer.Int64(value);
        }
        else if constexpr (std::is_floating_point_v<Value_type>)
        {
            writer.Double(value);
        }
        else if constexpr (std::is_same_v<Value_type, std::string> || std::is_same_v<Value_type, std::pmr::string> || std::is_same_v<Value_type, std::string_view>)
        {
            writer.String(value.data(), value.size());
        }
        else if constexpr (std::is_same_v<Value_type, std::filesystem::path>)
        {
            std::string const path_string = value.generic_string();
            writer.String(path_string.data(), path_string.size());
        }
        else if constexpr (std::is_enum_v<Value_type>)
        {
            {
                std::string_view const enum_value_string = write_enum(value);
                writer.String(enum_value_string.data(), enum_value_string.size());
            }
        }
        else if constexpr (Is_optional_v<Value_type>)
        {
            if (value.has_value())
            {
                write_value(writer, value.value());
            }
            else
            {
                writer.Null();
            }
        }
        else if constexpr (std::is_class_v<Value_type>)
        {
            write_object(writer, value);
        }
    }

    export template <typename Writer_type, typename Value_type>
        void write_object(
            Writer_type& writer,
            std::pmr::vector<Value_type> const& values
        )
    {
        writer.StartObject();

        writer.Key("size");
        writer.Uint64(values.size());

        writer.Key("elements");
        writer.StartArray();
        for (Value_type const& value : values)
        {
            write_value(writer, value);
        }
        writer.EndArray(values.size());

        writer.EndObject();
    }

    export template <typename Writer_type, typename Value_type>
        void write_optional(
            Writer_type& writer,
            char const* const key,
            std::optional<Value_type> const& value
        )
    {
        if (value.has_value())
        {
            writer.Key(key);
            write_value(writer, value);
        }
    }

    export template <typename Writer_type, typename Value_type>
        void write_optional_object(
            Writer_type& writer,
            char const* const key,
            std::optional<Value_type> const& value
        )
    {
        if (value.has_value())
        {
            writer.Key(key);
            write_object(writer, value.value());
        }
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Source_location const& output
        )
    {
        writer.StartObject();
        write_optional(writer, "file_path", output.file_path);
        writer.Key("line");
        writer.Uint(output.line);
        writer.Key("column");
        writer.Uint(output.column);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Source_position const& output
        )
    {
        writer.StartObject();
        writer.Key("line");
        writer.Uint(output.line);
        writer.Key("column");
        writer.Uint(output.column);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Source_range const& output
        )
    {
        writer.StartObject();
        writer.Key("start");
        write_object(writer, output.start);
        writer.Key("end");
        write_object(writer, output.end);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Source_range_location const& output
        )
    {
        writer.StartObject();
        write_optional(writer, "file_path", output.file_path);
        writer.Key("range");
        write_object(writer, output.range);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Integer_type const& output
        )
    {
        writer.StartObject();
        writer.Key("number_of_bits");
        writer.Uint(output.number_of_bits);
        writer.Key("is_signed");
        writer.Bool(output.is_signed);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Array_slice_type const& output
        )
    {
        writer.StartObject();
        writer.Key("element_type");
        write_object(writer, output.element_type);
        writer.Key("is_mutable");
        writer.Bool(output.is_mutable);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Builtin_type_reference const& output
        )
    {
        writer.StartObject();
        writer.Key("value");
        writer.String(output.value.data(), output.value.size());
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_type const& output
        )
    {
        writer.StartObject();
        writer.Key("input_parameter_types");
        write_object(writer, output.input_parameter_types);
        writer.Key("output_parameter_types");
        write_object(writer, output.output_parameter_types);
        writer.Key("is_variadic");
        writer.Bool(output.is_variadic);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_pointer_type const& output
        )
    {
        writer.StartObject();
        writer.Key("type");
        write_object(writer, output.type);
        writer.Key("input_parameter_names");
        write_object(writer, output.input_parameter_names);
        writer.Key("output_parameter_names");
        write_object(writer, output.output_parameter_names);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Null_pointer_type const& output
        )
    {
        writer.StartObject();
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Pointer_type const& output
        )
    {
        writer.StartObject();
        writer.Key("element_type");
        write_object(writer, output.element_type);
        writer.Key("is_mutable");
        writer.Bool(output.is_mutable);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Module_reference const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Constant_array_type const& output
        )
    {
        writer.StartObject();
        writer.Key("value_type");
        write_object(writer, output.value_type);
        writer.Key("size");
        writer.Uint64(output.size);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Custom_type_reference const& output
        )
    {
        writer.StartObject();
        writer.Key("module_reference");
        write_object(writer, output.module_reference);
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Type_instance const& output
        )
    {
        writer.StartObject();
        writer.Key("type_constructor");
        write_object(writer, output.type_constructor);
        writer.Key("arguments");
        write_object(writer, output.arguments);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Parameter_type const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Type_reference const& output
        )
    {
        writer.StartObject();
        writer.Key("data");

        writer.StartObject();
        if (std::holds_alternative<Array_slice_type>(output.data))
        {
            writer.Key("type");
            writer.String("Array_slice_type");
            writer.Key("value");
            Array_slice_type const& value = std::get<Array_slice_type>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Builtin_type_reference>(output.data))
        {
            writer.Key("type");
            writer.String("Builtin_type_reference");
            writer.Key("value");
            Builtin_type_reference const& value = std::get<Builtin_type_reference>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Constant_array_type>(output.data))
        {
            writer.Key("type");
            writer.String("Constant_array_type");
            writer.Key("value");
            Constant_array_type const& value = std::get<Constant_array_type>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Custom_type_reference>(output.data))
        {
            writer.Key("type");
            writer.String("Custom_type_reference");
            writer.Key("value");
            Custom_type_reference const& value = std::get<Custom_type_reference>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Fundamental_type>(output.data))
        {
            writer.Key("type");
            writer.String("Fundamental_type");
            writer.Key("value");
            {
                Fundamental_type const& value = std::get<Fundamental_type>(output.data);
                std::string_view const enum_value_string = write_enum(value);
                writer.String(enum_value_string.data(), enum_value_string.size());
            }
        }
        else if (std::holds_alternative<Function_pointer_type>(output.data))
        {
            writer.Key("type");
            writer.String("Function_pointer_type");
            writer.Key("value");
            Function_pointer_type const& value = std::get<Function_pointer_type>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Integer_type>(output.data))
        {
            writer.Key("type");
            writer.String("Integer_type");
            writer.Key("value");
            Integer_type const& value = std::get<Integer_type>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Null_pointer_type>(output.data))
        {
            writer.Key("type");
            writer.String("Null_pointer_type");
            writer.Key("value");
            Null_pointer_type const& value = std::get<Null_pointer_type>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Parameter_type>(output.data))
        {
            writer.Key("type");
            writer.String("Parameter_type");
            writer.Key("value");
            Parameter_type const& value = std::get<Parameter_type>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Pointer_type>(output.data))
        {
            writer.Key("type");
            writer.String("Pointer_type");
            writer.Key("value");
            Pointer_type const& value = std::get<Pointer_type>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Type_instance>(output.data))
        {
            writer.Key("type");
            writer.String("Type_instance");
            writer.Key("value");
            Type_instance const& value = std::get<Type_instance>(output.data);
            write_object(writer, value);
        }
        writer.EndObject();

        write_optional_object(writer, "source_range", output.source_range);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Indexed_comment const& output
        )
    {
        writer.StartObject();
        writer.Key("index");
        writer.Uint64(output.index);
        writer.Key("comment");
        writer.String(output.comment.data(), output.comment.size());
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Statement const& output
        )
    {
        writer.StartObject();
        writer.Key("expressions");
        write_object(writer, output.expressions);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Global_variable_declaration const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        write_optional(writer, "unique_name", output.unique_name);
        write_optional_object(writer, "type", output.type);
        writer.Key("initial_value");
        write_object(writer, output.initial_value);
        writer.Key("global_type");
        {
            std::string_view const enum_value_string = write_enum(output.global_type);
            writer.String(enum_value_string.data(), enum_value_string.size());
        }
        write_optional(writer, "comment", output.comment);
        write_optional_object(writer, "source_location", output.source_location);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Alias_type_declaration const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        write_optional(writer, "unique_name", output.unique_name);
        writer.Key("type");
        write_object(writer, output.type);
        write_optional(writer, "comment", output.comment);
        write_optional_object(writer, "source_location", output.source_location);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Enum_value const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        write_optional_object(writer, "value", output.value);
        write_optional(writer, "comment", output.comment);
        write_optional_object(writer, "source_location", output.source_location);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Enum_declaration const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        write_optional(writer, "unique_name", output.unique_name);
        writer.Key("values");
        write_object(writer, output.values);
        write_optional(writer, "comment", output.comment);
        write_optional_object(writer, "source_location", output.source_location);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Forward_declaration const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        write_optional(writer, "unique_name", output.unique_name);
        write_optional_object(writer, "source_location", output.source_location);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Struct_declaration const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        write_optional(writer, "unique_name", output.unique_name);
        writer.Key("member_types");
        write_object(writer, output.member_types);
        writer.Key("member_names");
        write_object(writer, output.member_names);
        writer.Key("member_bit_fields");
        write_object(writer, output.member_bit_fields);
        writer.Key("member_default_values");
        write_object(writer, output.member_default_values);
        writer.Key("is_packed");
        writer.Bool(output.is_packed);
        writer.Key("is_literal");
        writer.Bool(output.is_literal);
        write_optional(writer, "comment", output.comment);
        writer.Key("member_comments");
        write_object(writer, output.member_comments);
        write_optional_object(writer, "source_location", output.source_location);
        write_optional(writer, "member_source_positions", output.member_source_positions);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Union_declaration const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        write_optional(writer, "unique_name", output.unique_name);
        writer.Key("member_types");
        write_object(writer, output.member_types);
        writer.Key("member_names");
        write_object(writer, output.member_names);
        write_optional(writer, "comment", output.comment);
        writer.Key("member_comments");
        write_object(writer, output.member_comments);
        write_optional_object(writer, "source_location", output.source_location);
        write_optional(writer, "member_source_positions", output.member_source_positions);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_condition const& output
        )
    {
        writer.StartObject();
        writer.Key("description");
        writer.String(output.description.data(), output.description.size());
        writer.Key("condition");
        write_object(writer, output.condition);
        write_optional_object(writer, "source_range", output.source_range);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_declaration const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        write_optional(writer, "unique_name", output.unique_name);
        writer.Key("type");
        write_object(writer, output.type);
        writer.Key("input_parameter_names");
        write_object(writer, output.input_parameter_names);
        writer.Key("output_parameter_names");
        write_object(writer, output.output_parameter_names);
        writer.Key("linkage");
        {
            std::string_view const enum_value_string = write_enum(output.linkage);
            writer.String(enum_value_string.data(), enum_value_string.size());
        }
        writer.Key("is_test");
        writer.Bool(output.is_test);
        writer.Key("preconditions");
        write_object(writer, output.preconditions);
        writer.Key("postconditions");
        write_object(writer, output.postconditions);
        write_optional(writer, "comment", output.comment);
        write_optional_object(writer, "source_location", output.source_location);
        write_optional(writer, "input_parameter_source_positions", output.input_parameter_source_positions);
        write_optional(writer, "output_parameter_source_positions", output.output_parameter_source_positions);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_definition const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        writer.Key("statements");
        write_object(writer, output.statements);
        write_optional_object(writer, "source_location", output.source_location);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Variable_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Expression_index const& output
        )
    {
        writer.StartObject();
        writer.Key("expression_index");
        writer.Uint64(output.expression_index);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Access_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("expression");
        write_object(writer, output.expression);
        writer.Key("member_name");
        writer.String(output.member_name.data(), output.member_name.size());
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Access_array_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("expression");
        write_object(writer, output.expression);
        writer.Key("index");
        write_object(writer, output.index);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Assert_expression const& output
        )
    {
        writer.StartObject();
        write_optional(writer, "message", output.message);
        writer.Key("statement");
        write_object(writer, output.statement);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Assignment_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("left_hand_side");
        write_object(writer, output.left_hand_side);
        writer.Key("right_hand_side");
        write_object(writer, output.right_hand_side);
        write_optional(writer, "additional_operation", output.additional_operation);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Binary_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("left_hand_side");
        write_object(writer, output.left_hand_side);
        writer.Key("right_hand_side");
        write_object(writer, output.right_hand_side);
        writer.Key("operation");
        {
            std::string_view const enum_value_string = write_enum(output.operation);
            writer.String(enum_value_string.data(), enum_value_string.size());
        }
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Block_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("statements");
        write_object(writer, output.statements);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Break_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("loop_count");
        writer.Uint64(output.loop_count);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Call_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("expression");
        write_object(writer, output.expression);
        writer.Key("arguments");
        write_object(writer, output.arguments);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Cast_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("source");
        write_object(writer, output.source);
        writer.Key("destination_type");
        write_object(writer, output.destination_type);
        writer.Key("cast_type");
        {
            std::string_view const enum_value_string = write_enum(output.cast_type);
            writer.String(enum_value_string.data(), enum_value_string.size());
        }
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Comment_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("comment");
        writer.String(output.comment.data(), output.comment.size());
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Compile_time_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("expression");
        write_object(writer, output.expression);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Constant_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("type");
        write_object(writer, output.type);
        writer.Key("data");
        writer.String(output.data.data(), output.data.size());
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Constant_array_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("array_data");
        write_object(writer, output.array_data);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Continue_expression const& output
        )
    {
        writer.StartObject();
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Defer_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("expression_to_defer");
        write_object(writer, output.expression_to_defer);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Dereference_and_access_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("expression");
        write_object(writer, output.expression);
        writer.Key("member_name");
        writer.String(output.member_name.data(), output.member_name.size());
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            For_loop_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("variable_name");
        writer.String(output.variable_name.data(), output.variable_name.size());
        writer.Key("range_begin");
        write_object(writer, output.range_begin);
        writer.Key("range_end");
        write_object(writer, output.range_end);
        writer.Key("range_comparison_operation");
        {
            std::string_view const enum_value_string = write_enum(output.range_comparison_operation);
            writer.String(enum_value_string.data(), enum_value_string.size());
        }
        write_optional_object(writer, "step_by", output.step_by);
        writer.Key("then_statements");
        write_object(writer, output.then_statements);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("declaration");
        write_object(writer, output.declaration);
        writer.Key("definition");
        write_object(writer, output.definition);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Instance_call_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("left_hand_side");
        write_object(writer, output.left_hand_side);
        writer.Key("arguments");
        write_object(writer, output.arguments);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Instance_call_key const& output
        )
    {
        writer.StartObject();
        writer.Key("module_name");
        writer.String(output.module_name.data(), output.module_name.size());
        writer.Key("function_constructor_name");
        writer.String(output.function_constructor_name.data(), output.function_constructor_name.size());
        writer.Key("arguments");
        write_object(writer, output.arguments);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Condition_statement_pair const& output
        )
    {
        writer.StartObject();
        write_optional_object(writer, "condition", output.condition);
        writer.Key("then_statements");
        write_object(writer, output.then_statements);
        write_optional_object(writer, "block_source_range", output.block_source_range);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            If_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("series");
        write_object(writer, output.series);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Instantiate_member_value_pair const& output
        )
    {
        writer.StartObject();
        writer.Key("member_name");
        writer.String(output.member_name.data(), output.member_name.size());
        writer.Key("value");
        write_object(writer, output.value);
        write_optional_object(writer, "source_range", output.source_range);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Instantiate_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("type");
        {
            std::string_view const enum_value_string = write_enum(output.type);
            writer.String(enum_value_string.data(), enum_value_string.size());
        }
        writer.Key("members");
        write_object(writer, output.members);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Invalid_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("value");
        writer.String(output.value.data(), output.value.size());
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Null_pointer_expression const& output
        )
    {
        writer.StartObject();
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Parenthesis_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("expression");
        write_object(writer, output.expression);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Reflection_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        writer.Key("type_arguments");
        write_object(writer, output.type_arguments);
        writer.Key("arguments");
        write_object(writer, output.arguments);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Return_expression const& output
        )
    {
        writer.StartObject();
        write_optional_object(writer, "expression", output.expression);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Struct_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("declaration");
        write_object(writer, output.declaration);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Switch_case_expression_pair const& output
        )
    {
        writer.StartObject();
        write_optional_object(writer, "case_value", output.case_value);
        writer.Key("statements");
        write_object(writer, output.statements);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Switch_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("value");
        write_object(writer, output.value);
        writer.Key("cases");
        write_object(writer, output.cases);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Ternary_condition_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("condition");
        write_object(writer, output.condition);
        writer.Key("then_statement");
        write_object(writer, output.then_statement);
        writer.Key("else_statement");
        write_object(writer, output.else_statement);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Type_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("type");
        write_object(writer, output.type);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Unary_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("expression");
        write_object(writer, output.expression);
        writer.Key("operation");
        {
            std::string_view const enum_value_string = write_enum(output.operation);
            writer.String(enum_value_string.data(), enum_value_string.size());
        }
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Union_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("declaration");
        write_object(writer, output.declaration);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Variable_declaration_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        writer.Key("is_mutable");
        writer.Bool(output.is_mutable);
        writer.Key("right_hand_side");
        write_object(writer, output.right_hand_side);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Variable_declaration_with_type_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        writer.Key("is_mutable");
        writer.Bool(output.is_mutable);
        writer.Key("type");
        write_object(writer, output.type);
        writer.Key("right_hand_side");
        write_object(writer, output.right_hand_side);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            While_loop_expression const& output
        )
    {
        writer.StartObject();
        writer.Key("condition");
        write_object(writer, output.condition);
        writer.Key("then_statements");
        write_object(writer, output.then_statements);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Expression const& output
        )
    {
        writer.StartObject();
        writer.Key("data");

        writer.StartObject();
        if (std::holds_alternative<Access_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Access_expression");
            writer.Key("value");
            Access_expression const& value = std::get<Access_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Access_array_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Access_array_expression");
            writer.Key("value");
            Access_array_expression const& value = std::get<Access_array_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Assert_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Assert_expression");
            writer.Key("value");
            Assert_expression const& value = std::get<Assert_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Assignment_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Assignment_expression");
            writer.Key("value");
            Assignment_expression const& value = std::get<Assignment_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Binary_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Binary_expression");
            writer.Key("value");
            Binary_expression const& value = std::get<Binary_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Block_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Block_expression");
            writer.Key("value");
            Block_expression const& value = std::get<Block_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Break_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Break_expression");
            writer.Key("value");
            Break_expression const& value = std::get<Break_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Call_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Call_expression");
            writer.Key("value");
            Call_expression const& value = std::get<Call_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Cast_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Cast_expression");
            writer.Key("value");
            Cast_expression const& value = std::get<Cast_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Comment_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Comment_expression");
            writer.Key("value");
            Comment_expression const& value = std::get<Comment_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Compile_time_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Compile_time_expression");
            writer.Key("value");
            Compile_time_expression const& value = std::get<Compile_time_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Constant_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Constant_expression");
            writer.Key("value");
            Constant_expression const& value = std::get<Constant_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Constant_array_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Constant_array_expression");
            writer.Key("value");
            Constant_array_expression const& value = std::get<Constant_array_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Continue_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Continue_expression");
            writer.Key("value");
            Continue_expression const& value = std::get<Continue_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Defer_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Defer_expression");
            writer.Key("value");
            Defer_expression const& value = std::get<Defer_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Dereference_and_access_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Dereference_and_access_expression");
            writer.Key("value");
            Dereference_and_access_expression const& value = std::get<Dereference_and_access_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<For_loop_expression>(output.data))
        {
            writer.Key("type");
            writer.String("For_loop_expression");
            writer.Key("value");
            For_loop_expression const& value = std::get<For_loop_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Function_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Function_expression");
            writer.Key("value");
            Function_expression const& value = std::get<Function_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Instance_call_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Instance_call_expression");
            writer.Key("value");
            Instance_call_expression const& value = std::get<Instance_call_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<If_expression>(output.data))
        {
            writer.Key("type");
            writer.String("If_expression");
            writer.Key("value");
            If_expression const& value = std::get<If_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Instantiate_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Instantiate_expression");
            writer.Key("value");
            Instantiate_expression const& value = std::get<Instantiate_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Invalid_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Invalid_expression");
            writer.Key("value");
            Invalid_expression const& value = std::get<Invalid_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Null_pointer_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Null_pointer_expression");
            writer.Key("value");
            Null_pointer_expression const& value = std::get<Null_pointer_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Parenthesis_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Parenthesis_expression");
            writer.Key("value");
            Parenthesis_expression const& value = std::get<Parenthesis_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Reflection_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Reflection_expression");
            writer.Key("value");
            Reflection_expression const& value = std::get<Reflection_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Return_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Return_expression");
            writer.Key("value");
            Return_expression const& value = std::get<Return_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Struct_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Struct_expression");
            writer.Key("value");
            Struct_expression const& value = std::get<Struct_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Switch_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Switch_expression");
            writer.Key("value");
            Switch_expression const& value = std::get<Switch_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Ternary_condition_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Ternary_condition_expression");
            writer.Key("value");
            Ternary_condition_expression const& value = std::get<Ternary_condition_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Type_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Type_expression");
            writer.Key("value");
            Type_expression const& value = std::get<Type_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Unary_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Unary_expression");
            writer.Key("value");
            Unary_expression const& value = std::get<Unary_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Union_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Union_expression");
            writer.Key("value");
            Union_expression const& value = std::get<Union_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Variable_declaration_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Variable_declaration_expression");
            writer.Key("value");
            Variable_declaration_expression const& value = std::get<Variable_declaration_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Variable_declaration_with_type_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Variable_declaration_with_type_expression");
            writer.Key("value");
            Variable_declaration_with_type_expression const& value = std::get<Variable_declaration_with_type_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<Variable_expression>(output.data))
        {
            writer.Key("type");
            writer.String("Variable_expression");
            writer.Key("value");
            Variable_expression const& value = std::get<Variable_expression>(output.data);
            write_object(writer, value);
        }
        else if (std::holds_alternative<While_loop_expression>(output.data))
        {
            writer.Key("type");
            writer.String("While_loop_expression");
            writer.Key("value");
            While_loop_expression const& value = std::get<While_loop_expression>(output.data);
            write_object(writer, value);
        }
        writer.EndObject();

        write_optional_object(writer, "source_range", output.source_range);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Type_constructor_parameter const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        writer.Key("type");
        write_object(writer, output.type);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Type_constructor const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        writer.Key("parameters");
        write_object(writer, output.parameters);
        writer.Key("statements");
        write_object(writer, output.statements);
        write_optional(writer, "comment", output.comment);
        write_optional_object(writer, "source_location", output.source_location);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_constructor_parameter const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        writer.Key("type");
        write_object(writer, output.type);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Function_constructor const& output
        )
    {
        writer.StartObject();
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        writer.Key("parameters");
        write_object(writer, output.parameters);
        writer.Key("statements");
        write_object(writer, output.statements);
        write_optional(writer, "comment", output.comment);
        write_optional_object(writer, "source_location", output.source_location);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Language_version const& output
        )
    {
        writer.StartObject();
        writer.Key("major");
        writer.Uint(output.major);
        writer.Key("minor");
        writer.Uint(output.minor);
        writer.Key("patch");
        writer.Uint(output.patch);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Import_module_with_alias const& output
        )
    {
        writer.StartObject();
        writer.Key("module_name");
        writer.String(output.module_name.data(), output.module_name.size());
        writer.Key("alias");
        writer.String(output.alias.data(), output.alias.size());
        writer.Key("usages");
        write_object(writer, output.usages);
        write_optional_object(writer, "source_range", output.source_range);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Module_dependencies const& output
        )
    {
        writer.StartObject();
        writer.Key("alias_imports");
        write_object(writer, output.alias_imports);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Module_declarations const& output
        )
    {
        writer.StartObject();
        writer.Key("alias_type_declarations");
        write_object(writer, output.alias_type_declarations);
        writer.Key("enum_declarations");
        write_object(writer, output.enum_declarations);
        writer.Key("forward_declarations");
        write_object(writer, output.forward_declarations);
        writer.Key("global_variable_declarations");
        write_object(writer, output.global_variable_declarations);
        writer.Key("struct_declarations");
        write_object(writer, output.struct_declarations);
        writer.Key("union_declarations");
        write_object(writer, output.union_declarations);
        writer.Key("function_declarations");
        write_object(writer, output.function_declarations);
        writer.Key("function_constructors");
        write_object(writer, output.function_constructors);
        writer.Key("type_constructors");
        write_object(writer, output.type_constructors);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Module_definitions const& output
        )
    {
        writer.StartObject();
        writer.Key("function_definitions");
        write_object(writer, output.function_definitions);
        writer.EndObject();
    }

    export template<typename Writer_type>
        void write_object(
            Writer_type& writer,
            Module const& output
        )
    {
        writer.StartObject();
        writer.Key("language_version");
        write_object(writer, output.language_version);
        writer.Key("name");
        writer.String(output.name.data(), output.name.size());
        write_optional(writer, "content_hash", output.content_hash);
        writer.Key("dependencies");
        write_object(writer, output.dependencies);
        writer.Key("export_declarations");
        write_object(writer, output.export_declarations);
        writer.Key("internal_declarations");
        write_object(writer, output.internal_declarations);
        writer.Key("definitions");
        write_object(writer, output.definitions);
        write_optional(writer, "comment", output.comment);
        write_optional(writer, "source_file_path", output.source_file_path);
        writer.EndObject();
    }

}
