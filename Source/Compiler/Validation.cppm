export module h.compiler.validation;

import std;

import h.compiler.analysis;
import h.compiler.diagnostic;
import h.core;
import h.core.declarations;

namespace h::compiler
{
    export std::pmr::vector<h::compiler::Diagnostic> validate_module(
        h::Module const& core_module,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_imports(
        h::Module const& core_module,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_type_references(
        h::Module const& core_module,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_type_reference(
        h::Module const& core_module,
        h::Type_reference const& type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_custom_type_reference(
        h::Module const& core_module,
        h::Type_reference const& type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_integer_type(
        h::Module const& core_module,
        h::Type_reference const& type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<h::compiler::Diagnostic> validate_declarations(
        h::Module const& core_module,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<h::compiler::Diagnostic> validate_enum_declaration(
        h::Module const& core_module,
        h::Enum_declaration const& declaration,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<h::compiler::Diagnostic> validate_global_variable_declaration(
        h::Module const& core_module,
        h::Global_variable_declaration const& declaration,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<h::compiler::Diagnostic> validate_struct_declaration(
        h::Module const& core_module,
        h::Struct_declaration const& declaration,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<h::compiler::Diagnostic> validate_union_declaration(
        h::Module const& core_module,
        h::Union_declaration const& declaration,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<h::compiler::Diagnostic> validate_function(
        h::Module const& core_module,
        h::Function_declaration const& declaration,
        h::Function_definition const* const definition,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_function_contracts(
        h::Module const& core_module,
        Function_declaration const& function_declaration,
        h::compiler::Scope const& scope,
        std::span<h::Function_condition const> const function_conditions,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_statements(
        h::Module const& core_module,
        Function_declaration const* const function_declaration,
        Scope const& scope,
        std::span<h::Statement const> const statements,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<h::compiler::Diagnostic> validate_statement(
        h::Module const& core_module,
        Function_declaration const* const function_declaration,
        Scope const& scope,
        h::Statement const& statement,
        std::optional<h::Type_reference> const& expected_statement_type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export struct Validate_expression_parameters
    {
        h::Module const& core_module;
        h::Function_declaration const* function_declaration;
        Scope const& scope;
        h::Statement const& statement;
        std::optional<h::Type_reference> const& expected_statement_type;
        std::span<std::optional<Type_info> const> expression_types;
        std::size_t expression_index;
        Declaration_database const& declaration_database;
        std::pmr::polymorphic_allocator<> const& temporaries_allocator;
    };

    export std::pmr::vector<h::compiler::Diagnostic> validate_expression(
        Validate_expression_parameters const& parameters
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_access_expression(
        Validate_expression_parameters const& parameters,
        h::Access_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_assert_expression(
        Validate_expression_parameters const& parameters,
        h::Assert_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_binary_operation(
        Validate_expression_parameters const& parameters,
        h::Expression_index const left_hand_side_index,
        h::Expression_index const right_hand_side_index,
        h::Binary_operation const operation,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_assignment_expression(
        Validate_expression_parameters const& parameters,
        h::Assignment_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_binary_expression(
        Validate_expression_parameters const& parameters,
        h::Binary_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_block_expression(
        Validate_expression_parameters const& parameters,
        h::Block_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_break_expression(
        Validate_expression_parameters const& parameters,
        h::Break_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_call_expression(
        Validate_expression_parameters const& parameters,
        h::Call_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_cast_expression(
        Validate_expression_parameters const& parameters,
        h::Cast_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_continue_expression(
        Validate_expression_parameters const& parameters,
        h::Continue_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_for_loop_expression(
        Validate_expression_parameters const& parameters,
        h::For_loop_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_if_expression(
        Validate_expression_parameters const& parameters,
        h::If_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_instantiate_expression(
        Validate_expression_parameters const& parameters,
        h::Instantiate_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_reflection_expression(
        Validate_expression_parameters const& parameters,
        h::Reflection_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_return_expression(
        Validate_expression_parameters const& parameters,
        h::Return_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_switch_expression(
        Validate_expression_parameters const& parameters,
        h::Switch_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_ternary_condition_expression(
        Validate_expression_parameters const& parameters,
        h::Ternary_condition_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_unary_expression(
        Validate_expression_parameters const& parameters,
        h::Unary_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_variable_declaration_expression(
        Validate_expression_parameters const& parameters,
        h::Variable_declaration_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_variable_declaration_with_type_expression(
        Validate_expression_parameters const& parameters,
        h::Variable_declaration_with_type_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_variable_expression(
        Validate_expression_parameters const& parameters,
        h::Variable_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<h::compiler::Diagnostic> validate_while_loop_expression(
        Validate_expression_parameters const& parameters,
        h::While_loop_expression const& expression,
        std::optional<h::Source_range> const& source_range
    );

    std::pmr::vector<std::optional<Type_info>> calculate_expression_type_infos_of_statement(
        h::Module const& core_module,
        h::Function_declaration const* const function_declaration,
        Scope const& scope,
        h::Statement const& statement,
        std::optional<h::Type_reference> const expected_statement_type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<std::optional<h::Type_reference>> calculate_expression_types_of_statement(
        h::Module const& core_module,
        h::Function_declaration const* const function_declaration,
        Scope const& scope,
        h::Statement const& statement,
        std::optional<h::Type_reference> const expected_statement_type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::optional<h::Type_reference> get_expression_type_from_type_info(
        std::span<std::optional<Type_info> const> const type_infos,
        std::uint64_t const expression_index
    );

    std::optional<h::Type_reference> get_expression_type_from_type_info(
        std::span<std::optional<Type_info> const> const type_infos,
        h::Expression_index const expression_index
    );

    std::optional<h::Type_reference> get_expression_type_from_type_info_from_call_arguments(
        std::span<std::optional<Type_info> const> const type_infos,
        std::uint64_t const expression_index,
        bool const is_implicit_first_argument
    );

    bool is_computable_at_compile_time(
        h::Expression const& expression,
        std::optional<h::Type_reference> const& expression_type,
        Validate_expression_parameters const& parameters
    );

    bool is_computable_at_compile_time(
        h::Module const& core_module,
        h::compiler::Scope const& scope,
        h::Statement const& statement,
        std::span<std::optional<Type_info> const> const expression_types,
        Declaration_database const& declaration_database
    );

    bool is_computable_at_compile_time(
        h::Module const& core_module,
        h::compiler::Scope const& scope,
        h::Statement const& statement,
        h::Expression_index const& expression_index,
        std::span<std::optional<Type_info> const> const expression_types,
        Declaration_database const& declaration_database
    );

    bool is_computable_at_compile_time(
        h::Module const& core_module,
        h::compiler::Scope const& scope,
        h::Statement const& statement,
        h::Expression const& expression,
        std::optional<h::Type_reference> const& expression_type,
        std::span<std::optional<Type_info> const> const expression_types,
        Declaration_database const& declaration_database
    );

    Global_variable_declaration const* get_global_variable(
        std::string_view const current_module_name,
        h::Expression const& expression,
        Declaration_database const& declaration_database
    );

    bool is_constant_global_variable(
        std::string_view const current_module_name,
        h::Expression const& expression,
        Declaration_database const& declaration_database
    );

    bool is_mutable_global_variable(
        std::string_view const current_module_name,
        h::Expression const& expression,
        Declaration_database const& declaration_database
    );

    bool is_macro_global_variable(
        std::string_view const current_module_name,
        h::Expression const& expression,
        Declaration_database const& declaration_database
    );

    std::optional<h::Source_range> get_statement_source_range(
        h::Statement const& statement
    );

    std::optional<h::Source_range> create_source_range_from_source_location(
        std::optional<h::Source_location> const& source_location,
        std::uint32_t const count
    );

    std::optional<h::Source_range> create_source_range_from_source_location(
        std::optional<h::Source_range_location> const& source_location,
        std::uint32_t const count
    );

    std::optional<h::Source_range> create_source_range_from_source_position(
        std::optional<h::Source_position> const& source_position,
        std::uint32_t const count
    );

    struct Implicit_argument
    {
        Expression_index expression;
        bool take_address_of;
    };

    std::optional<Implicit_argument> get_implicit_first_call_argument(
        h::Statement const& statement,
        h::Call_expression const& expression,
        Scope const& scope,
        Declaration_database const& declaration_database
    );

    std::pmr::vector<Expression_index> get_call_aguments(
        h::Call_expression const& expression,
        std::optional<Implicit_argument> const& implicit_first_argument,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );
}
