module iris.compiler.pipeline_pass;

import std;

import iris.compiler.analysis;
import iris.compiler.validation;
import iris.core;
import iris.core.declarations;
import iris.core.expressions;
import iris.core.types;

namespace iris::compiler
{
    namespace
    {
        // Generated locals start with '$', which the grammar's Identifier cannot produce, so they
        // can never collide with or shadow a user name.
        std::pmr::string get_pipeline_index_name(std::size_t const pipeline_index)
        {
            return std::pmr::string{ std::format("$pipeline{}_i", pipeline_index) };
        }

        std::pmr::string get_pipeline_input_name(std::size_t const pipeline_index, std::size_t const input_index)
        {
            return std::pmr::string{ std::format("$pipeline{}_input{}", pipeline_index, input_index) };
        }

        struct Pipeline_input
        {
            // The variable the expansion reads elements from: the user's own variable, or a
            // generated one that holds the argument so it is evaluated exactly once.
            std::pmr::string variable_name;
            // Set for Constant_array inputs, whose length is known at compile time.
            std::optional<std::uint64_t> constant_length;
            // Set for Soa_array_view inputs, whose fields are stored in separate columns.
            bool is_soa = false;
        };

        std::uint64_t add_expression(iris::Statement& statement, iris::Expression expression)
        {
            std::uint64_t const index = statement.expressions.size();
            statement.expressions.push_back(std::move(expression));
            return index;
        }

        iris::Expression_index to_index(std::uint64_t const index)
        {
            return iris::Expression_index{ .expression_index = index };
        }

        iris::Expression create_uint64_constant_expression(std::uint64_t const value)
        {
            return create_constant_expression(create_integer_type_type_reference(64, false), std::format("{}", value));
        }

        // <input>.length, or the constant length of a Constant_array.
        std::uint64_t add_input_length_expression(iris::Statement& statement, Pipeline_input const& input)
        {
            if (input.constant_length.has_value())
                return add_expression(statement, create_uint64_constant_expression(input.constant_length.value()));

            std::uint64_t const access_index = add_expression(statement, iris::Expression{ .data = iris::Access_expression{} });
            std::uint64_t const variable_index = add_expression(statement, create_variable_expression(input.variable_name));

            std::get<iris::Access_expression>(statement.expressions[access_index].data) = iris::Access_expression
            {
                .expression = to_index(variable_index),
                .member_name = std::pmr::string{ "length" },
            };

            return access_index;
        }

        // var <name> = <expression>;  where the expression is the root of `value`.
        iris::Statement create_variable_declaration_statement(std::string_view const name, iris::Statement const& value)
        {
            iris::Statement statement;
            add_expression(statement, iris::Expression{ .data = iris::Variable_declaration_expression{} });
            add_expressions_to_expressions(statement.expressions, value.expressions);

            std::get<iris::Variable_declaration_expression>(statement.expressions[0].data) = iris::Variable_declaration_expression
            {
                .name = std::pmr::string{ name },
                .is_mutable = false,
                .right_hand_side = to_index(1),
            };

            return statement;
        }

        // var <name> = <source>;
        iris::Statement create_variable_copy_statement(std::string_view const name, std::string_view const source_name)
        {
            iris::Statement value;
            add_expression(value, create_variable_expression(std::pmr::string{ source_name }));
            return create_variable_declaration_statement(name, value);
        }

        // var <parameter_name> = <input>[<index_name>];
        iris::Statement create_input_element_statement(
            std::string_view const parameter_name,
            Pipeline_input const& input,
            std::string_view const index_name
        )
        {
            iris::Statement value;
            std::uint64_t const access_index = add_expression(value, iris::Expression{ .data = iris::Access_array_expression{} });
            std::uint64_t const input_index = add_expression(value, create_variable_expression(input.variable_name));
            std::uint64_t const index_index = add_expression(value, create_variable_expression(std::pmr::string{ index_name }));

            std::get<iris::Access_array_expression>(value.expressions[access_index].data) = iris::Access_array_expression
            {
                .expression = to_index(input_index),
                .index = to_index(index_index),
            };

            return create_variable_declaration_statement(parameter_name, value);
        }

        // if <condition> { <then_statements> }
        iris::Statement create_if_statement(iris::Statement const& condition, std::pmr::vector<iris::Statement> then_statements)
        {
            iris::Condition_statement_pair pair
            {
                .condition = condition,
                .then_statements = std::move(then_statements),
                .block_source_range = std::nullopt,
            };

            iris::If_expression if_expression;
            if_expression.series.push_back(std::move(pair));

            iris::Statement statement;
            add_expression(statement, iris::Expression{ .data = std::move(if_expression) });
            return statement;
        }

        // assert "Pipeline inputs have equal length" { <input>.length == <first_input>.length };
        iris::Statement create_equal_length_assert_statement(Pipeline_input const& input, Pipeline_input const& first_input)
        {
            iris::Statement condition;
            std::uint64_t const binary_index = add_expression(condition, iris::Expression{ .data = iris::Binary_expression{} });
            std::uint64_t const left_index = add_input_length_expression(condition, input);
            std::uint64_t const right_index = add_input_length_expression(condition, first_input);

            std::get<iris::Binary_expression>(condition.expressions[binary_index].data) = iris::Binary_expression
            {
                .left_hand_side = to_index(left_index),
                .right_hand_side = to_index(right_index),
                .operation = iris::Binary_operation::Equal,
            };

            iris::Statement statement;
            add_expression(statement, iris::Expression
            {
                .data = iris::Assert_expression
                {
                    .message = std::pmr::string{ "Pipeline inputs have equal length" },
                    .statement = std::move(condition),
                }
            });
            return statement;
        }

        // for <index_name> in 0u64 to <range_end> { <then_statements> }
        iris::Statement create_index_loop_statement(
            std::string_view const index_name,
            iris::Statement range_end,
            std::pmr::vector<iris::Statement> then_statements
        )
        {
            iris::Statement statement;
            std::uint64_t const loop_index = add_expression(statement, iris::Expression{ .data = iris::For_loop_expression{} });
            std::uint64_t const begin_index = add_expression(statement, create_uint64_constant_expression(0));

            std::get<iris::For_loop_expression>(statement.expressions[loop_index].data) = iris::For_loop_expression
            {
                .variable_name = std::pmr::string{ index_name },
                .range_begin = to_index(begin_index),
                .range_end = std::move(range_end),
                .range_comparison_operation = iris::Binary_operation::Less_than,
                .step_by = std::nullopt,
                .then_statements = std::move(then_statements),
            };

            return statement;
        }

        // for <index_name> in 0u64 to <first_input length> { <then_statements> }
        iris::Statement create_index_loop_statement(
            std::string_view const index_name,
            Pipeline_input const& first_input,
            std::pmr::vector<iris::Statement> then_statements
        )
        {
            iris::Statement range_end;
            add_input_length_expression(range_end, first_input);
            return create_index_loop_statement(index_name, std::move(range_end), std::move(then_statements));
        }

        struct Pipeline_pass_context
        {
            std::string_view module_name;
            iris::Function_declaration const& function_declaration;
            iris::Declaration_database const& declaration_database;
            std::size_t next_pipeline_index = 0;
        };

        std::pmr::string get_pipeline_local_name(std::size_t const pipeline_index, std::string_view const suffix)
        {
            return std::pmr::string{ std::format("$pipeline{}_{}", pipeline_index, suffix) };
        }

        iris::Expression create_bool_constant_expression(bool const value)
        {
            return create_constant_expression(create_fundamental_type_type_reference(iris::Fundamental_type::Bool), value ? "true" : "false");
        }

        iris::Statement create_single_expression_statement(iris::Expression expression)
        {
            iris::Statement statement;
            add_expression(statement, std::move(expression));
            return statement;
        }

        // mutable <name> = <value>;
        iris::Statement create_mutable_variable_statement(std::string_view const name, iris::Expression value)
        {
            iris::Statement statement = create_variable_declaration_statement(name, create_single_expression_statement(std::move(value)));
            std::get<iris::Variable_declaration_expression>(statement.expressions[0].data).is_mutable = true;
            return statement;
        }

        // <name> = <value>;  or  <name> += <value>;
        iris::Statement create_assignment_statement(
            std::string_view const name,
            iris::Expression value,
            std::optional<iris::Binary_operation> const additional_operation
        )
        {
            iris::Statement statement;
            std::uint64_t const assignment_index = add_expression(statement, iris::Expression{ .data = iris::Assignment_expression{} });
            std::uint64_t const left_index = add_expression(statement, create_variable_expression(std::pmr::string{ name }));
            std::uint64_t const right_index = add_expression(statement, std::move(value));

            std::get<iris::Assignment_expression>(statement.expressions[assignment_index].data) = iris::Assignment_expression
            {
                .left_hand_side = to_index(left_index),
                .right_hand_side = to_index(right_index),
                .additional_operation = additional_operation,
            };

            return statement;
        }

        iris::Statement create_break_statement()
        {
            return create_single_expression_statement(iris::Expression{ .data = iris::Break_expression{ .loop_count = 1 } });
        }

        // if [!]<name> { <name_to_set> = <value>; break; }
        iris::Statement create_short_circuit_statement(
            std::string_view const condition_name,
            bool const negate_condition,
            std::string_view const result_name,
            bool const result_value
        )
        {
            iris::Statement condition;
            if (negate_condition)
            {
                std::uint64_t const unary_index = add_expression(condition, iris::Expression{ .data = iris::Unary_expression{} });
                std::uint64_t const variable_index = add_expression(condition, create_variable_expression(std::pmr::string{ condition_name }));
                std::get<iris::Unary_expression>(condition.expressions[unary_index].data) = iris::Unary_expression
                {
                    .expression = to_index(variable_index),
                    .operation = iris::Unary_operation::Not,
                };
            }
            else
            {
                add_expression(condition, create_variable_expression(std::pmr::string{ condition_name }));
            }

            std::pmr::vector<iris::Statement> then_statements;
            then_statements.push_back(create_assignment_statement(result_name, create_bool_constant_expression(result_value), std::nullopt));
            then_statements.push_back(create_break_statement());

            return create_if_statement(condition, std::move(then_statements));
        }

        // assert "<message>" { <output>.length >= <first_input length> };
        iris::Statement create_output_capacity_assert_statement(
            Pipeline_input const& output,
            Pipeline_input const& first_input,
            std::string_view const message = "Pipeline output buffer is large enough"
        )
        {
            iris::Statement condition;
            std::uint64_t const binary_index = add_expression(condition, iris::Expression{ .data = iris::Binary_expression{} });
            std::uint64_t const left_index = add_input_length_expression(condition, output);
            std::uint64_t const right_index = add_input_length_expression(condition, first_input);

            std::get<iris::Binary_expression>(condition.expressions[binary_index].data) = iris::Binary_expression
            {
                .left_hand_side = to_index(left_index),
                .right_hand_side = to_index(right_index),
                .operation = iris::Binary_operation::Greater_than_or_equal_to,
            };

            return create_single_expression_statement(iris::Expression
            {
                .data = iris::Assert_expression
                {
                    .message = std::pmr::string{ message },
                    .statement = std::move(condition),
                }
            });
        }

        // <output>[<count_name>] = <element_name>;
        iris::Statement create_output_store_statement(std::string_view const output_name, std::string_view const count_name, std::string_view const element_name)
        {
            iris::Statement statement;
            std::uint64_t const assignment_index = add_expression(statement, iris::Expression{ .data = iris::Assignment_expression{} });
            std::uint64_t const access_index = add_expression(statement, iris::Expression{ .data = iris::Access_array_expression{} });
            std::uint64_t const output_index = add_expression(statement, create_variable_expression(std::pmr::string{ output_name }));
            std::uint64_t const count_index = add_expression(statement, create_variable_expression(std::pmr::string{ count_name }));
            std::uint64_t const element_index = add_expression(statement, create_variable_expression(std::pmr::string{ element_name }));

            std::get<iris::Access_array_expression>(statement.expressions[access_index].data) = iris::Access_array_expression
            {
                .expression = to_index(output_index),
                .index = to_index(count_index),
            };

            std::get<iris::Assignment_expression>(statement.expressions[assignment_index].data) = iris::Assignment_expression
            {
                .left_hand_side = to_index(access_index),
                .right_hand_side = to_index(element_index),
                .additional_operation = std::nullopt,
            };

            return statement;
        }

        // <receiver_name>.push(<element_name>);
        iris::Statement create_push_statement(std::string_view const receiver_name, std::string_view const element_name)
        {
            iris::Statement statement;
            std::uint64_t const call_index = add_expression(statement, iris::Expression{ .data = iris::Call_expression{} });
            std::uint64_t const access_index = add_expression(statement, iris::Expression{ .data = iris::Access_expression{} });
            std::uint64_t const receiver_index = add_expression(statement, create_variable_expression(std::pmr::string{ receiver_name }));
            std::uint64_t const element_index = add_expression(statement, create_variable_expression(std::pmr::string{ element_name }));

            std::get<iris::Access_expression>(statement.expressions[access_index].data) = iris::Access_expression
            {
                .expression = to_index(receiver_index),
                .member_name = std::pmr::string{ "push" },
            };

            iris::Call_expression& call_expression = std::get<iris::Call_expression>(statement.expressions[call_index].data);
            call_expression.expression = to_index(access_index);
            call_expression.arguments.push_back(to_index(element_index));

            return statement;
        }

        // create_array_slice_from_pointer(<output>.data, <count_name>)
        iris::Statement create_written_prefix_statement(std::string_view const output_name, std::string_view const count_name)
        {
            iris::Statement statement;
            std::uint64_t const call_index = add_expression(statement, iris::Expression{ .data = iris::Call_expression{} });
            std::uint64_t const callee_index = add_expression(statement, create_variable_expression(std::pmr::string{ "create_array_slice_from_pointer" }));
            std::uint64_t const access_index = add_expression(statement, iris::Expression{ .data = iris::Access_expression{} });
            std::uint64_t const output_index = add_expression(statement, create_variable_expression(std::pmr::string{ output_name }));
            std::uint64_t const count_index = add_expression(statement, create_variable_expression(std::pmr::string{ count_name }));

            std::get<iris::Access_expression>(statement.expressions[access_index].data) = iris::Access_expression
            {
                .expression = to_index(output_index),
                .member_name = std::pmr::string{ "data" },
            };

            iris::Call_expression& call_expression = std::get<iris::Call_expression>(statement.expressions[call_index].data);
            call_expression.expression = to_index(callee_index);
            call_expression.arguments.push_back(to_index(access_index));
            call_expression.arguments.push_back(to_index(count_index));

            return statement;
        }

        // <callee>(<argument_names>...)  where the callee is the root of `callee`.
        iris::Statement create_call_statement(iris::Statement const& callee, std::span<std::string_view const> const argument_names)
        {
            iris::Statement statement;
            std::uint64_t const call_index = add_expression(statement, iris::Expression{ .data = iris::Call_expression{} });
            std::uint64_t const callee_index = statement.expressions.size();
            add_expressions_to_expressions(statement.expressions, callee.expressions);

            std::pmr::vector<iris::Expression_index> arguments;
            for (std::string_view const argument_name : argument_names)
                arguments.push_back(to_index(add_expression(statement, create_variable_expression(std::pmr::string{ argument_name }))));

            iris::Call_expression& call_expression = std::get<iris::Call_expression>(statement.expressions[call_index].data);
            call_expression.expression = to_index(callee_index);
            call_expression.arguments = std::move(arguments);

            return statement;
        }

        // A variable or a chain of member accesses on one, such as `add` or `Math.add`: reading it
        // has no side effects, so it can be repeated instead of being bound to a local.
        bool is_access_path(iris::Statement const& statement, iris::Expression_index const index)
        {
            iris::Expression const& expression = statement.expressions[index.expression_index];
            if (std::holds_alternative<iris::Variable_expression>(expression.data))
                return true;
            if (std::holds_alternative<iris::Access_expression>(expression.data))
                return is_access_path(statement, std::get<iris::Access_expression>(expression.data).expression);
            return false;
        }

        // <name> = <expression>;  where the expression is the root of `value`.
        iris::Statement create_assignment_from_statement(std::string_view const name, iris::Statement const& value)
        {
            iris::Statement statement;
            std::uint64_t const assignment_index = add_expression(statement, iris::Expression{ .data = iris::Assignment_expression{} });
            std::uint64_t const left_index = add_expression(statement, create_variable_expression(std::pmr::string{ name }));
            std::uint64_t const right_index = statement.expressions.size();
            add_expressions_to_expressions(statement.expressions, value.expressions);

            std::get<iris::Assignment_expression>(statement.expressions[assignment_index].data) = iris::Assignment_expression
            {
                .left_hand_side = to_index(left_index),
                .right_hand_side = to_index(right_index),
                .additional_operation = std::nullopt,
            };

            return statement;
        }

        // mutable <name>: <type> = {};
        iris::Statement create_default_typed_variable_statement(std::string_view const name, iris::Type_reference type)
        {
            iris::Statement statement;
            std::uint64_t const declaration_index = add_expression(statement, iris::Expression{ .data = iris::Variable_declaration_with_type_expression{} });
            std::uint64_t const type_index = add_expression(statement, iris::Expression{ .data = iris::Type_expression{ .type = std::move(type) } });
            std::uint64_t const value_index = add_expression(statement, iris::Expression
            {
                .data = iris::Instantiate_expression{ .type = iris::Instantiate_expression_type::Default, .members = {} }
            });

            std::get<iris::Variable_declaration_with_type_expression>(statement.expressions[declaration_index].data) = iris::Variable_declaration_with_type_expression
            {
                .name = std::pmr::string{ name },
                .is_mutable = true,
                .type = to_index(type_index),
                .right_hand_side = to_index(value_index),
            };

            return statement;
        }

        // create_optional(<name>)
        iris::Expression_index add_create_optional_call(iris::Statement& statement, std::string_view const name)
        {
            std::uint64_t const call_index = add_expression(statement, iris::Expression{ .data = iris::Call_expression{} });
            std::uint64_t const callee_index = add_expression(statement, create_variable_expression(std::pmr::string{ "create_optional" }));
            std::uint64_t const argument_index = add_expression(statement, create_variable_expression(std::pmr::string{ name }));

            iris::Call_expression& call_expression = std::get<iris::Call_expression>(statement.expressions[call_index].data);
            call_expression.expression = to_index(callee_index);
            call_expression.arguments.push_back(to_index(argument_index));

            return to_index(call_index);
        }

        // if !(<condition>) { break; }
        iris::Statement create_stop_unless_statement(iris::Statement const& condition)
        {
            iris::Statement negated_condition;
            std::uint64_t const unary_index = add_expression(negated_condition, iris::Expression{ .data = iris::Unary_expression{} });
            std::uint64_t const parenthesis_index = add_expression(negated_condition, iris::Expression{ .data = iris::Parenthesis_expression{} });
            std::uint64_t const condition_index = negated_condition.expressions.size();
            add_expressions_to_expressions(negated_condition.expressions, condition.expressions);

            std::get<iris::Parenthesis_expression>(negated_condition.expressions[parenthesis_index].data) = iris::Parenthesis_expression
            {
                .expression = to_index(condition_index),
            };
            std::get<iris::Unary_expression>(negated_condition.expressions[unary_index].data) = iris::Unary_expression
            {
                .expression = to_index(parenthesis_index),
                .operation = iris::Unary_operation::Not,
            };

            std::pmr::vector<iris::Statement> then_statements;
            then_statements.push_back(create_break_statement());

            return create_if_statement(negated_condition, std::move(then_statements));
        }

        // The parts of an expansion that every sink shares.
        struct Pipeline_expansion
        {
            iris::Pipeline_expression const* pipeline;
            std::size_t pipeline_index;
            std::pmr::string index_name;
            std::pmr::string output_name;
            // Statements that run once before the loop: input bindings, then length checks.
            std::pmr::vector<iris::Statement> prologue;
            std::pmr::vector<Pipeline_input> inputs;
        };

        // Binds `argument_index` of `statement` to a name the expansion can read repeatedly: the user's own
        // variable, or a generated one declared in the prologue so the argument is evaluated exactly once.
        std::pmr::string bind_argument_once(
            iris::Statement const& statement,
            iris::Expression_index const argument_index,
            std::pmr::string generated_name,
            std::pmr::vector<iris::Statement>& prologue
        )
        {
            iris::Expression const& argument_expression = statement.expressions[argument_index.expression_index];
            if (std::holds_alternative<iris::Variable_expression>(argument_expression.data))
                return std::get<iris::Variable_expression>(argument_expression.data).name;

            iris::Statement value;
            copy_expressions_to_new_statement(value, statement, argument_index);
            prologue.push_back(create_variable_declaration_statement(generated_name, value));
            return generated_name;
        }

        std::optional<Pipeline_input> create_pipeline_input(
            Pipeline_pass_context const& context,
            Scope const& scope,
            iris::Statement const& statement,
            iris::Expression_index const argument_index,
            std::pmr::string generated_name,
            std::pmr::vector<iris::Statement>& prologue
        )
        {
            std::optional<iris::Type_reference> const argument_type = get_expression_type(
                context.module_name,
                &context.function_declaration,
                scope,
                statement,
                statement.expressions[argument_index.expression_index],
                std::nullopt,
                context.declaration_database
            );
            if (!argument_type.has_value())
                return std::nullopt;

            std::optional<iris::Type_reference> const underlying_type = get_underlying_type(context.declaration_database, argument_type.value());
            if (!underlying_type.has_value())
                return std::nullopt;

            Pipeline_input input;

            // A Fixed_capacity_array is read like a slice: its length is the fill count, `.length`.
            if (std::holds_alternative<iris::Constant_array_type>(underlying_type->data))
                input.constant_length = std::get<iris::Constant_array_type>(underlying_type->data).size;
            else if (std::holds_alternative<iris::Soa_array_view_type>(underlying_type->data))
                input.is_soa = true;
            else if (!std::holds_alternative<iris::Array_slice_type>(underlying_type->data) && !std::holds_alternative<iris::Fixed_capacity_array_type>(underlying_type->data))
                return std::nullopt;

            input.variable_name = bind_argument_once(statement, argument_index, std::move(generated_name), prologue);
            return input;
        }

        // Prepares the expansion of `pipeline` over the input expressions `arguments`. Returns nullopt when
        // it cannot be lowered; validation has already reported why.
        std::optional<Pipeline_expansion> begin_pipeline_expansion(
            Pipeline_pass_context& context,
            Scope const& scope,
            iris::Statement const& statement,
            iris::Pipeline_expression const* const pipeline,
            std::span<iris::Expression_index const> const arguments
        )
        {
            if (arguments.empty() || arguments.size() != pipeline->input_parameter_names.size())
                return std::nullopt;

            std::optional<std::string_view> const output_name = get_pipeline_output_name(*pipeline);
            if (!output_name.has_value())
                return std::nullopt;

            std::size_t const pipeline_index = context.next_pipeline_index;
            context.next_pipeline_index += 1;

            Pipeline_expansion expansion
            {
                .pipeline = pipeline,
                .pipeline_index = pipeline_index,
                .index_name = get_pipeline_index_name(pipeline_index),
                .output_name = std::pmr::string{ output_name.value() },
            };
            expansion.inputs.reserve(arguments.size());

            for (std::size_t input_index = 0; input_index < arguments.size(); ++input_index)
            {
                std::optional<Pipeline_input> input = create_pipeline_input(
                    context,
                    scope,
                    statement,
                    arguments[input_index],
                    get_pipeline_input_name(pipeline_index, input_index),
                    expansion.prologue
                );
                if (!input.has_value())
                    return std::nullopt;

                expansion.inputs.push_back(std::move(input.value()));
            }

            for (std::size_t input_index = 1; input_index < expansion.inputs.size(); ++input_index)
                expansion.prologue.push_back(create_equal_length_assert_statement(expansion.inputs[input_index], expansion.inputs[0]));

            return expansion;
        }

        // Calls `function` on every statement list reachable from `statements`, outermost first. Returns
        // false when it meets an expression whose nested statements it does not know how to reach, so
        // that callers can fall back to something that does not need to see everything.
        template <typename Function_t>
        bool visit_statement_lists(std::pmr::vector<iris::Statement>& statements, Function_t const& function);

        template <typename Function_t>
        bool visit_statement_lists(iris::Statement& statement, Function_t const& function)
        {
            std::pmr::vector<iris::Statement> single;
            single.push_back(std::move(statement));
            bool const result = visit_statement_lists(single, function);
            statement = std::move(single[0]);
            return result;
        }

        template <typename Function_t>
        bool visit_statement_lists(std::pmr::vector<iris::Statement>& statements, Function_t const& function)
        {
            function(statements);

            for (iris::Statement& statement : statements)
            {
                for (iris::Expression& expression : statement.expressions)
                {
                    bool const ok = std::visit(
                        [&](auto& data) -> bool
                        {
                            using Data_t = std::decay_t<decltype(data)>;

                            if constexpr (std::is_same_v<Data_t, iris::Assert_expression>)
                                return visit_statement_lists(data.statement, function);
                            else if constexpr (std::is_same_v<Data_t, iris::Block_expression>)
                                return visit_statement_lists(data.statements, function);
                            else if constexpr (std::is_same_v<Data_t, iris::Constant_array_expression>)
                                return visit_statement_lists(data.array_data, function);
                            else if constexpr (std::is_same_v<Data_t, iris::For_loop_expression>)
                                return visit_statement_lists(data.range_end, function) && visit_statement_lists(data.then_statements, function);
                            else if constexpr (std::is_same_v<Data_t, iris::If_expression>)
                            {
                                for (iris::Condition_statement_pair& pair : data.series)
                                {
                                    if (pair.condition.has_value() && !visit_statement_lists(pair.condition.value(), function))
                                        return false;
                                    if (!visit_statement_lists(pair.then_statements, function))
                                        return false;
                                }
                                return true;
                            }
                            else if constexpr (std::is_same_v<Data_t, iris::Lambda_expression>)
                                return visit_statement_lists(data.body, function);
                            else if constexpr (std::is_same_v<Data_t, iris::Statement_block_expression>)
                                return visit_statement_lists(data.statements, function) && visit_statement_lists(data.result, function);
                            else if constexpr (std::is_same_v<Data_t, iris::Switch_expression>)
                            {
                                for (iris::Switch_case_expression_pair& pair : data.cases)
                                {
                                    if (!visit_statement_lists(pair.statements, function))
                                        return false;
                                }
                                return true;
                            }
                            else if constexpr (std::is_same_v<Data_t, iris::Ternary_condition_expression>)
                                return visit_statement_lists(data.then_statement, function) && visit_statement_lists(data.else_statement, function);
                            else if constexpr (std::is_same_v<Data_t, iris::While_loop_expression>)
                                return visit_statement_lists(data.condition, function) && visit_statement_lists(data.then_statements, function);
                            else if constexpr (
                                std::is_same_v<Data_t, iris::Access_expression> ||
                                std::is_same_v<Data_t, iris::Access_array_expression> ||
                                std::is_same_v<Data_t, iris::Assignment_expression> ||
                                std::is_same_v<Data_t, iris::Binary_expression> ||
                                std::is_same_v<Data_t, iris::Break_expression> ||
                                std::is_same_v<Data_t, iris::Call_expression> ||
                                std::is_same_v<Data_t, iris::Cast_expression> ||
                                std::is_same_v<Data_t, iris::Comment_expression> ||
                                std::is_same_v<Data_t, iris::Constant_expression> ||
                                std::is_same_v<Data_t, iris::Continue_expression> ||
                                std::is_same_v<Data_t, iris::Defer_expression> ||
                                std::is_same_v<Data_t, iris::Dereference_and_access_expression> ||
                                std::is_same_v<Data_t, iris::Instantiate_expression> ||
                                std::is_same_v<Data_t, iris::Invalid_expression> ||
                                std::is_same_v<Data_t, iris::Null_pointer_expression> ||
                                std::is_same_v<Data_t, iris::Parenthesis_expression> ||
                                std::is_same_v<Data_t, iris::Return_expression> ||
                                std::is_same_v<Data_t, iris::Type_expression> ||
                                std::is_same_v<Data_t, iris::Unary_expression> ||
                                std::is_same_v<Data_t, iris::Variable_declaration_expression> ||
                                std::is_same_v<Data_t, iris::Variable_declaration_with_type_expression> ||
                                std::is_same_v<Data_t, iris::Variable_expression>
                            )
                                return true;
                            else
                                return false;
                        },
                        expression.data
                    );

                    if (!ok)
                        return false;
                }
            }

            return true;
        }

        bool is_variable_named(iris::Expression const& expression, std::span<std::pmr::string const> const names)
        {
            if (!std::holds_alternative<iris::Variable_expression>(expression.data))
                return false;

            std::pmr::string const& name = std::get<iris::Variable_expression>(expression.data).name;
            return std::find(names.begin(), names.end(), name) != names.end();
        }

        // `var <name> = <one of names>;`: an immutable copy of the element, which is the element itself.
        std::optional<std::string_view> get_alias_declaration_name(iris::Statement const& statement, std::span<std::pmr::string const> const names)
        {
            if (statement.expressions.size() != 2 || !std::holds_alternative<iris::Variable_declaration_expression>(statement.expressions[0].data))
                return std::nullopt;

            iris::Variable_declaration_expression const& declaration = std::get<iris::Variable_declaration_expression>(statement.expressions[0].data);
            if (declaration.is_mutable || !is_variable_named(statement.expressions[declaration.right_hand_side.expression_index], names))
                return std::nullopt;

            return std::string_view{ declaration.name };
        }

        struct Element_field_uses
        {
            // The parameter followed by its immutable copies, such as a for-each loop variable.
            std::pmr::vector<std::pmr::string> names;
            // The members read through any of `names`, in order of first use.
            std::pmr::vector<std::pmr::string> fields;
        };

        // The fields `body` reads from the element `parameter_name`, when every use of it (or of an
        // immutable copy of it) is a member access `p.field`. Nullopt when the whole element is used.
        std::optional<Element_field_uses> find_element_field_uses(
            std::pmr::vector<iris::Statement>& body,
            std::string_view const parameter_name
        )
        {
            std::pmr::vector<std::pmr::string> names;
            names.push_back(std::pmr::string{ parameter_name });

            // The immutable copies of the element, followed transitively.
            for (bool added = true; added;)
            {
                added = false;

                bool const visited_everything = visit_statement_lists(
                    body,
                    [&](std::pmr::vector<iris::Statement>& statements)
                    {
                        for (iris::Statement const& statement : statements)
                        {
                            std::optional<std::string_view> const alias_name = get_alias_declaration_name(statement, names);
                            if (alias_name.has_value() && std::find(names.begin(), names.end(), alias_name.value()) == names.end())
                            {
                                names.push_back(std::pmr::string{ alias_name.value() });
                                added = true;
                            }
                        }
                    }
                );
                if (!visited_everything)
                    return std::nullopt;
            }

            // Every reference must be the object of a member access or the source of a copy.
            bool only_member_accesses = true;
            std::pmr::vector<std::pmr::string> fields;

            visit_statement_lists(
                body,
                [&](std::pmr::vector<iris::Statement>& statements)
                {
                    for (iris::Statement const& statement : statements)
                    {
                        std::size_t covered_count = 0;
                        if (get_alias_declaration_name(statement, names).has_value())
                            covered_count += 1;

                        for (iris::Expression const& expression : statement.expressions)
                        {
                            if (std::holds_alternative<iris::Lambda_expression>(expression.data))
                            {
                                iris::Lambda_expression const& lambda_expression = std::get<iris::Lambda_expression>(expression.data);
                                for (std::pmr::string const& lambda_parameter_name : lambda_expression.parameter_names)
                                {
                                    if (std::find(names.begin(), names.end(), lambda_parameter_name) != names.end())
                                        only_member_accesses = false;
                                }
                            }

                            if (!std::holds_alternative<iris::Access_expression>(expression.data))
                                continue;

                            iris::Access_expression const& access_expression = std::get<iris::Access_expression>(expression.data);
                            if (!is_variable_named(statement.expressions[access_expression.expression.expression_index], names))
                                continue;

                            covered_count += 1;
                            if (std::find(fields.begin(), fields.end(), access_expression.member_name) == fields.end())
                                fields.push_back(access_expression.member_name);
                        }

                        std::size_t const reference_count = static_cast<std::size_t>(std::count_if(
                            statement.expressions.begin(),
                            statement.expressions.end(),
                            [&](iris::Expression const& expression) -> bool { return is_variable_named(expression, names); }
                        ));

                        if (reference_count != covered_count)
                            only_member_accesses = false;
                    }
                }
            );

            if (!only_member_accesses)
                return std::nullopt;

            return Element_field_uses
            {
                .names = std::move(names),
                .fields = std::move(fields),
            };
        }

        // Drops the immutable copies of the element from `body`, which the rewrite leaves unused, and
        // calls `rewrite(statement, access_index)` on every member access `p.field` of the element.
        template <typename Function_t>
        void rewrite_element_field_uses(
            std::pmr::vector<iris::Statement>& body,
            std::span<std::pmr::string const> const names,
            Function_t const& rewrite
        )
        {
            visit_statement_lists(
                body,
                [&](std::pmr::vector<iris::Statement>& statements)
                {
                    std::erase_if(
                        statements,
                        [&](iris::Statement const& statement) -> bool { return get_alias_declaration_name(statement, names).has_value(); }
                    );

                    for (iris::Statement& statement : statements)
                    {
                        for (std::size_t index = 0; index < statement.expressions.size(); ++index)
                        {
                            iris::Expression const& expression = statement.expressions[index];
                            if (!std::holds_alternative<iris::Access_expression>(expression.data))
                                continue;

                            iris::Access_expression const& access_expression = std::get<iris::Access_expression>(expression.data);
                            if (!is_variable_named(statement.expressions[access_expression.expression.expression_index], names))
                                continue;

                            rewrite(statement, index);
                        }
                    }
                }
            );
        }

        // For a Soa_array_view input, reads only the fields that the loop body uses instead of the whole
        // element: when every use of the parameter is a member access (see find_element_field_uses), each
        // used field is read once into a generated local at the top of the iteration and the accesses read
        // that local. Reading at the top keeps the semantics of `var p = input[i];` even when the body
        // writes to the view. Returns the field bindings to insert, or nullopt when the whole element has
        // to be read.
        std::optional<std::pmr::vector<iris::Statement>> bind_soa_fields(
            std::pmr::vector<iris::Statement>& body,
            std::string_view const parameter_name,
            Pipeline_input const& input,
            std::size_t const pipeline_index,
            std::string_view const index_name
        )
        {
            std::optional<Element_field_uses> const uses = find_element_field_uses(body, parameter_name);
            if (!uses.has_value())
                return std::nullopt;

            std::pmr::vector<std::pmr::string> const& fields = uses->fields;

            auto const get_field_local_name = [&](std::string_view const field) -> std::pmr::string
            {
                return get_pipeline_local_name(pipeline_index, std::format("{}_{}", parameter_name, field));
            };

            rewrite_element_field_uses(
                body,
                uses->names,
                [&](iris::Statement& statement, std::size_t const access_index) -> void
                {
                    iris::Access_expression const access_expression = std::get<iris::Access_expression>(statement.expressions[access_index].data);
                    statement.expressions[access_index].data = iris::Variable_expression{ .name = get_field_local_name(access_expression.member_name) };
                    invalidate_expression_and_descendants(statement, access_expression.expression.expression_index, {});
                }
            );

            // var $pipeline<N>_<p>_<field> = <input>-><field>[<index>];
            std::pmr::vector<iris::Statement> field_bindings;
            field_bindings.reserve(fields.size());

            for (std::pmr::string const& field : fields)
            {
                iris::Statement value;
                std::uint64_t const access_array_index = add_expression(value, iris::Expression{ .data = iris::Access_array_expression{} });
                std::uint64_t const column_index = add_expression(value, iris::Expression{ .data = iris::Dereference_and_access_expression{} });
                std::uint64_t const input_index = add_expression(value, create_variable_expression(input.variable_name));
                std::uint64_t const element_index = add_expression(value, create_variable_expression(std::pmr::string{ index_name }));

                std::get<iris::Dereference_and_access_expression>(value.expressions[column_index].data) = iris::Dereference_and_access_expression
                {
                    .expression = to_index(input_index),
                    .member_name = field,
                };

                std::get<iris::Access_array_expression>(value.expressions[access_array_index].data) = iris::Access_array_expression
                {
                    .expression = to_index(column_index),
                    .index = to_index(element_index),
                };

                field_bindings.push_back(create_variable_declaration_statement(get_field_local_name(field), value));
            }

            return field_bindings;
        }

        // For any other input, reads the element in place instead of copying it: when every use of the
        // parameter is a member access (see find_element_field_uses), the element's address is taken
        // once at the top of the iteration and `p.field` becomes `$pipeline<N>_<p>->field`. Unlike the
        // copy, the fields are read when used, so writes the body makes to the element before reading
        // a field are visible, as in a hand-written `var p = &input[i];` loop. Returns the binding to
        // insert (none when no field is read), or nullopt when the whole element has to be copied.
        std::optional<std::pmr::vector<iris::Statement>> bind_element_in_place(
            std::pmr::vector<iris::Statement>& body,
            std::string_view const parameter_name,
            Pipeline_input const& input,
            std::size_t const pipeline_index,
            std::string_view const index_name
        )
        {
            std::optional<Element_field_uses> const uses = find_element_field_uses(body, parameter_name);
            if (!uses.has_value())
                return std::nullopt;

            std::pmr::string const pointer_name = get_pipeline_local_name(pipeline_index, parameter_name);

            rewrite_element_field_uses(
                body,
                uses->names,
                [&](iris::Statement& statement, std::size_t const access_index) -> void
                {
                    iris::Access_expression const access_expression = std::get<iris::Access_expression>(statement.expressions[access_index].data);
                    statement.expressions[access_expression.expression.expression_index].data = iris::Variable_expression{ .name = pointer_name };
                    statement.expressions[access_index].data = iris::Dereference_and_access_expression
                    {
                        .expression = access_expression.expression,
                        .member_name = access_expression.member_name,
                    };
                }
            );

            std::pmr::vector<iris::Statement> bindings;
            if (uses->fields.empty())
                return bindings;

            // var $pipeline<N>_<p> = &<input>[<index>];
            iris::Statement value;
            std::uint64_t const address_index = add_expression(value, iris::Expression{ .data = iris::Unary_expression{} });
            std::uint64_t const access_index = add_expression(value, iris::Expression{ .data = iris::Access_array_expression{} });
            std::uint64_t const input_index = add_expression(value, create_variable_expression(input.variable_name));
            std::uint64_t const element_index = add_expression(value, create_variable_expression(std::pmr::string{ index_name }));

            std::get<iris::Access_array_expression>(value.expressions[access_index].data) = iris::Access_array_expression
            {
                .expression = to_index(input_index),
                .index = to_index(element_index),
            };
            std::get<iris::Unary_expression>(value.expressions[address_index].data) = iris::Unary_expression
            {
                .expression = to_index(access_index),
                .operation = iris::Unary_operation::Address_of,
            };

            bindings.push_back(create_variable_declaration_statement(pointer_name, value));
            return bindings;
        }

        // Binds the parameters to the elements at the expansion's index, at the top of `body`. A
        // parameter whose fields `body` only reads one by one is not copied: a Soa_array_view parameter
        // gets one local per field, and any other one a pointer to the element.
        void insert_input_bindings(
            Pipeline_expansion const& expansion,
            std::pmr::vector<iris::Statement>& body
        )
        {
            iris::Pipeline_expression const& pipeline = *expansion.pipeline;

            for (std::size_t input_index = expansion.inputs.size(); input_index > 0; --input_index)
            {
                Pipeline_input const& input = expansion.inputs[input_index - 1];
                std::string_view const parameter_name = pipeline.input_parameter_names[input_index - 1];

                std::optional<std::pmr::vector<iris::Statement>> const field_bindings =
                    input.is_soa ?
                    bind_soa_fields(body, parameter_name, input, expansion.pipeline_index, expansion.index_name) :
                    bind_element_in_place(body, parameter_name, input, expansion.pipeline_index, expansion.index_name);

                if (field_bindings.has_value())
                {
                    body.insert(body.begin(), field_bindings->begin(), field_bindings->end());
                    continue;
                }

                body.insert(body.begin(), create_input_element_statement(parameter_name, input, expansion.index_name));
            }
        }

        // The index loop: binds the inputs, runs the stages, and runs `innermost` for every element that
        // passes all filters, with the output element bound to its name.
        iris::Statement create_pipeline_loop_statement(
            Pipeline_expansion const& expansion,
            std::pmr::vector<iris::Statement> innermost
        )
        {
            std::pmr::vector<iris::Statement> body = std::move(innermost);
            iris::Pipeline_expression const& pipeline = *expansion.pipeline;

            for (auto stage = pipeline.stages.rbegin(); stage != pipeline.stages.rend(); ++stage)
            {
                if (stage->kind == iris::Pipeline_stage_kind::Map)
                {
                    if (!stage->binding_name.has_value())
                        continue;

                    body.insert(body.begin(), create_variable_declaration_statement(stage->binding_name.value(), stage->expression));
                }
                else if (stage->kind == iris::Pipeline_stage_kind::Filter)
                {
                    iris::Statement if_statement = create_if_statement(stage->expression, std::move(body));
                    body = {};
                    body.push_back(std::move(if_statement));
                }
                else if (stage->kind == iris::Pipeline_stage_kind::Take_while)
                {
                    // if !(<condition>) { break; }  stops the whole loop, also for elements that a later
                    // filter would have dropped.
                    body.insert(body.begin(), create_stop_unless_statement(stage->expression));
                }
                else if (stage->kind == iris::Pipeline_stage_kind::Enumerate)
                {
                    // var <name> = $pipeline<N>_i;  the index in the inputs, before any filter.
                    if (stage->binding_name.has_value())
                        body.insert(body.begin(), create_variable_copy_statement(stage->binding_name.value(), expansion.index_name));
                }
            }

            insert_input_bindings(expansion, body);

            return create_index_loop_statement(expansion.index_name, expansion.inputs[0], std::move(body));
        }

        // Lowers a for-each statement over a pipeline call. Returns nullopt when the statement is
        // not one, or when it cannot be lowered (validation has already reported why).
        std::optional<iris::Statement> lower_for_each_statement(
            Pipeline_pass_context& context,
            Scope const& scope,
            iris::Statement const& statement
        )
        {
            if (statement.expressions.empty() || !std::holds_alternative<iris::For_each_expression>(statement.expressions[0].data))
                return std::nullopt;

            iris::For_each_expression const& for_each_expression = std::get<iris::For_each_expression>(statement.expressions[0].data);
            iris::Expression const& range_expression = statement.expressions[for_each_expression.range_expression.expression_index];

            iris::Pipeline_expression const* const pipeline = get_pipeline_from_call_expression(scope, statement, range_expression);
            if (pipeline == nullptr)
                return std::nullopt;

            std::optional<Pipeline_expansion> expansion = begin_pipeline_expansion(
                context,
                scope,
                statement,
                pipeline,
                std::get<iris::Call_expression>(range_expression.data).arguments
            );
            if (!expansion.has_value())
                return std::nullopt;

            std::pmr::vector<iris::Statement> innermost;
            innermost.reserve(for_each_expression.then_statements.size() + 1);

            if (for_each_expression.variable_name != expansion->output_name)
                innermost.push_back(create_variable_copy_statement(for_each_expression.variable_name, expansion->output_name));

            innermost.insert(innermost.end(), for_each_expression.then_statements.begin(), for_each_expression.then_statements.end());

            iris::Statement loop_statement = create_pipeline_loop_statement(expansion.value(), std::move(innermost));

            if (expansion->prologue.empty())
                return loop_statement;

            std::pmr::vector<iris::Statement> statements = std::move(expansion->prologue);
            statements.push_back(std::move(loop_statement));

            return create_single_expression_statement(iris::Expression
            {
                .data = iris::Block_expression{ .statements = std::move(statements) },
            });
        }

        // Expands `p(inputs).separate([scratch]).into(out)`: one loop per stage, so that each loop does
        // one thing and vectorizes on its own. The first loop reads the inputs; each later loop reads
        // the element the previous stage left in a buffer. Elements of the output type live in `out`,
        // others in `scratch`, at the same index; a filter compacts its buffer in place, and the count
        // of live elements goes down as filters and take_while drop them.
        std::optional<iris::Statement_block_expression> lower_separated_into(
            Pipeline_pass_context& context,
            Scope const& scope,
            iris::Statement const& statement,
            iris::Call_expression const& sink_call_expression,
            Pipeline_sink_call const& sink_call,
            Pipeline_expansion& expansion
        )
        {
            if (sink_call.sink_name != "into" || sink_call_expression.arguments.size() != 1 || !sink_call.separate.has_value())
                return std::nullopt;

            iris::Pipeline_expression const& pipeline = *expansion.pipeline;

            std::optional<Separated_pipeline_layout> const layout = get_separated_pipeline_layout(
                context.module_name,
                &context.function_declaration,
                scope,
                pipeline,
                context.declaration_database
            );
            if (!layout.has_value() || layout->has_several_scratch_types)
                return std::nullopt;
            if (layout->scratch_type.has_value() != sink_call.separate->scratch_argument.has_value())
                return std::nullopt;

            std::size_t const pipeline_index = expansion.pipeline_index;
            std::pmr::vector<iris::Statement> statements = std::move(expansion.prologue);

            // The scratch argument is written before the sink's own argument, so it is evaluated first.
            std::optional<std::pmr::string> scratch_name;
            if (sink_call.separate->scratch_argument.has_value())
                scratch_name = bind_argument_once(statement, sink_call.separate->scratch_argument.value(), get_pipeline_local_name(pipeline_index, "scratch"), statements);

            std::pmr::string const output_name = bind_argument_once(statement, sink_call_expression.arguments[0], get_pipeline_local_name(pipeline_index, "output"), statements);

            // map preserves the element count and filter only removes elements, so buffers as long as
            // the first input always have room.
            statements.push_back(create_output_capacity_assert_statement(Pipeline_input{ .variable_name = output_name }, expansion.inputs[0]));
            if (scratch_name.has_value())
                statements.push_back(create_output_capacity_assert_statement(Pipeline_input{ .variable_name = scratch_name.value() }, expansion.inputs[0], "Pipeline scratch buffer is large enough"));

            std::pmr::string const count_name = get_pipeline_local_name(pipeline_index, "count");
            {
                iris::Statement input_length;
                add_input_length_expression(input_length, expansion.inputs[0]);
                iris::Statement count_declaration = create_variable_declaration_statement(count_name, input_length);
                std::get<iris::Variable_declaration_expression>(count_declaration.expressions[0].data).is_mutable = true;
                statements.push_back(std::move(count_declaration));
            }

            auto const get_buffer_name = [&](bool const is_in_scratch) -> std::string_view
            {
                return is_in_scratch ? std::string_view{ scratch_name.value() } : std::string_view{ output_name };
            };

            auto const create_variable_value = [](std::string_view const name) -> iris::Expression
            {
                return create_variable_expression(std::pmr::string{ name });
            };

            // Without stages the output element is the single parameter: copy it.
            if (pipeline.stages.empty())
            {
                std::pmr::vector<iris::Statement> body;
                body.push_back(create_output_store_statement(output_name, expansion.index_name, pipeline.input_parameter_names[0]));
                insert_input_bindings(expansion, body);
                statements.push_back(create_index_loop_statement(expansion.index_name, expansion.inputs[0], std::move(body)));
            }

            for (std::size_t stage_index = 0; stage_index < pipeline.stages.size(); ++stage_index)
            {
                iris::Pipeline_stage const& stage = pipeline.stages[stage_index];
                Separated_pipeline_stage const& stage_layout = layout->stages[stage_index];
                bool const is_first = stage_index == 0;

                std::pmr::string const index_name =
                    is_first ?
                    expansion.index_name :
                    get_pipeline_local_name(pipeline_index, std::format("i{}", stage_index));

                std::pmr::vector<iris::Statement> body;

                // A later stage starts from the element the previous one left in its buffer.
                if (!is_first)
                {
                    Separated_pipeline_stage const& previous = layout->stages[stage_index - 1];

                    iris::Statement value;
                    std::uint64_t const access_index = add_expression(value, iris::Expression{ .data = iris::Access_array_expression{} });
                    std::uint64_t const buffer_index = add_expression(value, create_variable_value(get_buffer_name(previous.is_in_scratch)));
                    std::uint64_t const element_index = add_expression(value, create_variable_value(index_name));
                    std::get<iris::Access_array_expression>(value.expressions[access_index].data) = iris::Access_array_expression
                    {
                        .expression = to_index(buffer_index),
                        .index = to_index(element_index),
                    };
                    body.push_back(create_variable_declaration_statement(previous.element_name, value));
                }

                std::optional<std::pmr::string> kept_name;

                if (stage.kind == iris::Pipeline_stage_kind::Map)
                {
                    body.push_back(create_variable_declaration_statement(stage_layout.element_name, stage.expression));
                    body.push_back(create_output_store_statement(get_buffer_name(stage_layout.is_in_scratch), index_name, stage_layout.element_name));
                }
                else if (stage.kind == iris::Pipeline_stage_kind::Filter)
                {
                    // Compacts the kept elements to the front of the buffer the element lives in.
                    kept_name = get_pipeline_local_name(pipeline_index, std::format("kept{}", stage_index));

                    std::pmr::vector<iris::Statement> then_statements;
                    then_statements.push_back(create_output_store_statement(get_buffer_name(stage_layout.is_in_scratch), kept_name.value(), stage_layout.element_name));
                    then_statements.push_back(create_assignment_statement(kept_name.value(), create_uint64_constant_expression(1), iris::Binary_operation::Add));
                    body.push_back(create_if_statement(stage.expression, std::move(then_statements)));
                }
                else if (stage.kind == iris::Pipeline_stage_kind::Take_while)
                {
                    // if !(<condition>) { <count> = <index>; break; }
                    iris::Statement stop = create_stop_unless_statement(stage.expression);
                    iris::If_expression& if_expression = std::get<iris::If_expression>(stop.expressions[0].data);
                    if_expression.series[0].then_statements.insert(
                        if_expression.series[0].then_statements.begin(),
                        create_assignment_statement(count_name, create_variable_value(index_name), std::nullopt)
                    );
                    body.push_back(std::move(stop));

                    // The first loop still has to move the element out of the inputs.
                    if (is_first)
                        body.push_back(create_output_store_statement(get_buffer_name(stage_layout.is_in_scratch), index_name, stage_layout.element_name));
                }
                else
                {
                    return std::nullopt;
                }

                if (kept_name.has_value())
                    statements.push_back(create_mutable_variable_statement(kept_name.value(), create_uint64_constant_expression(0)));

                if (is_first)
                {
                    insert_input_bindings(expansion, body);
                    statements.push_back(create_index_loop_statement(index_name, expansion.inputs[0], std::move(body)));
                }
                else
                {
                    iris::Statement range_end;
                    add_expression(range_end, create_variable_value(count_name));
                    statements.push_back(create_index_loop_statement(index_name, std::move(range_end), std::move(body)));
                }

                if (kept_name.has_value())
                    statements.push_back(create_assignment_statement(count_name, create_variable_value(kept_name.value()), std::nullopt));
            }

            return iris::Statement_block_expression
            {
                .statements = std::move(statements),
                .result = create_written_prefix_statement(output_name, count_name),
            };
        }

        // Expands a value sink `p(inputs).<sink>(<arguments>)` into a statement block expression.
        std::optional<iris::Statement_block_expression> lower_value_sink(
            Pipeline_pass_context& context,
            Scope const& scope,
            iris::Statement const& statement,
            iris::Expression const& sink_expression,
            Pipeline_sink_call const& sink_call
        )
        {
            std::optional<Pipeline_expansion> expansion = begin_pipeline_expansion(
                context,
                scope,
                statement,
                sink_call.pipeline,
                sink_call.input_arguments
            );
            if (!expansion.has_value())
                return std::nullopt;

            iris::Call_expression const& sink_call_expression = std::get<iris::Call_expression>(sink_expression.data);

            if (sink_call.separate.has_value())
                return lower_separated_into(context, scope, statement, sink_call_expression, sink_call, expansion.value());

            std::size_t const pipeline_index = expansion->pipeline_index;

            std::pmr::vector<iris::Statement> statements = std::move(expansion->prologue);
            std::pmr::vector<iris::Statement> innermost;
            iris::Statement result;

            if (sink_call.sink_name == "count")
            {
                std::pmr::string const count_name = get_pipeline_local_name(pipeline_index, "count");

                statements.push_back(create_mutable_variable_statement(count_name, create_uint64_constant_expression(0)));
                innermost.push_back(create_assignment_statement(count_name, create_uint64_constant_expression(1), iris::Binary_operation::Add));
                add_expression(result, create_variable_expression(count_name));
            }
            else if (sink_call.sink_name == "any" || sink_call.sink_name == "all")
            {
                // any: false until an element is true. all: true until an element is false. Both stop at
                // the first element that decides the answer.
                bool const is_any = sink_call.sink_name == "any";
                std::pmr::string const result_name = get_pipeline_local_name(pipeline_index, "result");

                statements.push_back(create_mutable_variable_statement(result_name, create_bool_constant_expression(!is_any)));
                innermost.push_back(create_short_circuit_statement(expansion->output_name, !is_any, result_name, is_any));
                add_expression(result, create_variable_expression(result_name));
            }
            else if (sink_call.sink_name == "fold")
            {
                // fold(init, lambda (accumulator, element) => body): the accumulator is the running value,
                // so the lambda is inlined rather than called.
                if (sink_call_expression.arguments.size() != 2)
                    return std::nullopt;

                iris::Expression_index const function_index = sink_call_expression.arguments[1];
                iris::Expression const& function_expression = statement.expressions[function_index.expression_index];
                if (!std::holds_alternative<iris::Lambda_expression>(function_expression.data))
                {
                    // fold(init, f): a function or a lambda value is called on every element.
                    std::pmr::string const accumulator_name = get_pipeline_local_name(pipeline_index, "accumulator");

                    iris::Statement initial_value;
                    copy_expressions_to_new_statement(initial_value, statement, sink_call_expression.arguments[0]);
                    iris::Statement accumulator_declaration = create_variable_declaration_statement(accumulator_name, initial_value);
                    std::get<iris::Variable_declaration_expression>(accumulator_declaration.expressions[0].data).is_mutable = true;
                    statements.push_back(std::move(accumulator_declaration));

                    iris::Statement callee;
                    if (is_access_path(statement, function_index))
                    {
                        copy_expressions_to_new_statement(callee, statement, function_index);
                    }
                    else
                    {
                        std::pmr::string const function_name = bind_argument_once(statement, function_index, get_pipeline_local_name(pipeline_index, "function"), statements);
                        add_expression(callee, create_variable_expression(function_name));
                    }

                    std::array<std::string_view, 2> const argument_names{ accumulator_name, expansion->output_name };
                    innermost.push_back(create_assignment_from_statement(accumulator_name, create_call_statement(callee, argument_names)));
                    add_expression(result, create_variable_expression(accumulator_name));

                    statements.push_back(create_pipeline_loop_statement(expansion.value(), std::move(innermost)));
                    return iris::Statement_block_expression
                    {
                        .statements = std::move(statements),
                        .result = std::move(result),
                    };
                }

                iris::Lambda_expression const& lambda_expression = std::get<iris::Lambda_expression>(function_expression.data);
                if (lambda_expression.parameter_names.size() != 2)
                    return std::nullopt;

                std::pmr::string const& accumulator_name = lambda_expression.parameter_names[0];
                std::pmr::string const& element_name = lambda_expression.parameter_names[1];

                iris::Statement initial_value;
                copy_expressions_to_new_statement(initial_value, statement, sink_call_expression.arguments[0]);
                iris::Statement accumulator_declaration = create_variable_declaration_statement(accumulator_name, initial_value);
                std::get<iris::Variable_declaration_expression>(accumulator_declaration.expressions[0].data).is_mutable = true;
                statements.push_back(std::move(accumulator_declaration));

                if (element_name != expansion->output_name)
                    innermost.push_back(create_variable_copy_statement(element_name, expansion->output_name));

                innermost.push_back(create_assignment_from_statement(accumulator_name, lambda_expression.body));
                add_expression(result, create_variable_expression(accumulator_name));
            }
            else if (sink_call.sink_name == "first")
            {
                std::optional<iris::Type_reference> output_type = get_pipeline_output_type(
                    context.module_name,
                    &context.function_declaration,
                    scope,
                    *expansion->pipeline,
                    context.declaration_database
                );
                if (!output_type.has_value())
                    return std::nullopt;

                std::pmr::string const result_name = get_pipeline_local_name(pipeline_index, "result");

                statements.push_back(create_default_typed_variable_statement(result_name, create_optional_type_reference({ std::move(output_type.value()) })));

                iris::Statement value;
                add_create_optional_call(value, expansion->output_name);
                innermost.push_back(create_assignment_from_statement(result_name, value));
                innermost.push_back(create_break_statement());

                add_expression(result, create_variable_expression(result_name));
            }
            else if (sink_call.sink_name == "in_place")
            {
                // Compacts the kept elements to the front of the input. Each element is read into a local
                // before anything is written, and the write index never passes the read index.
                std::pmr::string const count_name = get_pipeline_local_name(pipeline_index, "count");
                std::pmr::string const& input_name = expansion->inputs[0].variable_name;

                statements.push_back(create_mutable_variable_statement(count_name, create_uint64_constant_expression(0)));

                innermost.push_back(create_output_store_statement(input_name, count_name, expansion->output_name));
                innermost.push_back(create_assignment_statement(count_name, create_uint64_constant_expression(1), iris::Binary_operation::Add));
                result = create_written_prefix_statement(input_name, count_name);
            }
            else if (sink_call.sink_name == "into_fixed")
            {
                // The result is a Fixed_capacity_array::<U, N> where N is the inputs' static size. map keeps
                // the count and filter only removes elements, so every kept element fits.
                std::optional<iris::Type_reference> const result_type = get_pipeline_sink_type(
                    context.module_name,
                    &context.function_declaration,
                    scope,
                    statement,
                    sink_call,
                    context.declaration_database
                );
                if (!result_type.has_value())
                    return std::nullopt;

                std::pmr::string const result_name = get_pipeline_local_name(pipeline_index, "result");

                statements.push_back(create_default_typed_variable_statement(result_name, result_type.value()));
                innermost.push_back(create_push_statement(result_name, expansion->output_name));
                add_expression(result, create_variable_expression(result_name));
            }
            else if (sink_call.sink_name == "into")
            {
                if (sink_call_expression.arguments.size() != 1)
                    return std::nullopt;

                std::pmr::string const count_name = get_pipeline_local_name(pipeline_index, "count");
                std::pmr::string const output_name = bind_argument_once(
                    statement,
                    sink_call_expression.arguments[0],
                    get_pipeline_local_name(pipeline_index, "output"),
                    statements
                );

                // map preserves the element count and filter only removes elements, so a buffer as long as
                // the first input always has room.
                Pipeline_input const output_input{ .variable_name = output_name, .constant_length = std::nullopt };
                statements.push_back(create_output_capacity_assert_statement(output_input, expansion->inputs[0]));
                statements.push_back(create_mutable_variable_statement(count_name, create_uint64_constant_expression(0)));

                innermost.push_back(create_output_store_statement(output_name, count_name, expansion->output_name));
                innermost.push_back(create_assignment_statement(count_name, create_uint64_constant_expression(1), iris::Binary_operation::Add));
                result = create_written_prefix_statement(output_name, count_name);
            }
            else
            {
                return std::nullopt;
            }

            statements.push_back(create_pipeline_loop_statement(expansion.value(), std::move(innermost)));

            return iris::Statement_block_expression
            {
                .statements = std::move(statements),
                .result = std::move(result),
            };
        }

        // Replaces every value sink in `statement`, outermost first. The sink's own sub-expressions are
        // copied into the block where needed and invalidated in place, so nothing walks them again.
        bool lower_value_sinks(
            Pipeline_pass_context& context,
            Scope const& scope,
            iris::Statement& statement
        )
        {
            bool lowered_any = false;

            // Sub-expressions are stored after their parents, so the lowest index is the outermost sink.
            // A sink nested inside another one's arguments is copied into that block and lowered there.
            for (std::size_t index = 0; index < statement.expressions.size(); ++index)
            {
                iris::Expression const& expression = statement.expressions[index];

                std::optional<Pipeline_sink_call> const sink_call = get_pipeline_sink_call(scope, statement, expression);
                if (!sink_call.has_value() || !is_pipeline_sink_name(sink_call->sink_name))
                    continue;

                std::optional<iris::Statement_block_expression> block = lower_value_sink(context, scope, statement, expression, sink_call.value());
                if (!block.has_value())
                    continue;

                std::optional<iris::Source_range> const source_range = expression.source_range;

                invalidate_expression_and_descendants(statement, index, {});
                statement.expressions[index] = iris::Expression
                {
                    .data = std::move(block.value()),
                    .source_range = source_range,
                };

                lowered_any = true;
            }

            return lowered_any;
        }

        void remove_pipeline_declarations(std::pmr::vector<iris::Statement>& statements);

        void remove_pipeline_declarations(iris::Statement& statement)
        {
            for (iris::Expression& expression : statement.expressions)
            {
                if (std::holds_alternative<iris::Block_expression>(expression.data))
                {
                    remove_pipeline_declarations(std::get<iris::Block_expression>(expression.data).statements);
                }
                else if (std::holds_alternative<iris::For_each_expression>(expression.data))
                {
                    remove_pipeline_declarations(std::get<iris::For_each_expression>(expression.data).then_statements);
                }
                else if (std::holds_alternative<iris::For_loop_expression>(expression.data))
                {
                    remove_pipeline_declarations(std::get<iris::For_loop_expression>(expression.data).then_statements);
                }
                else if (std::holds_alternative<iris::If_expression>(expression.data))
                {
                    for (iris::Condition_statement_pair& pair : std::get<iris::If_expression>(expression.data).series)
                        remove_pipeline_declarations(pair.then_statements);
                }
                else if (std::holds_alternative<iris::Lambda_expression>(expression.data))
                {
                    remove_pipeline_declarations(std::get<iris::Lambda_expression>(expression.data).body);
                }
                else if (std::holds_alternative<iris::Switch_expression>(expression.data))
                {
                    for (iris::Switch_case_expression_pair& pair : std::get<iris::Switch_expression>(expression.data).cases)
                        remove_pipeline_declarations(pair.statements);
                }
                else if (std::holds_alternative<iris::While_loop_expression>(expression.data))
                {
                    remove_pipeline_declarations(std::get<iris::While_loop_expression>(expression.data).then_statements);
                }
            }
        }

        void remove_pipeline_declarations(std::pmr::vector<iris::Statement>& statements)
        {
            std::erase_if(
                statements,
                [](iris::Statement const& statement) -> bool
                {
                    return !statement.expressions.empty() && std::holds_alternative<iris::Pipeline_expression>(statement.expressions[0].data);
                }
            );

            for (iris::Statement& statement : statements)
                remove_pipeline_declarations(statement);
        }
    }

    void run_pipeline_pass_on_function(
        std::string_view const module_name,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition& function_definition,
        iris::Declaration_database const& declaration_database
    )
    {
        Scope scope{};

        add_parameters_to_scope(
            scope,
            function_declaration.input_parameter_names,
            function_declaration.type.input_parameter_types,
            function_declaration.input_parameter_source_positions
        );

        Pipeline_pass_context context
        {
            .module_name = module_name,
            .function_declaration = function_declaration,
            .declaration_database = declaration_database,
        };

        bool found_pipeline = false;

        // The visitor reads each statement again after the callback, so a for-each replaced here
        // is walked as the loop it became, and pipelines used inside its body are lowered too. It
        // also walks lambda bodies, with the lambda's parameters in scope, so pipelines declared or
        // consumed there are expanded inside the lambda before the lambda pass lowers it.
        auto const callback = [&](iris::Statement const& statement, Scope const& statement_scope) -> void
        {
            if (statement.expressions.empty())
                return;

            if (std::holds_alternative<iris::Pipeline_expression>(statement.expressions[0].data))
                found_pipeline = true;

            std::optional<iris::Statement> lowered = lower_for_each_statement(context, statement_scope, statement);
            if (lowered.has_value())
                const_cast<iris::Statement&>(statement) = std::move(lowered.value());

            // The visitor walks the statement blocks this creates, so sinks nested in their inputs are
            // lowered too, with the block's locals in scope.
            lower_value_sinks(context, statement_scope, const_cast<iris::Statement&>(statement));
        };

        visit_statements_using_scope_including_lambda_bodies(
            module_name,
            &function_declaration,
            scope,
            function_definition.statements,
            declaration_database,
            callback
        );

        // Scope entries pointed into the declarations, so they can only go once the walk is over.
        if (found_pipeline)
            remove_pipeline_declarations(function_definition.statements);
    }

    void run_pipeline_pass_on_module(
        iris::Module& core_module,
        iris::Declaration_database const& declaration_database
    )
    {
        for (iris::Function_definition& function_definition : core_module.definitions.function_definitions)
        {
            std::optional<Function_declaration const*> const function_declaration = find_function_declaration(core_module, function_definition.name);
            if (!function_declaration.has_value())
                continue;

            run_pipeline_pass_on_function(
                core_module.name,
                *function_declaration.value(),
                function_definition,
                declaration_database
            );
        }
    }
}
