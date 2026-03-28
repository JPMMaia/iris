module;

#include <cassert>

module h.core.expressions;

import std;

import h.core;
import h.core.types;

namespace h
{
    h::Statement create_statement(std::pmr::vector<h::Expression> expressions)
    {
        return h::Statement
        {
            .expressions = std::move(expressions)
        };
    }

    h::Expression create_call_expression(std::uint64_t const left_hand_side_expression, std::pmr::vector<Expression_index> arguments)
    {
        return h::Expression
        {
            .data = h::Call_expression
            {
                .expression = {.expression_index = left_hand_side_expression},
                .arguments = std::move(arguments)
            }
        };
    }

    h::Expression create_constant_expression(Type_reference type_reference, std::string_view const data)
    {
        return h::Expression
        {
            .data = h::Constant_expression
            {
                .type = std::move(type_reference),
                .data = std::pmr::string{ data }
            }
        };
    }

    h::Expression create_constant_array_expression(std::pmr::vector<h::Statement> array_data)
    {
        return h::Expression
        {
            .data = h::Constant_array_expression
            {
                .array_data = std::move(array_data)
            }
        };
    }

    void add_enum_value_expressions(h::Statement& statement, std::string_view const enum_name, std::string_view const member_name)
    {
        h::Expression access_expression
        {
            .data = h::Access_expression
            {
                .expression = {
                    .expression_index = statement.expressions.size() + 1
                },
                .member_name = std::pmr::string{ member_name },
            }
        };

        statement.expressions.push_back(std::move(access_expression));

        h::Expression variable_expression
        {
            .data = h::Variable_expression
            {
                .name = std::pmr::string{ enum_name },
            }
        };

        statement.expressions.push_back(std::move(variable_expression));
    }

    std::pmr::vector<h::Expression> create_enum_value_expressions(std::string_view const enum_name, std::string_view const member_name)
    {
        h::Statement statement = {};
        add_enum_value_expressions(statement, enum_name, member_name);
        return statement.expressions;
    }

    h::Expression create_instantiate_expression(Instantiate_expression_type const type, std::pmr::vector<Instantiate_member_value_pair> members)
    {
        return h::Expression
        {
            .data = h::Instantiate_expression
            {
                .type = type,
                .members = std::move(members)
            }
        };
    }

    h::Expression create_null_pointer_expression()
    {
        return create_expression(h::Null_pointer_expression{});
    }

    h::Expression create_variable_expression(std::pmr::string name)
    {
        return h::Expression
        {
            .data = h::Variable_expression
            {
                .name = std::move(name),
            }
        };
    }

    void invalidate_expression_and_descendants(
        h::Statement& statement,
        std::size_t const expression_index,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<std::size_t> indices_to_invalidate{temporaries_allocator};
        indices_to_invalidate.reserve(statement.expressions.size());
        
        auto const invalidate_descendants = [&](h::Expression const& expression, h::Statement const& statement) -> bool
        {
            for (std::size_t index = 0; index < statement.expressions.size(); ++index)
            {
                if (&statement.expressions[index] == &expression)
                {
                    indices_to_invalidate.push_back(index);
                    return false;
                }
            }

            return false;
        };

        h::Expression const& expression_to_invalidate = statement.expressions[expression_index];
        visit_expressions(expression_to_invalidate, statement, invalidate_descendants);

        for (std::size_t index : indices_to_invalidate)
            statement.expressions[index] = h::Expression{.data = h::Invalid_expression{}};
    }

    std::size_t get_or_create_expression_slot(
        h::Statement& statement
    )
    {
        for (std::size_t index = 0; index < statement.expressions.size(); ++index)
        {
            h::Expression const& expression = statement.expressions[index];
            if (std::holds_alternative<h::Invalid_expression>(expression.data))
                return index;
        }

        statement.expressions.push_back(h::Expression{.data = h::Invalid_expression{}});
        return statement.expressions.size() - 1;
    }

    std::size_t find_expression_index(
        h::Statement const& statement,
        h::Expression const& expression
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
        h::Statement& statement,
        h::Expression const& expression,
        h::Statement const& new_statement,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        assert(new_statement.expressions.size() == 1);

        std::size_t const expression_index = find_expression_index(statement, expression);

        invalidate_expression_and_descendants(statement, expression_index, temporaries_allocator);
        std::size_t const new_expression_index = get_or_create_expression_slot(statement);
        statement.expressions[new_expression_index] = new_statement.expressions[0];
    }
}
