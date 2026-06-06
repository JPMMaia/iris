export module iris.core.expressions;

import std;

import iris.core;

namespace iris
{
    export template <typename T>
        iris::Expression create_expression(T expression)
    {
        return iris::Expression
        {
            .data = std::move(expression)
        };
    }

    export iris::Statement create_statement(std::pmr::vector<iris::Expression> expressions);

    export iris::Expression create_call_expression(std::uint64_t const left_hand_side_expression, std::pmr::vector<Expression_index> arguments);

    export iris::Expression create_constant_expression(Type_reference type_reference, std::string_view const data);

    export iris::Expression create_constant_array_expression(std::pmr::vector<iris::Statement> array_data);

    export void add_enum_value_expressions(iris::Statement& statement, std::string_view const enum_name, std::string_view const member_name);

    export std::pmr::vector<iris::Expression> create_enum_value_expressions(std::string_view const enum_name, std::string_view const member_name);

    export iris::Expression create_instantiate_expression(Instantiate_expression_type type, std::pmr::vector<Instantiate_member_value_pair> members);

    export iris::Expression create_null_pointer_expression();

    export iris::Expression create_variable_expression(std::pmr::string name);


    export void invalidate_expression_and_descendants(
        iris::Statement& statement,
        std::size_t const expression_index,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::size_t get_or_create_expression_slot(
        iris::Statement& statement
    );

    export std::size_t find_expression_index(
        iris::Statement const& statement,
        iris::Expression const& expression
    );

    export void replace_expression(
        iris::Statement& statement,
        iris::Expression const& expression,
        iris::Statement const& new_statement,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export void offset_expression_indices(
        iris::Expression& expression,
        std::uint64_t offset
    );

    export void add_expressions_to_expressions(
        std::pmr::vector<iris::Expression>& output,
        std::span<iris::Expression const> const expressions
    );

    export template<typename Expression_type>
    struct Expression_reference
    {
        Expression_type* value;
        std::size_t index;
    };

    export template<typename Expression_type>
    Expression_reference<Expression_type> create_expression_reference(
        std::pmr::vector<iris::Expression>& expressions,
        std::size_t index
    )
    {
        iris::Expression& expression = expressions[index];
        return {
            .value = &std::get<Expression_type>(expression.data),
            .index = index
        };
    }

    export template<typename Expression_type>
    Expression_reference<Expression_type> create_expression_inside_statement(
        std::pmr::vector<iris::Expression>& expressions
    )
    {
        std::size_t const index = expressions.size();
        expressions.push_back(iris::Expression{ .data = Expression_type{} });
        return create_expression_reference<Expression_type>(expressions, index);
    }
}
