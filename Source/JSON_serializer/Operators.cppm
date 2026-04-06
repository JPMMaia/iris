module;

#include "Generics.h"

#include <compare>
#include <variant>

#include <nlohmann/json.hpp>

export module h.json_serializer.operators;

import h.json_serializer.generated;
//import h.json_serializer.generics;
import h.core;

namespace h::json::operators
{

        export std::istream& operator>>(std::istream& input_stream, Fundamental_type& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Fundamental_type const value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Global_variable_type& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Global_variable_type const value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Linkage& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Linkage const value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Binary_operation& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Binary_operation const value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Cast_type& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Cast_type const value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Instantiate_expression_type& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Instantiate_expression_type const value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Unary_operation& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Unary_operation const value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Source_location& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Source_location const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Source_position& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Source_position const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Source_range& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Source_range const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Source_range_location& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Source_range_location const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Integer_type& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Integer_type const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Decimal_type& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Decimal_type const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Array_slice_type& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Array_slice_type const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Builtin_type_reference& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Builtin_type_reference const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Function_type& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Function_type const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Function_pointer_type& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Function_pointer_type const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Null_pointer_type& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Null_pointer_type const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Pointer_type& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Pointer_type const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Module_reference& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Module_reference const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Constant_array_type& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Constant_array_type const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Custom_type_reference& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Custom_type_reference const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Type_instance& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Type_instance const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Parameter_type& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Parameter_type const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Type_reference& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Type_reference const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Indexed_comment& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Indexed_comment const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Statement& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Statement const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Global_variable_declaration& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Global_variable_declaration const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Alias_type_declaration& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Alias_type_declaration const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Enum_value& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Enum_value const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Enum_declaration& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Enum_declaration const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Forward_declaration& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Forward_declaration const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Struct_declaration& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Struct_declaration const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Union_declaration& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Union_declaration const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Function_condition& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Function_condition const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Function_declaration& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Function_declaration const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Function_definition& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Function_definition const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Variable_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Variable_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Expression_index& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Expression_index const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Access_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Access_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Access_array_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Access_array_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Assert_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Assert_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Assignment_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Assignment_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Binary_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Binary_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Block_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Block_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Break_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Break_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Call_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Call_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Cast_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Cast_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Comment_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Comment_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Compile_time_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Compile_time_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Constant_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Constant_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Constant_array_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Constant_array_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Continue_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Continue_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Defer_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Defer_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Dereference_and_access_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Dereference_and_access_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, For_loop_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, For_loop_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Function_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Function_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Instance_call_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Instance_call_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Instance_call_key& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Instance_call_key const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Condition_statement_pair& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Condition_statement_pair const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, If_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, If_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Instantiate_member_value_pair& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Instantiate_member_value_pair const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Instantiate_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Instantiate_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Invalid_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Invalid_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Null_pointer_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Null_pointer_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Parenthesis_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Parenthesis_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Reflection_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Reflection_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Return_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Return_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Struct_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Struct_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Switch_case_expression_pair& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Switch_case_expression_pair const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Switch_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Switch_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Ternary_condition_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Ternary_condition_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Type_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Type_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Unary_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Unary_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Union_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Union_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Variable_declaration_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Variable_declaration_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Variable_declaration_with_type_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Variable_declaration_with_type_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, While_loop_expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, While_loop_expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Expression& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Expression const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Type_constructor_parameter& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Type_constructor_parameter const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Type_constructor& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Type_constructor const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Function_constructor_parameter& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Function_constructor_parameter const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Function_constructor& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Function_constructor const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Language_version& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Language_version const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Import_module_with_alias& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Import_module_with_alias const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Module_dependencies& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Module_dependencies const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Module_declarations& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Module_declarations const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Module_instanced_declarations& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Module_instanced_declarations const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Module_definitions& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Module_definitions const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }

        export std::istream& operator>>(std::istream& input_stream, Module& value)
        {
            JSON data{};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }

        export std::ostream& operator<<(std::ostream& output_stream, Module const& value)
        {
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }
}
