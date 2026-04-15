export module iris.core.types;

import std;

import iris.core;

namespace iris
{
    export Type_reference create_array_slice_type_reference(std::pmr::vector<Type_reference> element_type, bool is_mutable);
    export bool is_array_slice_type_reference(Type_reference const& type);

    export Type_reference create_bool_type_reference();
    export bool is_bool(Type_reference const& type);
    export bool is_c_bool(Type_reference const& type);

    export Type_reference create_builtin_type_reference(std::pmr::string value);
    export bool is_builtin_type_reference(Type_reference const& type);

    export Type_reference create_constant_array_type_reference(std::pmr::vector<Type_reference> value_type, std::uint64_t size);
    export std::uint64_t get_constant_array_type_size(Type_reference const& type_reference);
    export bool is_constant_array_type_reference(Type_reference const& type);

    export Type_reference create_soa_array_type_reference(std::pmr::vector<Type_reference> value_type, std::uint64_t const size);
    export bool is_soa_array_type_reference(Type_reference const& type);
    export bool is_soa_array_view_type_reference(Type_reference const& type);
    export std::optional<Type_reference> get_soa_element_type(Type_reference const& type);

    export Type_reference create_custom_type_reference(std::string_view module_name, std::string_view name);
    export bool is_custom_type_reference(Type_reference const& type);
    Custom_type_reference fix_custom_type_reference(Custom_type_reference type, Module const& core_module);
    Type_reference fix_custom_type_reference(Type_reference type, Module const& core_module);
    export void fix_custom_type_references(Module& core_module);

    export Type_reference create_function_type_type_reference(Function_type const& function_type, std::pmr::vector<std::pmr::string> input_parameter_names, std::pmr::vector<std::pmr::string> output_parameter_names);
    export std::optional<Type_reference> get_function_output_type_reference(Function_type const& function_type, Module const& core_module);
    export std::optional<Type_reference> get_function_output_type_reference(Type_reference const& type, Module const& core_module);
    export bool is_function_pointer(Type_reference const& type);

    export Type_reference create_fundamental_type_type_reference(Fundamental_type const value);
    export bool is_byte(Type_reference const& type);
    export bool is_floating_point(Type_reference const& type);
    export bool is_any_type(Type_reference const& type);
    
    export Type_reference create_c_string_type_reference(bool const is_mutable);
    export bool is_c_string(Type_reference const& type_reference);

    export Type_reference create_integer_type_type_reference(std::uint32_t number_of_bits, bool is_signed);
    export bool is_integer(Type_reference const& type);
    export bool is_signed_integer(Type_reference const& type);
    export bool is_unsigned_integer(Type_reference const& type);

    export Type_reference create_decimal_type_reference(std::uint32_t scale);
    export bool is_decimal(Type_reference const& type);
    export std::uint32_t get_decimal_scale(Type_reference const& type);
    export std::uint32_t get_decimal_size_in_bits(std::uint32_t const scale);
    export Type_reference get_decimal_backing_type_reference(std::uint32_t scale);

    export bool is_number_or_c_number(Type_reference const& type);

    export Type_reference create_null_pointer_type_type_reference();
    export bool is_null_pointer_type(Type_reference const& type);

    export Type_reference create_pointer_type_type_reference(std::pmr::vector<Type_reference> element_type, bool const is_mutable);
    export std::optional<Type_reference> remove_pointer(Type_reference const& type);
    export bool is_pointer(Type_reference const& type);
    export bool is_non_void_pointer(Type_reference const& type);

    export std::optional<Type_reference> get_element_or_pointee_type(Type_reference const& type);

    export std::optional<std::string_view> get_type_module_name(Type_reference const& type);

    export iris::Struct_declaration create_array_slice_type_struct_declaration(std::pmr::vector<Type_reference> const& element_type);

    export template <typename Value_t, typename Function_t>
        bool visit_type_references_recursively(
            Value_t const& value,
            Function_t predicate
        );

    export template <typename Function_t>
        bool visit_expressions(
            iris::Expression const& expression,
            iris::Statement const& statement,
            Function_t predicate
        );

    export template <typename Function_t>
        bool visit_expressions(
            iris::Statement const& statement,
            Function_t predicate
        );

    export template <typename Function_t>
        bool visit_expressions(
            std::optional<iris::Statement> const statement,
            Function_t predicate
        );

    export template <typename Function_t>
        bool visit_expressions(
            std::span<iris::Statement const> const statements,
            Function_t predicate
        );

    export template <typename Function_t>
        bool visit_type_references(
            Type_reference const& type_reference,
            Function_t predicate
        )
    {
        return predicate(type_reference);
    }

    export template <typename Function_t>
        bool visit_type_references_recursively(
            Type_reference const& type_reference,
            Function_t predicate
        )
    {
        bool const done = predicate(type_reference);
        if (done)
            return true;

        if (std::holds_alternative<Array_slice_type>(type_reference.data))
        {
            Array_slice_type const& data = std::get<Array_slice_type>(type_reference.data);
            for (Type_reference const& nested_type_reference : data.element_type)
            {
                if (visit_type_references_recursively(nested_type_reference, predicate))
                    return true;
            }

            return false;
        }
        else if (std::holds_alternative<Builtin_type_reference>(type_reference.data))
        {
            return false;
        }
        else if (std::holds_alternative<Constant_array_type>(type_reference.data))
        {
            Constant_array_type const& data = std::get<Constant_array_type>(type_reference.data);
            for (Type_reference const& nested_type_reference : data.value_type)
            {
                if (visit_type_references_recursively(nested_type_reference, predicate))
                    return true;
            }

            return false;
        }
        else if (std::holds_alternative<Custom_type_reference>(type_reference.data))
        {
            return false;
        }
        else if (std::holds_alternative<Decimal_type>(type_reference.data))
        {
            return false;
        }
        else if (std::holds_alternative<Fundamental_type>(type_reference.data))
        {
            return false;
        }
        else if (std::holds_alternative<Function_pointer_type>(type_reference.data))
        {
            Function_pointer_type const& data = std::get<Function_pointer_type>(type_reference.data);
            for (Type_reference const& nested_type_reference : data.type.input_parameter_types)
            {
                if (visit_type_references_recursively(nested_type_reference, predicate))
                    return true;
            }
            for (Type_reference const& nested_type_reference : data.type.output_parameter_types)
            {
                if (visit_type_references_recursively(nested_type_reference, predicate))
                    return true;
            }

            return false;
        }
        else if (std::holds_alternative<Integer_type>(type_reference.data))
        {
            return false;
        }
        else if (std::holds_alternative<Null_pointer_type>(type_reference.data))
        {
            return false;
        }
        else if (std::holds_alternative<Parameter_type>(type_reference.data))
        {
            return false;
        }
        else if (std::holds_alternative<Pointer_type>(type_reference.data))
        {
            Pointer_type const& data = std::get<Pointer_type>(type_reference.data);
            for (Type_reference const& nested_type_reference : data.element_type)
            {
                if (visit_type_references_recursively(nested_type_reference, predicate))
                    return true;
            }

            return false;
        }
        else if (std::holds_alternative<Soa_array_type>(type_reference.data))
        {
            Soa_array_type const& data = std::get<Soa_array_type>(type_reference.data);
            for (Type_reference const& nested_type_reference : data.value_type)
            {
                if (visit_type_references_recursively(nested_type_reference, predicate))
                    return true;
            }

            return false;
        }
        else if (std::holds_alternative<Soa_array_view_type>(type_reference.data))
        {
            Soa_array_view_type const& data = std::get<Soa_array_view_type>(type_reference.data);
            for (Type_reference const& nested_type_reference : data.value_type)
            {
                if (visit_type_references_recursively(nested_type_reference, predicate))
                    return true;
            }

            return false;
        }
        else if (std::holds_alternative<Type_instance>(type_reference.data))
        {
            Type_instance const& data = std::get<Type_instance>(type_reference.data);
            
            Type_reference const type_constructor = create_custom_type_reference(data.type_constructor.module_reference.name, data.type_constructor.name);
            if (visit_type_references_recursively(type_constructor, predicate))
                return true;

            return visit_type_references_recursively(data.arguments, predicate);
        }
        else
        {
            throw std::runtime_error{"visit_type_references_recursively: Did not handle type!"};
        }
    }

    export template <typename Function_t>
        bool visit_type_references(
            iris::Alias_type_declaration const& declaration,
            Function_t predicate
        )
    {
        for (iris::Type_reference const& type_reference : declaration.type)
        {
            if (visit_type_references(type_reference, predicate))
                return true;
        }

        return false;
    }

    export template <typename Function_t>
        bool visit_type_references(
            iris::Enum_declaration const& declaration,
            Function_t predicate
        )
    {
        return false;
    }

    export template <typename Function_t>
    bool visit_type_references(
        iris::Forward_declaration const& declaration,
        Function_t predicate
    )
    {
        return false;
    }

    export template <typename Function_t>
    bool visit_type_references(
        iris::Global_variable_declaration const& declaration,
        Function_t predicate
    )
    {
        if (declaration.type.has_value())
        {
            if (visit_type_references(*declaration.type, predicate))
                return true;
        }

        if (visit_type_references(declaration.initial_value, predicate))
            return true;

        return false;
    }

    export template <typename Function_t>
        bool visit_type_references(
            iris::Struct_declaration const& declaration,
            Function_t predicate
        )
    {
        for (iris::Type_reference const& type_reference : declaration.member_types)
        {
            if (visit_type_references(type_reference, predicate))
                return true;
        }

        return visit_type_references(declaration.member_default_values, predicate);
    }

    export template <typename Function_t>
        bool visit_type_references(
            iris::Union_declaration const& declaration,
            Function_t predicate
        )
    {
        for (iris::Type_reference const& type_reference : declaration.member_types)
        {
            if (visit_type_references(type_reference, predicate))
                return true;
        }

        return false;
    }

    export template <typename Function_t>
        bool visit_type_references(
            iris::Function_constructor const& declaration,
            Function_t predicate
        )
    {
        for (iris::Function_constructor_parameter const& parameter : declaration.parameters)
        {
            if (visit_type_references(parameter.type, predicate))
                return true;
        }

        return visit_type_references(declaration.statements, predicate);
    }

    export template <typename Function_t>
        bool visit_type_references(
            iris::Type_constructor const& declaration,
            Function_t predicate
        )
    {
        for (iris::Type_constructor_parameter const& parameter : declaration.parameters)
        {
            if (visit_type_references(parameter.type, predicate))
                return true;
        }

        return visit_type_references(declaration.statements, predicate);
    }

    export template <typename Function_t>
        bool visit_type_references(
            iris::Function_declaration const& declaration,
            Function_t predicate
        )
    {
        for (iris::Type_reference const& type_reference : declaration.type.input_parameter_types)
        {
            if (visit_type_references(type_reference, predicate))
                return true;
        }

        for (iris::Type_reference const& type_reference : declaration.type.output_parameter_types)
        {
            if (visit_type_references(type_reference, predicate))
                return true;
        }

        return false;
    }

    export template <typename Function_t>
        bool visit_type_references(
            iris::Expression const& expression,
            iris::Statement const& statement,
            Function_t predicate
        );

    export template <typename Function_t>
        bool visit_type_references(
            iris::Statement const& statement,
            Function_t predicate
        )
    {
        for (iris::Expression const& expression : statement.expressions)
        {
            if (visit_type_references(expression, statement, predicate))
                return true;
        }

        return false;
    }

    export template <typename Function_t>
        bool visit_type_references(
            std::span<iris::Statement const> const statements,
            Function_t predicate
        )
    {
        for (iris::Statement const& statement : statements)
        {
            for (iris::Expression const& expression : statement.expressions)
            {
                if (visit_type_references(expression, statement, predicate))
                    return true;
            }
        }

        return false;
    }

    export template <typename Function_t>
        bool visit_type_references(
            iris::Expression const& expression,
            iris::Statement const& statement,
            Function_t predicate
        )
    {
        auto const process_expression = [&predicate](iris::Expression const& expression, iris::Statement const& statement) -> bool
        {
            if (std::holds_alternative<Cast_expression>(expression.data))
            {
                Cast_expression const& data = std::get<Cast_expression>(expression.data);
                return visit_type_references(data.destination_type, predicate);
            }
            else if (std::holds_alternative<Constant_expression>(expression.data))
            {
                Constant_expression const& data = std::get<Constant_expression>(expression.data);
                return visit_type_references(data.type, predicate);
            }
            else if (std::holds_alternative<Function_expression>(expression.data))
            {
                Function_expression const& data = std::get<Function_expression>(expression.data);
                if (visit_type_references(data.declaration, predicate))
                    return true;
                return visit_type_references(data.definition, predicate);
            }
            else if (std::holds_alternative<Reflection_expression>(expression.data))
            {
                Reflection_expression const& data = std::get<Reflection_expression>(expression.data);
                for (Type_reference const& type : data.type_arguments)
                {
                    if (visit_type_references(type, predicate))
                        return true;
                }
                return false;
            }
            else if (std::holds_alternative<Type_expression>(expression.data))
            {
                Type_expression const& data = std::get<Type_expression>(expression.data);
                return visit_type_references(data.type, predicate);
            }
            else if (std::holds_alternative<Struct_expression>(expression.data))
            {
                Struct_expression const& data = std::get<Struct_expression>(expression.data);
                return visit_type_references(data.declaration, predicate);
            }
            else if (std::holds_alternative<Union_expression>(expression.data))
            {
                Union_expression const& data = std::get<Union_expression>(expression.data);
                return visit_type_references(data.declaration, predicate);
            }
            else if (std::holds_alternative<Variable_declaration_with_type_expression>(expression.data))
            {
                Variable_declaration_with_type_expression const& data = std::get<Variable_declaration_with_type_expression>(expression.data);

                if (data.type.expression_index >= statement.expressions.size())
                    return false;

                return visit_type_references(statement.expressions[data.type.expression_index], statement, predicate);
            }

            return false;
        };

        if (visit_expressions(expression, statement, process_expression))
            return true;

        return false;
    }

    export template <typename Function_t>
        bool visit_type_references(
            iris::Function_definition const& definition,
            Function_t predicate
        )
    {
        for (iris::Statement const& statement : definition.statements)
        {
            for (iris::Expression const& expression : statement.expressions)
            {
                if (visit_type_references(expression, statement, predicate))
                    return true;
            }
        }

        return false;
    }

    export template <typename Function_t>
        bool visit_type_references(
            Module_declarations const& declarations,
            Function_t predicate
        )
    {
        for (Alias_type_declaration const& declaration : declarations.alias_type_declarations)
        {
            auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
            {
                return predicate(declaration.name, type_reference);
            };

            if (visit_type_references(declaration, predicate_with_name))
                return true;
        }

        for (Global_variable_declaration const& declaration : declarations.global_variable_declarations)
        {
            auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
            {
                return predicate(declaration.name, type_reference);
            };

            if (visit_type_references(declaration, predicate_with_name))
                return true;
        }

        for (Struct_declaration const& declaration : declarations.struct_declarations)
        {
            auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
            {
                return predicate(declaration.name, type_reference);
            };

            if (visit_type_references(declaration, predicate_with_name))
                return true;
        }

        for (Union_declaration const& declaration : declarations.union_declarations)
        {
            auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
            {
                return predicate(declaration.name, type_reference);
            };

            if (visit_type_references(declaration, predicate_with_name))
                return true;
        }

        for (Function_declaration const& declaration : declarations.function_declarations)
        {
            auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
            {
                return predicate(declaration.name, type_reference);
            };

            if (visit_type_references(declaration, predicate_with_name))
                return true;
        }

        return false;
    }

    export template <typename Function_t>
        bool visit_type_references(
            Module_definitions const& definitions,
            Function_t predicate
        )
    {
        for (Function_definition const& definition : definitions.function_definitions)
        {
            auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
            {
                return predicate(definition.name, type_reference);
            };

            for (iris::Statement const& statement : definition.statements)
            {
                if (visit_type_references(statement, predicate_with_name))
                    return true;
            }
        }

        return false;
    }

    export template <typename Function_t>
        bool visit_type_references(
            Module const& core_module,
            Function_t predicate
        )
    {
        visit_type_references(core_module.export_declarations, predicate);
        visit_type_references(core_module.internal_declarations, predicate);
        visit_type_references(core_module.definitions, predicate);
        return false;
    }

    export template <typename Function_t>
        bool visit_type_references(
            iris::Module const& core_module,
            std::string_view const declaration_name,
            Function_t predicate
        )
    {
        for (Alias_type_declaration const& declaration : core_module.export_declarations.alias_type_declarations)
        {
            if (declaration.name == declaration_name)
            {
                auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
                {
                    return predicate(declaration.name, type_reference);
                };

                visit_type_references(declaration, predicate_with_name);
                return true;
            }
        }

        for (Alias_type_declaration const& declaration : core_module.internal_declarations.alias_type_declarations)
        {
            if (declaration.name == declaration_name)
            {
                auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
                {
                    return predicate(declaration.name, type_reference);
                };

                visit_type_references(declaration, predicate_with_name);
                return true;
            }
        }

        for (Global_variable_declaration const& declaration : core_module.export_declarations.global_variable_declarations)
        {
            if (declaration.name == declaration_name)
            {
                auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
                {
                    return predicate(declaration.name, type_reference);
                };

                visit_type_references(declaration, predicate_with_name);
                return true;
            }
        }

        for (Global_variable_declaration const& declaration : core_module.internal_declarations.global_variable_declarations)
        {
            if (declaration.name == declaration_name)
            {
                auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
                {
                    return predicate(declaration.name, type_reference);
                };

                visit_type_references(declaration, predicate_with_name);
                return true;
            }
        }


        for (Struct_declaration const& declaration : core_module.export_declarations.struct_declarations)
        {
            if (declaration.name == declaration_name)
            {
                auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
                {
                    return predicate(declaration.name, type_reference);
                };

                visit_type_references(declaration, predicate_with_name);
                return true;
            }
        }

        for (Struct_declaration const& declaration : core_module.internal_declarations.struct_declarations)
        {
            if (declaration.name == declaration_name)
            {
                auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
                {
                    return predicate(declaration.name, type_reference);
                };

                visit_type_references(declaration, predicate_with_name);
                return true;
            }
        }

        for (Union_declaration const& declaration : core_module.export_declarations.union_declarations)
        {
            if (declaration.name == declaration_name)
            {
                auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
                {
                    return predicate(declaration.name, type_reference);
                };

                visit_type_references(declaration, predicate_with_name);
                return true;
            }
        }

        for (Union_declaration const& declaration : core_module.internal_declarations.union_declarations)
        {
            if (declaration.name == declaration_name)
            {
                auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
                {
                    return predicate(declaration.name, type_reference);
                };

                visit_type_references(declaration, predicate_with_name);
                return true;
            }
        }

        for (Function_declaration const& declaration : core_module.export_declarations.function_declarations)
        {
            if (declaration.name == declaration_name)
            {
                auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
                {
                    return predicate(declaration.name, type_reference);
                };

                visit_type_references(declaration, predicate_with_name);
                return true;
            }
        }

        for (Function_declaration const& declaration : core_module.internal_declarations.function_declarations)
        {
            if (declaration.name == declaration_name)
            {
                auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
                {
                    return predicate(declaration.name, type_reference);
                };

                visit_type_references(declaration, predicate_with_name);
                return true;
            }
        }

        return false;
    }

    export template <typename Value_t, typename Function_t>
        bool visit_type_references_recursively(
            Value_t const& value,
            Function_t predicate
        )
    {
        auto const call_recursive = [&](iris::Type_reference const& type_reference) -> bool
        {
            return visit_type_references_recursively(type_reference, predicate);
        };

        return visit_type_references(value, call_recursive);
    }

    export template <typename Function_t>
        bool visit_type_references_recursively_with_declaration_name(
            iris::Module const& core_module,
            std::string_view const declaration_name,
            Function_t predicate
        )
    {
        auto const call_recursive = [&](std::string_view const declaration_name, iris::Type_reference const& type_reference) -> bool
        {
            auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
            {
                return predicate(declaration_name, type_reference);
            };

            return visit_type_references_recursively(type_reference, predicate_with_name);
        };

        return visit_type_references(core_module, declaration_name, call_recursive);
    }

    export template <typename Function_t>
        bool visit_type_references_recursively(
            iris::Module const& core_module,
            Function_t predicate
        )
    {
        auto const call_recursive = [&](std::string_view const declaration_name, iris::Type_reference const& type_reference) -> bool
        {
            return visit_type_references_recursively(type_reference, predicate);
        };

        return visit_type_references(core_module, call_recursive);
    }

    export template <typename Value_t, typename Function_t>
        bool visit_type_references_recursively_with_declaration_name(
            Value_t const& value,
            Function_t predicate
        )
    {
        auto const call_recursive = [&](std::string_view const declaration_name, iris::Type_reference const& type_reference) -> bool
        {
            auto const predicate_with_name = [&](iris::Type_reference const& type_reference) -> bool
            {
                return predicate(declaration_name, type_reference);
            };

            return visit_type_references_recursively(type_reference, predicate_with_name);
        };

        return visit_type_references(value, call_recursive);
    }

    export template <typename Function_t>
        bool visit_expressions(
            iris::Expression const& expression,
            iris::Statement const& statement,
            Function_t predicate
        )
    {
        if (predicate(expression, statement))
            return true;

        if (std::holds_alternative<Block_expression>(expression.data))
        {
            Block_expression const& data = std::get<Block_expression>(expression.data);
            return visit_expressions(data.statements, predicate);
        }
        else if (std::holds_alternative<Constant_array_expression>(expression.data))
        {
            Constant_array_expression const& data = std::get<Constant_array_expression>(expression.data);
            return visit_expressions(data.array_data, predicate);
        }
        else if (std::holds_alternative<For_loop_expression>(expression.data))
        {
            For_loop_expression const& data = std::get<For_loop_expression>(expression.data);

            if (visit_expressions(data.range_end, predicate))
                return true;

            return visit_expressions(data.then_statements, predicate);
        }
        else if (std::holds_alternative<If_expression>(expression.data))
        {
            If_expression const& data = std::get<If_expression>(expression.data);

            for (Condition_statement_pair const& pair : data.series)
            {
                if (visit_expressions(pair.condition, predicate))
                    return true;

                if (visit_expressions(pair.then_statements, predicate))
                    return true;
            }
        }
        else if (std::holds_alternative<Switch_expression>(expression.data))
        {
            Switch_expression const& data = std::get<Switch_expression>(expression.data);

            for (Switch_case_expression_pair const& pair : data.cases)
            {
                if (visit_expressions(pair.statements, predicate))
                    return true;
            }
        }
        else if (std::holds_alternative<Ternary_condition_expression>(expression.data))
        {
            Ternary_condition_expression const& data = std::get<Ternary_condition_expression>(expression.data);

            if (visit_expressions(data.then_statement, predicate))
                return true;

            if (visit_expressions(data.else_statement, predicate))
                return true;
        }
        else if (std::holds_alternative<While_loop_expression>(expression.data))
        {
            While_loop_expression const& data = std::get<While_loop_expression>(expression.data);

            if (visit_expressions(data.condition, predicate))
                return true;

            if (visit_expressions(data.then_statements, predicate))
                return true;
        }

        return false;
    }

    export template <typename Function_t>
        bool visit_expressions(
            iris::Statement const& statement,
            Function_t predicate
        )
    {
        for (iris::Expression const& expression : statement.expressions)
        {
            if (visit_expressions(expression, statement, predicate))
                return true;
        }

        return false;
    }

    export template <typename Function_t>
        bool visit_expressions(
            std::optional<iris::Statement> const statement,
            Function_t predicate
        )
    {
        if (statement)
        {
            return visit_expressions(*statement, predicate);
        }

        return false;
    }

    export template <typename Function_t>
        bool visit_expressions(
            std::span<iris::Statement const> const statements,
            Function_t predicate
        )
    {
        for (iris::Statement const& statement : statements)
        {
            if (visit_expressions(statement, predicate))
                return true;
        }

        return false;
    }

    export template <typename Function_t>
    bool visit_expressions(
        iris::Function_declaration const& declaration,
        Function_t predicate
    )
    {
        return false;
    }

    export template <typename Function_t>
        bool visit_expressions(
            iris::Function_definition const& definition,
            Function_t predicate
        )
    {
        return visit_expressions(definition.statements, predicate);
    }

    export template <typename Function_t>
        bool visit_expressions(
            iris::Enum_declaration const& declaration,
            Function_t predicate
        )
    {
        for (Enum_value const& enum_value : declaration.values)
        {
            if (visit_expressions(enum_value.value, predicate))
                return true;
        }

        return false;
    }

    export template <typename Function_t>
    bool visit_expressions(
        iris::Global_variable_declaration const& declaration,
        Function_t predicate
    )
    {
        if (visit_expressions(declaration.initial_value, predicate))
            return true;

        return false;
    }

    export template <typename Function_t>
        bool visit_expressions(
            iris::Struct_declaration const& declaration,
            Function_t predicate
        )
    {
        return visit_expressions(declaration.member_default_values, predicate);
    }

    export template <typename Function_t>
        bool visit_expressions(
            Module_declarations const& declarations,
            Function_t predicate
        )
    {
        for (Enum_declaration const& declaration : declarations.enum_declarations)
        {
            if (visit_expressions(declaration, predicate))
                return true;
        }

        for (Global_variable_declaration const& declaration : declarations.global_variable_declarations)
        {
            if (visit_expressions(declaration, predicate))
                return true;
        }

        for (Struct_declaration const& declaration : declarations.struct_declarations)
        {
            if (visit_expressions(declaration, predicate))
                return true;
        }

        return false;
    }

    export template <typename Function_t>
        bool visit_expressions(
            Module_definitions const& definitions,
            Function_t predicate
        )
    {
        for (Function_definition const& definition : definitions.function_definitions)
        {
            for (iris::Statement const& statement : definition.statements)
            {
                if (visit_expressions(statement, predicate))
                    return true;
            }
        }

        return false;
    }

    export template <typename Function_t>
        bool visit_expressions(
            Module const& core_module,
            Function_t predicate
        )
    {
        if (visit_expressions(core_module.export_declarations, predicate))
            return true;

        if (visit_expressions(core_module.internal_declarations, predicate))
            return true;

        if (visit_expressions(core_module.definitions, predicate))
            return true;

        return false;
    }

    export bool replace_parameter_types_by_instance_arguments(
        Type_reference& type_reference,
        std::span<Function_constructor_parameter const> function_constructor_parameters,
        std::span<Statement const> instance_arguments
    );

    export bool replace_parameter_types_by_instance_arguments(
        Type_reference& type_reference,
        std::span<Type_constructor_parameter const> type_constructor_parameters,
        std::span<Statement const> instance_arguments
    );

    export bool replace_parameter_types_by_instance_arguments(
        Function_type& function_type,
        std::span<Function_constructor_parameter const> function_constructor_parameters,
        std::span<Statement const> instance_arguments
    );

    export bool replace_parameter_types_by_instance_arguments(
        Function_type& function_type,
        std::span<Type_constructor_parameter const> type_constructor_parameters,
        std::span<Statement const> instance_arguments
    );
}
