module;

#define EXPORT export
#include "Generics.h"

#include <compare>

export module iris.json_serializer.generated;

//import iris.json_serializer.generics;
import iris.core;

namespace iris::json
{
    EXPORT template <>
    JSON to_json(Fundamental_type const& value)
    {
        switch (value)
        {
            case Fundamental_type::Bool: return "Bool";
            case Fundamental_type::Byte: return "Byte";
            case Fundamental_type::Float16: return "Float16";
            case Fundamental_type::Float32: return "Float32";
            case Fundamental_type::Float64: return "Float64";
            case Fundamental_type::String: return "String";
            case Fundamental_type::Any_type: return "Any_type";
            case Fundamental_type::C_bool: return "C_bool";
            case Fundamental_type::C_char: return "C_char";
            case Fundamental_type::C_schar: return "C_schar";
            case Fundamental_type::C_uchar: return "C_uchar";
            case Fundamental_type::C_short: return "C_short";
            case Fundamental_type::C_ushort: return "C_ushort";
            case Fundamental_type::C_int: return "C_int";
            case Fundamental_type::C_uint: return "C_uint";
            case Fundamental_type::C_long: return "C_long";
            case Fundamental_type::C_ulong: return "C_ulong";
            case Fundamental_type::C_longlong: return "C_longlong";
            case Fundamental_type::C_ulonglong: return "C_ulonglong";
            case Fundamental_type::C_longdouble: return "C_longdouble";
        default: return "Bool";
        }
    }

    EXPORT template <>
    void from_json(JSON const& data, Fundamental_type& output)
    {
        std::string const& value = data.get<std::string>();
        if (value == "Bool") { output = Fundamental_type::Bool; return; }
        if (value == "Byte") { output = Fundamental_type::Byte; return; }
        if (value == "Float16") { output = Fundamental_type::Float16; return; }
        if (value == "Float32") { output = Fundamental_type::Float32; return; }
        if (value == "Float64") { output = Fundamental_type::Float64; return; }
        if (value == "String") { output = Fundamental_type::String; return; }
        if (value == "Any_type") { output = Fundamental_type::Any_type; return; }
        if (value == "C_bool") { output = Fundamental_type::C_bool; return; }
        if (value == "C_char") { output = Fundamental_type::C_char; return; }
        if (value == "C_schar") { output = Fundamental_type::C_schar; return; }
        if (value == "C_uchar") { output = Fundamental_type::C_uchar; return; }
        if (value == "C_short") { output = Fundamental_type::C_short; return; }
        if (value == "C_ushort") { output = Fundamental_type::C_ushort; return; }
        if (value == "C_int") { output = Fundamental_type::C_int; return; }
        if (value == "C_uint") { output = Fundamental_type::C_uint; return; }
        if (value == "C_long") { output = Fundamental_type::C_long; return; }
        if (value == "C_ulong") { output = Fundamental_type::C_ulong; return; }
        if (value == "C_longlong") { output = Fundamental_type::C_longlong; return; }
        if (value == "C_ulonglong") { output = Fundamental_type::C_ulonglong; return; }
        if (value == "C_longdouble") { output = Fundamental_type::C_longdouble; return; }
        output = Fundamental_type::Bool;
    }

    EXPORT template <>
    JSON to_json(Global_variable_type const& value)
    {
        switch (value)
        {
            case Global_variable_type::Constant: return "Constant";
            case Global_variable_type::Mutable: return "Mutable";
            case Global_variable_type::Macro: return "Macro";
        default: return "Constant";
        }
    }

    EXPORT template <>
    void from_json(JSON const& data, Global_variable_type& output)
    {
        std::string const& value = data.get<std::string>();
        if (value == "Constant") { output = Global_variable_type::Constant; return; }
        if (value == "Mutable") { output = Global_variable_type::Mutable; return; }
        if (value == "Macro") { output = Global_variable_type::Macro; return; }
        output = Global_variable_type::Constant;
    }

    EXPORT template <>
    JSON to_json(Linkage const& value)
    {
        switch (value)
        {
            case Linkage::External: return "External";
            case Linkage::Private: return "Private";
        default: return "External";
        }
    }

    EXPORT template <>
    void from_json(JSON const& data, Linkage& output)
    {
        std::string const& value = data.get<std::string>();
        if (value == "External") { output = Linkage::External; return; }
        if (value == "Private") { output = Linkage::Private; return; }
        output = Linkage::External;
    }

    EXPORT template <>
    JSON to_json(Binary_operation const& value)
    {
        switch (value)
        {
            case Binary_operation::Add: return "Add";
            case Binary_operation::Subtract: return "Subtract";
            case Binary_operation::Multiply: return "Multiply";
            case Binary_operation::Divide: return "Divide";
            case Binary_operation::Modulus: return "Modulus";
            case Binary_operation::Equal: return "Equal";
            case Binary_operation::Not_equal: return "Not_equal";
            case Binary_operation::Less_than: return "Less_than";
            case Binary_operation::Less_than_or_equal_to: return "Less_than_or_equal_to";
            case Binary_operation::Greater_than: return "Greater_than";
            case Binary_operation::Greater_than_or_equal_to: return "Greater_than_or_equal_to";
            case Binary_operation::Logical_and: return "Logical_and";
            case Binary_operation::Logical_or: return "Logical_or";
            case Binary_operation::Bitwise_and: return "Bitwise_and";
            case Binary_operation::Bitwise_or: return "Bitwise_or";
            case Binary_operation::Bitwise_xor: return "Bitwise_xor";
            case Binary_operation::Bit_shift_left: return "Bit_shift_left";
            case Binary_operation::Bit_shift_right: return "Bit_shift_right";
            case Binary_operation::Has: return "Has";
        default: return "Add";
        }
    }

    EXPORT template <>
    void from_json(JSON const& data, Binary_operation& output)
    {
        std::string const& value = data.get<std::string>();
        if (value == "Add") { output = Binary_operation::Add; return; }
        if (value == "Subtract") { output = Binary_operation::Subtract; return; }
        if (value == "Multiply") { output = Binary_operation::Multiply; return; }
        if (value == "Divide") { output = Binary_operation::Divide; return; }
        if (value == "Modulus") { output = Binary_operation::Modulus; return; }
        if (value == "Equal") { output = Binary_operation::Equal; return; }
        if (value == "Not_equal") { output = Binary_operation::Not_equal; return; }
        if (value == "Less_than") { output = Binary_operation::Less_than; return; }
        if (value == "Less_than_or_equal_to") { output = Binary_operation::Less_than_or_equal_to; return; }
        if (value == "Greater_than") { output = Binary_operation::Greater_than; return; }
        if (value == "Greater_than_or_equal_to") { output = Binary_operation::Greater_than_or_equal_to; return; }
        if (value == "Logical_and") { output = Binary_operation::Logical_and; return; }
        if (value == "Logical_or") { output = Binary_operation::Logical_or; return; }
        if (value == "Bitwise_and") { output = Binary_operation::Bitwise_and; return; }
        if (value == "Bitwise_or") { output = Binary_operation::Bitwise_or; return; }
        if (value == "Bitwise_xor") { output = Binary_operation::Bitwise_xor; return; }
        if (value == "Bit_shift_left") { output = Binary_operation::Bit_shift_left; return; }
        if (value == "Bit_shift_right") { output = Binary_operation::Bit_shift_right; return; }
        if (value == "Has") { output = Binary_operation::Has; return; }
        output = Binary_operation::Add;
    }

    EXPORT template <>
    JSON to_json(Cast_type const& value)
    {
        switch (value)
        {
            case Cast_type::Numeric: return "Numeric";
            case Cast_type::BitCast: return "BitCast";
        default: return "Numeric";
        }
    }

    EXPORT template <>
    void from_json(JSON const& data, Cast_type& output)
    {
        std::string const& value = data.get<std::string>();
        if (value == "Numeric") { output = Cast_type::Numeric; return; }
        if (value == "BitCast") { output = Cast_type::BitCast; return; }
        output = Cast_type::Numeric;
    }

    EXPORT template <>
    JSON to_json(Instantiate_expression_type const& value)
    {
        switch (value)
        {
            case Instantiate_expression_type::Default: return "Default";
            case Instantiate_expression_type::Explicit: return "Explicit";
            case Instantiate_expression_type::Uninitialized: return "Uninitialized";
            case Instantiate_expression_type::Zero_initialized: return "Zero_initialized";
        default: return "Default";
        }
    }

    EXPORT template <>
    void from_json(JSON const& data, Instantiate_expression_type& output)
    {
        std::string const& value = data.get<std::string>();
        if (value == "Default") { output = Instantiate_expression_type::Default; return; }
        if (value == "Explicit") { output = Instantiate_expression_type::Explicit; return; }
        if (value == "Uninitialized") { output = Instantiate_expression_type::Uninitialized; return; }
        if (value == "Zero_initialized") { output = Instantiate_expression_type::Zero_initialized; return; }
        output = Instantiate_expression_type::Default;
    }

    EXPORT template <>
    JSON to_json(Unary_operation const& value)
    {
        switch (value)
        {
            case Unary_operation::Not: return "Not";
            case Unary_operation::Bitwise_not: return "Bitwise_not";
            case Unary_operation::Minus: return "Minus";
            case Unary_operation::Pre_increment: return "Pre_increment";
            case Unary_operation::Post_increment: return "Post_increment";
            case Unary_operation::Pre_decrement: return "Pre_decrement";
            case Unary_operation::Post_decrement: return "Post_decrement";
            case Unary_operation::Indirection: return "Indirection";
            case Unary_operation::Address_of: return "Address_of";
        default: return "Not";
        }
    }

    EXPORT template <>
    void from_json(JSON const& data, Unary_operation& output)
    {
        std::string const& value = data.get<std::string>();
        if (value == "Not") { output = Unary_operation::Not; return; }
        if (value == "Bitwise_not") { output = Unary_operation::Bitwise_not; return; }
        if (value == "Minus") { output = Unary_operation::Minus; return; }
        if (value == "Pre_increment") { output = Unary_operation::Pre_increment; return; }
        if (value == "Post_increment") { output = Unary_operation::Post_increment; return; }
        if (value == "Pre_decrement") { output = Unary_operation::Pre_decrement; return; }
        if (value == "Post_decrement") { output = Unary_operation::Post_decrement; return; }
        if (value == "Indirection") { output = Unary_operation::Indirection; return; }
        if (value == "Address_of") { output = Unary_operation::Address_of; return; }
        output = Unary_operation::Not;
    }

    EXPORT template <>
    JSON to_json(Source_location const& value)
    {
        JSON data;
        if (value.file_path.has_value()) data["file_path"] = to_json(value.file_path);
        data["line"] = to_json(value.line);
        data["column"] = to_json(value.column);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Source_location& value)
    {
        if (data.contains("file_path")) from_json(data.at("file_path"), value.file_path);
        from_json(data.at("line"), value.line);
        from_json(data.at("column"), value.column);
    }

    EXPORT template <>
    JSON to_json(Source_position const& value)
    {
        JSON data;
        data["line"] = to_json(value.line);
        data["column"] = to_json(value.column);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Source_position& value)
    {
        from_json(data.at("line"), value.line);
        from_json(data.at("column"), value.column);
    }

    EXPORT template <>
    JSON to_json(Source_range const& value)
    {
        JSON data;
        data["start"] = to_json(value.start);
        data["end"] = to_json(value.end);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Source_range& value)
    {
        from_json(data.at("start"), value.start);
        from_json(data.at("end"), value.end);
    }

    EXPORT template <>
    JSON to_json(Source_range_location const& value)
    {
        JSON data;
        if (value.file_path.has_value()) data["file_path"] = to_json(value.file_path);
        data["range"] = to_json(value.range);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Source_range_location& value)
    {
        if (data.contains("file_path")) from_json(data.at("file_path"), value.file_path);
        from_json(data.at("range"), value.range);
    }

    EXPORT template <>
    JSON to_json(Integer_type const& value)
    {
        JSON data;
        data["number_of_bits"] = to_json(value.number_of_bits);
        data["is_signed"] = to_json(value.is_signed);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Integer_type& value)
    {
        from_json(data.at("number_of_bits"), value.number_of_bits);
        from_json(data.at("is_signed"), value.is_signed);
    }

    EXPORT template <>
    JSON to_json(Decimal_type const& value)
    {
        JSON data;
        data["scale"] = to_json(value.scale);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Decimal_type& value)
    {
        from_json(data.at("scale"), value.scale);
    }

    EXPORT template <>
    JSON to_json(Array_slice_type const& value)
    {
        JSON data;
        data["element_type"] = to_json(value.element_type);
        data["is_mutable"] = to_json(value.is_mutable);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Array_slice_type& value)
    {
        if (data.contains("element_type")) from_json(data.at("element_type"), value.element_type);
        from_json(data.at("is_mutable"), value.is_mutable);
    }

    EXPORT template <>
    JSON to_json(Builtin_type_reference const& value)
    {
        JSON data;
        data["value"] = to_json(value.value);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Builtin_type_reference& value)
    {
        from_json(data.at("value"), value.value);
    }

    EXPORT template <>
    JSON to_json(Function_type const& value)
    {
        JSON data;
        data["input_parameter_types"] = to_json(value.input_parameter_types);
        data["output_parameter_types"] = to_json(value.output_parameter_types);
        data["is_variadic"] = to_json(value.is_variadic);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Function_type& value)
    {
        if (data.contains("input_parameter_types")) from_json(data.at("input_parameter_types"), value.input_parameter_types);
        if (data.contains("output_parameter_types")) from_json(data.at("output_parameter_types"), value.output_parameter_types);
        from_json(data.at("is_variadic"), value.is_variadic);
    }

    EXPORT template <>
    JSON to_json(Function_pointer_type const& value)
    {
        JSON data;
        data["type"] = to_json(value.type);
        data["input_parameter_names"] = to_json(value.input_parameter_names);
        data["output_parameter_names"] = to_json(value.output_parameter_names);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Function_pointer_type& value)
    {
        from_json(data.at("type"), value.type);
        if (data.contains("input_parameter_names")) from_json(data.at("input_parameter_names"), value.input_parameter_names);
        if (data.contains("output_parameter_names")) from_json(data.at("output_parameter_names"), value.output_parameter_names);
    }

    EXPORT template <>
    JSON to_json(Null_pointer_type const& value)
    {
        JSON data;

        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Null_pointer_type& value)
    {

    }

    EXPORT template <>
    JSON to_json(Pointer_type const& value)
    {
        JSON data;
        data["element_type"] = to_json(value.element_type);
        data["is_mutable"] = to_json(value.is_mutable);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Pointer_type& value)
    {
        if (data.contains("element_type")) from_json(data.at("element_type"), value.element_type);
        from_json(data.at("is_mutable"), value.is_mutable);
    }

    EXPORT template <>
    JSON to_json(Module_reference const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Module_reference& value)
    {
        from_json(data.at("name"), value.name);
    }

    EXPORT template <>
    JSON to_json(Constant_array_type const& value)
    {
        JSON data;
        data["value_type"] = to_json(value.value_type);
        data["size"] = to_json(value.size);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Constant_array_type& value)
    {
        if (data.contains("value_type")) from_json(data.at("value_type"), value.value_type);
        from_json(data.at("size"), value.size);
    }

    EXPORT template <>
    JSON to_json(Soa_array_type const& value)
    {
        JSON data;
        data["value_type"] = to_json(value.value_type);
        data["size"] = to_json(value.size);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Soa_array_type& value)
    {
        if (data.contains("value_type")) from_json(data.at("value_type"), value.value_type);
        from_json(data.at("size"), value.size);
    }

    EXPORT template <>
    JSON to_json(Soa_array_view_type const& value)
    {
        JSON data;
        data["value_type"] = to_json(value.value_type);
        data["is_mutable"] = to_json(value.is_mutable);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Soa_array_view_type& value)
    {
        if (data.contains("value_type")) from_json(data.at("value_type"), value.value_type);
        from_json(data.at("is_mutable"), value.is_mutable);
    }

    EXPORT template <>
    JSON to_json(Custom_type_reference const& value)
    {
        JSON data;
        data["module_reference"] = to_json(value.module_reference);
        data["name"] = to_json(value.name);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Custom_type_reference& value)
    {
        from_json(data.at("module_reference"), value.module_reference);
        from_json(data.at("name"), value.name);
    }

    EXPORT template <>
    JSON to_json(Type_instance const& value)
    {
        JSON data;
        data["type_constructor"] = to_json(value.type_constructor);
        data["arguments"] = to_json(value.arguments);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Type_instance& value)
    {
        from_json(data.at("type_constructor"), value.type_constructor);
        if (data.contains("arguments")) from_json(data.at("arguments"), value.arguments);
    }

    EXPORT template <>
    JSON to_json(Parameter_type const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Parameter_type& value)
    {
        from_json(data.at("name"), value.name);
    }

    EXPORT template <>
    JSON to_json(Type_reference const& value)
    {
        JSON data;
        data["data"] = to_json(value.data);
        if (value.source_range.has_value()) data["source_range"] = to_json(value.source_range);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Type_reference& value)
    {
        from_json(data.at("data"), value.data);
        if (data.contains("source_range")) from_json(data.at("source_range"), value.source_range);
    }

    EXPORT template <>
    JSON to_json(Indexed_comment const& value)
    {
        JSON data;
        data["index"] = to_json(value.index);
        data["comment"] = to_json(value.comment);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Indexed_comment& value)
    {
        from_json(data.at("index"), value.index);
        from_json(data.at("comment"), value.comment);
    }

    EXPORT template <>
    JSON to_json(Statement const& value)
    {
        JSON data;
        data["expressions"] = to_json(value.expressions);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Statement& value)
    {
        if (data.contains("expressions")) from_json(data.at("expressions"), value.expressions);
    }

    EXPORT template <>
    JSON to_json(Global_variable_declaration const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        if (value.unique_name.has_value()) data["unique_name"] = to_json(value.unique_name);
        if (value.type.has_value()) data["type"] = to_json(value.type);
        data["initial_value"] = to_json(value.initial_value);
        data["global_type"] = to_json(value.global_type);
        if (value.comment.has_value()) data["comment"] = to_json(value.comment);
        if (value.source_location.has_value()) data["source_location"] = to_json(value.source_location);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Global_variable_declaration& value)
    {
        from_json(data.at("name"), value.name);
        if (data.contains("unique_name")) from_json(data.at("unique_name"), value.unique_name);
        if (data.contains("type")) from_json(data.at("type"), value.type);
        from_json(data.at("initial_value"), value.initial_value);
        from_json(data.at("global_type"), value.global_type);
        if (data.contains("comment")) from_json(data.at("comment"), value.comment);
        if (data.contains("source_location")) from_json(data.at("source_location"), value.source_location);
    }

    EXPORT template <>
    JSON to_json(Alias_type_declaration const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        if (value.unique_name.has_value()) data["unique_name"] = to_json(value.unique_name);
        data["type"] = to_json(value.type);
        if (value.comment.has_value()) data["comment"] = to_json(value.comment);
        if (value.source_location.has_value()) data["source_location"] = to_json(value.source_location);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Alias_type_declaration& value)
    {
        from_json(data.at("name"), value.name);
        if (data.contains("unique_name")) from_json(data.at("unique_name"), value.unique_name);
        if (data.contains("type")) from_json(data.at("type"), value.type);
        if (data.contains("comment")) from_json(data.at("comment"), value.comment);
        if (data.contains("source_location")) from_json(data.at("source_location"), value.source_location);
    }

    EXPORT template <>
    JSON to_json(Enum_value const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        if (value.value.has_value()) data["value"] = to_json(value.value);
        if (value.comment.has_value()) data["comment"] = to_json(value.comment);
        if (value.source_location.has_value()) data["source_location"] = to_json(value.source_location);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Enum_value& value)
    {
        from_json(data.at("name"), value.name);
        if (data.contains("value")) from_json(data.at("value"), value.value);
        if (data.contains("comment")) from_json(data.at("comment"), value.comment);
        if (data.contains("source_location")) from_json(data.at("source_location"), value.source_location);
    }

    EXPORT template <>
    JSON to_json(Enum_declaration const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        if (value.unique_name.has_value()) data["unique_name"] = to_json(value.unique_name);
        data["values"] = to_json(value.values);
        if (value.comment.has_value()) data["comment"] = to_json(value.comment);
        if (value.source_location.has_value()) data["source_location"] = to_json(value.source_location);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Enum_declaration& value)
    {
        from_json(data.at("name"), value.name);
        if (data.contains("unique_name")) from_json(data.at("unique_name"), value.unique_name);
        if (data.contains("values")) from_json(data.at("values"), value.values);
        if (data.contains("comment")) from_json(data.at("comment"), value.comment);
        if (data.contains("source_location")) from_json(data.at("source_location"), value.source_location);
    }

    EXPORT template <>
    JSON to_json(Forward_declaration const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        if (value.unique_name.has_value()) data["unique_name"] = to_json(value.unique_name);
        if (value.source_location.has_value()) data["source_location"] = to_json(value.source_location);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Forward_declaration& value)
    {
        from_json(data.at("name"), value.name);
        if (data.contains("unique_name")) from_json(data.at("unique_name"), value.unique_name);
        if (data.contains("source_location")) from_json(data.at("source_location"), value.source_location);
    }

    EXPORT template <>
    JSON to_json(Struct_declaration const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        if (value.unique_name.has_value()) data["unique_name"] = to_json(value.unique_name);
        data["member_types"] = to_json(value.member_types);
        data["member_names"] = to_json(value.member_names);
        data["member_bit_fields"] = to_json(value.member_bit_fields);
        data["member_default_values"] = to_json(value.member_default_values);
        data["is_packed"] = to_json(value.is_packed);
        data["is_literal"] = to_json(value.is_literal);
        if (value.comment.has_value()) data["comment"] = to_json(value.comment);
        data["member_comments"] = to_json(value.member_comments);
        if (value.source_location.has_value()) data["source_location"] = to_json(value.source_location);
        if (value.member_source_positions.has_value()) data["member_source_positions"] = to_json(value.member_source_positions);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Struct_declaration& value)
    {
        from_json(data.at("name"), value.name);
        if (data.contains("unique_name")) from_json(data.at("unique_name"), value.unique_name);
        if (data.contains("member_types")) from_json(data.at("member_types"), value.member_types);
        if (data.contains("member_names")) from_json(data.at("member_names"), value.member_names);
        if (data.contains("member_bit_fields")) from_json(data.at("member_bit_fields"), value.member_bit_fields);
        if (data.contains("member_default_values")) from_json(data.at("member_default_values"), value.member_default_values);
        from_json(data.at("is_packed"), value.is_packed);
        from_json(data.at("is_literal"), value.is_literal);
        if (data.contains("comment")) from_json(data.at("comment"), value.comment);
        if (data.contains("member_comments")) from_json(data.at("member_comments"), value.member_comments);
        if (data.contains("source_location")) from_json(data.at("source_location"), value.source_location);
        if (data.contains("member_source_positions")) from_json(data.at("member_source_positions"), value.member_source_positions);
    }

    EXPORT template <>
    JSON to_json(Union_declaration const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        if (value.unique_name.has_value()) data["unique_name"] = to_json(value.unique_name);
        data["member_types"] = to_json(value.member_types);
        data["member_names"] = to_json(value.member_names);
        if (value.comment.has_value()) data["comment"] = to_json(value.comment);
        data["member_comments"] = to_json(value.member_comments);
        if (value.source_location.has_value()) data["source_location"] = to_json(value.source_location);
        if (value.member_source_positions.has_value()) data["member_source_positions"] = to_json(value.member_source_positions);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Union_declaration& value)
    {
        from_json(data.at("name"), value.name);
        if (data.contains("unique_name")) from_json(data.at("unique_name"), value.unique_name);
        if (data.contains("member_types")) from_json(data.at("member_types"), value.member_types);
        if (data.contains("member_names")) from_json(data.at("member_names"), value.member_names);
        if (data.contains("comment")) from_json(data.at("comment"), value.comment);
        if (data.contains("member_comments")) from_json(data.at("member_comments"), value.member_comments);
        if (data.contains("source_location")) from_json(data.at("source_location"), value.source_location);
        if (data.contains("member_source_positions")) from_json(data.at("member_source_positions"), value.member_source_positions);
    }

    EXPORT template <>
    JSON to_json(Function_condition const& value)
    {
        JSON data;
        data["description"] = to_json(value.description);
        data["condition"] = to_json(value.condition);
        if (value.source_range.has_value()) data["source_range"] = to_json(value.source_range);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Function_condition& value)
    {
        from_json(data.at("description"), value.description);
        from_json(data.at("condition"), value.condition);
        if (data.contains("source_range")) from_json(data.at("source_range"), value.source_range);
    }

    EXPORT template <>
    JSON to_json(Function_declaration const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        if (value.unique_name.has_value()) data["unique_name"] = to_json(value.unique_name);
        data["type"] = to_json(value.type);
        data["input_parameter_names"] = to_json(value.input_parameter_names);
        data["output_parameter_names"] = to_json(value.output_parameter_names);
        data["linkage"] = to_json(value.linkage);
        data["is_test"] = to_json(value.is_test);
        data["preconditions"] = to_json(value.preconditions);
        data["postconditions"] = to_json(value.postconditions);
        if (value.comment.has_value()) data["comment"] = to_json(value.comment);
        if (value.source_location.has_value()) data["source_location"] = to_json(value.source_location);
        if (value.input_parameter_source_positions.has_value()) data["input_parameter_source_positions"] = to_json(value.input_parameter_source_positions);
        if (value.output_parameter_source_positions.has_value()) data["output_parameter_source_positions"] = to_json(value.output_parameter_source_positions);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Function_declaration& value)
    {
        from_json(data.at("name"), value.name);
        if (data.contains("unique_name")) from_json(data.at("unique_name"), value.unique_name);
        from_json(data.at("type"), value.type);
        if (data.contains("input_parameter_names")) from_json(data.at("input_parameter_names"), value.input_parameter_names);
        if (data.contains("output_parameter_names")) from_json(data.at("output_parameter_names"), value.output_parameter_names);
        from_json(data.at("linkage"), value.linkage);
        from_json(data.at("is_test"), value.is_test);
        if (data.contains("preconditions")) from_json(data.at("preconditions"), value.preconditions);
        if (data.contains("postconditions")) from_json(data.at("postconditions"), value.postconditions);
        if (data.contains("comment")) from_json(data.at("comment"), value.comment);
        if (data.contains("source_location")) from_json(data.at("source_location"), value.source_location);
        if (data.contains("input_parameter_source_positions")) from_json(data.at("input_parameter_source_positions"), value.input_parameter_source_positions);
        if (data.contains("output_parameter_source_positions")) from_json(data.at("output_parameter_source_positions"), value.output_parameter_source_positions);
    }

    EXPORT template <>
    JSON to_json(Function_definition const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        data["statements"] = to_json(value.statements);
        if (value.source_location.has_value()) data["source_location"] = to_json(value.source_location);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Function_definition& value)
    {
        from_json(data.at("name"), value.name);
        if (data.contains("statements")) from_json(data.at("statements"), value.statements);
        if (data.contains("source_location")) from_json(data.at("source_location"), value.source_location);
    }

    EXPORT template <>
    JSON to_json(Variable_expression const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Variable_expression& value)
    {
        from_json(data.at("name"), value.name);
    }

    EXPORT template <>
    JSON to_json(Expression_index const& value)
    {
        JSON data;
        data["expression_index"] = to_json(value.expression_index);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Expression_index& value)
    {
        from_json(data.at("expression_index"), value.expression_index);
    }

    EXPORT template <>
    JSON to_json(Access_expression const& value)
    {
        JSON data;
        data["expression"] = to_json(value.expression);
        data["member_name"] = to_json(value.member_name);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Access_expression& value)
    {
        from_json(data.at("expression"), value.expression);
        from_json(data.at("member_name"), value.member_name);
    }

    EXPORT template <>
    JSON to_json(Access_array_expression const& value)
    {
        JSON data;
        data["expression"] = to_json(value.expression);
        data["index"] = to_json(value.index);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Access_array_expression& value)
    {
        from_json(data.at("expression"), value.expression);
        from_json(data.at("index"), value.index);
    }

    EXPORT template <>
    JSON to_json(Assert_expression const& value)
    {
        JSON data;
        if (value.message.has_value()) data["message"] = to_json(value.message);
        data["statement"] = to_json(value.statement);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Assert_expression& value)
    {
        if (data.contains("message")) from_json(data.at("message"), value.message);
        from_json(data.at("statement"), value.statement);
    }

    EXPORT template <>
    JSON to_json(Assignment_expression const& value)
    {
        JSON data;
        data["left_hand_side"] = to_json(value.left_hand_side);
        data["right_hand_side"] = to_json(value.right_hand_side);
        if (value.additional_operation.has_value()) data["additional_operation"] = to_json(value.additional_operation);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Assignment_expression& value)
    {
        from_json(data.at("left_hand_side"), value.left_hand_side);
        from_json(data.at("right_hand_side"), value.right_hand_side);
        if (data.contains("additional_operation")) from_json(data.at("additional_operation"), value.additional_operation);
    }

    EXPORT template <>
    JSON to_json(Binary_expression const& value)
    {
        JSON data;
        data["left_hand_side"] = to_json(value.left_hand_side);
        data["right_hand_side"] = to_json(value.right_hand_side);
        data["operation"] = to_json(value.operation);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Binary_expression& value)
    {
        from_json(data.at("left_hand_side"), value.left_hand_side);
        from_json(data.at("right_hand_side"), value.right_hand_side);
        from_json(data.at("operation"), value.operation);
    }

    EXPORT template <>
    JSON to_json(Block_expression const& value)
    {
        JSON data;
        data["statements"] = to_json(value.statements);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Block_expression& value)
    {
        if (data.contains("statements")) from_json(data.at("statements"), value.statements);
    }

    EXPORT template <>
    JSON to_json(Break_expression const& value)
    {
        JSON data;
        data["loop_count"] = to_json(value.loop_count);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Break_expression& value)
    {
        from_json(data.at("loop_count"), value.loop_count);
    }

    EXPORT template <>
    JSON to_json(Call_expression const& value)
    {
        JSON data;
        data["expression"] = to_json(value.expression);
        data["arguments"] = to_json(value.arguments);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Call_expression& value)
    {
        from_json(data.at("expression"), value.expression);
        if (data.contains("arguments")) from_json(data.at("arguments"), value.arguments);
    }

    EXPORT template <>
    JSON to_json(Cast_expression const& value)
    {
        JSON data;
        data["source"] = to_json(value.source);
        data["destination_type"] = to_json(value.destination_type);
        data["cast_type"] = to_json(value.cast_type);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Cast_expression& value)
    {
        from_json(data.at("source"), value.source);
        from_json(data.at("destination_type"), value.destination_type);
        from_json(data.at("cast_type"), value.cast_type);
    }

    EXPORT template <>
    JSON to_json(Comment_expression const& value)
    {
        JSON data;
        data["comment"] = to_json(value.comment);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Comment_expression& value)
    {
        from_json(data.at("comment"), value.comment);
    }

    EXPORT template <>
    JSON to_json(Compile_time_expression const& value)
    {
        JSON data;
        data["expression"] = to_json(value.expression);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Compile_time_expression& value)
    {
        from_json(data.at("expression"), value.expression);
    }

    EXPORT template <>
    JSON to_json(Constant_expression const& value)
    {
        JSON data;
        data["type"] = to_json(value.type);
        data["data"] = to_json(value.data);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Constant_expression& value)
    {
        from_json(data.at("type"), value.type);
        from_json(data.at("data"), value.data);
    }

    EXPORT template <>
    JSON to_json(Constant_array_expression const& value)
    {
        JSON data;
        data["array_data"] = to_json(value.array_data);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Constant_array_expression& value)
    {
        if (data.contains("array_data")) from_json(data.at("array_data"), value.array_data);
    }

    EXPORT template <>
    JSON to_json(Continue_expression const& value)
    {
        JSON data;

        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Continue_expression& value)
    {

    }

    EXPORT template <>
    JSON to_json(Defer_expression const& value)
    {
        JSON data;
        data["expression_to_defer"] = to_json(value.expression_to_defer);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Defer_expression& value)
    {
        from_json(data.at("expression_to_defer"), value.expression_to_defer);
    }

    EXPORT template <>
    JSON to_json(Dereference_and_access_expression const& value)
    {
        JSON data;
        data["expression"] = to_json(value.expression);
        data["member_name"] = to_json(value.member_name);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Dereference_and_access_expression& value)
    {
        from_json(data.at("expression"), value.expression);
        from_json(data.at("member_name"), value.member_name);
    }

    EXPORT template <>
    JSON to_json(For_loop_expression const& value)
    {
        JSON data;
        data["variable_name"] = to_json(value.variable_name);
        data["range_begin"] = to_json(value.range_begin);
        data["range_end"] = to_json(value.range_end);
        data["range_comparison_operation"] = to_json(value.range_comparison_operation);
        if (value.step_by.has_value()) data["step_by"] = to_json(value.step_by);
        data["then_statements"] = to_json(value.then_statements);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, For_loop_expression& value)
    {
        from_json(data.at("variable_name"), value.variable_name);
        from_json(data.at("range_begin"), value.range_begin);
        from_json(data.at("range_end"), value.range_end);
        from_json(data.at("range_comparison_operation"), value.range_comparison_operation);
        if (data.contains("step_by")) from_json(data.at("step_by"), value.step_by);
        if (data.contains("then_statements")) from_json(data.at("then_statements"), value.then_statements);
    }

    EXPORT template <>
    JSON to_json(Function_expression const& value)
    {
        JSON data;
        data["declaration"] = to_json(value.declaration);
        data["definition"] = to_json(value.definition);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Function_expression& value)
    {
        from_json(data.at("declaration"), value.declaration);
        from_json(data.at("definition"), value.definition);
    }

    EXPORT template <>
    JSON to_json(Instance_call_expression const& value)
    {
        JSON data;
        data["left_hand_side"] = to_json(value.left_hand_side);
        data["arguments"] = to_json(value.arguments);
        data["arguments_mutability"] = to_json(value.arguments_mutability);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Instance_call_expression& value)
    {
        from_json(data.at("left_hand_side"), value.left_hand_side);
        if (data.contains("arguments")) from_json(data.at("arguments"), value.arguments);
        if (data.contains("arguments_mutability")) from_json(data.at("arguments_mutability"), value.arguments_mutability);
    }

    EXPORT template <>
    JSON to_json(Instance_call_key const& value)
    {
        JSON data;
        data["module_name"] = to_json(value.module_name);
        data["function_constructor_name"] = to_json(value.function_constructor_name);
        data["arguments"] = to_json(value.arguments);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Instance_call_key& value)
    {
        from_json(data.at("module_name"), value.module_name);
        from_json(data.at("function_constructor_name"), value.function_constructor_name);
        if (data.contains("arguments")) from_json(data.at("arguments"), value.arguments);
    }

    EXPORT template <>
    JSON to_json(Condition_statement_pair const& value)
    {
        JSON data;
        if (value.condition.has_value()) data["condition"] = to_json(value.condition);
        data["then_statements"] = to_json(value.then_statements);
        if (value.block_source_range.has_value()) data["block_source_range"] = to_json(value.block_source_range);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Condition_statement_pair& value)
    {
        if (data.contains("condition")) from_json(data.at("condition"), value.condition);
        if (data.contains("then_statements")) from_json(data.at("then_statements"), value.then_statements);
        if (data.contains("block_source_range")) from_json(data.at("block_source_range"), value.block_source_range);
    }

    EXPORT template <>
    JSON to_json(If_expression const& value)
    {
        JSON data;
        data["series"] = to_json(value.series);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, If_expression& value)
    {
        if (data.contains("series")) from_json(data.at("series"), value.series);
    }

    EXPORT template <>
    JSON to_json(Instantiate_member_value_pair const& value)
    {
        JSON data;
        data["member_name"] = to_json(value.member_name);
        data["value"] = to_json(value.value);
        if (value.source_range.has_value()) data["source_range"] = to_json(value.source_range);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Instantiate_member_value_pair& value)
    {
        from_json(data.at("member_name"), value.member_name);
        from_json(data.at("value"), value.value);
        if (data.contains("source_range")) from_json(data.at("source_range"), value.source_range);
    }

    EXPORT template <>
    JSON to_json(Instantiate_expression const& value)
    {
        JSON data;
        data["type"] = to_json(value.type);
        data["members"] = to_json(value.members);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Instantiate_expression& value)
    {
        from_json(data.at("type"), value.type);
        if (data.contains("members")) from_json(data.at("members"), value.members);
    }

    EXPORT template <>
    JSON to_json(Invalid_expression const& value)
    {
        JSON data;
        data["value"] = to_json(value.value);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Invalid_expression& value)
    {
        from_json(data.at("value"), value.value);
    }

    EXPORT template <>
    JSON to_json(Null_pointer_expression const& value)
    {
        JSON data;

        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Null_pointer_expression& value)
    {

    }

    EXPORT template <>
    JSON to_json(Parenthesis_expression const& value)
    {
        JSON data;
        data["expression"] = to_json(value.expression);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Parenthesis_expression& value)
    {
        from_json(data.at("expression"), value.expression);
    }

    EXPORT template <>
    JSON to_json(Reflection_expression const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        data["type_arguments"] = to_json(value.type_arguments);
        data["arguments"] = to_json(value.arguments);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Reflection_expression& value)
    {
        from_json(data.at("name"), value.name);
        if (data.contains("type_arguments")) from_json(data.at("type_arguments"), value.type_arguments);
        if (data.contains("arguments")) from_json(data.at("arguments"), value.arguments);
    }

    EXPORT template <>
    JSON to_json(Return_expression const& value)
    {
        JSON data;
        if (value.expression.has_value()) data["expression"] = to_json(value.expression);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Return_expression& value)
    {
        if (data.contains("expression")) from_json(data.at("expression"), value.expression);
    }

    EXPORT template <>
    JSON to_json(Struct_expression const& value)
    {
        JSON data;
        data["declaration"] = to_json(value.declaration);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Struct_expression& value)
    {
        from_json(data.at("declaration"), value.declaration);
    }

    EXPORT template <>
    JSON to_json(Switch_case_expression_pair const& value)
    {
        JSON data;
        if (value.case_value.has_value()) data["case_value"] = to_json(value.case_value);
        data["statements"] = to_json(value.statements);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Switch_case_expression_pair& value)
    {
        if (data.contains("case_value")) from_json(data.at("case_value"), value.case_value);
        if (data.contains("statements")) from_json(data.at("statements"), value.statements);
    }

    EXPORT template <>
    JSON to_json(Switch_expression const& value)
    {
        JSON data;
        data["value"] = to_json(value.value);
        data["cases"] = to_json(value.cases);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Switch_expression& value)
    {
        from_json(data.at("value"), value.value);
        if (data.contains("cases")) from_json(data.at("cases"), value.cases);
    }

    EXPORT template <>
    JSON to_json(Ternary_condition_expression const& value)
    {
        JSON data;
        data["condition"] = to_json(value.condition);
        data["then_statement"] = to_json(value.then_statement);
        data["else_statement"] = to_json(value.else_statement);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Ternary_condition_expression& value)
    {
        from_json(data.at("condition"), value.condition);
        from_json(data.at("then_statement"), value.then_statement);
        from_json(data.at("else_statement"), value.else_statement);
    }

    EXPORT template <>
    JSON to_json(Type_expression const& value)
    {
        JSON data;
        data["type"] = to_json(value.type);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Type_expression& value)
    {
        from_json(data.at("type"), value.type);
    }

    EXPORT template <>
    JSON to_json(Unary_expression const& value)
    {
        JSON data;
        data["expression"] = to_json(value.expression);
        data["operation"] = to_json(value.operation);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Unary_expression& value)
    {
        from_json(data.at("expression"), value.expression);
        from_json(data.at("operation"), value.operation);
    }

    EXPORT template <>
    JSON to_json(Union_expression const& value)
    {
        JSON data;
        data["declaration"] = to_json(value.declaration);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Union_expression& value)
    {
        from_json(data.at("declaration"), value.declaration);
    }

    EXPORT template <>
    JSON to_json(Variable_declaration_expression const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        data["is_mutable"] = to_json(value.is_mutable);
        data["right_hand_side"] = to_json(value.right_hand_side);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Variable_declaration_expression& value)
    {
        from_json(data.at("name"), value.name);
        from_json(data.at("is_mutable"), value.is_mutable);
        from_json(data.at("right_hand_side"), value.right_hand_side);
    }

    EXPORT template <>
    JSON to_json(Variable_declaration_with_type_expression const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        data["is_mutable"] = to_json(value.is_mutable);
        data["type"] = to_json(value.type);
        data["right_hand_side"] = to_json(value.right_hand_side);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Variable_declaration_with_type_expression& value)
    {
        from_json(data.at("name"), value.name);
        from_json(data.at("is_mutable"), value.is_mutable);
        from_json(data.at("type"), value.type);
        from_json(data.at("right_hand_side"), value.right_hand_side);
    }

    EXPORT template <>
    JSON to_json(While_loop_expression const& value)
    {
        JSON data;
        data["condition"] = to_json(value.condition);
        data["then_statements"] = to_json(value.then_statements);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, While_loop_expression& value)
    {
        from_json(data.at("condition"), value.condition);
        if (data.contains("then_statements")) from_json(data.at("then_statements"), value.then_statements);
    }

    EXPORT template <>
    JSON to_json(Expression const& value)
    {
        JSON data;
        data["data"] = to_json(value.data);
        if (value.source_range.has_value()) data["source_range"] = to_json(value.source_range);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Expression& value)
    {
        from_json(data.at("data"), value.data);
        if (data.contains("source_range")) from_json(data.at("source_range"), value.source_range);
    }

    EXPORT template <>
    JSON to_json(Type_constructor_parameter const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        data["type"] = to_json(value.type);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Type_constructor_parameter& value)
    {
        from_json(data.at("name"), value.name);
        from_json(data.at("type"), value.type);
    }

    EXPORT template <>
    JSON to_json(Type_constructor const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        data["parameters"] = to_json(value.parameters);
        data["statements"] = to_json(value.statements);
        if (value.comment.has_value()) data["comment"] = to_json(value.comment);
        if (value.source_location.has_value()) data["source_location"] = to_json(value.source_location);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Type_constructor& value)
    {
        from_json(data.at("name"), value.name);
        if (data.contains("parameters")) from_json(data.at("parameters"), value.parameters);
        if (data.contains("statements")) from_json(data.at("statements"), value.statements);
        if (data.contains("comment")) from_json(data.at("comment"), value.comment);
        if (data.contains("source_location")) from_json(data.at("source_location"), value.source_location);
    }

    EXPORT template <>
    JSON to_json(Function_constructor_parameter const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        data["type"] = to_json(value.type);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Function_constructor_parameter& value)
    {
        from_json(data.at("name"), value.name);
        from_json(data.at("type"), value.type);
    }

    EXPORT template <>
    JSON to_json(Function_constructor const& value)
    {
        JSON data;
        data["name"] = to_json(value.name);
        data["parameters"] = to_json(value.parameters);
        data["statements"] = to_json(value.statements);
        if (value.comment.has_value()) data["comment"] = to_json(value.comment);
        if (value.source_location.has_value()) data["source_location"] = to_json(value.source_location);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Function_constructor& value)
    {
        from_json(data.at("name"), value.name);
        if (data.contains("parameters")) from_json(data.at("parameters"), value.parameters);
        if (data.contains("statements")) from_json(data.at("statements"), value.statements);
        if (data.contains("comment")) from_json(data.at("comment"), value.comment);
        if (data.contains("source_location")) from_json(data.at("source_location"), value.source_location);
    }

    EXPORT template <>
    JSON to_json(Language_version const& value)
    {
        JSON data;
        data["major"] = to_json(value.major);
        data["minor"] = to_json(value.minor);
        data["patch"] = to_json(value.patch);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Language_version& value)
    {
        from_json(data.at("major"), value.major);
        from_json(data.at("minor"), value.minor);
        from_json(data.at("patch"), value.patch);
    }

    EXPORT template <>
    JSON to_json(Import_module_with_alias const& value)
    {
        JSON data;
        data["module_name"] = to_json(value.module_name);
        data["alias"] = to_json(value.alias);
        data["usages"] = to_json(value.usages);
        if (value.source_range.has_value()) data["source_range"] = to_json(value.source_range);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Import_module_with_alias& value)
    {
        from_json(data.at("module_name"), value.module_name);
        from_json(data.at("alias"), value.alias);
        if (data.contains("usages")) from_json(data.at("usages"), value.usages);
        if (data.contains("source_range")) from_json(data.at("source_range"), value.source_range);
    }

    EXPORT template <>
    JSON to_json(Module_dependencies const& value)
    {
        JSON data;
        data["alias_imports"] = to_json(value.alias_imports);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Module_dependencies& value)
    {
        if (data.contains("alias_imports")) from_json(data.at("alias_imports"), value.alias_imports);
    }

    EXPORT template <>
    JSON to_json(Module_declarations const& value)
    {
        JSON data;
        data["alias_type_declarations"] = to_json(value.alias_type_declarations);
        data["enum_declarations"] = to_json(value.enum_declarations);
        data["forward_declarations"] = to_json(value.forward_declarations);
        data["global_variable_declarations"] = to_json(value.global_variable_declarations);
        data["struct_declarations"] = to_json(value.struct_declarations);
        data["union_declarations"] = to_json(value.union_declarations);
        data["function_declarations"] = to_json(value.function_declarations);
        data["function_constructors"] = to_json(value.function_constructors);
        data["type_constructors"] = to_json(value.type_constructors);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Module_declarations& value)
    {
        if (data.contains("alias_type_declarations")) from_json(data.at("alias_type_declarations"), value.alias_type_declarations);
        if (data.contains("enum_declarations")) from_json(data.at("enum_declarations"), value.enum_declarations);
        if (data.contains("forward_declarations")) from_json(data.at("forward_declarations"), value.forward_declarations);
        if (data.contains("global_variable_declarations")) from_json(data.at("global_variable_declarations"), value.global_variable_declarations);
        if (data.contains("struct_declarations")) from_json(data.at("struct_declarations"), value.struct_declarations);
        if (data.contains("union_declarations")) from_json(data.at("union_declarations"), value.union_declarations);
        if (data.contains("function_declarations")) from_json(data.at("function_declarations"), value.function_declarations);
        if (data.contains("function_constructors")) from_json(data.at("function_constructors"), value.function_constructors);
        if (data.contains("type_constructors")) from_json(data.at("type_constructors"), value.type_constructors);
    }

    EXPORT template <>
    JSON to_json(Module_instanced_declarations const& value)
    {
        JSON data;
        data["struct_declarations"] = to_json(value.struct_declarations);
        data["union_declarations"] = to_json(value.union_declarations);
        data["function_declarations"] = to_json(value.function_declarations);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Module_instanced_declarations& value)
    {
        if (data.contains("struct_declarations")) from_json(data.at("struct_declarations"), value.struct_declarations);
        if (data.contains("union_declarations")) from_json(data.at("union_declarations"), value.union_declarations);
        if (data.contains("function_declarations")) from_json(data.at("function_declarations"), value.function_declarations);
    }

    EXPORT template <>
    JSON to_json(Module_definitions const& value)
    {
        JSON data;
        data["function_definitions"] = to_json(value.function_definitions);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Module_definitions& value)
    {
        if (data.contains("function_definitions")) from_json(data.at("function_definitions"), value.function_definitions);
    }

    EXPORT template <>
    JSON to_json(Module const& value)
    {
        JSON data;
        data["language_version"] = to_json(value.language_version);
        data["name"] = to_json(value.name);
        if (value.content_hash.has_value()) data["content_hash"] = to_json(value.content_hash);
        data["dependencies"] = to_json(value.dependencies);
        data["export_declarations"] = to_json(value.export_declarations);
        data["internal_declarations"] = to_json(value.internal_declarations);
        data["instanced_declarations"] = to_json(value.instanced_declarations);
        data["definitions"] = to_json(value.definitions);
        if (value.comment.has_value()) data["comment"] = to_json(value.comment);
        if (value.source_file_path.has_value()) data["source_file_path"] = to_json(value.source_file_path);
        return data;
    }

    EXPORT template <>
    void from_json(JSON const& data, Module& value)
    {
        from_json(data.at("language_version"), value.language_version);
        from_json(data.at("name"), value.name);
        if (data.contains("content_hash")) from_json(data.at("content_hash"), value.content_hash);
        from_json(data.at("dependencies"), value.dependencies);
        from_json(data.at("export_declarations"), value.export_declarations);
        from_json(data.at("internal_declarations"), value.internal_declarations);
        from_json(data.at("instanced_declarations"), value.instanced_declarations);
        from_json(data.at("definitions"), value.definitions);
        if (data.contains("comment")) from_json(data.at("comment"), value.comment);
        if (data.contains("source_file_path")) from_json(data.at("source_file_path"), value.source_file_path);
    }

}
