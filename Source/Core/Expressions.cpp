module;

#include <cassert>

module iris.core.expressions;

import std;

import iris.core;
import iris.core.expressions_visitor;
import iris.core.types;

namespace iris
{
    iris::Statement create_statement(std::pmr::vector<iris::Expression> expressions)
    {
        return iris::Statement
        {
            .expressions = std::move(expressions)
        };
    }

    iris::Expression create_call_expression(std::uint64_t const left_hand_side_expression, std::pmr::vector<Expression_index> arguments)
    {
        return iris::Expression
        {
            .data = iris::Call_expression
            {
                .expression = {.expression_index = left_hand_side_expression},
                .arguments = std::move(arguments)
            }
        };
    }

    iris::Expression create_constant_expression(Type_reference type_reference, std::string_view const data)
    {
        return iris::Expression
        {
            .data = iris::Constant_expression
            {
                .type = std::move(type_reference),
                .data = std::pmr::string{ data }
            }
        };
    }

    iris::Expression create_constant_array_expression(std::pmr::vector<iris::Statement> array_data)
    {
        return iris::Expression
        {
            .data = iris::Constant_array_expression
            {
                .array_data = std::move(array_data)
            }
        };
    }

    void add_enum_value_expressions(iris::Statement& statement, std::string_view const enum_name, std::string_view const member_name)
    {
        iris::Expression access_expression
        {
            .data = iris::Access_expression
            {
                .expression = {
                    .expression_index = statement.expressions.size() + 1
                },
                .member_name = std::pmr::string{ member_name },
            }
        };

        statement.expressions.push_back(std::move(access_expression));

        iris::Expression variable_expression
        {
            .data = iris::Variable_expression
            {
                .name = std::pmr::string{ enum_name },
            }
        };

        statement.expressions.push_back(std::move(variable_expression));
    }

    std::pmr::vector<iris::Expression> create_enum_value_expressions(std::string_view const enum_name, std::string_view const member_name)
    {
        iris::Statement statement = {};
        add_enum_value_expressions(statement, enum_name, member_name);
        return statement.expressions;
    }

    iris::Expression create_instantiate_expression(Instantiate_expression_type const type, std::pmr::vector<Instantiate_member_value_pair> members)
    {
        return iris::Expression
        {
            .data = iris::Instantiate_expression
            {
                .type = type,
                .members = std::move(members)
            }
        };
    }

    iris::Expression create_null_pointer_expression()
    {
        return create_expression(iris::Null_pointer_expression{});
    }

    iris::Expression create_variable_expression(std::pmr::string name)
    {
        return iris::Expression
        {
            .data = iris::Variable_expression
            {
                .name = std::move(name),
            }
        };
    }

    void invalidate_expression_and_descendants(
        iris::Statement& statement,
        std::size_t const expression_index,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<std::size_t> indices_to_invalidate{temporaries_allocator};
        indices_to_invalidate.reserve(statement.expressions.size());
        
        auto const invalidate_descendants = [&](iris::Statement const& statement, iris::Expression const& expression) -> bool
        {
            std::size_t const index = find_expression_index(statement, expression);
            indices_to_invalidate.push_back(index);
            return false;
        };

        iris::Expression const& expression_to_invalidate = statement.expressions[expression_index];
        visit_expressions_recursively(statement, expression_to_invalidate, invalidate_descendants);

        for (std::size_t index : indices_to_invalidate)
            statement.expressions[index] = iris::Expression{.data = iris::Invalid_expression{}};
    }

    std::size_t get_or_create_expression_slot(
        std::pmr::vector<iris::Expression>& expressions
    )
    {
        for (std::size_t index = 0; index < expressions.size(); ++index)
        {
            iris::Expression const& expression = expressions[index];
            if (std::holds_alternative<iris::Invalid_expression>(expression.data))
                return index;
        }

        expressions.push_back(iris::Expression{.data = iris::Invalid_expression{}});
        return expressions.size() - 1;
    }

    std::size_t find_expression_index(
        iris::Statement const& statement,
        iris::Expression const& expression
    )
    {
        for (std::size_t index = 0; index < statement.expressions.size(); ++index)
        {
            if (&statement.expressions[index] == &expression)
                return index;
        }

        return -1;
    }

    void replace_expression(
        iris::Statement& statement,
        iris::Expression const& expression,
        iris::Statement const& new_statement,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::size_t const expression_index = find_expression_index(statement, expression);
        invalidate_expression_and_descendants(statement, expression_index, temporaries_allocator);

        statement.expressions[expression_index] = new_statement.expressions[0];

        if (new_statement.expressions.size() > 1)
        {
            offset_expression_indices(statement.expressions[expression_index], static_cast<std::uint64_t>(statement.expressions.size() - 1));
            add_expressions_to_expressions(statement.expressions, {&new_statement.expressions[1], new_statement.expressions.size() - 1});
        }
    }

    void offset_expression_indices(
        iris::Expression& expression,
        std::uint64_t const offset
    )
    {
        auto const offset_index = [&](Expression_index& idx)
        {
            if (idx.expression_index != static_cast<std::uint64_t>(-1))
                idx.expression_index += offset;
        };

        std::visit([&](auto& data)
        {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, Access_expression>)
            {
                offset_index(data.expression);
            }
            else if constexpr (std::is_same_v<T, Access_array_expression>)
            {
                offset_index(data.expression);
                offset_index(data.index);
            }
            else if constexpr (std::is_same_v<T, Assignment_expression>)
            {
                offset_index(data.left_hand_side);
                offset_index(data.right_hand_side);
            }
            else if constexpr (std::is_same_v<T, Binary_expression>)
            {
                offset_index(data.left_hand_side);
                offset_index(data.right_hand_side);
            }
            else if constexpr (std::is_same_v<T, Call_expression>)
            {
                offset_index(data.expression);
                for (Expression_index& arg : data.arguments)
                    offset_index(arg);
            }
            else if constexpr (std::is_same_v<T, Cast_expression>)
            {
                offset_index(data.source);
            }
            else if constexpr (std::is_same_v<T, Compile_time_expression>)
            {
                offset_index(data.expression);
            }
            else if constexpr (std::is_same_v<T, Defer_expression>)
            {
                offset_index(data.expression_to_defer);
            }
            else if constexpr (std::is_same_v<T, Dereference_and_access_expression>)
            {
                offset_index(data.expression);
            }
            else if constexpr (std::is_same_v<T, For_loop_expression>)
            {
                offset_index(data.range_begin);
                if (data.step_by)
                    offset_index(*data.step_by);
            }
            else if constexpr (std::is_same_v<T, Instantiate_expression>)
            {
                for (Instantiate_member_value_pair& member : data.members)
                    offset_index(member.value);
            }
            else if constexpr (std::is_same_v<T, Instance_call_expression>)
            {
                offset_index(data.left_hand_side);
            }
            else if constexpr (std::is_same_v<T, Parenthesis_expression>)
            {
                offset_index(data.expression);
            }
            else if constexpr (std::is_same_v<T, Reflection_expression>)
            {
                for (Expression_index& arg : data.arguments)
                    offset_index(arg);
            }
            else if constexpr (std::is_same_v<T, Return_expression>)
            {
                if (data.expression)
                    offset_index(*data.expression);
            }
            else if constexpr (std::is_same_v<T, Switch_expression>)
            {
                offset_index(data.value);
                for (Switch_case_expression_pair& case_pair : data.cases)
                {
                    if (case_pair.case_value)
                        offset_index(*case_pair.case_value);
                }
            }
            else if constexpr (std::is_same_v<T, Ternary_condition_expression>)
            {
                offset_index(data.condition);
            }
            else if constexpr (std::is_same_v<T, Unary_expression>)
            {
                offset_index(data.expression);
            }
            else if constexpr (std::is_same_v<T, Variable_declaration_expression>)
            {
                offset_index(data.right_hand_side);
            }
            else if constexpr (std::is_same_v<T, Variable_declaration_with_type_expression>)
            {
                offset_index(data.type);
                offset_index(data.right_hand_side);
            }
            // All other types have no parent-level Expression_index fields.
        }, expression.data);
    }

    void add_expressions_to_expressions(
        std::pmr::vector<iris::Expression>& output,
        std::span<iris::Expression const> const expressions
    )
    {
        std::size_t const offset = output.size();
        
        output.insert(output.end(), expressions.begin(), expressions.end());

        for (std::size_t index = offset; index < output.size(); ++index)
            offset_expression_indices(output[index], static_cast<std::uint64_t>(offset));
    }
}
