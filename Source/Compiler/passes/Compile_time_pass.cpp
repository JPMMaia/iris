module;

#include <compare>

module h.compiler.compile_time_pass;

import std;
import std.compat;

import h.core;
import h.core.types;

namespace h::compiler
{
    static void invalidate_expression_and_descendants(
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

    static std::size_t get_or_create_expression_slot(
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
    
    static h::Statement create_block_statement(
        std::pmr::vector<h::Statement> statements,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<Statement> block_statements{output_allocator};
        block_statements.assign(statements.begin(), statements.end());

        return h::Statement
        {
            .expressions = {
                h::Expression
                {
                    .data = h::Block_expression
                    {
                        .statements = std::move(statements)
                    }
                },
            },
        };
    }

    static h::Statement create_constant_expression_statement(
        Type_reference type,
        std::pmr::string data
    )
    {
        return h::Statement
        {
            .expressions = {
                h::Expression
                {
                    .data = h::Constant_expression
                    {
                        .type = std::move(type),
                        .data = std::move(data)
                    }
                },
            },
        };
    }

    static h::Statement create_constant_bool_expression_statement(bool const value)
    {
        return h::Statement
        {
            .expressions = {
                h::Expression
                {
                    .data = h::Constant_expression
                    {
                        .type = create_bool_type_reference(),
                        .data = value ? "true" : "false"
                    }
                },
            },
        };
    }

    static Compile_time_value_and_type create_value_and_type(
        h::Statement statement
    )
    {
        return Compile_time_value_and_type
        {
            .statement = std::move(statement),
            .type = std::nullopt,
        };
    }

    static std::optional<bool> get_bool_from_value(
        Compile_time_value_and_type const& value
    )
    {
        h::Statement const& statement = value.statement;
        if (statement.expressions.empty())
            return std::nullopt;

        h::Expression const& expression = statement.expressions[0];
        if (std::holds_alternative<h::Constant_expression>(expression.data))
        {
            h::Constant_expression const constant_expression = std::get<h::Constant_expression>(expression.data);
            if (h::is_bool(constant_expression.type) || h::is_c_bool(constant_expression.type))
                return constant_expression.data == "true" || constant_expression.data == "1";
        }

        return std::nullopt;
    }

    struct Compile_time_integer_value
    {
        bool is_signed;
        std::int64_t signed_value;
        std::uint64_t unsigned_value;
    };

    static std::optional<Compile_time_integer_value> get_integer_from_value(
        Compile_time_value_and_type const& value
    )
    {
        h::Statement const& statement = value.statement;
        if (statement.expressions.empty())
            return std::nullopt;

        h::Expression const& expression = statement.expressions[0];
        if (!std::holds_alternative<h::Constant_expression>(expression.data))
            return std::nullopt;

        h::Constant_expression const constant_expression = std::get<h::Constant_expression>(expression.data);
        if (!h::is_integer(constant_expression.type) && !h::is_byte(constant_expression.type))
            return std::nullopt;

        if (h::is_unsigned_integer(constant_expression.type) || h::is_byte(constant_expression.type))
        {
            std::uint64_t unsigned_value = 0;
            auto [pointer, error_code] = std::from_chars(constant_expression.data.data(), constant_expression.data.data() + constant_expression.data.size(), unsigned_value);
            if (error_code != std::errc() || pointer != constant_expression.data.data() + constant_expression.data.size())
                return std::nullopt;

            return Compile_time_integer_value{.is_signed = false, .signed_value = 0, .unsigned_value = unsigned_value};
        }

        if (h::is_signed_integer(constant_expression.type))
        {
            std::int64_t signed_value = 0;
            auto [pointer, error_code] = std::from_chars(constant_expression.data.data(), constant_expression.data.data() + constant_expression.data.size(), signed_value);
            if (error_code != std::errc() || pointer != constant_expression.data.data() + constant_expression.data.size())
                return std::nullopt;

            return Compile_time_integer_value{.is_signed = true, .signed_value = signed_value, .unsigned_value = 0};
        }

        return std::nullopt;
    }

    static void replace_variable_with_constant_in_statement(
        h::Statement& statement,
        std::string_view const variable_name,
        h::Type_reference const& constant_type,
        std::pmr::string const& constant_data
    );

    static void replace_variable_with_constant_in_expression(
        h::Expression& expression,
        std::string_view const variable_name,
        h::Type_reference const& constant_type,
        std::pmr::string const& constant_data
    );

    static void replace_variable_with_constant_in_statement(
        h::Statement& statement,
        std::string_view const variable_name,
        h::Type_reference const& constant_type,
        std::pmr::string const& constant_data
    )
    {
        for (h::Expression& expression : statement.expressions)
            replace_variable_with_constant_in_expression(expression, variable_name, constant_type, constant_data);
    }

    static void replace_variable_with_constant_in_expression(
        h::Expression& expression,
        std::string_view const variable_name,
        h::Type_reference const& constant_type,
        std::pmr::string const& constant_data
    )
    {
        if (std::holds_alternative<h::Variable_expression>(expression.data))
        {
            h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(expression.data);
            if (variable_expression.name == variable_name)
            {
                expression = h::Expression
                {
                    .data = h::Constant_expression
                    {
                        .type = constant_type,
                        .data = constant_data
                    }
                };
                return;
            }
        }

        if (std::holds_alternative<h::Block_expression>(expression.data))
        {
            h::Block_expression& data = std::get<h::Block_expression>(expression.data);
            for (h::Statement& statement : data.statements)
                replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
        }
        else if (std::holds_alternative<h::Constant_array_expression>(expression.data))
        {
            h::Constant_array_expression& data = std::get<h::Constant_array_expression>(expression.data);
            for (h::Statement& statement : data.array_data)
                replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
        }
        else if (std::holds_alternative<h::For_loop_expression>(expression.data))
        {
            h::For_loop_expression& data = std::get<h::For_loop_expression>(expression.data);
            replace_variable_with_constant_in_statement(data.range_end, variable_name, constant_type, constant_data);
            for (h::Statement& statement : data.then_statements)
                replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
        }
        else if (std::holds_alternative<h::If_expression>(expression.data))
        {
            h::If_expression& data = std::get<h::If_expression>(expression.data);
            for (h::Condition_statement_pair& pair : data.series)
            {
                if (pair.condition.has_value())
                    replace_variable_with_constant_in_statement(*pair.condition, variable_name, constant_type, constant_data);

                for (h::Statement& statement : pair.then_statements)
                    replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
            }
        }
        else if (std::holds_alternative<h::Switch_expression>(expression.data))
        {
            h::Switch_expression& data = std::get<h::Switch_expression>(expression.data);
            for (h::Switch_case_expression_pair& pair : data.cases)
                for (h::Statement& statement : pair.statements)
                    replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
        }
        else if (std::holds_alternative<h::Ternary_condition_expression>(expression.data))
        {
            h::Ternary_condition_expression& data = std::get<h::Ternary_condition_expression>(expression.data);
            replace_variable_with_constant_in_statement(data.then_statement, variable_name, constant_type, constant_data);
            replace_variable_with_constant_in_statement(data.else_statement, variable_name, constant_type, constant_data);
        }
        else if (std::holds_alternative<h::While_loop_expression>(expression.data))
        {
            h::While_loop_expression& data = std::get<h::While_loop_expression>(expression.data);
            replace_variable_with_constant_in_statement(data.condition, variable_name, constant_type, constant_data);
            for (h::Statement& statement : data.then_statements)
                replace_variable_with_constant_in_statement(statement, variable_name, constant_type, constant_data);
        }
    }

    static std::optional<Compile_time_value_and_type> evaluate_compile_time_for_loop_expression(
        h::Statement const& statement,
        h::For_loop_expression const& expression,
        Compile_time_parameters const& parameters
    )
    {
        if (expression.range_begin.expression_index >= statement.expressions.size())
            return std::nullopt;

        switch (expression.range_comparison_operation)
        {
            case h::Binary_operation::Less_than:
            case h::Binary_operation::Less_than_or_equal_to:
            case h::Binary_operation::Greater_than:
            case h::Binary_operation::Greater_than_or_equal_to:
                break;
            default:
                return std::nullopt;
        }

        h::Expression const& range_begin_expression = statement.expressions[expression.range_begin.expression_index];
        std::optional<Compile_time_value_and_type> const range_begin_value = evaluate_compile_time_expression(statement, range_begin_expression, parameters);
        if (!range_begin_value.has_value())
            return std::nullopt;

        std::optional<Compile_time_integer_value> const range_begin_integer = get_integer_from_value(range_begin_value.value());
        if (!range_begin_integer.has_value())
            return std::nullopt;

        std::optional<Compile_time_value_and_type> const range_end_value = evaluate_compile_time_statement(expression.range_end, parameters);
        if (!range_end_value.has_value())
            return std::nullopt;

        std::optional<Compile_time_integer_value> const range_end_integer = get_integer_from_value(range_end_value.value());
        if (!range_end_integer.has_value())
            return std::nullopt;

        auto const get_step_integer = [&]() -> std::optional<Compile_time_integer_value>
        {
            if (expression.step_by.has_value())
            {
                if (expression.step_by->expression_index >= statement.expressions.size())
                    return std::nullopt;

                h::Expression const& step_expression = statement.expressions[expression.step_by->expression_index];
                std::optional<Compile_time_value_and_type> const step_value = evaluate_compile_time_expression(statement, step_expression, parameters);
                if (!step_value.has_value())
                    return std::nullopt;

                return get_integer_from_value(step_value.value());
            }

            return std::nullopt;
        };
        std::optional<Compile_time_integer_value> const step_integer = get_step_integer();

        // Limits to avoid runaway unrolling
        constexpr std::size_t maximum_unroll_iterations = 1024;
        std::pmr::vector<h::Statement> iteration_blocks{parameters.output_allocator};
        iteration_blocks.reserve(16);

        auto const create_iteration_block = [&](auto const loop_index_value) -> void
        {
            std::pmr::vector<h::Statement> body{parameters.output_allocator};
            body.assign(expression.then_statements.begin(), expression.then_statements.end());

            std::pmr::string const integer_string = std::pmr::string{std::to_string(loop_index_value)};
            h::Type_reference const index_type = range_begin_value->type.value_or(create_integer_type_type_reference(64, true));

            for (h::Statement& statement : body)
                replace_variable_with_constant_in_statement(statement, expression.variable_name, index_type, integer_string);

            iteration_blocks.push_back(create_block_statement(std::move(body), parameters.output_allocator));
        };

        auto compare = [&](auto const value, auto const range_end) -> bool
        {
            switch (expression.range_comparison_operation)
            {
                case h::Binary_operation::Less_than:
                    return value < range_end;
                case h::Binary_operation::Less_than_or_equal_to:
                    return value <= range_end;
                case h::Binary_operation::Greater_than:
                    return value > range_end;
                case h::Binary_operation::Greater_than_or_equal_to:
                    return value >= range_end;
                default:
                    return false;
            }

            return false;
        };

        if (range_begin_integer->is_signed)
        {
            std::int64_t const range_begin = range_begin_integer->signed_value;
            std::int64_t const range_end = range_end_integer->signed_value;
            std::int64_t const step_value = step_integer.has_value() ? step_integer->signed_value : std::int64_t{1};
            std::int64_t current = range_begin;

            while (compare(current, range_end))
            {
                if (iteration_blocks.size() >= maximum_unroll_iterations)
                    return std::nullopt;

                create_iteration_block(current);
                current += step_value;
            }
        }
        else
        {
            std::uint64_t const range_begin = range_begin_integer->unsigned_value;
            std::uint64_t const range_end = range_end_integer->unsigned_value;
            std::uint64_t const step_value = step_integer.has_value() ? step_integer->unsigned_value : std::uint64_t{1};
            std::uint64_t current = range_begin;

            while (compare(current, range_end))
            {
                if (iteration_blocks.size() >= maximum_unroll_iterations)
                    return std::nullopt;

                create_iteration_block(current);
                current += step_value;
            }
        }

        return create_value_and_type(create_block_statement(std::move(iteration_blocks), parameters.output_allocator));
    }

    static std::optional<Compile_time_value_and_type> evaluate_compile_time_unary_expression(
        h::Statement const& statement,
        h::Unary_expression const& expression,
        Compile_time_parameters const& parameters
    )
    {
        switch (expression.operation)
        {
            case h::Unary_operation::Bitwise_not:
            case h::Unary_operation::Minus:
            case h::Unary_operation::Pre_increment:
            case h::Unary_operation::Post_increment:
            case h::Unary_operation::Pre_decrement:
            case h::Unary_operation::Post_decrement:
            case h::Unary_operation::Indirection:
            case h::Unary_operation::Address_of:
                return std::nullopt;
            default:
                break;
        }

        h::Expression const& right_side_expression = statement.expressions[expression.expression.expression_index];
        std::optional<Compile_time_value_and_type> const right_side_value = evaluate_compile_time_expression(statement, right_side_expression, parameters);
        if (!right_side_value.has_value())
            return std::nullopt;

        if (expression.operation == h::Unary_operation::Not)
        {
            std::optional<bool> const value = get_bool_from_value(right_side_value.value());
            if (!value.has_value())
                return std::nullopt;

            bool const not_value = !value.value();
            return create_value_and_type(create_constant_bool_expression_statement(not_value));
        }

        return std::nullopt;
    }

    static std::optional<Compile_time_value_and_type> evaluate_compile_time_variable_expression(
        h::Statement const& statement,
        h::Variable_expression const& expression,
        Compile_time_parameters const& parameters
    )
    {
        // Search for global variables:
        {
            std::optional<h::Global_variable_declaration const*> const declaration = h::find_global_variable_declaration(parameters.core_module, expression.name);
            if (declaration.has_value())
            {
                h::Global_variable_declaration const& global_variable_declaration = *declaration.value();
                return Compile_time_value_and_type
                {
                    .statement = global_variable_declaration.initial_value,
                    .type = global_variable_declaration.type
                };
            }
        }

        return std::nullopt;
    }

    std::optional<Compile_time_value_and_type> evaluate_compile_time_expression(
        h::Statement const& statement,
        h::Expression const& expression,
        Compile_time_parameters const& parameters
    )
    {
        if (std::holds_alternative<h::Compile_time_expression>(expression.data))
        {
            h::Compile_time_expression const& compile_time_expression = std::get<h::Compile_time_expression>(expression.data);
            if (compile_time_expression.expression.expression_index >= statement.expressions.size())
                return std::nullopt;
                
            h::Expression const& right_side_expression = statement.expressions[compile_time_expression.expression.expression_index];
            return evaluate_compile_time_expression(statement, right_side_expression, parameters);
        }
        else if (std::holds_alternative<h::Constant_expression>(expression.data))
        {
            h::Constant_expression const& constant_expression = std::get<h::Constant_expression>(expression.data);
            return create_value_and_type({.expressions = {h::Expression{.data = constant_expression}}});
        }
        else if (std::holds_alternative<h::If_expression>(expression.data))
        {
            h::If_expression const& if_expression = std::get<h::If_expression>(expression.data);

            for (std::size_t index = 0; index < if_expression.series.size(); ++index)
            {
                h::Condition_statement_pair const& serie = if_expression.series[index];

                if (!serie.condition.has_value())
                    return create_value_and_type(create_block_statement(serie.then_statements, parameters.output_allocator));

                h::Statement const& condition_statement = serie.condition.value();
                std::optional<Compile_time_value_and_type> const condition_value = evaluate_compile_time_statement(condition_statement, parameters);
                if (!condition_value.has_value())
                    return std::nullopt;

                std::optional<bool> const condition = get_bool_from_value(condition_value.value());
                if (!condition.has_value())
                    return std::nullopt;
                
                if (condition.value())
                    return create_value_and_type(create_block_statement(serie.then_statements, parameters.output_allocator));
            }

            return std::nullopt;
        }
        else if (std::holds_alternative<h::For_loop_expression>(expression.data))
        {
            h::For_loop_expression const& for_loop_expression = std::get<h::For_loop_expression>(expression.data);
            return evaluate_compile_time_for_loop_expression(statement, for_loop_expression, parameters);
        }
        else if (std::holds_alternative<h::Unary_expression>(expression.data))
        {
            h::Unary_expression const& unary_expression = std::get<h::Unary_expression>(expression.data);
            return evaluate_compile_time_unary_expression(statement, unary_expression, parameters);
        }
        else if (std::holds_alternative<h::Variable_expression>(expression.data))
        {
            h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(expression.data);
            return evaluate_compile_time_variable_expression(statement, variable_expression, parameters);
        }

        return std::nullopt;
    }

    std::optional<Compile_time_value_and_type> evaluate_compile_time_statement(
        h::Statement const& statement,
        Compile_time_parameters const& parameters
    )
    {
        if (statement.expressions.empty())
            return std::nullopt;

        h::Expression const& expression = statement.expressions[0];
        return evaluate_compile_time_expression(
            statement,
            expression,
            parameters
        );
    }

    void run_compile_time_pass_on_function(
        h::Module const& core_module,
        h::Function_declaration const& function_declaration,
        h::Function_definition& function_definition,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        Compile_time_parameters const parameters
        {
            .core_module = core_module,
            .output_allocator = output_allocator,
        };

        for (std::size_t index = 0; index < function_definition.statements.size(); ++index)
        {
            h::Statement& statement = function_definition.statements[index];
            if (statement.expressions.empty())
                return;

            h::Expression const& expression = statement.expressions[0];

            if (std::holds_alternative<h::Compile_time_expression>(expression.data))
            {
                std::optional<Compile_time_value_and_type> new_value = evaluate_compile_time_expression(
                    statement,
                    expression,
                    parameters
                );
                if (new_value.has_value())
                    statement = std::move(new_value->statement);
            }
        }
    }
}
