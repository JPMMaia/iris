module h.core.execution_engine;

import std;

import h.core;
import h.core.types;

namespace h::execution_engine
{
    Value_storage create_value_storage_struct(
        Struct_declaration const& declaration,
        std::pmr::polymorphic_allocator<> const& allocator
    )
    {
        // TODO do copy with allocator
        return
        {
            .data = declaration
        };
    }

    Value_storage create_value_storage_function_expression(
        Function_expression const& expression,
        std::pmr::polymorphic_allocator<> const& allocator
    )
    {
        // TODO do copy with allocator
        return
        {
            .data = expression
        };
    }

    Execution_engine create_execution_engine(
        std::pmr::polymorphic_allocator<> const& allocator
    )
    {
        return
        {
            .allocator = allocator,
        };
    }

    Value_storage evaluate_type_constructor(
        Execution_engine& engine,
        Type_constructor const& type_constructor,
        std::span<Statement const> const arguments
    )
    {
        if (type_constructor.parameters.size() != arguments.size())
            throw std::runtime_error{ "Number of arguments of type instance do not match type constructor!" };

        engine.type_constructor = &type_constructor;
        engine.type_instance_arguments = arguments;

        for (std::size_t index = 0; index < type_constructor.parameters.size(); ++index)
        {
            Type_constructor_parameter const& parameter = type_constructor.parameters[index];
            Statement const& argument = arguments[index];

            // TODO if constant, then add variable
        }

        std::optional<Value_storage> const returned_value_optional = evaluate_statements(engine, type_constructor.statements);

        if (!returned_value_optional.has_value())
            throw std::runtime_error{ "Could not evaluate type constructor!" };

        // TODO check that value_storage type is a declaration type (not function type, or global variable type)

        Value_storage const& returned_value = returned_value_optional.value();
        return returned_value;
    }

    Function_expression evaluate_function_constructor(
        Execution_engine& engine,
        Function_constructor const& function_constructor,
        std::span<Statement const> const arguments
    )
    {
        if (function_constructor.parameters.size() != arguments.size())
            throw std::runtime_error{ "Number of arguments of type instance do not match type constructor!" };

        engine.function_constructor = &function_constructor;
        engine.function_instance_arguments = arguments;

        for (std::size_t index = 0; index < function_constructor.parameters.size(); ++index)
        {
            Function_constructor_parameter const& parameter = function_constructor.parameters[index];
            Statement const& argument = arguments[index];

            // TODO if constant, then add variable
        }

        std::optional<Value_storage> const returned_value_optional = evaluate_statements(engine, function_constructor.statements);

        if (!returned_value_optional.has_value())
            throw std::runtime_error{ "Could not evaluate function constructor!" };

        if (!std::holds_alternative<Function_expression>(returned_value_optional.value().data))
            throw std::runtime_error{ "A function constructor must return a function expression!" };

        Function_expression const& function_expression = std::get<Function_expression>(returned_value_optional.value().data);
        return function_expression;
    }

    std::optional<Value_storage> evaluate_statements(
        Execution_engine& engine,
        std::span<Statement const> const statements
    )
    {
        for (std::size_t index = 0; index < statements.size(); ++index)
        {
            Statement const& statement = statements[index];

            if (!statement.expressions.empty())
            {
                std::pair<std::optional<Value_storage>, bool> const result = evaluate_expression(engine, statement, statement.expressions[0]);
                bool const stop = result.second;
                if (stop)
                    return result.first;
            }
        }

        return std::nullopt;
    }

    bool is_compile_time_builtin_type(Type_reference const& type_reference)
    {
        if (std::holds_alternative<Builtin_type_reference>(type_reference.data))
        {
            Builtin_type_reference const& builtin_type = std::get<Builtin_type_reference>(type_reference.data);
            return builtin_type.value == "Type";
        }

        return false;
    }

    template<class T>
    void replace_parameter_types_by_instance_arguments(
        T& value,
        std::span<Type_constructor_parameter const> const type_constructor_parameters,
        std::span<Statement const> const type_instance_arguments
    )
    {
        auto const process_type = [&](Type_reference const& type_reference) -> bool
        {
            Type_reference& mutable_type_reference = const_cast<Type_reference&>(type_reference);
            auto const location = std::find_if(
                type_constructor_parameters.begin(),
                type_constructor_parameters.end(),
                [&](Type_constructor_parameter const& parameter) -> bool {
                    return std::holds_alternative<Parameter_type>(mutable_type_reference.data)
                        && std::get<Parameter_type>(mutable_type_reference.data).name == parameter.name;
                }
            );
            if (location != type_constructor_parameters.end() && !is_compile_time_builtin_type(location->type))
                throw std::runtime_error{ "Type constructor parameter type is not the compile time builtin type!"};

            if (!h::replace_parameter_types_by_instance_arguments(mutable_type_reference, type_constructor_parameters, type_instance_arguments))
                throw std::runtime_error{ "Could not replace parameter type by instance argument!" };

            return false;
        };

        visit_type_references_recursively(value, process_type);
    }

    std::optional<h::Type_expression> find_type_expression_from_function_constructor_arguments(
        std::span<Function_constructor_parameter const> const function_constructor_parameters,
        std::span<Statement const> const type_instance_arguments,
        std::string_view const parameter_name
    )
    {
        auto const location = std::find_if(
            function_constructor_parameters.begin(),
            function_constructor_parameters.end(), 
            [&](Function_constructor_parameter const& parameter) -> bool { return parameter.name == parameter_name; }
        );
        if (location == function_constructor_parameters.end())
            return std::nullopt;

        Function_constructor_parameter const& parameter = *location;
        if (!is_compile_time_builtin_type(parameter.type))
            return std::nullopt;

        std::size_t const parameter_index = std::distance(function_constructor_parameters.begin(), location);
        if (parameter_index >= type_instance_arguments.size())
            return std::nullopt;

        Expression const& expression = type_instance_arguments[parameter_index].expressions[0];
        if (!std::holds_alternative<Type_expression>(expression.data))
            return std::nullopt;

        Type_expression const& type_expression = std::get<Type_expression>(expression.data);
        return type_expression;
    }

    template<class T>
    void replace_parameter_types_by_instance_arguments(
        T& value,
        std::span<Function_constructor_parameter const> const function_constructor_parameters,
        std::span<Statement const> const type_instance_arguments
    )
    {
        auto const process_expression = [&](h::Expression const& expression, h::Statement const& statement) -> bool
        {
            h::Expression& mutable_expression = const_cast<h::Expression&>(expression);
            if (std::holds_alternative<Variable_expression>(expression.data))
            {
                Variable_expression const& variable_expression = std::get<Variable_expression>(expression.data);

                std::optional<Type_expression> const new_parameter_type = find_type_expression_from_function_constructor_arguments(
                    function_constructor_parameters,
                    type_instance_arguments,
                    variable_expression.name
                );
                if (!new_parameter_type.has_value())
                    return false;

                Type_expression const& type_expression = new_parameter_type.value();

                // TODO use allocator to copy
                mutable_expression.data = type_expression;
            }

            return false;
        };

        visit_expressions(value, process_expression);

        auto const process_type = [&](Type_reference const& type_reference) -> bool
        {
            Type_reference& mutable_type_reference = const_cast<Type_reference&>(type_reference);
            if (!h::replace_parameter_types_by_instance_arguments(mutable_type_reference, function_constructor_parameters, type_instance_arguments))
                throw std::runtime_error{ "Could not find parameter type in type constructor!"};

            return false;
        };

        visit_type_references_recursively(value, process_type);
    }

    std::pair<std::optional<Value_storage>, bool> make_pair(
        std::optional<Value_storage> value_and_type,
        bool const stop
    )
    {
        return std::make_pair(std::move(value_and_type), stop);
    }

    std::pair<std::optional<Value_storage>, bool> evaluate_expression(
        Execution_engine& engine,
        Statement const& statement,
        Expression const& expression
    )
    {
        if (std::holds_alternative<Function_expression>(expression.data))
        {
            // TODO copy using allocator
            Function_expression function_expression = std::get<Function_expression>(expression.data);
            
            if (engine.function_constructor != nullptr)
            {
                replace_parameter_types_by_instance_arguments(
                    function_expression.declaration,
                    engine.function_constructor->parameters,
                    engine.function_instance_arguments
                );

                replace_parameter_types_by_instance_arguments(
                    function_expression.definition,
                    engine.function_constructor->parameters,
                    engine.function_instance_arguments
                );
            }

            return make_pair(
                create_value_storage_function_expression(function_expression, engine.allocator),
                false
            );
        }
        else if (std::holds_alternative<Return_expression>(expression.data))
        {
            Return_expression const& return_expression = std::get<Return_expression>(expression.data);
            if (!return_expression.expression.has_value())
                return make_pair(std::nullopt, false);

            std::size_t const next_expression_index = return_expression.expression.value().expression_index;
            Expression const& next_expression = statement.expressions[next_expression_index];

            std::pair<std::optional<Value_storage>, bool> result = evaluate_expression(engine, statement, next_expression);
            result.second = true;
            return result;
        }
        else if (std::holds_alternative<Struct_expression>(expression.data))
        {
            Struct_expression const& struct_expression = std::get<Struct_expression>(expression.data);

            // TODO copy using allocator
            Struct_declaration struct_declaration = struct_expression.declaration;

            if (engine.type_constructor != nullptr)
            {
                replace_parameter_types_by_instance_arguments(
                    struct_declaration,
                    engine.type_constructor->parameters,
                    engine.type_instance_arguments
                );
            }

            return make_pair(
                create_value_storage_struct(struct_declaration, engine.allocator),
                false
            );
        }
        else
        {
            throw std::runtime_error{ "Execution_engine::evaluate_expression(): expression not implemented!" };
        }
    }
}
