module;

#include <compare>

export module iris.binary_serializer.generated;

import iris.binary_serializer.generics;
import iris.core;

namespace iris::binary_serializer
{
    export template <typename T>
    void serialize(
        Serializer& serializer,
        T const& data
    );

    export template <typename T>
    void deserialize(
        Serializer& serializer,
        T& data
    );

    export template <>
    void serialize(Serializer& serializer, Source_location const& value)
    {
        serialize(serializer, value.file_path);
        serialize(serializer, value.line);
        serialize(serializer, value.column);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Source_location& value)
    {
        deserialize(deserializer, value.file_path);
        deserialize(deserializer, value.line);
        deserialize(deserializer, value.column);
    }

    export template <>
    void serialize(Serializer& serializer, Source_position const& value)
    {
        serialize(serializer, value.line);
        serialize(serializer, value.column);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Source_position& value)
    {
        deserialize(deserializer, value.line);
        deserialize(deserializer, value.column);
    }

    export template <>
    void serialize(Serializer& serializer, Source_range const& value)
    {
        serialize(serializer, value.start);
        serialize(serializer, value.end);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Source_range& value)
    {
        deserialize(deserializer, value.start);
        deserialize(deserializer, value.end);
    }

    export template <>
    void serialize(Serializer& serializer, Source_range_location const& value)
    {
        serialize(serializer, value.file_path);
        serialize(serializer, value.range);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Source_range_location& value)
    {
        deserialize(deserializer, value.file_path);
        deserialize(deserializer, value.range);
    }

    export template <>
    void serialize(Serializer& serializer, Integer_type const& value)
    {
        serialize(serializer, value.number_of_bits);
        serialize(serializer, value.is_signed);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Integer_type& value)
    {
        deserialize(deserializer, value.number_of_bits);
        deserialize(deserializer, value.is_signed);
    }

    export template <>
    void serialize(Serializer& serializer, Decimal_type const& value)
    {
        serialize(serializer, value.scale);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Decimal_type& value)
    {
        deserialize(deserializer, value.scale);
    }

    export template <>
    void serialize(Serializer& serializer, Array_slice_type const& value)
    {
        serialize(serializer, value.element_type);
        serialize(serializer, value.is_mutable);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Array_slice_type& value)
    {
        deserialize(deserializer, value.element_type);
        deserialize(deserializer, value.is_mutable);
    }

    export template <>
    void serialize(Serializer& serializer, Builtin_type_reference const& value)
    {
        serialize(serializer, value.value);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Builtin_type_reference& value)
    {
        deserialize(deserializer, value.value);
    }

    export template <>
    void serialize(Serializer& serializer, Function_type const& value)
    {
        serialize(serializer, value.input_parameter_types);
        serialize(serializer, value.output_parameter_types);
        serialize(serializer, value.is_variadic);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Function_type& value)
    {
        deserialize(deserializer, value.input_parameter_types);
        deserialize(deserializer, value.output_parameter_types);
        deserialize(deserializer, value.is_variadic);
    }

    export template <>
    void serialize(Serializer& serializer, Function_pointer_type const& value)
    {
        serialize(serializer, value.type);
        serialize(serializer, value.input_parameter_names);
        serialize(serializer, value.output_parameter_names);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Function_pointer_type& value)
    {
        deserialize(deserializer, value.type);
        deserialize(deserializer, value.input_parameter_names);
        deserialize(deserializer, value.output_parameter_names);
    }

    export template <>
    void serialize(Serializer& serializer, Null_pointer_type const& value)
    {

    }

    export template <>
    void deserialize(Deserializer& deserializer, Null_pointer_type& value)
    {

    }

    export template <>
    void serialize(Serializer& serializer, Pointer_type const& value)
    {
        serialize(serializer, value.element_type);
        serialize(serializer, value.is_mutable);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Pointer_type& value)
    {
        deserialize(deserializer, value.element_type);
        deserialize(deserializer, value.is_mutable);
    }

    export template <>
    void serialize(Serializer& serializer, Module_reference const& value)
    {
        serialize(serializer, value.name);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Module_reference& value)
    {
        deserialize(deserializer, value.name);
    }

    export template <>
    void serialize(Serializer& serializer, Constant_array_type const& value)
    {
        serialize(serializer, value.value_type);
        serialize(serializer, value.size);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Constant_array_type& value)
    {
        deserialize(deserializer, value.value_type);
        deserialize(deserializer, value.size);
    }

    export template <>
    void serialize(Serializer& serializer, Soa_array_type const& value)
    {
        serialize(serializer, value.value_type);
        serialize(serializer, value.size);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Soa_array_type& value)
    {
        deserialize(deserializer, value.value_type);
        deserialize(deserializer, value.size);
    }

    export template <>
    void serialize(Serializer& serializer, Soa_array_view_type const& value)
    {
        serialize(serializer, value.value_type);
        serialize(serializer, value.is_mutable);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Soa_array_view_type& value)
    {
        deserialize(deserializer, value.value_type);
        deserialize(deserializer, value.is_mutable);
    }

    export template <>
    void serialize(Serializer& serializer, Custom_type_reference const& value)
    {
        serialize(serializer, value.module_reference);
        serialize(serializer, value.name);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Custom_type_reference& value)
    {
        deserialize(deserializer, value.module_reference);
        deserialize(deserializer, value.name);
    }

    export template <>
    void serialize(Serializer& serializer, Type_instance const& value)
    {
        serialize(serializer, value.type_constructor);
        serialize(serializer, value.arguments);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Type_instance& value)
    {
        deserialize(deserializer, value.type_constructor);
        deserialize(deserializer, value.arguments);
    }

    export template <>
    void serialize(Serializer& serializer, Parameter_type const& value)
    {
        serialize(serializer, value.name);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Parameter_type& value)
    {
        deserialize(deserializer, value.name);
    }

    export template <>
    void serialize(Serializer& serializer, Type_reference const& value)
    {
        serialize(serializer, value.data);
        serialize(serializer, value.source_range);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Type_reference& value)
    {
        deserialize(deserializer, value.data);
        deserialize(deserializer, value.source_range);
    }

    export template <>
    void serialize(Serializer& serializer, Indexed_comment const& value)
    {
        serialize(serializer, value.index);
        serialize(serializer, value.comment);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Indexed_comment& value)
    {
        deserialize(deserializer, value.index);
        deserialize(deserializer, value.comment);
    }

    export template <>
    void serialize(Serializer& serializer, Statement const& value)
    {
        serialize(serializer, value.expressions);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Statement& value)
    {
        deserialize(deserializer, value.expressions);
    }

    export template <>
    void serialize(Serializer& serializer, Global_variable_declaration const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.unique_name);
        serialize(serializer, value.type);
        serialize(serializer, value.initial_value);
        serialize(serializer, value.global_type);
        serialize(serializer, value.comment);
        serialize(serializer, value.source_location);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Global_variable_declaration& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.unique_name);
        deserialize(deserializer, value.type);
        deserialize(deserializer, value.initial_value);
        deserialize(deserializer, value.global_type);
        deserialize(deserializer, value.comment);
        deserialize(deserializer, value.source_location);
    }

    export template <>
    void serialize(Serializer& serializer, Alias_type_declaration const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.unique_name);
        serialize(serializer, value.type);
        serialize(serializer, value.comment);
        serialize(serializer, value.source_location);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Alias_type_declaration& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.unique_name);
        deserialize(deserializer, value.type);
        deserialize(deserializer, value.comment);
        deserialize(deserializer, value.source_location);
    }

    export template <>
    void serialize(Serializer& serializer, Enum_value const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.value);
        serialize(serializer, value.comment);
        serialize(serializer, value.source_location);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Enum_value& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.value);
        deserialize(deserializer, value.comment);
        deserialize(deserializer, value.source_location);
    }

    export template <>
    void serialize(Serializer& serializer, Enum_declaration const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.unique_name);
        serialize(serializer, value.values);
        serialize(serializer, value.comment);
        serialize(serializer, value.source_location);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Enum_declaration& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.unique_name);
        deserialize(deserializer, value.values);
        deserialize(deserializer, value.comment);
        deserialize(deserializer, value.source_location);
    }

    export template <>
    void serialize(Serializer& serializer, Forward_declaration const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.unique_name);
        serialize(serializer, value.source_location);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Forward_declaration& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.unique_name);
        deserialize(deserializer, value.source_location);
    }

    export template <>
    void serialize(Serializer& serializer, Struct_declaration const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.unique_name);
        serialize(serializer, value.member_types);
        serialize(serializer, value.member_names);
        serialize(serializer, value.member_bit_fields);
        serialize(serializer, value.member_default_values);
        serialize(serializer, value.is_packed);
        serialize(serializer, value.is_literal);
        serialize(serializer, value.comment);
        serialize(serializer, value.member_comments);
        serialize(serializer, value.source_location);
        serialize(serializer, value.member_source_positions);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Struct_declaration& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.unique_name);
        deserialize(deserializer, value.member_types);
        deserialize(deserializer, value.member_names);
        deserialize(deserializer, value.member_bit_fields);
        deserialize(deserializer, value.member_default_values);
        deserialize(deserializer, value.is_packed);
        deserialize(deserializer, value.is_literal);
        deserialize(deserializer, value.comment);
        deserialize(deserializer, value.member_comments);
        deserialize(deserializer, value.source_location);
        deserialize(deserializer, value.member_source_positions);
    }

    export template <>
    void serialize(Serializer& serializer, Union_declaration const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.unique_name);
        serialize(serializer, value.member_types);
        serialize(serializer, value.member_names);
        serialize(serializer, value.comment);
        serialize(serializer, value.member_comments);
        serialize(serializer, value.source_location);
        serialize(serializer, value.member_source_positions);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Union_declaration& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.unique_name);
        deserialize(deserializer, value.member_types);
        deserialize(deserializer, value.member_names);
        deserialize(deserializer, value.comment);
        deserialize(deserializer, value.member_comments);
        deserialize(deserializer, value.source_location);
        deserialize(deserializer, value.member_source_positions);
    }

    export template <>
    void serialize(Serializer& serializer, Function_condition const& value)
    {
        serialize(serializer, value.description);
        serialize(serializer, value.condition);
        serialize(serializer, value.source_range);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Function_condition& value)
    {
        deserialize(deserializer, value.description);
        deserialize(deserializer, value.condition);
        deserialize(deserializer, value.source_range);
    }

    export template <>
    void serialize(Serializer& serializer, Function_declaration const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.unique_name);
        serialize(serializer, value.type);
        serialize(serializer, value.input_parameter_names);
        serialize(serializer, value.output_parameter_names);
        serialize(serializer, value.linkage);
        serialize(serializer, value.is_test);
        serialize(serializer, value.preconditions);
        serialize(serializer, value.postconditions);
        serialize(serializer, value.comment);
        serialize(serializer, value.source_location);
        serialize(serializer, value.input_parameter_source_positions);
        serialize(serializer, value.output_parameter_source_positions);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Function_declaration& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.unique_name);
        deserialize(deserializer, value.type);
        deserialize(deserializer, value.input_parameter_names);
        deserialize(deserializer, value.output_parameter_names);
        deserialize(deserializer, value.linkage);
        deserialize(deserializer, value.is_test);
        deserialize(deserializer, value.preconditions);
        deserialize(deserializer, value.postconditions);
        deserialize(deserializer, value.comment);
        deserialize(deserializer, value.source_location);
        deserialize(deserializer, value.input_parameter_source_positions);
        deserialize(deserializer, value.output_parameter_source_positions);
    }

    export template <>
    void serialize(Serializer& serializer, Function_definition const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.statements);
        serialize(serializer, value.source_location);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Function_definition& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.statements);
        deserialize(deserializer, value.source_location);
    }

    export template <>
    void serialize(Serializer& serializer, Variable_expression const& value)
    {
        serialize(serializer, value.name);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Variable_expression& value)
    {
        deserialize(deserializer, value.name);
    }

    export template <>
    void serialize(Serializer& serializer, Expression_index const& value)
    {
        serialize(serializer, value.expression_index);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Expression_index& value)
    {
        deserialize(deserializer, value.expression_index);
    }

    export template <>
    void serialize(Serializer& serializer, Access_expression const& value)
    {
        serialize(serializer, value.expression);
        serialize(serializer, value.member_name);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Access_expression& value)
    {
        deserialize(deserializer, value.expression);
        deserialize(deserializer, value.member_name);
    }

    export template <>
    void serialize(Serializer& serializer, Access_array_expression const& value)
    {
        serialize(serializer, value.expression);
        serialize(serializer, value.index);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Access_array_expression& value)
    {
        deserialize(deserializer, value.expression);
        deserialize(deserializer, value.index);
    }

    export template <>
    void serialize(Serializer& serializer, Assert_expression const& value)
    {
        serialize(serializer, value.message);
        serialize(serializer, value.statement);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Assert_expression& value)
    {
        deserialize(deserializer, value.message);
        deserialize(deserializer, value.statement);
    }

    export template <>
    void serialize(Serializer& serializer, Assignment_expression const& value)
    {
        serialize(serializer, value.left_hand_side);
        serialize(serializer, value.right_hand_side);
        serialize(serializer, value.additional_operation);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Assignment_expression& value)
    {
        deserialize(deserializer, value.left_hand_side);
        deserialize(deserializer, value.right_hand_side);
        deserialize(deserializer, value.additional_operation);
    }

    export template <>
    void serialize(Serializer& serializer, Binary_expression const& value)
    {
        serialize(serializer, value.left_hand_side);
        serialize(serializer, value.right_hand_side);
        serialize(serializer, value.operation);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Binary_expression& value)
    {
        deserialize(deserializer, value.left_hand_side);
        deserialize(deserializer, value.right_hand_side);
        deserialize(deserializer, value.operation);
    }

    export template <>
    void serialize(Serializer& serializer, Block_expression const& value)
    {
        serialize(serializer, value.statements);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Block_expression& value)
    {
        deserialize(deserializer, value.statements);
    }

    export template <>
    void serialize(Serializer& serializer, Break_expression const& value)
    {
        serialize(serializer, value.loop_count);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Break_expression& value)
    {
        deserialize(deserializer, value.loop_count);
    }

    export template <>
    void serialize(Serializer& serializer, Call_expression const& value)
    {
        serialize(serializer, value.expression);
        serialize(serializer, value.arguments);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Call_expression& value)
    {
        deserialize(deserializer, value.expression);
        deserialize(deserializer, value.arguments);
    }

    export template <>
    void serialize(Serializer& serializer, Cast_expression const& value)
    {
        serialize(serializer, value.source);
        serialize(serializer, value.destination_type);
        serialize(serializer, value.cast_type);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Cast_expression& value)
    {
        deserialize(deserializer, value.source);
        deserialize(deserializer, value.destination_type);
        deserialize(deserializer, value.cast_type);
    }

    export template <>
    void serialize(Serializer& serializer, Comment_expression const& value)
    {
        serialize(serializer, value.comment);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Comment_expression& value)
    {
        deserialize(deserializer, value.comment);
    }

    export template <>
    void serialize(Serializer& serializer, Compile_time_expression const& value)
    {
        serialize(serializer, value.expression);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Compile_time_expression& value)
    {
        deserialize(deserializer, value.expression);
    }

    export template <>
    void serialize(Serializer& serializer, Constant_expression const& value)
    {
        serialize(serializer, value.type);
        serialize(serializer, value.data);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Constant_expression& value)
    {
        deserialize(deserializer, value.type);
        deserialize(deserializer, value.data);
    }

    export template <>
    void serialize(Serializer& serializer, Constant_array_expression const& value)
    {
        serialize(serializer, value.array_data);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Constant_array_expression& value)
    {
        deserialize(deserializer, value.array_data);
    }

    export template <>
    void serialize(Serializer& serializer, Continue_expression const& value)
    {

    }

    export template <>
    void deserialize(Deserializer& deserializer, Continue_expression& value)
    {

    }

    export template <>
    void serialize(Serializer& serializer, Defer_expression const& value)
    {
        serialize(serializer, value.expression_to_defer);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Defer_expression& value)
    {
        deserialize(deserializer, value.expression_to_defer);
    }

    export template <>
    void serialize(Serializer& serializer, Dereference_and_access_expression const& value)
    {
        serialize(serializer, value.expression);
        serialize(serializer, value.member_name);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Dereference_and_access_expression& value)
    {
        deserialize(deserializer, value.expression);
        deserialize(deserializer, value.member_name);
    }

    export template <>
    void serialize(Serializer& serializer, For_loop_expression const& value)
    {
        serialize(serializer, value.variable_name);
        serialize(serializer, value.range_begin);
        serialize(serializer, value.range_end);
        serialize(serializer, value.range_comparison_operation);
        serialize(serializer, value.step_by);
        serialize(serializer, value.then_statements);
    }

    export template <>
    void deserialize(Deserializer& deserializer, For_loop_expression& value)
    {
        deserialize(deserializer, value.variable_name);
        deserialize(deserializer, value.range_begin);
        deserialize(deserializer, value.range_end);
        deserialize(deserializer, value.range_comparison_operation);
        deserialize(deserializer, value.step_by);
        deserialize(deserializer, value.then_statements);
    }

    export template <>
    void serialize(Serializer& serializer, Function_expression const& value)
    {
        serialize(serializer, value.declaration);
        serialize(serializer, value.definition);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Function_expression& value)
    {
        deserialize(deserializer, value.declaration);
        deserialize(deserializer, value.definition);
    }

    export template <>
    void serialize(Serializer& serializer, Instance_call_expression const& value)
    {
        serialize(serializer, value.left_hand_side);
        serialize(serializer, value.arguments);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Instance_call_expression& value)
    {
        deserialize(deserializer, value.left_hand_side);
        deserialize(deserializer, value.arguments);
    }

    export template <>
    void serialize(Serializer& serializer, Instance_call_key const& value)
    {
        serialize(serializer, value.module_name);
        serialize(serializer, value.function_constructor_name);
        serialize(serializer, value.arguments);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Instance_call_key& value)
    {
        deserialize(deserializer, value.module_name);
        deserialize(deserializer, value.function_constructor_name);
        deserialize(deserializer, value.arguments);
    }

    export template <>
    void serialize(Serializer& serializer, Condition_statement_pair const& value)
    {
        serialize(serializer, value.condition);
        serialize(serializer, value.then_statements);
        serialize(serializer, value.block_source_range);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Condition_statement_pair& value)
    {
        deserialize(deserializer, value.condition);
        deserialize(deserializer, value.then_statements);
        deserialize(deserializer, value.block_source_range);
    }

    export template <>
    void serialize(Serializer& serializer, If_expression const& value)
    {
        serialize(serializer, value.series);
    }

    export template <>
    void deserialize(Deserializer& deserializer, If_expression& value)
    {
        deserialize(deserializer, value.series);
    }

    export template <>
    void serialize(Serializer& serializer, Instantiate_member_value_pair const& value)
    {
        serialize(serializer, value.member_name);
        serialize(serializer, value.value);
        serialize(serializer, value.source_range);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Instantiate_member_value_pair& value)
    {
        deserialize(deserializer, value.member_name);
        deserialize(deserializer, value.value);
        deserialize(deserializer, value.source_range);
    }

    export template <>
    void serialize(Serializer& serializer, Instantiate_expression const& value)
    {
        serialize(serializer, value.type);
        serialize(serializer, value.members);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Instantiate_expression& value)
    {
        deserialize(deserializer, value.type);
        deserialize(deserializer, value.members);
    }

    export template <>
    void serialize(Serializer& serializer, Invalid_expression const& value)
    {
        serialize(serializer, value.value);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Invalid_expression& value)
    {
        deserialize(deserializer, value.value);
    }

    export template <>
    void serialize(Serializer& serializer, Null_pointer_expression const& value)
    {

    }

    export template <>
    void deserialize(Deserializer& deserializer, Null_pointer_expression& value)
    {

    }

    export template <>
    void serialize(Serializer& serializer, Parenthesis_expression const& value)
    {
        serialize(serializer, value.expression);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Parenthesis_expression& value)
    {
        deserialize(deserializer, value.expression);
    }

    export template <>
    void serialize(Serializer& serializer, Reflection_expression const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.type_arguments);
        serialize(serializer, value.arguments);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Reflection_expression& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.type_arguments);
        deserialize(deserializer, value.arguments);
    }

    export template <>
    void serialize(Serializer& serializer, Return_expression const& value)
    {
        serialize(serializer, value.expression);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Return_expression& value)
    {
        deserialize(deserializer, value.expression);
    }

    export template <>
    void serialize(Serializer& serializer, Struct_expression const& value)
    {
        serialize(serializer, value.declaration);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Struct_expression& value)
    {
        deserialize(deserializer, value.declaration);
    }

    export template <>
    void serialize(Serializer& serializer, Switch_case_expression_pair const& value)
    {
        serialize(serializer, value.case_value);
        serialize(serializer, value.statements);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Switch_case_expression_pair& value)
    {
        deserialize(deserializer, value.case_value);
        deserialize(deserializer, value.statements);
    }

    export template <>
    void serialize(Serializer& serializer, Switch_expression const& value)
    {
        serialize(serializer, value.value);
        serialize(serializer, value.cases);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Switch_expression& value)
    {
        deserialize(deserializer, value.value);
        deserialize(deserializer, value.cases);
    }

    export template <>
    void serialize(Serializer& serializer, Ternary_condition_expression const& value)
    {
        serialize(serializer, value.condition);
        serialize(serializer, value.then_statement);
        serialize(serializer, value.else_statement);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Ternary_condition_expression& value)
    {
        deserialize(deserializer, value.condition);
        deserialize(deserializer, value.then_statement);
        deserialize(deserializer, value.else_statement);
    }

    export template <>
    void serialize(Serializer& serializer, Type_expression const& value)
    {
        serialize(serializer, value.type);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Type_expression& value)
    {
        deserialize(deserializer, value.type);
    }

    export template <>
    void serialize(Serializer& serializer, Unary_expression const& value)
    {
        serialize(serializer, value.expression);
        serialize(serializer, value.operation);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Unary_expression& value)
    {
        deserialize(deserializer, value.expression);
        deserialize(deserializer, value.operation);
    }

    export template <>
    void serialize(Serializer& serializer, Union_expression const& value)
    {
        serialize(serializer, value.declaration);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Union_expression& value)
    {
        deserialize(deserializer, value.declaration);
    }

    export template <>
    void serialize(Serializer& serializer, Variable_declaration_expression const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.is_mutable);
        serialize(serializer, value.right_hand_side);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Variable_declaration_expression& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.is_mutable);
        deserialize(deserializer, value.right_hand_side);
    }

    export template <>
    void serialize(Serializer& serializer, Variable_declaration_with_type_expression const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.is_mutable);
        serialize(serializer, value.type);
        serialize(serializer, value.right_hand_side);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Variable_declaration_with_type_expression& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.is_mutable);
        deserialize(deserializer, value.type);
        deserialize(deserializer, value.right_hand_side);
    }

    export template <>
    void serialize(Serializer& serializer, While_loop_expression const& value)
    {
        serialize(serializer, value.condition);
        serialize(serializer, value.then_statements);
    }

    export template <>
    void deserialize(Deserializer& deserializer, While_loop_expression& value)
    {
        deserialize(deserializer, value.condition);
        deserialize(deserializer, value.then_statements);
    }

    export template <>
    void serialize(Serializer& serializer, Expression const& value)
    {
        serialize(serializer, value.data);
        serialize(serializer, value.source_range);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Expression& value)
    {
        deserialize(deserializer, value.data);
        deserialize(deserializer, value.source_range);
    }

    export template <>
    void serialize(Serializer& serializer, Type_constructor_parameter const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.type);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Type_constructor_parameter& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.type);
    }

    export template <>
    void serialize(Serializer& serializer, Type_constructor const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.parameters);
        serialize(serializer, value.statements);
        serialize(serializer, value.comment);
        serialize(serializer, value.source_location);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Type_constructor& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.parameters);
        deserialize(deserializer, value.statements);
        deserialize(deserializer, value.comment);
        deserialize(deserializer, value.source_location);
    }

    export template <>
    void serialize(Serializer& serializer, Function_constructor_parameter const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.type);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Function_constructor_parameter& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.type);
    }

    export template <>
    void serialize(Serializer& serializer, Function_constructor const& value)
    {
        serialize(serializer, value.name);
        serialize(serializer, value.parameters);
        serialize(serializer, value.statements);
        serialize(serializer, value.comment);
        serialize(serializer, value.source_location);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Function_constructor& value)
    {
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.parameters);
        deserialize(deserializer, value.statements);
        deserialize(deserializer, value.comment);
        deserialize(deserializer, value.source_location);
    }

    export template <>
    void serialize(Serializer& serializer, Language_version const& value)
    {
        serialize(serializer, value.major);
        serialize(serializer, value.minor);
        serialize(serializer, value.patch);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Language_version& value)
    {
        deserialize(deserializer, value.major);
        deserialize(deserializer, value.minor);
        deserialize(deserializer, value.patch);
    }

    export template <>
    void serialize(Serializer& serializer, Import_module_with_alias const& value)
    {
        serialize(serializer, value.module_name);
        serialize(serializer, value.alias);
        serialize(serializer, value.usages);
        serialize(serializer, value.source_range);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Import_module_with_alias& value)
    {
        deserialize(deserializer, value.module_name);
        deserialize(deserializer, value.alias);
        deserialize(deserializer, value.usages);
        deserialize(deserializer, value.source_range);
    }

    export template <>
    void serialize(Serializer& serializer, Module_dependencies const& value)
    {
        serialize(serializer, value.alias_imports);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Module_dependencies& value)
    {
        deserialize(deserializer, value.alias_imports);
    }

    export template <>
    void serialize(Serializer& serializer, Module_declarations const& value)
    {
        serialize(serializer, value.alias_type_declarations);
        serialize(serializer, value.enum_declarations);
        serialize(serializer, value.forward_declarations);
        serialize(serializer, value.global_variable_declarations);
        serialize(serializer, value.struct_declarations);
        serialize(serializer, value.union_declarations);
        serialize(serializer, value.function_declarations);
        serialize(serializer, value.function_constructors);
        serialize(serializer, value.type_constructors);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Module_declarations& value)
    {
        deserialize(deserializer, value.alias_type_declarations);
        deserialize(deserializer, value.enum_declarations);
        deserialize(deserializer, value.forward_declarations);
        deserialize(deserializer, value.global_variable_declarations);
        deserialize(deserializer, value.struct_declarations);
        deserialize(deserializer, value.union_declarations);
        deserialize(deserializer, value.function_declarations);
        deserialize(deserializer, value.function_constructors);
        deserialize(deserializer, value.type_constructors);
    }

    export template <>
    void serialize(Serializer& serializer, Module_instanced_declarations const& value)
    {
        serialize(serializer, value.struct_declarations);
        serialize(serializer, value.union_declarations);
        serialize(serializer, value.function_declarations);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Module_instanced_declarations& value)
    {
        deserialize(deserializer, value.struct_declarations);
        deserialize(deserializer, value.union_declarations);
        deserialize(deserializer, value.function_declarations);
    }

    export template <>
    void serialize(Serializer& serializer, Module_definitions const& value)
    {
        serialize(serializer, value.function_definitions);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Module_definitions& value)
    {
        deserialize(deserializer, value.function_definitions);
    }

    export template <>
    void serialize(Serializer& serializer, Module const& value)
    {
        serialize(serializer, value.language_version);
        serialize(serializer, value.name);
        serialize(serializer, value.content_hash);
        serialize(serializer, value.dependencies);
        serialize(serializer, value.export_declarations);
        serialize(serializer, value.internal_declarations);
        serialize(serializer, value.instanced_declarations);
        serialize(serializer, value.definitions);
        serialize(serializer, value.comment);
        serialize(serializer, value.source_file_path);
    }

    export template <>
    void deserialize(Deserializer& deserializer, Module& value)
    {
        deserialize(deserializer, value.language_version);
        deserialize(deserializer, value.name);
        deserialize(deserializer, value.content_hash);
        deserialize(deserializer, value.dependencies);
        deserialize(deserializer, value.export_declarations);
        deserialize(deserializer, value.internal_declarations);
        deserialize(deserializer, value.instanced_declarations);
        deserialize(deserializer, value.definitions);
        deserialize(deserializer, value.comment);
        deserialize(deserializer, value.source_file_path);
    }

}
