module;

#include <filesystem>
#include <istream>
#include <optional>
#include <ostream>

#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/reader.h>
#include <rapidjson/writer.h>

export module h.json_serializer.operators;

import h.core;
import h.json_serializer;

namespace h::json::operators
{
    export std::istream& operator>>(std::istream& input_stream, Fundamental_type& value)
    {
        std::pmr::string string;
        input_stream >> string;

        value = h::json::read_enum<Fundamental_type>(string);

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Fundamental_type const value)
    {
        output_stream << h::json::write_enum(value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Global_variable_type& value)
    {
        std::pmr::string string;
        input_stream >> string;

        value = h::json::read_enum<Global_variable_type>(string);

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Global_variable_type const value)
    {
        output_stream << h::json::write_enum(value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Linkage& value)
    {
        std::pmr::string string;
        input_stream >> string;

        value = h::json::read_enum<Linkage>(string);

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Linkage const value)
    {
        output_stream << h::json::write_enum(value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Binary_operation& value)
    {
        std::pmr::string string;
        input_stream >> string;

        value = h::json::read_enum<Binary_operation>(string);

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Binary_operation const value)
    {
        output_stream << h::json::write_enum(value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Cast_type& value)
    {
        std::pmr::string string;
        input_stream >> string;

        value = h::json::read_enum<Cast_type>(string);

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Cast_type const value)
    {
        output_stream << h::json::write_enum(value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Instantiate_expression_type& value)
    {
        std::pmr::string string;
        input_stream >> string;

        value = h::json::read_enum<Instantiate_expression_type>(string);

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Instantiate_expression_type const value)
    {
        output_stream << h::json::write_enum(value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Unary_operation& value)
    {
        std::pmr::string string;
        input_stream >> string;

        value = h::json::read_enum<Unary_operation>(string);

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Unary_operation const value)
    {
        output_stream << h::json::write_enum(value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Source_location& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Source_location> const output = h::json::read<Source_location>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Source_location const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Source_position& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Source_position> const output = h::json::read<Source_position>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Source_position const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Source_range& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Source_range> const output = h::json::read<Source_range>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Source_range const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Source_range_location& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Source_range_location> const output = h::json::read<Source_range_location>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Source_range_location const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Integer_type& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Integer_type> const output = h::json::read<Integer_type>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Integer_type const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Array_slice_type& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Array_slice_type> const output = h::json::read<Array_slice_type>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Array_slice_type const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Builtin_type_reference& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Builtin_type_reference> const output = h::json::read<Builtin_type_reference>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Builtin_type_reference const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Function_type& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Function_type> const output = h::json::read<Function_type>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Function_type const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Function_pointer_type& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Function_pointer_type> const output = h::json::read<Function_pointer_type>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Function_pointer_type const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Null_pointer_type& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Null_pointer_type> const output = h::json::read<Null_pointer_type>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Null_pointer_type const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Pointer_type& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Pointer_type> const output = h::json::read<Pointer_type>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Pointer_type const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Module_reference& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Module_reference> const output = h::json::read<Module_reference>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Module_reference const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Constant_array_type& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Constant_array_type> const output = h::json::read<Constant_array_type>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Constant_array_type const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Custom_type_reference& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Custom_type_reference> const output = h::json::read<Custom_type_reference>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Custom_type_reference const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Type_instance& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Type_instance> const output = h::json::read<Type_instance>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Type_instance const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Parameter_type& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Parameter_type> const output = h::json::read<Parameter_type>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Parameter_type const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Type_reference& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Type_reference> const output = h::json::read<Type_reference>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Type_reference const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Indexed_comment& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Indexed_comment> const output = h::json::read<Indexed_comment>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Indexed_comment const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Statement& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Statement> const output = h::json::read<Statement>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Statement const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Global_variable_declaration& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Global_variable_declaration> const output = h::json::read<Global_variable_declaration>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Global_variable_declaration const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Alias_type_declaration& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Alias_type_declaration> const output = h::json::read<Alias_type_declaration>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Alias_type_declaration const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Enum_value& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Enum_value> const output = h::json::read<Enum_value>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Enum_value const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Enum_declaration& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Enum_declaration> const output = h::json::read<Enum_declaration>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Enum_declaration const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Forward_declaration& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Forward_declaration> const output = h::json::read<Forward_declaration>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Forward_declaration const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Struct_declaration& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Struct_declaration> const output = h::json::read<Struct_declaration>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Struct_declaration const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Union_declaration& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Union_declaration> const output = h::json::read<Union_declaration>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Union_declaration const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Function_condition& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Function_condition> const output = h::json::read<Function_condition>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Function_condition const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Function_declaration& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Function_declaration> const output = h::json::read<Function_declaration>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Function_declaration const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Function_definition& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Function_definition> const output = h::json::read<Function_definition>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Function_definition const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Variable_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Variable_expression> const output = h::json::read<Variable_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Variable_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Expression_index& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Expression_index> const output = h::json::read<Expression_index>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Expression_index const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Access_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Access_expression> const output = h::json::read<Access_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Access_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Access_array_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Access_array_expression> const output = h::json::read<Access_array_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Access_array_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Assert_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Assert_expression> const output = h::json::read<Assert_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Assert_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Assignment_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Assignment_expression> const output = h::json::read<Assignment_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Assignment_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Binary_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Binary_expression> const output = h::json::read<Binary_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Binary_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Block_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Block_expression> const output = h::json::read<Block_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Block_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Break_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Break_expression> const output = h::json::read<Break_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Break_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Call_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Call_expression> const output = h::json::read<Call_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Call_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Cast_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Cast_expression> const output = h::json::read<Cast_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Cast_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Comment_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Comment_expression> const output = h::json::read<Comment_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Comment_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Compile_time_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Compile_time_expression> const output = h::json::read<Compile_time_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Compile_time_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Constant_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Constant_expression> const output = h::json::read<Constant_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Constant_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Constant_array_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Constant_array_expression> const output = h::json::read<Constant_array_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Constant_array_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Continue_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Continue_expression> const output = h::json::read<Continue_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Continue_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Defer_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Defer_expression> const output = h::json::read<Defer_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Defer_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Dereference_and_access_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Dereference_and_access_expression> const output = h::json::read<Dereference_and_access_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Dereference_and_access_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, For_loop_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<For_loop_expression> const output = h::json::read<For_loop_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, For_loop_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Function_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Function_expression> const output = h::json::read<Function_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Function_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Instance_call_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Instance_call_expression> const output = h::json::read<Instance_call_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Instance_call_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Instance_call_key& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Instance_call_key> const output = h::json::read<Instance_call_key>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Instance_call_key const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Condition_statement_pair& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Condition_statement_pair> const output = h::json::read<Condition_statement_pair>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Condition_statement_pair const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, If_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<If_expression> const output = h::json::read<If_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, If_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Instantiate_member_value_pair& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Instantiate_member_value_pair> const output = h::json::read<Instantiate_member_value_pair>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Instantiate_member_value_pair const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Instantiate_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Instantiate_expression> const output = h::json::read<Instantiate_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Instantiate_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Invalid_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Invalid_expression> const output = h::json::read<Invalid_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Invalid_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Null_pointer_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Null_pointer_expression> const output = h::json::read<Null_pointer_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Null_pointer_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Parenthesis_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Parenthesis_expression> const output = h::json::read<Parenthesis_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Parenthesis_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Reflection_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Reflection_expression> const output = h::json::read<Reflection_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Reflection_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Return_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Return_expression> const output = h::json::read<Return_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Return_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Struct_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Struct_expression> const output = h::json::read<Struct_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Struct_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Switch_case_expression_pair& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Switch_case_expression_pair> const output = h::json::read<Switch_case_expression_pair>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Switch_case_expression_pair const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Switch_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Switch_expression> const output = h::json::read<Switch_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Switch_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Ternary_condition_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Ternary_condition_expression> const output = h::json::read<Ternary_condition_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Ternary_condition_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Type_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Type_expression> const output = h::json::read<Type_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Type_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Unary_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Unary_expression> const output = h::json::read<Unary_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Unary_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Union_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Union_expression> const output = h::json::read<Union_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Union_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Variable_declaration_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Variable_declaration_expression> const output = h::json::read<Variable_declaration_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Variable_declaration_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Variable_declaration_with_type_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Variable_declaration_with_type_expression> const output = h::json::read<Variable_declaration_with_type_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Variable_declaration_with_type_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, While_loop_expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<While_loop_expression> const output = h::json::read<While_loop_expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, While_loop_expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Expression& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Expression> const output = h::json::read<Expression>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Expression const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Type_constructor_parameter& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Type_constructor_parameter> const output = h::json::read<Type_constructor_parameter>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Type_constructor_parameter const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Type_constructor& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Type_constructor> const output = h::json::read<Type_constructor>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Type_constructor const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Function_constructor_parameter& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Function_constructor_parameter> const output = h::json::read<Function_constructor_parameter>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Function_constructor_parameter const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Function_constructor& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Function_constructor> const output = h::json::read<Function_constructor>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Function_constructor const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Language_version& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Language_version> const output = h::json::read<Language_version>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Language_version const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Import_module_with_alias& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Import_module_with_alias> const output = h::json::read<Import_module_with_alias>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Import_module_with_alias const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Module_dependencies& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Module_dependencies> const output = h::json::read<Module_dependencies>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Module_dependencies const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Module_declarations& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Module_declarations> const output = h::json::read<Module_declarations>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Module_declarations const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Module_definitions& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Module_definitions> const output = h::json::read<Module_definitions>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Module_definitions const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

    export std::istream& operator>>(std::istream& input_stream, Module& value)
    {
        rapidjson::Reader reader;
        rapidjson::IStreamWrapper stream_wrapper{ input_stream };
        std::optional<Module> const output = h::json::read<Module>(reader, stream_wrapper);

        if (output)
        {
            value = std::move(*output);
        }

        return input_stream;
    }

    export std::ostream& operator<<(std::ostream& output_stream, Module const& value)
    {
        rapidjson::OStreamWrapper stream_wrapper{ output_stream };
        rapidjson::Writer<rapidjson::OStreamWrapper> writer{ stream_wrapper };
        h::json::write(writer, value);

        return output_stream;
    }

}
