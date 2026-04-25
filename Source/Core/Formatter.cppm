export module iris.core.formatter;

import std;

import iris.core;

namespace iris
{
    export struct Format_options
    {
        std::span<iris::Import_module_with_alias const> const alias_imports;
        std::pmr::polymorphic_allocator<> const& output_allocator;
        std::pmr::polymorphic_allocator<> const& temporaries_allocator;
    };

    export std::pmr::string format_module(
        iris::Module const& core_module,
        Format_options const& options
    );

    export std::pmr::string format_type_reference(
        iris::Module_dependencies const& dependencies,
        std::optional<iris::Type_reference> const& type_reference,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::string_view format_fundamental_type(
        Fundamental_type const value
    );

    export std::pmr::string format_integer_type(
        iris::Integer_type const value
    );

    export std::pmr::string format_statement(
        iris::Module const& core_module,
        Statement const& statement,
        std::uint32_t indentation,
        bool const add_semicolon = true,
        std::pmr::polymorphic_allocator<> const& output_allocator = {},
        std::pmr::polymorphic_allocator<> const& temporaries_allocator = {}
    );

    export std::pmr::string format_expression(
        iris::Module const& core_module,
        Statement const& statement,
        Expression const& expression,
        std::uint32_t indentation,
        bool const add_semicolon,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    struct String_buffer
    {
        std::stringstream string_stream;
        std::uint32_t current_line = 0;
    };

    void add_format_alias_type_declaration(
        String_buffer& buffer,
        Alias_type_declaration const& alias_declaration,
        Format_options const& options
    );

    void add_format_enum_declaration(
        String_buffer& buffer,
        Enum_declaration const& enum_declaration,
        Format_options const& options
    );

    void add_format_global_variable_declaration(
        String_buffer& buffer,
        Global_variable_declaration const& declaration,
        Format_options const& options
    );

    void add_format_struct_declaration(
        String_buffer& buffer,
        Struct_declaration const& struct_declaration,
        std::uint32_t const outside_indentation,
        Format_options const& options
    );

    void add_format_union_declaration(
        String_buffer& buffer,
        Union_declaration const& union_declaration,
        std::uint32_t const outside_indentation,
        Format_options const& options
    );

    void add_format_function_declaration(
        String_buffer& buffer,
        Function_declaration const& function_declaration,
        std::uint32_t const outside_indentation,
        Format_options const& options
    );

    void add_format_function_definition(
        String_buffer& buffer,
        Function_definition const& function_definition,
        std::uint32_t const outside_indentation,
        Format_options const& options
    );

    void add_format_function_parameters(
        String_buffer& buffer,
        std::span<std::pmr::string const> const parameter_names,
        std::span<iris::Type_reference const> const parameter_types,
        std::optional<std::pmr::vector<Source_position>> const parameter_source_positions,
        bool const is_variadic,
        bool const same_line,
        std::uint32_t const indentation,
        Format_options const& options
    );

    void add_format_function_constructor(
        String_buffer& buffer,
        Function_constructor const& function_constructor,
        Format_options const& options
    );

    void add_format_type_constructor(
        String_buffer& buffer,
        Type_constructor const& type_constructor,
        Format_options const& options
    );

    void add_format_expression(
        String_buffer& buffer,
        Statement const& statement,
        Expression const& expression,
        std::uint32_t const indentation,
        Format_options const& options
    );

    void add_format_expression_access(
        String_buffer& buffer,
        Statement const& statement,
        Access_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_access_array(
        String_buffer& buffer,
        Statement const& statement,
        Access_array_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_assert(
        String_buffer& buffer,
        Statement const& statement,
        Assert_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_assignment(
        String_buffer& buffer,
        Statement const& statement,
        Assignment_expression const& expression,
        std::uint32_t const indentation,
        Format_options const& options
    );

    void add_format_expression_binary(
        String_buffer& buffer,
        Statement const& statement,
        Binary_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_block(
        String_buffer& buffer,
        std::span<Statement const> const statements,
        std::uint32_t outside_indentation,
        Format_options const& options
    );

    void add_format_expression_block(
        String_buffer& buffer,
        Statement const& statement,
        Block_expression const& expression,
        std::uint32_t outside_indentation,
        Format_options const& options
    );

    void add_format_expression_break(
        String_buffer& buffer,
        Statement const& statement,
        Break_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_call(
        String_buffer& buffer,
        Statement const& statement,
        Call_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_cast(
        String_buffer& buffer,
        Statement const& statement,
        Cast_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_comment(
        String_buffer& buffer,
        Statement const& statement,
        Comment_expression const& expression,
        std::uint32_t const indentation,
        Format_options const& options
    );

    void add_format_expression_compile_time(
        String_buffer& buffer,
        Statement const& statement,
        Compile_time_expression const& expression,
        std::uint32_t const indentation,
        Format_options const& options
    );

    void add_format_expression_constant(
        String_buffer& buffer,
        Statement const& statement,
        Constant_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_constant_array(
        String_buffer& buffer,
        Statement const& statement,
        Constant_array_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_continue(
        String_buffer& buffer,
        Statement const& statement,
        Continue_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_defer(
        String_buffer& buffer,
        Statement const& statement,
        Defer_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_dereference_and_access(
        String_buffer& buffer,
        Statement const& statement,
        Dereference_and_access_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_for_loop(
        String_buffer& buffer,
        Statement const& statement,
        For_loop_expression const& expression,
        std::uint32_t outside_indentation,
        Format_options const& options
    );

    void add_format_expression_function(
        String_buffer& buffer,
        Function_expression const& expression,
        std::uint32_t outside_indentation,
        Format_options const& options
    );

    void add_format_expression_if(
        String_buffer& buffer,
        Statement const& statement,
        If_expression const& expression,
        std::uint32_t outside_indentation,
        Format_options const& options
    );

    void add_format_expression_instance_call(
        String_buffer& buffer,
        Statement const& statement,
        Instance_call_expression const& expression,
        std::optional<iris::Source_range> const source_range,
        std::uint32_t outside_indentation,
        Format_options const& options
    );

    void add_format_expression_instantiate(
        String_buffer& buffer,
        Statement const& statement,
        Instantiate_expression const& expression,
        std::optional<iris::Source_range> const source_range,
        std::uint32_t outside_indentation,
        Format_options const& options
    );

    void add_format_expression_invalid(
        String_buffer& buffer,
        Statement const& statement,
        Invalid_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_null_pointer(
        String_buffer& buffer,
        Statement const& statement,
        Null_pointer_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_parenthesis(
        String_buffer& buffer,
        Statement const& statement,
        Parenthesis_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_reflection(
        String_buffer& buffer,
        Statement const& statement,
        Reflection_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_return(
        String_buffer& buffer,
        Statement const& statement,
        Return_expression const& expression,
        std::uint32_t const outside_indentation,
        Format_options const& options
    );

    void add_format_expression_struct(
        String_buffer& buffer,
        Struct_expression const& expression,
        std::uint32_t const outside_indentation,
        Format_options const& options
    );

    void add_format_expression_switch(
        String_buffer& buffer,
        Statement const& statement,
        Switch_expression const& expression,
        std::uint32_t outside_indentation,
        Format_options const& options
    );

    void add_format_expression_ternary_condition(
        String_buffer& buffer,
        Statement const& statement,
        Ternary_condition_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_type(
        String_buffer& buffer,
        Statement const& statement,
        Type_expression const& expression,
        Format_options const& options
    );

    export std::string_view unary_operation_symbol_to_string(
        Unary_operation const operation
    );

    void add_format_expression_unary(
        String_buffer& buffer,
        Statement const& statement,
        Unary_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_variable(
        String_buffer& buffer,
        Statement const& statement,
        Variable_expression const& expression,
        Format_options const& options
    );

    void add_format_expression_variable_declaration(
        String_buffer& buffer,
        Statement const& statement,
        Variable_declaration_expression const& expression,
        std::uint32_t outside_indentation,
        Format_options const& options
    );

    void add_format_expression_variable_declaration_with_type(
        String_buffer& buffer,
        Statement const& statement,
        Variable_declaration_with_type_expression const& expression,
        std::uint32_t outside_indentation,
        Format_options const& options
    );

    void add_format_expression_while_loop(
        String_buffer& buffer,
        Statement const& statement,
        While_loop_expression const& expression,
        std::uint32_t outside_indentation,
        Format_options const& options
    );

    export std::string_view binary_operation_symbol_to_string(
        Binary_operation operation
    );

    void add_format_binary_operation_symbol(
        String_buffer& buffer,
        Binary_operation operation
    );

    Expression const& get_expression(
        Statement const& statement,
        Expression_index const expression_index
    );

    void add_format_type_name(
        String_buffer& buffer,
        Type_reference const& type,
        Format_options const& options
    );

    void add_format_type_name(
        String_buffer& buffer,
        std::span<Type_reference const> types,
        Format_options const& options
    );
}
