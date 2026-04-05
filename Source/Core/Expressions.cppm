export module h.core.expressions;

import std;

import h.core;

namespace h
{
    export template <typename T>
        h::Expression create_expression(T expression)
    {
        return h::Expression
        {
            .data = std::move(expression)
        };
    }

    export h::Statement create_statement(std::pmr::vector<h::Expression> expressions);

    export h::Expression create_call_expression(std::uint64_t const left_hand_side_expression, std::pmr::vector<Expression_index> arguments);

    export h::Expression create_constant_expression(Type_reference type_reference, std::string_view const data);

    export h::Expression create_constant_array_expression(std::pmr::vector<h::Statement> array_data);

    export void add_enum_value_expressions(h::Statement& statement, std::string_view const enum_name, std::string_view const member_name);

    export std::pmr::vector<h::Expression> create_enum_value_expressions(std::string_view const enum_name, std::string_view const member_name);

    export h::Expression create_instantiate_expression(Instantiate_expression_type type, std::pmr::vector<Instantiate_member_value_pair> members);

    export h::Expression create_null_pointer_expression();

    export h::Expression create_variable_expression(std::pmr::string name);


    export void invalidate_expression_and_descendants(
        h::Statement& statement,
        std::size_t const expression_index,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::size_t get_or_create_expression_slot(
        h::Statement& statement
    );

    export std::size_t find_expression_index(
        h::Statement const& statement,
        h::Expression const& expression
    );

    export void replace_expression(
        h::Statement& statement,
        h::Expression const& expression,
        h::Statement const& new_statement,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export void offset_expression_indices(
        h::Expression& expression,
        std::uint64_t offset
    );

    export void add_expressions_to_expressions(
        std::pmr::vector<h::Expression>& output,
        std::span<h::Expression const> const expressions
    );

    export template<typename Expression_type>
    struct Expression_reference
    {
        Expression_type* value;
        std::size_t index;
    };

    export template<typename Expression_type>
    Expression_reference<Expression_type> create_expression_reference(
        std::pmr::vector<h::Expression>& expressions,
        std::size_t index
    )
    {
        h::Expression& expression = expressions[index];
        return {
            .value = &std::get<Expression_type>(expression.data),
            .index = index
        };
    }

    export template<typename Expression_type>
    Expression_reference<Expression_type> create_expression_inside_statement(
        std::pmr::vector<h::Expression>& expressions
    )
    {
        std::size_t const index = expressions.size();
        expressions.push_back(h::Expression{ .data = Expression_type{} });
        return create_expression_reference<Expression_type>(expressions, index);
    }
}
