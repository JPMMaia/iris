export module h.core.expressions_visitor;

import std;
import h.core;

namespace h
{

    export void visit_expressions_recursively(h::Statement const& statement, h::Expression const& expression, std::function<void(h::Statement const& statement, h::Expression const& expression)> const& predicate)
    {
        predicate(statement, expression);

        if (std::holds_alternative<Access_expression>(expression.data))
        {
            Access_expression const& data = std::get<Access_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.expression.expression_index], predicate);
        }
        
        if (std::holds_alternative<Access_array_expression>(expression.data))
        {
            Access_array_expression const& data = std::get<Access_array_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.expression.expression_index], predicate);
            visit_expressions_recursively(statement, statement.expressions[data.index.expression_index], predicate);
        }
        
        if (std::holds_alternative<Assignment_expression>(expression.data))
        {
            Assignment_expression const& data = std::get<Assignment_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.left_hand_side.expression_index], predicate);
            visit_expressions_recursively(statement, statement.expressions[data.right_hand_side.expression_index], predicate);
        }
        
        if (std::holds_alternative<Binary_expression>(expression.data))
        {
            Binary_expression const& data = std::get<Binary_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.left_hand_side.expression_index], predicate);
            visit_expressions_recursively(statement, statement.expressions[data.right_hand_side.expression_index], predicate);
        }
        
        if (std::holds_alternative<Call_expression>(expression.data))
        {
            Call_expression const& data = std::get<Call_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.expression.expression_index], predicate);
            for (std::size_t index = 0; index < data.arguments.size(); ++index)
                visit_expressions_recursively(statement, statement.expressions[data.arguments[index].expression_index], predicate);
        }
        
        if (std::holds_alternative<Cast_expression>(expression.data))
        {
            Cast_expression const& data = std::get<Cast_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.source.expression_index], predicate);
        }
        
        if (std::holds_alternative<Compile_time_expression>(expression.data))
        {
            Compile_time_expression const& data = std::get<Compile_time_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.expression.expression_index], predicate);
        }
        
        if (std::holds_alternative<Defer_expression>(expression.data))
        {
            Defer_expression const& data = std::get<Defer_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.expression_to_defer.expression_index], predicate);
        }
        
        if (std::holds_alternative<Dereference_and_access_expression>(expression.data))
        {
            Dereference_and_access_expression const& data = std::get<Dereference_and_access_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.expression.expression_index], predicate);
        }
        
        if (std::holds_alternative<For_loop_expression>(expression.data))
        {
            For_loop_expression const& data = std::get<For_loop_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.range_begin.expression_index], predicate);
            if (data.step_by.has_value())
                visit_expressions_recursively(statement, statement.expressions[data.step_by->expression_index], predicate);
        }
        
        if (std::holds_alternative<Instance_call_expression>(expression.data))
        {
            Instance_call_expression const& data = std::get<Instance_call_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.left_hand_side.expression_index], predicate);
        }
        
        if (std::holds_alternative<Parenthesis_expression>(expression.data))
        {
            Parenthesis_expression const& data = std::get<Parenthesis_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.expression.expression_index], predicate);
        }
        
        if (std::holds_alternative<Reflection_expression>(expression.data))
        {
            Reflection_expression const& data = std::get<Reflection_expression>(expression.data);

            for (std::size_t index = 0; index < data.arguments.size(); ++index)
                visit_expressions_recursively(statement, statement.expressions[data.arguments[index].expression_index], predicate);
        }
        
        if (std::holds_alternative<Return_expression>(expression.data))
        {
            Return_expression const& data = std::get<Return_expression>(expression.data);

            if (data.expression.has_value())
                visit_expressions_recursively(statement, statement.expressions[data.expression->expression_index], predicate);
        }
        
        if (std::holds_alternative<Switch_expression>(expression.data))
        {
            Switch_expression const& data = std::get<Switch_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.value.expression_index], predicate);
        }
        
        if (std::holds_alternative<Ternary_condition_expression>(expression.data))
        {
            Ternary_condition_expression const& data = std::get<Ternary_condition_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.condition.expression_index], predicate);
        }
        
        if (std::holds_alternative<Unary_expression>(expression.data))
        {
            Unary_expression const& data = std::get<Unary_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.expression.expression_index], predicate);
        }
        
        if (std::holds_alternative<Variable_declaration_expression>(expression.data))
        {
            Variable_declaration_expression const& data = std::get<Variable_declaration_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.right_hand_side.expression_index], predicate);
        }
        
        if (std::holds_alternative<Variable_declaration_with_type_expression>(expression.data))
        {
            Variable_declaration_with_type_expression const& data = std::get<Variable_declaration_with_type_expression>(expression.data);

            visit_expressions_recursively(statement, statement.expressions[data.right_hand_side.expression_index], predicate);
        }
        
    }
}
