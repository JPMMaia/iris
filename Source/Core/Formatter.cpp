module iris.core.formatter;

import std;

import iris.core;
import iris.core.declarations;
import iris.core.types;

namespace iris
{
    static std::pmr::vector<std::string_view> decode_comment(
        std::string_view const value,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<std::string_view> temporary{temporaries_allocator};

        std::size_t index = 0;
        while (index < value.size())
        {
            std::size_t const end_index = value.find("\n", index);
            std::size_t const content_index = 
                end_index == std::string_view::npos ?
                value.size() :
                end_index;

            std::string_view const content = value.substr(index, end_index - index);
            temporary.push_back(content);

            index = content_index + 1;
        }

        return std::pmr::vector<std::string_view>{std::move(temporary), output_allocator};
    }

    static std::pmr::string to_string(String_buffer const& buffer)
    {
        return std::pmr::string{buffer.string_stream.str()};
    }

    static void add_text(
        String_buffer& buffer,
        std::string_view const text
    )
    {
        buffer.string_stream << text;
    }

    static void add_integer_text(
        String_buffer& buffer,
        std::int64_t const value
    )
    {
        buffer.string_stream << value;
    }

    static void add_integer_text(
        String_buffer& buffer,
        std::uint64_t const value
    )
    {
        buffer.string_stream << value;
    }

    static void add_new_line(
        String_buffer& buffer
    )
    {
        add_text(buffer, "\n");
        buffer.current_line += 1;
    }

    static void add_indentation(
        String_buffer& buffer,
        std::uint32_t const indentation
    )
    {
        for (std::uint32_t index = 0; index < indentation; ++index)
            add_text(buffer, " ");
    }

    static void add_comment(
        String_buffer& buffer,
        std::string_view const comment,
        std::uint32_t const indentation
    )
    {
        add_text(buffer, "//");

        for (std::size_t index = 0; index < comment.size(); ++index)
        {
            char const character = comment[index];
            
            if (character == '\n')
            {
                add_new_line(buffer);
                add_indentation(buffer, indentation);
                add_text(buffer, "//");
            }
            else
            {
                add_text(buffer, std::string_view{&character, 1});
            }
        }
    }

    std::uint32_t get_current_line_position(
        String_buffer const& buffer
    )
    {
        return buffer.current_line;
    }

    static std::optional<std::string_view> get_declaration_comment(
        Declaration const& declaration
    )
    {
        if (std::holds_alternative<Alias_type_declaration const*>(declaration.data))
        {
            Alias_type_declaration const& value = *std::get<Alias_type_declaration const*>(declaration.data);
            return value.comment.has_value() ? std::optional<std::string_view>{value.comment.value()} : std::nullopt;
        }
        else if (std::holds_alternative<Enum_declaration const*>(declaration.data))
        {
            Enum_declaration const& value = *std::get<Enum_declaration const*>(declaration.data);
            return value.comment.has_value() ? std::optional<std::string_view>{value.comment.value()} : std::nullopt;
        }
        else if (std::holds_alternative<Function_constructor const*>(declaration.data))
        {
            Function_constructor const& value = *std::get<Function_constructor const*>(declaration.data);
            return value.comment.has_value() ? std::optional<std::string_view>{value.comment.value()} : std::nullopt;
        }
        else if (std::holds_alternative<Function_declaration const*>(declaration.data))
        {
            Function_declaration const& value = *std::get<Function_declaration const*>(declaration.data);
            return value.comment.has_value() ? std::optional<std::string_view>{value.comment.value()} : std::nullopt;
        }
        else if (std::holds_alternative<Global_variable_declaration const*>(declaration.data))
        {
            Global_variable_declaration const& value = *std::get<Global_variable_declaration const*>(declaration.data);
            return value.comment.has_value() ? std::optional<std::string_view>{value.comment.value()} : std::nullopt;
        }
        else if (std::holds_alternative<Struct_declaration const*>(declaration.data))
        {
            Struct_declaration const& value = *std::get<Struct_declaration const*>(declaration.data);
            return value.comment.has_value() ? std::optional<std::string_view>{value.comment.value()} : std::nullopt;
        }
        else if (std::holds_alternative<Type_constructor const*>(declaration.data))
        {
            Type_constructor const& value = *std::get<Type_constructor const*>(declaration.data);
            return value.comment.has_value() ? std::optional<std::string_view>{value.comment.value()} : std::nullopt;
        }
        else if (std::holds_alternative<Union_declaration const*>(declaration.data))
        {
            Union_declaration const& value = *std::get<Union_declaration const*>(declaration.data);
            return value.comment.has_value() ? std::optional<std::string_view>{value.comment.value()} : std::nullopt;
        }

        return std::nullopt;
    }

    static bool is_test_declaration(Declaration const& declaration)
    {
        if (std::holds_alternative<iris::Function_declaration const*>(declaration.data))
        {
            iris::Function_declaration const& function_declaration = *std::get<iris::Function_declaration const*>(declaration.data);
            return function_declaration.is_test;
        }

        return false;
    }

    static void add_format_declaration(
        String_buffer& buffer,
        iris::Module const& core_module,
        Declaration const& declaration,
        std::optional<std::pmr::string> const& unique_name,
        bool const is_export,
        Format_options const& options
    )
    {
        std::optional<std::string_view> const comment = get_declaration_comment(declaration);
        if (comment.has_value())
        {
            add_comment(buffer, comment.value(), 0);
            add_new_line(buffer);
        }

        if (is_test_declaration(declaration))
        {
            add_text(buffer, "@test");
            add_new_line(buffer);
        }

        if (unique_name.has_value())
        {
            add_text(buffer, "@unique_name(\"");
            add_text(buffer, unique_name.value());
            add_text(buffer, "\")");
            add_new_line(buffer);
        }

        if (is_export)
            add_text(buffer, "export ");

        auto const visitor = [&](auto const& value) -> void
        {
            using Declaration_type = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<Declaration_type, Alias_type_declaration const*>)
            {
                add_format_alias_type_declaration(buffer, *value, options);
            }
            else if constexpr (std::is_same_v<Declaration_type, Enum_declaration const*>)
            {
                add_format_enum_declaration(buffer, *value, options);
            }
            else if constexpr (std::is_same_v<Declaration_type, Function_declaration const*>)
            {
                add_format_function_declaration(buffer, *value, 0, options);

                std::optional<Function_definition const*> function_definition = find_function_definition(core_module, value->name);
                if (function_definition.has_value())
                {
                    add_format_function_definition(buffer, *function_definition.value(), 0, options);
                }
                else
                {
                    add_text(buffer, ";");
                }
            }
            else if constexpr (std::is_same_v<Declaration_type, Function_constructor const*>)
            {
                add_format_function_constructor(buffer, *value, options);
            }
            else if constexpr (std::is_same_v<Declaration_type, Global_variable_declaration const*>)
            {
                add_format_global_variable_declaration(buffer, *value, options);
            }
            else if constexpr (std::is_same_v<Declaration_type, Struct_declaration const*>)
            {
                add_format_struct_declaration(buffer, *value, 0, options);
            }
            else if constexpr (std::is_same_v<Declaration_type, Type_constructor const*>)
            {
                add_format_type_constructor(buffer, *value, options);
            }
            else if constexpr (std::is_same_v<Declaration_type, Union_declaration const*>)
            {
                add_format_union_declaration(buffer, *value, 0, options);   
            }
        };

        std::visit(visitor, declaration.data);
    }

    static void add_format_import_module_with_alias(
        String_buffer& buffer,
        Import_module_with_alias const& alias_import,
        Format_options const& options
    )
    {
        add_text(buffer, "import ");
        add_text(buffer, alias_import.module_name);
        add_text(buffer, " as ");
        add_text(buffer, alias_import.alias);
        add_text(buffer, ";");
    }

    void add_format_expression(
        String_buffer& buffer,
        Statement const& statement,
        Expression const& expression,
        std::uint32_t const indentation,
        Format_options const& options
    );

    static bool statement_ends_with_semicolon(
        iris::Expression const& expression
    )
    {
        bool const does_not_end_with_semicolon =
            std::holds_alternative<iris::Block_expression>(expression.data) ||
            std::holds_alternative<iris::Comment_expression>(expression.data) ||
            std::holds_alternative<iris::Compile_time_expression>(expression.data) ||
            std::holds_alternative<iris::For_loop_expression>(expression.data) ||
            std::holds_alternative<iris::If_expression>(expression.data) ||
            std::holds_alternative<iris::Switch_expression>(expression.data) ||
            std::holds_alternative<iris::While_loop_expression>(expression.data);
        return !does_not_end_with_semicolon;
    }

    static void add_format_statement(
        String_buffer& buffer,
        Statement const& statement,
        std::uint32_t indentation,
        Format_options const& options,
        bool const add_semicolon = true
    )
    {
        if (statement.expressions.size() > 0)
        {
            iris::Expression const& first_expression = statement.expressions[0];
            add_format_expression(buffer, statement, first_expression, indentation, options);

            if (add_semicolon && statement_ends_with_semicolon(first_expression))
                add_text(buffer, ";");
        }
    }

    std::pmr::string format_statement(
        iris::Module const& core_module,
        Statement const& statement,
        std::uint32_t indentation,
        bool const add_semicolon,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        Format_options const options
        {
            .alias_imports = core_module.dependencies.alias_imports,
            .output_allocator = output_allocator,
            .temporaries_allocator = temporaries_allocator,
        };

        String_buffer buffer;

        add_format_statement(
            buffer,
            statement,
            indentation,
            options,
            add_semicolon
        );

        return to_string(buffer);
    }

    void add_format_expression(
        String_buffer& buffer,
        Statement const& statement,
        Expression const& expression,
        std::uint32_t const indentation,
        Format_options const& options
    )
    {
        if (std::holds_alternative<Access_expression>(expression.data))
        {
            Access_expression const& value = std::get<Access_expression>(expression.data);
            add_format_expression_access(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Access_array_expression>(expression.data))
        {
            Access_array_expression const& value = std::get<Access_array_expression>(expression.data);
            add_format_expression_access_array(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Assert_expression>(expression.data))
        {
            Assert_expression const& value = std::get<Assert_expression>(expression.data);
            add_format_expression_assert(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Assignment_expression>(expression.data))
        {
            Assignment_expression const& value = std::get<Assignment_expression>(expression.data);
            add_format_expression_assignment(buffer, statement, value, indentation, options);
        }
        else if (std::holds_alternative<Binary_expression>(expression.data))
        {
            Binary_expression const& value = std::get<Binary_expression>(expression.data);
            add_format_expression_binary(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Block_expression>(expression.data))
        {
            Block_expression const& value = std::get<Block_expression>(expression.data);
            add_format_expression_block(buffer, statement, value, indentation, options);
        }
        else if (std::holds_alternative<Break_expression>(expression.data))
        {
            Break_expression const& value = std::get<Break_expression>(expression.data);
            add_format_expression_break(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Call_expression>(expression.data))
        {
            Call_expression const& value = std::get<Call_expression>(expression.data);
            add_format_expression_call(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Cast_expression>(expression.data))
        {
            Cast_expression const& value = std::get<Cast_expression>(expression.data);
            add_format_expression_cast(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Comment_expression>(expression.data))
        {
            Comment_expression const& value = std::get<Comment_expression>(expression.data);
            add_format_expression_comment(buffer, statement, value, indentation, options);
        }
        else if (std::holds_alternative<Compile_time_expression>(expression.data))
        {
            Compile_time_expression const& value = std::get<Compile_time_expression>(expression.data);
            add_format_expression_compile_time(buffer, statement, value, indentation, options);
        }
        else if (std::holds_alternative<Continue_expression>(expression.data))
        {
            Continue_expression const& value = std::get<Continue_expression>(expression.data);
            add_format_expression_continue(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Constant_array_expression>(expression.data))
        {
            Constant_array_expression const& value = std::get<Constant_array_expression>(expression.data);
            add_format_expression_constant_array(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Constant_expression>(expression.data))
        {
            Constant_expression const& value = std::get<Constant_expression>(expression.data);
            add_format_expression_constant(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Defer_expression>(expression.data))
        {
            Defer_expression const& value = std::get<Defer_expression>(expression.data);
            add_format_expression_defer(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Dereference_and_access_expression>(expression.data))
        {
            Dereference_and_access_expression const& value = std::get<Dereference_and_access_expression>(expression.data);
            add_format_expression_dereference_and_access(buffer, statement, value, options);
        }
        else if (std::holds_alternative<For_loop_expression>(expression.data))
        {
            For_loop_expression const& value = std::get<For_loop_expression>(expression.data);
            add_format_expression_for_loop(buffer, statement, value, indentation, options);
        }
        else if (std::holds_alternative<Function_expression>(expression.data))
        {
            Function_expression const& value = std::get<Function_expression>(expression.data);
            add_format_expression_function(buffer, value, indentation, options);
        }
        else if (std::holds_alternative<If_expression>(expression.data))
        {
            If_expression const& value = std::get<If_expression>(expression.data);
            add_format_expression_if(buffer, statement, value, indentation, options);
        }
        else if (std::holds_alternative<Instance_call_expression>(expression.data))
        {
            Instance_call_expression const& value = std::get<Instance_call_expression>(expression.data);
            add_format_expression_instance_call(buffer, statement, value, expression.source_range, indentation, options);
        }
        else if (std::holds_alternative<Instantiate_expression>(expression.data))
        {
            Instantiate_expression const& value = std::get<Instantiate_expression>(expression.data);
            add_format_expression_instantiate(buffer, statement, value, expression.source_range, indentation, options);
        }
        else if (std::holds_alternative<Invalid_expression>(expression.data))
        {
            Invalid_expression const& value = std::get<Invalid_expression>(expression.data);
            add_format_expression_invalid(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Null_pointer_expression>(expression.data))
        {
            Null_pointer_expression const& value = std::get<Null_pointer_expression>(expression.data);
            add_format_expression_null_pointer(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Parenthesis_expression>(expression.data))
        {
            Parenthesis_expression const& value = std::get<Parenthesis_expression>(expression.data);
            add_format_expression_parenthesis(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Reflection_expression>(expression.data))
        {
            Reflection_expression const& value = std::get<Reflection_expression>(expression.data);
            add_format_expression_reflection(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Return_expression>(expression.data))
        {
            Return_expression const& value = std::get<Return_expression>(expression.data);
            add_format_expression_return(buffer, statement, value, indentation, options);
        }
        else if (std::holds_alternative<Struct_expression>(expression.data))
        {
            Struct_expression const& value = std::get<Struct_expression>(expression.data);
            add_format_expression_struct(buffer, value, indentation, options);
        }
        else if (std::holds_alternative<Switch_expression>(expression.data))
        {
            Switch_expression const& value = std::get<Switch_expression>(expression.data);
            add_format_expression_switch(buffer, statement, value, indentation, options);
        }
        else if (std::holds_alternative<Ternary_condition_expression>(expression.data))
        {
            Ternary_condition_expression const& value = std::get<Ternary_condition_expression>(expression.data);
            add_format_expression_ternary_condition(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Type_expression>(expression.data))
        {
            Type_expression const& value = std::get<Type_expression>(expression.data);
            add_format_expression_type(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Unary_expression>(expression.data))
        {
            Unary_expression const& value = std::get<Unary_expression>(expression.data);
            add_format_expression_unary(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Variable_expression>(expression.data))
        {
            Variable_expression const& value = std::get<Variable_expression>(expression.data);
            add_format_expression_variable(buffer, statement, value, options);
        }
        else if (std::holds_alternative<Variable_declaration_expression>(expression.data))
        {
            Variable_declaration_expression const& value = std::get<Variable_declaration_expression>(expression.data);
            add_format_expression_variable_declaration(buffer, statement, value, indentation, options);
        }
        else if (std::holds_alternative<Variable_declaration_with_type_expression>(expression.data))
        {
            Variable_declaration_with_type_expression const& value = std::get<Variable_declaration_with_type_expression>(expression.data);
            add_format_expression_variable_declaration_with_type(buffer, statement, value, indentation, options);
        }
        else if (std::holds_alternative<While_loop_expression>(expression.data))
        {
            While_loop_expression const& value = std::get<While_loop_expression>(expression.data);
            add_format_expression_while_loop(buffer, statement, value, indentation, options);
        }
    }

    std::pmr::string format_expression(
        iris::Module const& core_module,
        Statement const& statement,
        Expression const& expression,
        std::uint32_t indentation,
        bool const add_semicolon,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        Format_options const options
        {
            .alias_imports = core_module.dependencies.alias_imports,
            .output_allocator = output_allocator,
            .temporaries_allocator = temporaries_allocator,
        };

        String_buffer buffer;

        add_format_expression(
            buffer,
            statement,
            expression,
            indentation,
            options
        );

        return to_string(buffer);
    }

    void add_format_expression_access(
        String_buffer& buffer,
        Statement const& statement,
        Access_expression const& expression,
        Format_options const& options
    )
    {
        add_format_expression(buffer, statement, get_expression(statement, expression.expression), 0, options);
        add_text(buffer, ".");
        add_text(buffer, expression.member_name);
    }

    void add_format_expression_access_array(
        String_buffer& buffer,
        Statement const& statement,
        Access_array_expression const& expression,
        Format_options const& options
    )
    {
        add_format_expression(buffer, statement, get_expression(statement, expression.expression), 0, options);
        add_text(buffer, "[");
        add_format_expression(buffer, statement, get_expression(statement, expression.index), 0, options);
        add_text(buffer, "]");
    }

    void add_format_expression_assert(
        String_buffer& buffer,
        Statement const& statement,
        Assert_expression const& expression,
        Format_options const& options
    )
    {
        add_text(buffer, "assert ");

        if (expression.message.has_value())
        {
            add_text(buffer, "\"");
            add_text(buffer, expression.message.value());
            add_text(buffer, "\" ");
        }

        add_text(buffer, "{ ");

        if (!expression.statement.expressions.empty())
        {
            add_format_expression(buffer, expression.statement, expression.statement.expressions[0], 0, options);
        }

        add_text(buffer, " }");
    }

    void add_format_expression_assignment(
        String_buffer& buffer,
        Statement const& statement,
        Assignment_expression const& expression,
        std::uint32_t const indentation,
        Format_options const& options
    )
    {
        add_format_expression(buffer, statement, get_expression(statement, expression.left_hand_side), 0, options);

        add_text(buffer, " ");
        if (expression.additional_operation)
        {
            add_format_binary_operation_symbol(buffer, *expression.additional_operation);
        }
        add_text(buffer, "= ");

        add_format_expression(buffer, statement, get_expression(statement, expression.right_hand_side), indentation, options);
    }

    void add_format_expression_binary(
        String_buffer& buffer,
        Statement const& statement,
        Binary_expression const& expression,
        Format_options const& options
    )
    {
        add_format_expression(buffer, statement, get_expression(statement, expression.left_hand_side), 0, options);
        add_text(buffer, " ");
        add_format_binary_operation_symbol(buffer, expression.operation);
        add_text(buffer, " ");
        add_format_expression(buffer, statement, get_expression(statement, expression.right_hand_side), 0, options);
    }

    std::string_view binary_operation_symbol_to_string(
        Binary_operation operation
    )
    {
        switch (operation)
        {
            case Binary_operation::Add:
                return "+";
            case Binary_operation::Subtract:
                return "-";
            case Binary_operation::Multiply:
                return "*";
            case Binary_operation::Divide:
                return "/";
            case Binary_operation::Modulus:
                return "%";
            case Binary_operation::Equal:
                return "==";
            case Binary_operation::Not_equal:
                return "!=";
            case Binary_operation::Less_than:
                return "<";
            case Binary_operation::Less_than_or_equal_to:
                return "<=";
            case Binary_operation::Greater_than:
                return ">";
            case Binary_operation::Greater_than_or_equal_to:
                return ">=";
            case Binary_operation::Logical_and:
                return "&&";
            case Binary_operation::Logical_or:
                return "||";
            case Binary_operation::Bitwise_and:
                return "&";
            case Binary_operation::Bitwise_or:
                return "|";
            case Binary_operation::Bitwise_xor:
                return "^";
            case Binary_operation::Bit_shift_left:
                return "<<";
            case Binary_operation::Bit_shift_right:
                return ">>";
            case Binary_operation::Has:
                return "has";
            default:
                return "<unknown>";
        }
    }

    void add_format_binary_operation_symbol(
        String_buffer& buffer,
        Binary_operation operation
    )
    {
        std::string_view const symbol = binary_operation_symbol_to_string(
            operation
        );

        add_text(buffer, symbol);
    }

    void add_format_expression_statements(
        String_buffer& buffer,
        std::span<Statement const> const statements,
        std::uint32_t outside_indentation,
        Format_options const& options
    )
    {
        std::uint32_t current_line = get_current_line_position(
            buffer
        );
        
        for (std::size_t statement_index = 0; statement_index < statements.size(); ++statement_index)
        {
            Statement const& current_statement = statements[statement_index];

            // Skip statements that were erased (e.g. compile_time var declarations after propagation).
            if (current_statement.expressions.empty())
                continue;

            if (statement_index > 0)
            {
                Statement const& previous_statement = statements[statement_index - 1];
                
                if (!previous_statement.expressions.empty() && previous_statement.expressions[0].source_range.has_value())
                {
                    if (!current_statement.expressions.empty() && current_statement.expressions[0].source_range.has_value())
                    {
                        std::uint32_t const previous_statement_line = previous_statement.expressions[0].source_range->start.line;
                        std::uint32_t const current_statement_line = current_statement.expressions[0].source_range->start.line;

                        if (current_statement_line > previous_statement_line)
                        {
                            std::uint32_t const difference = current_statement_line - previous_statement_line;
                            std::uint32_t const new_lines_in_previous_statement = get_current_line_position(buffer) - current_line;

                            if (difference > new_lines_in_previous_statement)
                            {
                                std::uint32_t const new_lines_to_add = difference - new_lines_in_previous_statement;

                                for (std::uint32_t index = 1; index < new_lines_to_add; ++index)
                                    add_new_line(buffer);
                            }
                        }
                    }
                } 
            }
            add_new_line(buffer);

            add_indentation(buffer, outside_indentation);

            current_line = get_current_line_position(buffer);

            add_format_statement(buffer, current_statement, outside_indentation, options);
        }
    }

    void add_format_expression_block(
        String_buffer& buffer,
        std::span<Statement const> const statements,
        std::uint32_t outside_indentation,
        Format_options const& options
    )
    {
        add_text(buffer, "{");

        add_format_expression_statements(
            buffer,
            statements,
            outside_indentation + 4,
            options
        );

        add_new_line(buffer);
        add_indentation(buffer, outside_indentation);
        add_text(buffer, "}");
    }

    void add_format_expression_block(
        String_buffer& buffer,
        Statement const& statement,
        Block_expression const& expression,
        std::uint32_t outside_indentation,
        Format_options const& options
    )
    {
        add_format_expression_block(buffer, expression.statements, outside_indentation, options);
    }

    void add_format_expression_break(
        String_buffer& buffer,
        Statement const& statement,
        Break_expression const& expression,
        Format_options const& options
    )
    {
        add_text(buffer, "break");
        if (expression.loop_count > 1)
        {
            add_text(buffer, " ");
            add_text(buffer, std::to_string(expression.loop_count));
        }
    }

    void add_format_expression_call(
        String_buffer& buffer,
        Statement const& statement,
        Call_expression const& expression,
        Format_options const& options
    )
    {
        add_format_expression(buffer, statement, get_expression(statement, expression.expression), 0, options);
        add_text(buffer, "(");
        for (std::size_t i = 0; i < expression.arguments.size(); ++i)
        {
            if (i > 0)
                add_text(buffer, ", ");
            add_format_expression(buffer, statement, get_expression(statement, expression.arguments[i]), 0, options);
        }
        add_text(buffer, ")");
    }

    void add_format_expression_cast(
        String_buffer& buffer,
        Statement const& statement,
        Cast_expression const& expression,
        Format_options const& options
    )
    {
        add_format_expression(buffer, statement, get_expression(statement, expression.source), 0, options);
        add_text(buffer, " as ");
        add_format_type_name(buffer, expression.destination_type, options);
    }

    void add_format_expression_comment(
        String_buffer& buffer,
        Statement const& statement,
        Comment_expression const& expression,
        std::uint32_t const indentation,
        Format_options const& options
    )
    {
        std::pmr::vector<std::string_view> const comments = decode_comment(
            expression.comment,
            options.temporaries_allocator,
            options.temporaries_allocator
        );

        for (std::size_t index = 0; index < comments.size(); ++index)
        {
            std::string_view const comment = comments[index];

            if (index > 0)
            {
                add_new_line(buffer);
                add_indentation(buffer, indentation);
            }

            add_text(buffer, "//");
            add_text(buffer, comment);
        }
    }

    void add_format_expression_compile_time(
        String_buffer& buffer,
        Statement const& statement,
        Compile_time_expression const& expression,
        std::uint32_t const indentation,
        Format_options const& options
    )
    {
        add_text(buffer, "compile_time ");

        iris::Expression const& wrapped_expression = get_expression(statement, expression.expression);
        add_format_expression(buffer, statement, wrapped_expression, indentation, options);

        if (statement_ends_with_semicolon(wrapped_expression))
            add_text(buffer, ";");
    }

    void add_format_expression_constant(
        String_buffer& buffer,
        Statement const& statement,
        Constant_expression const& expression,
        Format_options const& options
    )
    {
        iris::Type_reference const& type = expression.type;

        if (std::holds_alternative<iris::Fundamental_type>(type.data))
        {
            iris::Fundamental_type const fundamental_type = std::get<iris::Fundamental_type>(type.data);

            switch (fundamental_type)
            {
            case iris::Fundamental_type::Byte:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "b");
                break;
            }
            case iris::Fundamental_type::Float16:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "f16");
                break;
            }
            case iris::Fundamental_type::Float32:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "f32");
                break;
            }
            case iris::Fundamental_type::Float64:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "f64");
                break;
            }
            case iris::Fundamental_type::String:
            {
                add_text(buffer, "\"");
                add_text(buffer, expression.data);
                add_text(buffer, "\"");
                break;
            }
            case iris::Fundamental_type::C_bool:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "cb");
                break;
            }
            case iris::Fundamental_type::C_char:
            {
                add_text(buffer, "'");
                add_text(buffer, expression.data);
                add_text(buffer, "'");
                break;
            }
            case iris::Fundamental_type::C_schar:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "csc");
                break;
            }
            case iris::Fundamental_type::C_uchar:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "cuc");
                break;
            }
            case iris::Fundamental_type::C_short:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "cs");
                break;
            }
            case iris::Fundamental_type::C_ushort:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "cus");
                break;
            }
            case iris::Fundamental_type::C_int:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "ci");
                break;
            }
            case iris::Fundamental_type::C_uint:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "cui");
                break;
            }
            case iris::Fundamental_type::C_long:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "cl");
                break;
            }
            case iris::Fundamental_type::C_ulong:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "cul");
                break;
            }
            case iris::Fundamental_type::C_longlong:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "cll");
                break;
            }
            case iris::Fundamental_type::C_ulonglong:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "cull");
                break;
            }
            case iris::Fundamental_type::C_longdouble:
            {
                add_text(buffer, expression.data);
                add_text(buffer, "cld");
                break;
            }
            default:
            {
                add_text(buffer, expression.data);
                return;
            }
            }
        }
        else if (std::holds_alternative<iris::Integer_type>(type.data))
        {
            iris::Integer_type const integer_type = std::get<iris::Integer_type>(type.data);
            if (integer_type.number_of_bits == 32 && integer_type.is_signed == true)
            {
                add_text(buffer, expression.data);
                return;
            }

            add_text(buffer, expression.data);

            std::string_view const signed_suffix = integer_type.is_signed ? "i" : "u";
            add_text(buffer, signed_suffix);
            add_integer_text(buffer, static_cast<std::uint64_t>(integer_type.number_of_bits));
        }
        else if (std::holds_alternative<iris::Decimal_type>(type.data))
        {
            iris::Decimal_type const decimal_type = std::get<iris::Decimal_type>(type.data);
            add_text(buffer, expression.data);
            add_text(buffer, "d");
            add_integer_text(buffer, static_cast<std::uint64_t>(decimal_type.scale));
        }
        else if (iris::is_c_string(type))
        {
            add_text(buffer, "\"");
            add_text(buffer, expression.data);
            add_text(buffer, "\"c");
        }
        else
        {
            add_text(buffer, expression.data);
        }
    }

    void add_format_expression_constant_array(
        String_buffer& buffer,
        Statement const& statement,
        Constant_array_expression const& expression,
        Format_options const& options
    )
    {
        add_text(buffer, "[");
        for (std::size_t i = 0; i < expression.array_data.size(); ++i)
        {
            if (i > 0)
                add_text(buffer, ", ");

            add_format_statement(buffer, expression.array_data[i], 0, options, false);
        }
        add_text(buffer, "]");
    }

    void add_format_expression_continue(
        String_buffer& buffer,
        Statement const& statement,
        Continue_expression const&,
        Format_options const&
    )
    {
        add_text(buffer, "continue");
    }

    void add_format_expression_defer(
        String_buffer& buffer,
        Statement const& statement,
        Defer_expression const& expression,
        Format_options const& options
    )
    {
        add_text(buffer, "defer ");
        add_format_expression(buffer, statement, get_expression(statement, expression.expression_to_defer), 0, options);
    }
    
    void add_format_expression_dereference_and_access(
        String_buffer& buffer,
        Statement const& statement,
        Dereference_and_access_expression const& expression,
        Format_options const& options
    )
    {
        add_format_expression(buffer, statement, get_expression(statement, expression.expression), 0, options);
        add_text(buffer, "->"); 
        add_text(buffer, expression.member_name);
    }

    void add_format_expression_if(
        String_buffer& buffer,
        Statement const& statement,
        If_expression const& expression,
        std::uint32_t outside_indentation,
        Format_options const& options
    )
    {
        for (std::size_t index = 0; index < expression.series.size(); ++index)
        {
            Condition_statement_pair const& pair = expression.series[index];

            if (index > 0)
            {
                add_new_line(buffer);
                add_indentation(buffer, outside_indentation);
                add_text(buffer, "else");
            }

            if (pair.condition.has_value())
            {
                if (index > 0)
                    add_text(buffer, " ");
                
                add_text(buffer, "if ");

                add_format_statement(buffer, pair.condition.value(), 0, options, false);
            }

            add_new_line(buffer);
            add_indentation(buffer, outside_indentation);

            add_format_expression_block(buffer, pair.then_statements, outside_indentation, options);
        }
    }

    bool place_instantiate_members_on_the_same_line(
        iris::Statement const& statement,
        Instantiate_expression const& expression,
        std::optional<iris::Source_position> const source_position
    )
    {
        if (!source_position.has_value())
            return true;

        if (expression.members.empty())
            return true;

        Instantiate_member_value_pair const& pair = expression.members[0];
        
        iris::Expression const& first_expression = statement.expressions[pair.value.expression_index];
        if (!first_expression.source_range.has_value())
            return true;

        Source_position const first_member_source_position = first_expression.source_range.value().start;

        return first_member_source_position.line == source_position->line;
    }

    void add_format_expression_instance_call(
        String_buffer& buffer,
        Statement const& statement,
        Instance_call_expression const& expression,
        std::optional<iris::Source_range> const source_range,
        std::uint32_t outside_indentation,
        Format_options const& options
    )
    {
        add_format_expression(buffer, statement, get_expression(statement, expression.left_hand_side), outside_indentation, options);

        add_text(buffer, "::<");

        for (std::size_t index = 0; index < expression.arguments.size(); ++index)
        {
            if (index > 0)
            {
                add_text(buffer, ", ");
            }

            iris::Statement const& statement = expression.arguments[index];
            add_format_statement(buffer, statement, outside_indentation + 4, options, false);
        }

        add_text(buffer, ">");
    }

    void add_format_expression_instantiate(
        String_buffer& buffer,
        Statement const& statement,
        Instantiate_expression const& expression,
        std::optional<iris::Source_range> const source_range,
        std::uint32_t outside_indentation,
        Format_options const& options
    )
    {
        bool const same_line = 
            source_range.has_value() ? 
            place_instantiate_members_on_the_same_line(statement, expression, source_range->start) :
            false;

        if (expression.type == Instantiate_expression_type::Explicit)
            add_text(buffer, "explicit ");
        else if (expression.type == Instantiate_expression_type::Uninitialized)
            add_text(buffer, "uninitialized ");
        else if (expression.type == Instantiate_expression_type::Zero_initialized)
            add_text(buffer, "zero_initialized ");
            
        add_text(buffer, "{");
        if (!expression.members.empty())
        {
            if (!same_line)
            {
                add_new_line(buffer);
                add_indentation(buffer, outside_indentation + 4);
            }
            else
            {
                add_text(buffer, " ");
            }

            for (std::size_t index = 0; index < expression.members.size(); ++index)
            {
                if (index > 0)
                {
                    add_text(buffer, ",");

                    if (!same_line)
                    {
                        add_new_line(buffer);
                        add_indentation(buffer, outside_indentation + 4);
                    }
                    else
                    {
                        add_text(buffer, " ");
                    }
                }
                    
                Instantiate_member_value_pair const& member = expression.members[index];
                add_text(buffer, member.member_name);
                add_text(buffer, ": ");
                add_format_expression(buffer, statement, get_expression(statement, member.value), outside_indentation + 4, options);
            }

            if (!same_line)
            {
                add_new_line(buffer);
                add_indentation(buffer, outside_indentation);
            }
            else
            {
                add_text(buffer, " ");
            }
        }
        add_text(buffer, "}");
    }

    void add_format_expression_invalid(
        String_buffer& buffer,
        Statement const& statement,
        Invalid_expression const& expression,
        Format_options const&
    )
    {
        add_text(buffer, expression.value);
    }

    void add_format_expression_for_loop(
        String_buffer& buffer,
        Statement const& statement,
        For_loop_expression const& expression,
        std::uint32_t outside_indentation,
        Format_options const& options
    )
    {
        add_text(buffer, "for ");
        add_text(buffer, expression.variable_name);
        add_text(buffer, " in ");
        add_format_expression(buffer, statement, get_expression(statement, expression.range_begin), 0, options);
        add_text(buffer, " to ");
        add_format_statement(buffer, expression.range_end, outside_indentation, options, false);
        
        if (expression.step_by.has_value())
        {
            add_text(buffer, " step_by ");
            add_format_expression(buffer, statement, get_expression(statement, expression.step_by.value()), outside_indentation, options);
        }

        if (expression.range_comparison_operation == iris::Binary_operation::Greater_than)
        {
            add_text(buffer, " reverse");
        }

        add_new_line(buffer);
        add_indentation(buffer, outside_indentation);

        add_format_expression_block(buffer, expression.then_statements, outside_indentation, options);
    }

    void add_format_expression_function(
        String_buffer& buffer,
        Function_expression const& expression,
        std::uint32_t const outside_indentation,
        Format_options const& options
    )
    {
        add_format_function_declaration(buffer, expression.declaration, outside_indentation, options);
        add_format_function_definition(buffer, expression.definition, outside_indentation, options);
    }

    void add_format_expression_null_pointer(
        String_buffer& buffer,
        Statement const& statement,
        Null_pointer_expression const&,
        Format_options const&
    )
    {
        add_text(buffer, "null");
    }

    void add_format_expression_parenthesis(
        String_buffer& buffer,
        Statement const& statement,
        Parenthesis_expression const& expression,
        Format_options const& options
    )
    {
        add_text(buffer, "(");
        add_format_expression(buffer, statement, get_expression(statement, expression.expression), 0, options);
        add_text(buffer, ")");
    }

    void add_format_expression_reflection(
        String_buffer& buffer,
        Statement const& statement,
        Reflection_expression const& expression,
        Format_options const& options
    )
    {
        add_text(buffer, "@");
        add_text(buffer, expression.name);

        if (expression.type_arguments.size() > 0)
        {
            add_text(buffer, "::<");
            for (std::size_t i = 0; i < expression.type_arguments.size(); ++i)
            {
                if (i > 0)
                    add_text(buffer, ", ");
                add_format_type_name(buffer, {&expression.type_arguments[i], 1}, options);
            }
            add_text(buffer, ">");
        }

        add_text(buffer, "(");
        for (std::size_t i = 0; i < expression.arguments.size(); ++i)
        {
            if (i > 0)
                add_text(buffer, ", ");
            add_format_expression(buffer, statement, get_expression(statement, expression.arguments[i]), 0, options);
        }
        add_text(buffer, ")");
    }

    void add_format_expression_return(
        String_buffer& buffer,
        Statement const& statement,
        Return_expression const& expression,
        std::uint32_t const outside_indentation,
        Format_options const& options
    )
    {
        add_text(buffer, "return");
        if (expression.expression.has_value())
        {
            add_text(buffer, " ");
            add_format_expression(buffer, statement, get_expression(statement, *expression.expression), outside_indentation, options);
        }
    }

    void add_format_expression_struct(
        String_buffer& buffer,
        Struct_expression const& expression,
        std::uint32_t const outside_indentation,
        Format_options const& options
    )
    {
        add_format_struct_declaration(buffer, expression.declaration, outside_indentation, options);
    }

    void add_format_expression_switch(
        String_buffer& buffer,
        Statement const& statement,
        Switch_expression const& expression,
        std::uint32_t outside_indentation,
        Format_options const& options
    )
    {
        add_text(buffer, "switch ");
        add_format_expression(buffer, statement, get_expression(statement, expression.value), 0, options);
        add_new_line(buffer);
        add_indentation(buffer, outside_indentation);
        
        add_text(buffer, "{");

        for (Switch_case_expression_pair const& case_pair : expression.cases)
        {
            add_new_line(buffer);
            add_indentation(buffer, outside_indentation);

            if (case_pair.case_value.has_value())
            {
                add_text(buffer, "case ");
                add_format_expression(buffer, statement, get_expression(statement, *case_pair.case_value), 0, options);
            }
            else
            {
                add_text(buffer, "default");
            }
            add_text(buffer, ":");

            add_format_expression_statements(buffer, case_pair.statements, outside_indentation + 4, options);
        }

        add_new_line(buffer);
        add_indentation(buffer, outside_indentation);
        add_text(buffer, "}");
    }

    void add_format_expression_ternary_condition(
        String_buffer& buffer,
        Statement const& statement,
        Ternary_condition_expression const& expression,
        Format_options const& options
    )
    {
        add_format_expression(buffer, statement, get_expression(statement, expression.condition), 0, options);
        add_text(buffer, " ? ");
        add_format_statement(buffer, expression.then_statement, 0, options, false);
        add_text(buffer, " : ");
        add_format_statement(buffer, expression.else_statement, 0, options, false);
    }

    void add_format_expression_type(
        String_buffer& buffer,
        Statement const& statement,
        Type_expression const& expression,
        Format_options const& options
    )
    {
        add_format_type_name(buffer, {&expression.type, 1}, options);
    }

    std::string_view unary_operation_symbol_to_string(
        Unary_operation const operation
    )
    {
        switch (operation)
        {
            case Unary_operation::Not:
                return "!";
            case Unary_operation::Bitwise_not:
                return "~";
            case Unary_operation::Minus:
                return "-";
            case Unary_operation::Indirection:
                return "*";
            case Unary_operation::Address_of:
                return "&";
            default:
                return "<unknown>";
        }
    }

    void add_format_expression_unary(
        String_buffer& buffer,
        Statement const& statement,
        Unary_expression const& expression,
        Format_options const& options
    )
    {
        std::string_view const symbol_string = unary_operation_symbol_to_string(
            expression.operation
        );
        add_text(buffer, symbol_string);
        
        add_format_expression(buffer, statement, get_expression(statement, expression.expression), 0, options);
    }

    void add_format_expression_variable(
        String_buffer& buffer,
        Statement const& statement,
        Variable_expression const& expression,
        Format_options const& options
    )
    {
        add_text(buffer, expression.name);
    }

    void add_format_expression_variable_declaration(
        String_buffer& buffer,
        Statement const& statement,
        Variable_declaration_expression const& expression,
        std::uint32_t outside_indentation,
        Format_options const& options
    )
    {
        std::string_view const mutability_text = expression.is_mutable ? "mutable" : "var";
        add_text(buffer, mutability_text);
        add_text(buffer, " ");
        add_text(buffer, expression.name);
        add_text(buffer, " = ");
        add_format_expression(buffer, statement, get_expression(statement, expression.right_hand_side), outside_indentation, options);
    }

    void add_format_expression_variable_declaration_with_type(
        String_buffer& buffer,
        Statement const& statement,
        Variable_declaration_with_type_expression const& expression,
        std::uint32_t outside_indentation,
        Format_options const& options
    )
    {
        std::string_view const mutability_text = expression.is_mutable ? "mutable" : "var";
        add_text(buffer, mutability_text);
        add_text(buffer, " ");
        add_text(buffer, expression.name);
        add_text(buffer, ": ");

        Expression const& declared_type_expression = get_expression(statement, expression.type);
        if (std::holds_alternative<Type_expression>(declared_type_expression.data))
        {
            Type_expression const& type_expression = std::get<Type_expression>(declared_type_expression.data);
            add_format_type_name(buffer, type_expression.type, options);
        }
        else if (std::holds_alternative<Reflection_expression>(declared_type_expression.data))
        {
            Reflection_expression const& reflection_expression = std::get<Reflection_expression>(declared_type_expression.data);
            add_format_expression_reflection(buffer, statement, reflection_expression, options);
        }
        else
        {
            add_text(buffer, "<invalid_type_expression>");
        }

        add_text(buffer, " = ");
        add_format_expression(buffer, statement, get_expression(statement, expression.right_hand_side), outside_indentation, options);
    }

    void add_format_expression_while_loop(
        String_buffer& buffer,
        Statement const& statement,
        While_loop_expression const& expression,
        std::uint32_t outside_indentation,
        Format_options const& options
    )
    {
        add_text(buffer, "while ");
        add_format_statement(buffer, expression.condition, outside_indentation, options, false);
        add_new_line(buffer);
        add_indentation(buffer, outside_indentation);
        add_format_expression_block(buffer, expression.then_statements, outside_indentation, options);
    }

    std::string_view format_fundamental_type(
        Fundamental_type const value
    )
    {
        switch (value)
        {
        case Fundamental_type::Bool:
            return "Bool";
        case Fundamental_type::Byte:
            return "Byte";
        case Fundamental_type::Float16:
            return "Float16";
        case Fundamental_type::Float32:
            return "Float32";
        case Fundamental_type::Float64:
            return "Float64";
        case Fundamental_type::String:
            return "String";
        case Fundamental_type::Any_type:
            return "Any_type";
        case Fundamental_type::C_bool:
            return "C_bool";
        case Fundamental_type::C_char:
            return "C_char";
        case Fundamental_type::C_schar:
            return "C_schar";
        case Fundamental_type::C_uchar:
            return "C_uchar";
        case Fundamental_type::C_short:
            return "C_short";
        case Fundamental_type::C_ushort:
            return "C_ushort";
        case Fundamental_type::C_int:
            return "C_int";
        case Fundamental_type::C_uint:
            return "C_uint";
        case Fundamental_type::C_long:
            return "C_long";
        case Fundamental_type::C_ulong:
            return "C_ulong";
        case Fundamental_type::C_longlong:
            return "C_longlong";
        case Fundamental_type::C_ulonglong:
            return "C_ulonglong";
        case Fundamental_type::C_longdouble:
            return "C_longdouble";
        default:
            return "<unknown>";
        }
    }

    void add_format_integer_type(
        String_buffer& buffer,
        Integer_type const value
    )
    {
        add_text(buffer, value.is_signed ? "Int" : "Uint");
        add_integer_text(buffer, static_cast<std::uint64_t>(value.number_of_bits));
    }

    std::pmr::string format_integer_type(
        iris::Integer_type const value
    )
    {
        String_buffer buffer;
        add_format_integer_type(buffer, value);
        return to_string(buffer);
    }

    void add_format_custom_type_reference(
        String_buffer& buffer,
        Custom_type_reference const& value,
        Format_options const& options
    )
    {
        if (value.module_reference.name == "iris.builtin")
        {
            add_text(buffer, value.name);
            return;
        }

        auto const is_import = [&](Import_module_with_alias const& import_module) -> bool
        {
            return import_module.module_name == value.module_reference.name;
        };
        
        auto const location = std::find_if(
            options.alias_imports.begin(),
            options.alias_imports.end(),
            is_import
        );

        if (location != options.alias_imports.end())
        {
            Import_module_with_alias const& alias_import = *location;
            add_text(buffer, alias_import.alias);
            add_text(buffer, ".");
        }

        add_text(buffer, value.name);
    }

    void add_format_type_name(
        String_buffer& buffer,
        Type_reference const& type,
        Format_options const& options
    )
    {
        if (std::holds_alternative<Array_slice_type>(type.data))
        {
            Array_slice_type const& value = std::get<Array_slice_type>(type.data);
            add_text(buffer, "Array_slice::<");

            if (value.is_mutable)
                add_text(buffer, "mutable ");

            add_format_type_name(buffer, value.element_type, options);
            add_text(buffer, ">");
        }
        else if (std::holds_alternative<Builtin_type_reference>(type.data))
        {
            Builtin_type_reference const& value = std::get<Builtin_type_reference>(type.data);
            add_text(buffer, value.value);
        }
        else if (std::holds_alternative<Constant_array_type>(type.data))
        {
            Constant_array_type const& value = std::get<Constant_array_type>(type.data);
            add_text(buffer, "Constant_array::<");
            add_format_type_name(buffer, value.value_type, options);
            add_text(buffer, ", ");
            add_integer_text(buffer, value.size);
            add_text(buffer, ">");
        }
        else if (std::holds_alternative<Custom_type_reference>(type.data))
        {
            Custom_type_reference const& value = std::get<Custom_type_reference>(type.data);

            add_format_custom_type_reference(
                buffer, value, options
            );
        }
        else if (std::holds_alternative<Decimal_type>(type.data))
        {
            Decimal_type const& value = std::get<Decimal_type>(type.data);
            add_text(buffer, "Decimal");
            add_integer_text(buffer, static_cast<std::uint64_t>(value.scale));
        }
        else if (std::holds_alternative<Fundamental_type>(type.data))
        {
            Fundamental_type const& value = std::get<Fundamental_type>(type.data);
            std::string_view const name = format_fundamental_type(value);
            add_text(buffer, name);
        }
        else if (std::holds_alternative<Function_pointer_type>(type.data))
        {
            Function_pointer_type const& value = std::get<Function_pointer_type>(type.data);
            add_text(buffer, "function<");

            add_format_function_parameters(
                buffer,
                value.input_parameter_names,
                value.type.input_parameter_types,
                std::nullopt,
                value.type.is_variadic,
                true,
                0,
                options
            );

            add_text(buffer, " -> ");

            add_format_function_parameters(
                buffer,
                value.output_parameter_names,
                value.type.output_parameter_types,
                std::nullopt,
                false,
                true,
                0,
                options
            );

            add_text(buffer, ">");
        }
        else if (std::holds_alternative<Integer_type>(type.data))
        {
            Integer_type const& value = std::get<Integer_type>(type.data);
            add_format_integer_type(buffer, value);
        }
        else if (std::holds_alternative<Null_pointer_type>(type.data))
        {
            add_text(buffer, "Null_pointer_type");
        }
        else if (std::holds_alternative<Parameter_type>(type.data))
        {
            Parameter_type const& value = std::get<Parameter_type>(type.data);
            add_text(buffer, value.name);
        }
        else if (std::holds_alternative<Pointer_type>(type.data))
        {
            Pointer_type const& value = std::get<Pointer_type>(type.data);
            add_text(buffer, "*");

            if (value.is_mutable)
                add_text(buffer, "mutable ");

            add_format_type_name(buffer, value.element_type, options);
        }
        else if (std::holds_alternative<Soa_array_type>(type.data))
        {
            Soa_array_type const& value = std::get<Soa_array_type>(type.data);
            add_text(buffer, "Soa_array::<");
            add_format_type_name(buffer, value.value_type, options);
            add_text(buffer, ", ");
            add_integer_text(buffer, value.size);
            add_text(buffer, ">");
        }
        else if (std::holds_alternative<Soa_array_view_type>(type.data))
        {
            Soa_array_view_type const& value = std::get<Soa_array_view_type>(type.data);
            add_text(buffer, "Soa_array_view::<");

            if (value.is_mutable)
                add_text(buffer, "mutable ");

            add_format_type_name(buffer, value.value_type, options);
            add_text(buffer, ">");
        }
        else if (std::holds_alternative<Type_instance>(type.data))
        {
            Type_instance const& value = std::get<Type_instance>(type.data);

            add_format_custom_type_reference(
                buffer, value.type_constructor, options
            );

            add_text(buffer, "::<");

            for (std::size_t index = 0; index < value.arguments.size(); ++index)
            {
                if (index > 0)
                {
                    add_text(buffer, ", ");
                }

                iris::Statement const& statement = value.arguments[index];
                add_format_statement(buffer, statement, 0, options, false);
            }

            add_text(buffer, ">");
        }
    }

    void add_format_type_name(
        String_buffer& buffer,
        std::span<Type_reference const> types,
        Format_options const& options
    )
    {
        if (types.empty())
        {
            add_text(buffer, "Void");
            return;
        }

        Type_reference const& type = types[0];

        add_format_type_name(buffer, type, options);
    }

    bool place_parameters_on_the_same_line(
        std::optional<Source_range_location> const& declaration_source_location,
        std::optional<std::pmr::vector<Source_position>> const parameter_source_positions
    )
    {
        if (!declaration_source_location.has_value())
            return true;

        if (!parameter_source_positions.has_value())
            return true;

        if (parameter_source_positions->empty())
            return true;

        Source_position const first_parameter_source_position = parameter_source_positions->front();

        return first_parameter_source_position.line == declaration_source_location->range.start.line;
    }

    void add_format_function_parameters(
        String_buffer& buffer,
        std::span<std::pmr::string const> const parameter_names,
        std::span<iris::Type_reference const> const parameter_types,
        std::optional<std::pmr::vector<Source_position>> const parameter_source_positions,
        bool const is_variadic,
        bool const same_line,
        std::uint32_t const indentation,
        Format_options const& options
    )
    {
        add_text(buffer, "(");

        if (!same_line)
        {
            add_new_line(buffer);
            add_indentation(buffer, indentation + 4);
        }
        
        for (std::size_t i = 0; i < parameter_names.size(); ++i)
        {
            if (i > 0)
            {
                add_text(buffer, ",");

                if (!same_line)
                {
                    add_new_line(buffer);
                    add_indentation(buffer, indentation + 4);
                }
                else
                {
                    add_text(buffer, " ");
                }
            }
            
            add_text(buffer, parameter_names[i]);
            add_text(buffer, ": ");
            add_format_type_name(buffer, parameter_types[i], options);
        }
        
        if (is_variadic)
        {
            if (!parameter_names.empty())
            {
                add_text(buffer, ",");

                if (!same_line)
                {
                    add_new_line(buffer);
                    add_indentation(buffer, indentation + 4);
                }
                else
                {
                    add_text(buffer, " ");
                }
            }

            add_text(buffer, "...");
        }

        if (!same_line)
        {
            add_new_line(buffer);
            add_indentation(buffer, indentation);
        }

        add_text(buffer, ")");
    }

    void add_format_function_parameters(
        String_buffer& buffer,
        Function_declaration const& function_declaration,
        std::span<std::pmr::string const> const parameter_names,
        std::span<iris::Type_reference const> const parameter_types,
        std::optional<std::pmr::vector<Source_position>> const parameter_source_positions,
        bool const is_variadic,
        std::uint32_t const indentation,
        Format_options const& options
    )
    {
        bool const same_line = place_parameters_on_the_same_line(
            function_declaration.source_location,
            parameter_source_positions
        );

        add_format_function_parameters(
            buffer,
            parameter_names,
            parameter_types,
            parameter_source_positions,
            is_variadic,
            same_line,
            indentation,
            options
        );
    }

    void add_format_function_condition(
        String_buffer& buffer,
        Function_condition const& condition,
        bool const is_precondition,
        Format_options const& options
    )
    {
        add_text(buffer, is_precondition ? "precondition" : "postcondition");
        add_text(buffer, " \"");
        add_text(buffer, condition.description);
        add_text(buffer, "\" { ");
        add_format_statement(buffer, condition.condition, 0, options, false);
        add_text(buffer, " }");
    }

    void add_format_function_declaration(
        String_buffer& buffer,
        Function_declaration const& function_declaration,
        std::uint32_t const outside_indentation,
        Format_options const& options
    )
    {
        add_text(buffer, "function ");
        add_text(buffer, function_declaration.name);

        add_format_function_parameters(
            buffer,
            function_declaration,
            function_declaration.input_parameter_names,
            function_declaration.type.input_parameter_types,
            function_declaration.input_parameter_source_positions,
            function_declaration.type.is_variadic,
            outside_indentation,
            options
        );
        
        add_text(buffer, " -> ");
        
        add_format_function_parameters(
            buffer,
            function_declaration,
            function_declaration.output_parameter_names,
            function_declaration.type.output_parameter_types,
            function_declaration.output_parameter_source_positions,
            false,
            outside_indentation,
            options
        );

        for (Function_condition const& condition : function_declaration.preconditions)
        {
            add_new_line(buffer);
            add_indentation(buffer, outside_indentation + 4);
            add_format_function_condition(buffer, condition, true, options);
        }

        for (Function_condition const& condition : function_declaration.postconditions)
        {
            add_new_line(buffer);
            add_indentation(buffer, outside_indentation + 4);
            add_format_function_condition(buffer, condition, false, options);
        }
    }

    void add_format_function_definition(
        String_buffer& buffer,
        Function_definition const& function_definition,
        std::uint32_t const outside_indentation,
        Format_options const& options
    )
    {
        add_new_line(buffer);
        add_indentation(buffer, outside_indentation);
        add_format_expression_block(buffer, function_definition.statements, outside_indentation, options);
    }

    void add_format_function_constructor(
        String_buffer& buffer,
        Function_constructor const& function_constructor,
        Format_options const& options
    )
    {
        add_text(buffer, "function_constructor ");
        add_text(buffer, function_constructor.name);

        bool const same_line = true;

        add_text(buffer, "(");
        for (std::size_t i = 0; i < function_constructor.parameters.size(); ++i)
        {
            if (i > 0)
            {
                add_text(buffer, ",");

                if (!same_line)
                {
                    add_new_line(buffer);
                    add_indentation(buffer, 4);
                }
                else
                {
                    add_text(buffer, " ");
                }
            }

            Function_constructor_parameter const& parameter = function_constructor.parameters[i];
            
            add_text(buffer, parameter.name);
            add_text(buffer, ": ");
            add_format_type_name(buffer, parameter.type, options);
        }
        add_text(buffer, ")");

        add_new_line(buffer);

        add_format_expression_block(buffer, function_constructor.statements, 0, options);
    }

    static void add_format_enum_value(
        String_buffer& buffer,
        Enum_value const& enum_value,
        std::uint32_t indentation,
        Format_options const& options
    )
    {
        add_text(buffer, enum_value.name);

        if (enum_value.value)
        {
            add_text(buffer, " = ");
            add_format_statement(buffer, *enum_value.value, indentation, options, false);
        }

        add_text(buffer, ",");
    }

    void add_format_enum_declaration(
        String_buffer& buffer,
        Enum_declaration const& enum_declaration,
        Format_options const& options
    )
    {
        add_text(buffer, "enum ");
        add_text(buffer, enum_declaration.name);
        add_new_line(buffer);
        add_text(buffer, "{");
        
        for (Enum_value const& value : enum_declaration.values)
        {
            if (value.comment.has_value())
            {
                add_new_line(buffer);
                add_indentation(buffer, 4);
                add_comment(buffer, value.comment.value(), 4);
            }

            add_new_line(buffer);
            add_indentation(buffer, 4);
            add_format_enum_value(buffer, value, 4, options);
        }
        
        add_new_line(buffer);
        add_text(buffer, "}");
    }

    void add_format_global_variable_declaration(
        String_buffer& buffer,
        Global_variable_declaration const& declaration,
        Format_options const& options
    )
    {
        if (declaration.global_type == iris::Global_variable_type::Constant)
            add_text(buffer, "var");
        else if (declaration.global_type == iris::Global_variable_type::Mutable)
            add_text(buffer, "mutable");
        else if (declaration.global_type == iris::Global_variable_type::Macro)
            add_text(buffer, "macro");

        add_text(buffer, " ");
        add_text(buffer, declaration.name);

        if (declaration.type.has_value())
        {
            add_text(buffer, ": ");
            add_format_type_name(buffer, declaration.type.value(), options);
        }

        add_text(buffer, " = ");
        add_format_statement(buffer, declaration.initial_value, 0, options, false);
        add_text(buffer, ";");
    }

    void add_format_struct_declaration(
        String_buffer& buffer,
        Struct_declaration const& struct_declaration,
        std::uint32_t const outside_indentation,
        Format_options const& options
    )
    {       
        add_text(buffer, "struct");
        if (!struct_declaration.name.empty())
        {
            add_text(buffer, " ");
            add_text(buffer, struct_declaration.name);
        }
        add_new_line(buffer);
        add_indentation(buffer, outside_indentation);
        add_text(buffer, "{");

        for (std::size_t i = 0; i < struct_declaration.member_names.size(); ++i)
        {
            add_new_line(buffer);
            add_indentation(buffer, outside_indentation + 4);

            // Add member comment if exists
            auto member_comment_it = std::find_if(
                struct_declaration.member_comments.begin(),
                struct_declaration.member_comments.end(),
                [i](auto const& comment) { return comment.index == i; }
            );
            if (member_comment_it != struct_declaration.member_comments.end())
            {
                add_comment(buffer, member_comment_it->comment, 4);
                add_new_line(buffer);
                add_indentation(buffer, outside_indentation + 4);
            }

            // Member name and type
            add_text(buffer, struct_declaration.member_names[i]);
            add_text(buffer, ": ");
            add_format_type_name(buffer, {&struct_declaration.member_types[i], 1}, options);

            if (struct_declaration.member_bit_fields[i].has_value())
            {
                std::uint32_t const bits = struct_declaration.member_bit_fields[i].value();
                add_text(buffer, " : ");
                add_integer_text(buffer, static_cast<std::uint64_t>(bits));
            }

            // Default value if exists
            if (i < struct_declaration.member_default_values.size())
            {
                add_text(buffer, " = ");
                add_format_statement(buffer, struct_declaration.member_default_values[i], 4, options, false);
            }

            add_text(buffer, ";");
        }

        add_new_line(buffer);
        add_indentation(buffer, outside_indentation);
        add_text(buffer, "}");
    }

    void add_format_type_constructor(
        String_buffer& buffer,
        Type_constructor const& type_constructor,
        Format_options const& options
    )
    {
        add_text(buffer, "type_constructor ");
        add_text(buffer, type_constructor.name);

        bool const same_line = true;

        add_text(buffer, "(");
        for (std::size_t i = 0; i < type_constructor.parameters.size(); ++i)
        {
            if (i > 0)
            {
                add_text(buffer, ",");

                if (!same_line)
                {
                    add_new_line(buffer);
                    add_indentation(buffer, 4);
                }
                else
                {
                    add_text(buffer, " ");
                }
            }

            Type_constructor_parameter const& parameter = type_constructor.parameters[i];
            
            add_text(buffer, parameter.name);
            add_text(buffer, ": ");
            add_format_type_name(buffer, parameter.type, options);
        }
        add_text(buffer, ")");

        add_new_line(buffer);

        add_format_expression_block(buffer, type_constructor.statements, 0, options);
    }

    void add_format_union_declaration(
        String_buffer& buffer,
        Union_declaration const& union_declaration,
        std::uint32_t const outside_indentation,
        Format_options const& options
    )
    {
        add_text(buffer, "union ");
        add_text(buffer, union_declaration.name);
        add_new_line(buffer);
        add_text(buffer, "{");

        for (std::size_t i = 0; i < union_declaration.member_names.size(); ++i)
        {
            add_new_line(buffer);
            add_indentation(buffer, outside_indentation + 4);

            auto member_comment_it = std::find_if(
                union_declaration.member_comments.begin(),
                union_declaration.member_comments.end(),
                [i](auto const& comment) { return comment.index == i; }
            );
            if (member_comment_it != union_declaration.member_comments.end())
            {
                add_comment(buffer, member_comment_it->comment, 4);
                add_new_line(buffer);
                add_indentation(buffer, outside_indentation + 4);
            }

            add_text(buffer, union_declaration.member_names[i]);
            add_text(buffer, ": ");
            add_format_type_name(buffer, {&union_declaration.member_types[i], 1}, options);
            add_text(buffer, ";");
        }

        add_new_line(buffer);
        add_text(buffer, "}");
    }

    void add_format_alias_type_declaration(
        String_buffer& buffer,
        Alias_type_declaration const& alias_declaration,
        Format_options const& options
    )
    {
        add_text(buffer, "using ");
        add_text(buffer, alias_declaration.name);
        add_text(buffer, " = ");
        add_format_type_name(buffer, alias_declaration.type, options);
        add_text(buffer, ";");
    }

    struct Declaration_info
    {
        Declaration declaration = {};
        std::optional<std::pmr::string> unique_name = std::nullopt;
        bool is_export = false;
        std::optional<Source_range_location> const* source_location;
    };

    void add_sorted_declaration_info(
        std::pmr::vector<Declaration_info>& declaration_infos,
        Declaration_info element
    )
    {
        if (!element.source_location->has_value())
        {
            declaration_infos.push_back(element);
            return;
        }

        for (std::size_t index = 0; index < declaration_infos.size(); ++index)
        {
            Declaration_info const& current_element = declaration_infos[index];
            if (!current_element.source_location->has_value())
            {
                declaration_infos.push_back(element);
                return;
            }

            if (element.source_location->value() < current_element.source_location->value())
            {
                declaration_infos.insert(declaration_infos.begin() + index, element);
                return;
            }
        }

        declaration_infos.push_back(element);
    }

    void add_sorted_declaration_infos(
        std::pmr::vector<Declaration_info>& declaration_infos,
        Module_declarations const& declarations,
        bool const is_export
    )
    {
        auto const process = [&](auto const& declaration) -> void
        {
            Declaration_info info
            {
                .declaration = {.data = &declaration},
                .unique_name = declaration.unique_name,
                .is_export = is_export,
                .source_location = &declaration.source_location
            };

            add_sorted_declaration_info(declaration_infos, info);
        };

        for (Alias_type_declaration const& declaration : declarations.alias_type_declarations)
            process(declaration);

        for (Enum_declaration const& declaration : declarations.enum_declarations)
            process(declaration);

        for (Global_variable_declaration const& declaration : declarations.global_variable_declarations)
            process(declaration);

        for (Struct_declaration const& declaration : declarations.struct_declarations)
            process(declaration);

        for (Union_declaration const& declaration : declarations.union_declarations)
            process(declaration);

        for (Function_declaration const& declaration : declarations.function_declarations)
            process(declaration);

        auto const process_without_unique_name = [&](auto const& declaration) -> void
        {
            Declaration_info info
            {
                .declaration = {.data = &declaration},
                .unique_name = std::nullopt,
                .is_export = is_export,
                .source_location = &declaration.source_location
            };

            add_sorted_declaration_info(declaration_infos, info);
        };

        for (Function_constructor const& declaration : declarations.function_constructors)
            process_without_unique_name(declaration);

        for (Type_constructor const& declaration : declarations.type_constructors)
            process_without_unique_name(declaration);
    }

    static void add_sorted_declaration_infos(
        std::pmr::vector<Declaration_info>& declaration_infos,
        Module_instanced_declarations const& declarations,
        bool const is_export
    )
    {
        auto const process = [&](auto const& declaration) -> void
        {
            Declaration_info info
            {
                .declaration = {.data = &declaration},
                .unique_name = declaration.unique_name,
                .is_export = is_export,
                .source_location = &declaration.source_location
            };

            add_sorted_declaration_info(declaration_infos, info);
        };

        for (Struct_declaration const& declaration : declarations.struct_declarations)
            process(declaration);

        for (Union_declaration const& declaration : declarations.union_declarations)
            process(declaration);

        for (Function_declaration const& declaration : declarations.function_declarations)
            process(declaration);
    }

    std::pmr::vector<Declaration_info> get_declaration_infos(
        iris::Module const& core_module,
        std::pmr::polymorphic_allocator<> const& allocator
    )
    {
        std::pmr::vector<Declaration_info> output{allocator};

        add_sorted_declaration_infos(
            output,
            core_module.export_declarations,
            true
        );

        add_sorted_declaration_infos(
            output,
            core_module.internal_declarations,
            false
        );

        add_sorted_declaration_infos(
            output,
            core_module.instanced_declarations,
            false
        );

        return output;
    }

    std::pmr::string format_module(
        iris::Module const& core_module,
        Format_options const& options
    )
    {
        String_buffer buffer;

        if (core_module.comment.has_value())
        {
            add_comment(buffer, core_module.comment.value(), 0);
            add_new_line(buffer);
        }

        add_text(buffer, "module ");
        add_text(buffer, core_module.name);
        add_text(buffer, ";");
        add_new_line(buffer);

        if (core_module.dependencies.alias_imports.size() > 0)
            add_new_line(buffer);

        for (Import_module_with_alias const& alias_import : core_module.dependencies.alias_imports)
        {
            add_format_import_module_with_alias(buffer, alias_import, options);
            add_new_line(buffer);
        }

        Format_options const new_options
        {
            .alias_imports = core_module.dependencies.alias_imports,
            .output_allocator = options.output_allocator,
            .temporaries_allocator = options.temporaries_allocator,
        };

        std::pmr::vector<Declaration_info> const declaration_infos = get_declaration_infos(core_module, options.temporaries_allocator);
        if (declaration_infos.size() > 0)
            add_new_line(buffer);

        for (std::size_t declaration_index = 0; declaration_index < declaration_infos.size(); ++declaration_index)
        {
            Declaration_info const& declaration_info = declaration_infos[declaration_index];

            add_format_declaration(buffer, core_module, declaration_info.declaration, declaration_info.unique_name, declaration_info.is_export, new_options);
            add_new_line(buffer);

            if (declaration_index + 1 < declaration_infos.size())
                add_new_line(buffer);
        }

        return to_string(buffer);
    }

    std::pmr::string format_type_reference(
        iris::Module_dependencies const& dependencies,
        std::optional<iris::Type_reference> const& type_reference,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        String_buffer buffer;

        Format_options const options
        {
            .alias_imports = dependencies.alias_imports,
            .output_allocator = output_allocator,
            .temporaries_allocator = temporaries_allocator,
        };

        if (type_reference.has_value())
        {
            add_format_type_name(buffer, type_reference.value(), options);
        }
        else
        {
            add_text(buffer, "Void");
        }
        
        return to_string(buffer);
    }

    Expression const& get_expression(
        Statement const& statement,
        Expression_index const expression_index
    )
    {
        if (expression_index.expression_index == static_cast<std::uint64_t>(-1) || expression_index.expression_index >= statement.expressions.size())
        {
            static iris::Expression invalid_expression = {
                .data = iris::Invalid_expression{ .value = "" }
            };
            return invalid_expression;
        }

        return statement.expressions[expression_index.expression_index];
    }
}
