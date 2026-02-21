module;

#include <cstdint>
#include <compare>
#include <exception>
#include <filesystem>
#include <memory_resource>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

module h.core;

import h.common;

namespace h
{
#if HACK_SPACESHIP_OPERATOR
    template<typename... Ts>
    std::strong_ordering operator<=>(std::variant<Ts...> const& lhs, std::variant<Ts...> const& rhs)
    {
        std::size_t const i = lhs.index();
        std::size_t const j = rhs.index();
        if (i != j)
            return i <=> j;

        std::strong_ordering result = std::strong_ordering::equal;
        
        auto const visitor = [&](auto const& lhs_value) -> void {
            using real_type = std::remove_cv_t<std::remove_reference_t<decltype(lhs_value)>>;
            auto const& rhs_value = std::get<real_type>(rhs);
            result = lhs_value <=> rhs_value;
        };

        std::visit(visitor, lhs);

        return result;
    }

    std::strong_ordering operator<=>(Type_instance const& lhs, Type_instance const& rhs) = default;
    std::strong_ordering operator<=>(Type_reference const& lhs, Type_reference const& rhs) = default;
    std::strong_ordering operator<=>(Expression const& lhs, Expression const& rhs) = default;
    std::strong_ordering operator<=>(Statement const& lhs, Statement const& rhs) = default;

    bool operator==(Statement const& lhs, Statement const& rhs)
    {
        if (lhs.expressions.size() != rhs.expressions.size())
            return false;

        for (std::size_t index = 0; index < lhs.expressions.size(); ++index)
        {
            std::strong_ordering const result = lhs.expressions[index] <=> rhs.expressions[index];
            if (result != std::strong_ordering::equal)
                return false;
        }

        return true;
    }

    bool operator==(Type_instance const& lhs, Type_instance const& rhs) = default;
    
    bool operator==(Type_reference const& lhs, Type_reference const& rhs)
    {
        return false; // TODo
    }
#endif

    bool operator==(Expression const& lhs, Expression const& rhs)
    {
        return lhs.data == rhs.data;
    }

    bool operator==(Type_reference const& lhs, Type_reference const& rhs)
    {
        return lhs.data == rhs.data;
    }

    bool operator==(Import_module_with_alias const& lhs, Import_module_with_alias const& rhs)
    {
        return lhs.module_name == rhs.module_name &&
               lhs.alias == rhs.alias;
    }

    bool operator==(Function_condition const& lhs, Function_condition const& rhs)
    {
        return lhs.description == rhs.description &&
               lhs.condition == rhs.condition;
    }

    Source_range create_source_range(
        std::uint32_t const start_line,
        std::uint32_t const start_column,
        std::uint32_t const end_line,
        std::uint32_t const end_column
    )
    {
        return Source_range
        {
            .start = { start_line, start_column },
            .end = { end_line, end_column }
        };
    }

    std::optional<h::Source_range> create_sub_source_range(
        std::optional<h::Source_range> const& source_range,
        std::uint32_t const start_index,
        std::uint32_t const count
    )
    {
        if (!source_range.has_value())
            return std::nullopt;

        h::Source_range const& original_source_range = source_range.value();

        return h::Source_range
        {
            .start = {
                .line = original_source_range.start.line,
                .column = original_source_range.start.column + start_index
            },
            .end = {
                .line = original_source_range.start.line,
                .column = original_source_range.start.column + start_index + count
            }
        };
    }

    Source_range_location create_source_range_location(
        std::optional<std::filesystem::path> const& file_path,
        std::uint32_t const start_line,
        std::uint32_t const start_column,
        std::uint32_t const end_line,
        std::uint32_t const end_column
    )
    {
        return Source_range_location
        {
            .file_path = file_path,
            .range = create_source_range(start_line, start_column, end_line, end_column),
        };
    }

    bool range_contains_position(
        h::Source_range const& range,
        h::Source_position const& position
    )
    {
        if (position.line < range.start.line || (position.line == range.start.line && position.column < range.start.column))
            return false;

        if (position.line > range.end.line || (position.line == range.end.line && position.column >= range.end.column))
            return false;

        return true;
    }

    bool range_contains_position_inclusive(
        h::Source_range const& range,
        h::Source_position const& position
    )
    {
        if (position.line < range.start.line || (position.line == range.start.line && position.column < range.start.column))
            return false;

        if (position.line > range.end.line || (position.line == range.end.line && position.column > range.end.column))
            return false;

        return true;
    }

    
    bool is_bit_shift_binary_operation(h::Binary_operation const operation)
    {
        switch (operation)
        {
            case h::Binary_operation::Bit_shift_left:
            case h::Binary_operation::Bit_shift_right:
                return true;
            default:
                return false;
        }
    }
    
    bool is_bitwise_binary_operation(h::Binary_operation const operation)
    {
        switch (operation)
        {
            case h::Binary_operation::Bitwise_and:
            case h::Binary_operation::Bitwise_or:
            case h::Binary_operation::Bitwise_xor:
                return true;
            default:
                return false;
        }
    }

    bool is_equality_binary_operation(h::Binary_operation const operation)
    {
        switch (operation)
        {
            case h::Binary_operation::Equal:
            case h::Binary_operation::Not_equal:
                return true;
            default:
                return false;
        }
    }

    bool is_comparison_binary_operation(h::Binary_operation const operation)
    {
        switch (operation)
        {
            case h::Binary_operation::Less_than:
            case h::Binary_operation::Less_than_or_equal_to:
            case h::Binary_operation::Greater_than:
            case h::Binary_operation::Greater_than_or_equal_to:
                return true;
            default:
                return false;
        }
    }

    bool is_logical_binary_operation(h::Binary_operation const operation)
    {
        switch (operation)
        {
            case h::Binary_operation::Logical_and:
            case h::Binary_operation::Logical_or:
                return true;
            default:
                return false;
        }
    }

    
    bool is_numeric_binary_operation(h::Binary_operation const operation)
    {
        switch (operation)
        {
            case h::Binary_operation::Add:
            case h::Binary_operation::Subtract:
            case h::Binary_operation::Multiply:
            case h::Binary_operation::Divide:
            case h::Binary_operation::Modulus:
                return true;
            default:
                return false;
        }
    }

    h::Module const& find_module(
        h::Module const& core_module,
        std::pmr::unordered_map<std::pmr::string, h::Module> const& core_module_dependencies,
        std::string_view const name
    )
    {
        if (core_module.name == name)
            return core_module;

        auto const location = core_module_dependencies.find(name.data());
        if (location != core_module_dependencies.end())
            return location->second;

        h::common::print_message_and_exit(std::format("Could not find module '{}'", name));
        std::unreachable();
    }

    std::string_view find_module_name(
        h::Module const& core_module,
        h::Module_reference const& module_reference
    )
    {
        return module_reference.name;
    }

    Custom_type_reference const* find_declaration_type_reference(
        Type_reference const& type_reference
    )
    {
        if (std::holds_alternative<Custom_type_reference>(type_reference.data))
        {
            Custom_type_reference const& custom_type_reference = std::get<Custom_type_reference>(type_reference.data);
            return &custom_type_reference;
        }
        else if (std::holds_alternative<Type_instance>(type_reference.data))
        {
            Type_instance const& type_instance = std::get<Type_instance>(type_reference.data);
            return &type_instance.type_constructor;
        }

        return nullptr;
    }

    template<typename T>
    concept Has_name = requires(T a)
    {
        { a.name } -> std::convertible_to<std::pmr::string>;
    };

    template<Has_name Type>
    Type const* get_value(
        std::string_view const name,
        std::span<Type const> const values
    )
    {
        auto const location = std::find_if(values.begin(), values.end(), [name](Type const& value) { return value.name == name; });
        return location != values.end() ? *location : nullptr;
    }

    template<Has_name Type>
    std::optional<Type const*> get_value(
        std::string_view const name,
        std::pmr::vector<Type> const& span_0,
        std::pmr::vector<Type> const& span_1
    )
    {
        auto const find_declaration = [name](Type const& declaration) -> bool { return declaration.name == name; };

        {
            auto const location = std::find_if(span_0.begin(), span_0.end(), find_declaration);
            if (location != span_0.end())
                return &(*location);
        }

        {
            auto const location = std::find_if(span_1.begin(), span_1.end(), find_declaration);
            if (location != span_1.end())
                return &(*location);
        }

        return std::nullopt;
    }

    std::optional<Alias_type_declaration const*> find_alias_type_declaration(h::Module const& module, std::string_view const name)
    {
        return get_value(name, module.export_declarations.alias_type_declarations, module.internal_declarations.alias_type_declarations);
    }

    std::optional<Enum_declaration const*> find_enum_declaration(h::Module const& module, std::string_view const name)
    {
        return get_value(name, module.export_declarations.enum_declarations, module.internal_declarations.enum_declarations);
    }

    std::optional<Forward_declaration const*> find_forward_declaration(h::Module const& module, std::string_view const name)
    {
        return get_value(name, module.export_declarations.forward_declarations, module.internal_declarations.forward_declarations);
    }

    std::optional<Global_variable_declaration const*> find_global_variable_declaration(h::Module const& module, std::string_view name)
    {
        return get_value(name, module.export_declarations.global_variable_declarations, module.internal_declarations.global_variable_declarations);
    }

    std::optional<Function_declaration const*> find_function_declaration(h::Module const& module, std::string_view const name)
    {
        return get_value(name, module.export_declarations.function_declarations, module.internal_declarations.function_declarations);
    }

    std::optional<Function_definition const*> find_function_definition(Module const& module, std::string_view name)
    {
        return get_value(name, module.definitions.function_definitions, {});
    }

    std::optional<Struct_declaration const*> find_struct_declaration(h::Module const& module, std::string_view const name)
    {
        return get_value(name, module.export_declarations.struct_declarations, module.internal_declarations.struct_declarations);
    }

    std::optional<Union_declaration const*> find_union_declaration(h::Module const& module, std::string_view const name)
    {
        return get_value(name, module.export_declarations.union_declarations, module.internal_declarations.union_declarations);
    }

    Import_module_with_alias const* find_import_module_with_alias(
        h::Module const& core_module,
        std::string_view const alias_name
    )
    {
        auto const location = std::find_if(
            core_module.dependencies.alias_imports.begin(),
            core_module.dependencies.alias_imports.end(),
            [&](Import_module_with_alias const& import_alias) -> bool { return import_alias.alias == alias_name; }
        );
        if (location == core_module.dependencies.alias_imports.end())
            return nullptr;

        return &(*location);
    }

    Import_module_with_alias* find_import_module_with_alias(
        h::Module& core_module,
        std::string_view const alias_name
    )
    {
        auto const location = std::find_if(
            core_module.dependencies.alias_imports.begin(),
            core_module.dependencies.alias_imports.end(),
            [&](Import_module_with_alias const& import_alias) -> bool { return import_alias.alias == alias_name; }
        );
        if (location == core_module.dependencies.alias_imports.end())
            return nullptr;

        return &(*location);
    }

    Import_module_with_alias const* find_import_module_with_module_name(
        h::Module const& core_module,
        std::string_view const module_name
    )
    {
        auto const location = std::find_if(
            core_module.dependencies.alias_imports.begin(),
            core_module.dependencies.alias_imports.end(),
            [&](Import_module_with_alias const& import_alias) -> bool { return import_alias.module_name == module_name; }
        );
        if (location == core_module.dependencies.alias_imports.end())
            return nullptr;

        return &(*location);
    }

    h::Expression_index copy_expressions_to_new_statement(
        h::Statement& destination_statement,
        h::Statement const& source_statement,
        h::Expression_index const source_expression_index
    )
    {
        h::Expression current_expression = source_statement.expressions[source_expression_index.expression_index];
        
        std::uint64_t const destination_expression_index = destination_statement.expressions.size();
        destination_statement.expressions.push_back({});

        if (std::holds_alternative<h::Access_expression>(current_expression.data))
        {
            Access_expression& data = std::get<Access_expression>(current_expression.data);
            data.expression = copy_expressions_to_new_statement(destination_statement, source_statement, data.expression);
        }
        else if (std::holds_alternative<h::Access_array_expression>(current_expression.data))
        {
            Access_array_expression& data = std::get<Access_array_expression>(current_expression.data);
            data.expression = copy_expressions_to_new_statement(destination_statement, source_statement, data.expression);
            data.index = copy_expressions_to_new_statement(destination_statement, source_statement, data.index);
        }
        else if (std::holds_alternative<h::Assert_expression>(current_expression.data))
        {
        }
        else if (std::holds_alternative<h::Assignment_expression>(current_expression.data))
        {
            Assignment_expression& data = std::get<Assignment_expression>(current_expression.data);
            data.left_hand_side = copy_expressions_to_new_statement(destination_statement, source_statement, data.left_hand_side);
            data.right_hand_side = copy_expressions_to_new_statement(destination_statement, source_statement, data.right_hand_side);
        }
        else if (std::holds_alternative<h::Binary_expression>(current_expression.data))
        {
            Binary_expression& data = std::get<Binary_expression>(current_expression.data);
            data.left_hand_side = copy_expressions_to_new_statement(destination_statement, source_statement, data.left_hand_side);
            data.right_hand_side = copy_expressions_to_new_statement(destination_statement, source_statement, data.right_hand_side);
        }
        else if (std::holds_alternative<h::Block_expression>(current_expression.data))
        {
        }
        else if (std::holds_alternative<h::Break_expression>(current_expression.data))
        {
        }
        else if (std::holds_alternative<h::Call_expression>(current_expression.data))
        {
            Call_expression& data = std::get<Call_expression>(current_expression.data);
            data.expression = copy_expressions_to_new_statement(destination_statement, source_statement, data.expression);
            for (h::Expression_index& expression_index : data.arguments)
                expression_index = copy_expressions_to_new_statement(destination_statement, source_statement, expression_index);
        }
        else if (std::holds_alternative<h::Cast_expression>(current_expression.data))
        {
            Cast_expression& data = std::get<Cast_expression>(current_expression.data);
            data.source = copy_expressions_to_new_statement(destination_statement, source_statement, data.source);
        }
        else if (std::holds_alternative<h::Comment_expression>(current_expression.data))
        {
        }
        else if (std::holds_alternative<h::Compile_time_expression>(current_expression.data))
        {
            Compile_time_expression& data = std::get<Compile_time_expression>(current_expression.data);
            data.expression = copy_expressions_to_new_statement(destination_statement, source_statement, data.expression);
        }
        else if (std::holds_alternative<h::Constant_expression>(current_expression.data))
        {
        }
        else if (std::holds_alternative<h::Constant_array_expression>(current_expression.data))
        {
        }
        else if (std::holds_alternative<h::Continue_expression>(current_expression.data))
        {
        }
        else if (std::holds_alternative<h::Defer_expression>(current_expression.data))
        {
            Defer_expression& data = std::get<Defer_expression>(current_expression.data);
            data.expression_to_defer = copy_expressions_to_new_statement(destination_statement, source_statement, data.expression_to_defer);
        }
        else if (std::holds_alternative<h::Dereference_and_access_expression>(current_expression.data))
        {
            Dereference_and_access_expression& data = std::get<Dereference_and_access_expression>(current_expression.data);
            data.expression = copy_expressions_to_new_statement(destination_statement, source_statement, data.expression);
        }
        else if (std::holds_alternative<h::For_loop_expression>(current_expression.data))
        {
            For_loop_expression& data = std::get<For_loop_expression>(current_expression.data);
            data.range_begin = copy_expressions_to_new_statement(destination_statement, source_statement, data.range_begin);
            if (data.step_by.has_value())
                data.step_by = copy_expressions_to_new_statement(destination_statement, source_statement, data.step_by.value());
        }
        else if (std::holds_alternative<h::Function_expression>(current_expression.data))
        {
        }
        else if (std::holds_alternative<h::Instance_call_expression>(current_expression.data))
        {
            Instance_call_expression& data = std::get<Instance_call_expression>(current_expression.data);
            data.left_hand_side = copy_expressions_to_new_statement(destination_statement, source_statement, data.left_hand_side);
        }
        else if (std::holds_alternative<h::If_expression>(current_expression.data))
        {
        }
        else if (std::holds_alternative<h::Instantiate_expression>(current_expression.data))
        {
            Instantiate_expression& data = std::get<Instantiate_expression>(current_expression.data);
            for (h::Instantiate_member_value_pair& member : data.members)
                member.value = copy_expressions_to_new_statement(destination_statement, source_statement, member.value);
        }
        else if (std::holds_alternative<h::Invalid_expression>(current_expression.data))
        {
        }
        else if (std::holds_alternative<h::Null_pointer_expression>(current_expression.data))
        {
        }
        else if (std::holds_alternative<h::Parenthesis_expression>(current_expression.data))
        {
            Parenthesis_expression& data = std::get<Parenthesis_expression>(current_expression.data);
            data.expression = copy_expressions_to_new_statement(destination_statement, source_statement, data.expression);
        }
        else if (std::holds_alternative<h::Reflection_expression>(current_expression.data))
        {
            Reflection_expression& data = std::get<Reflection_expression>(current_expression.data);
            for (h::Expression_index& expression_index : data.arguments)
                expression_index = copy_expressions_to_new_statement(destination_statement, source_statement, expression_index);
        }
        else if (std::holds_alternative<h::Return_expression>(current_expression.data))
        {
            Return_expression& data = std::get<Return_expression>(current_expression.data);
            if (data.expression.has_value())
                data.expression = copy_expressions_to_new_statement(destination_statement, source_statement, data.expression.value());
        }
        else if (std::holds_alternative<h::Struct_expression>(current_expression.data))
        {
        }
        else if (std::holds_alternative<h::Switch_expression>(current_expression.data))
        {
            Switch_expression& data = std::get<Switch_expression>(current_expression.data);
            data.value = copy_expressions_to_new_statement(destination_statement, source_statement, data.value);
            for (h::Switch_case_expression_pair& pair : data.cases)
            {
                if (pair.case_value.has_value())
                    pair.case_value = copy_expressions_to_new_statement(destination_statement, source_statement, pair.case_value.value());
            }
        }
        else if (std::holds_alternative<h::Ternary_condition_expression>(current_expression.data))
        {
            Ternary_condition_expression& data = std::get<Ternary_condition_expression>(current_expression.data);
            data.condition = copy_expressions_to_new_statement(destination_statement, source_statement, data.condition);
        }
        else if (std::holds_alternative<h::Type_expression>(current_expression.data))
        {
        }
        else if (std::holds_alternative<h::Unary_expression>(current_expression.data))
        {
            Unary_expression& data = std::get<Unary_expression>(current_expression.data);
            data.expression = copy_expressions_to_new_statement(destination_statement, source_statement, data.expression);
        }
        else if (std::holds_alternative<h::Union_expression>(current_expression.data))
        {
        }
        else if (std::holds_alternative<h::Variable_declaration_expression>(current_expression.data))
        {
            Variable_declaration_expression& data = std::get<Variable_declaration_expression>(current_expression.data);
            data.right_hand_side = copy_expressions_to_new_statement(destination_statement, source_statement, data.right_hand_side);
        }
        else if (std::holds_alternative<h::Variable_declaration_with_type_expression>(current_expression.data))
        {
            Variable_declaration_with_type_expression& data = std::get<Variable_declaration_with_type_expression>(current_expression.data);
            data.right_hand_side = copy_expressions_to_new_statement(destination_statement, source_statement, data.right_hand_side);
        }
        else if (std::holds_alternative<h::Variable_expression>(current_expression.data))
        {
        }
        else if (std::holds_alternative<h::While_loop_expression>(current_expression.data))
        {
        }
        else
        {
            throw std::runtime_error{"copy_expressions_to_new_statement: Not implemented!"};
        }
        
        destination_statement.expressions[destination_expression_index] = std::move(current_expression);

        return h::Expression_index{.expression_index = destination_expression_index};
    }

    bool is_builtin_function_name(
        std::string_view const name
    )
    {
        return name == "check" ||
               name == "create_array_slice_from_pointer" ||
               name == "create_stack_array_uninitialized" ||
               name == "offset_pointer" ||
               name == "reinterpret_as";
    }

    bool is_expression_address_of(
        h::Expression const& expression
    )
    {
        if (std::holds_alternative<h::Unary_expression>(expression.data))
        {
            h::Unary_expression const& unary_expression = std::get<h::Unary_expression>(expression.data);
            return unary_expression.operation == Unary_operation::Address_of;
        }

        return false;
    }

    bool is_offset_pointer(
        h::Statement const& statement,
        h::Expression const& expression
    )
    {
        if (std::holds_alternative<h::Call_expression>(expression.data))
        {
            h::Call_expression const& call_expression = std::get<h::Call_expression>(expression.data);

            h::Expression const& left_call_expression = statement.expressions[call_expression.expression.expression_index];
            if (std::holds_alternative<h::Variable_expression>(left_call_expression.data))
            {
                h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(left_call_expression.data);
                return variable_expression.name == "offset_pointer";
            }
        }

        return false;
    }

    bool is_add_scope_expression(h::Expression const& expression)
    {
        return std::holds_alternative<h::Block_expression>(expression.data)
            || std::holds_alternative<h::For_loop_expression>(expression.data)
            || std::holds_alternative<h::If_expression>(expression.data)
            || std::holds_alternative<h::Switch_expression>(expression.data)
            || std::holds_alternative<h::While_loop_expression>(expression.data);
    }
}