export module iris.compiler.validation;

import std;

import iris.compiler.analysis;
import iris.compiler.diagnostic;
import iris.core;
import iris.core.declarations;

namespace iris::compiler
{
    export std::pmr::vector<iris::compiler::Diagnostic> validate_module(
        iris::Module const& core_module,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_imports(
        iris::Module const& core_module,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_type_references(
        iris::Module const& core_module,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_type_reference(
        iris::Module const& core_module,
        iris::Type_reference const& type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_custom_type_reference(
        iris::Module const& core_module,
        iris::Type_reference const& type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_integer_type(
        iris::Module const& core_module,
        iris::Type_reference const& type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<iris::compiler::Diagnostic> validate_declarations(
        iris::Module const& core_module,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<iris::compiler::Diagnostic> validate_enum_declaration(
        iris::Module const& core_module,
        iris::Enum_declaration const& declaration,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<iris::compiler::Diagnostic> validate_global_variable_declaration(
        iris::Module const& core_module,
        iris::Global_variable_declaration const& declaration,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<iris::compiler::Diagnostic> validate_struct_declaration(
        iris::Module const& core_module,
        iris::Struct_declaration const& declaration,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<iris::compiler::Diagnostic> validate_union_declaration(
        iris::Module const& core_module,
        iris::Union_declaration const& declaration,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<iris::compiler::Diagnostic> validate_function(
        iris::Module const& core_module,
        iris::Function_declaration const& declaration,
        iris::Function_definition const* const definition,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_function_contracts(
        iris::Module const& core_module,
        Function_declaration const& function_declaration,
        iris::compiler::Scope const& scope,
        std::span<iris::Function_condition const> const function_conditions,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_statements(
        iris::Module const& core_module,
        Function_declaration const* const function_declaration,
        Scope const& scope,
        std::span<iris::Statement const> const statements,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::vector<iris::compiler::Diagnostic> validate_statement(
        iris::Module const& core_module,
        Function_declaration const* const function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        std::optional<iris::Type_reference> const& expected_statement_type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export struct Validate_expression_parameters
    {
        iris::Module const& core_module;
        iris::Function_declaration const* function_declaration;
        Scope const& scope;
        iris::Statement const& statement;
        std::optional<iris::Type_reference> const& expected_statement_type;
        std::span<std::optional<Type_info> const> expression_types;
        std::size_t expression_index;
        Declaration_database const& declaration_database;
        std::pmr::polymorphic_allocator<> const& temporaries_allocator;
    };

    export std::pmr::vector<iris::compiler::Diagnostic> validate_expression(
        Validate_expression_parameters const& parameters
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_access_expression(
        Validate_expression_parameters const& parameters,
        iris::Access_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_assert_expression(
        Validate_expression_parameters const& parameters,
        iris::Assert_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_binary_operation(
        Validate_expression_parameters const& parameters,
        iris::Expression_index const left_hand_side_index,
        iris::Expression_index const right_hand_side_index,
        iris::Binary_operation const operation,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_assignment_expression(
        Validate_expression_parameters const& parameters,
        iris::Assignment_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_binary_expression(
        Validate_expression_parameters const& parameters,
        iris::Binary_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_block_expression(
        Validate_expression_parameters const& parameters,
        iris::Block_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_break_expression(
        Validate_expression_parameters const& parameters,
        iris::Break_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_call_expression(
        Validate_expression_parameters const& parameters,
        iris::Call_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_cast_expression(
        Validate_expression_parameters const& parameters,
        iris::Cast_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_continue_expression(
        Validate_expression_parameters const& parameters,
        iris::Continue_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_for_loop_expression(
        Validate_expression_parameters const& parameters,
        iris::For_loop_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_if_expression(
        Validate_expression_parameters const& parameters,
        iris::If_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_instantiate_expression(
        Validate_expression_parameters const& parameters,
        iris::Instantiate_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_reflection_expression(
        Validate_expression_parameters const& parameters,
        iris::Reflection_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_return_expression(
        Validate_expression_parameters const& parameters,
        iris::Return_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_switch_expression(
        Validate_expression_parameters const& parameters,
        iris::Switch_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_ternary_condition_expression(
        Validate_expression_parameters const& parameters,
        iris::Ternary_condition_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_unary_expression(
        Validate_expression_parameters const& parameters,
        iris::Unary_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_variable_declaration_expression(
        Validate_expression_parameters const& parameters,
        iris::Variable_declaration_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_variable_declaration_with_type_expression(
        Validate_expression_parameters const& parameters,
        iris::Variable_declaration_with_type_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_variable_expression(
        Validate_expression_parameters const& parameters,
        iris::Variable_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<iris::compiler::Diagnostic> validate_while_loop_expression(
        Validate_expression_parameters const& parameters,
        iris::While_loop_expression const& expression,
        std::optional<iris::Source_range> const& source_range
    );

    std::pmr::vector<std::optional<Type_info>> calculate_expression_type_infos_of_statement(
        std::string_view const module_name,
        iris::Function_declaration const* const function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        std::optional<iris::Type_reference> const expected_statement_type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<std::optional<iris::Type_reference>> calculate_expression_types_of_statement(
        std::string_view const module_name,
        iris::Function_declaration const* const function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        std::optional<iris::Type_reference> const expected_statement_type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::optional<iris::Type_reference> get_expression_type_from_type_info(
        std::span<std::optional<Type_info> const> const type_infos,
        std::uint64_t const expression_index
    );

    std::optional<iris::Type_reference> get_expression_type_from_type_info(
        std::span<std::optional<Type_info> const> const type_infos,
        iris::Expression_index const expression_index
    );

    std::optional<iris::Type_reference> get_expression_type_from_type_info_from_call_arguments(
        std::span<std::optional<Type_info> const> const type_infos,
        std::uint64_t const expression_index,
        bool const is_implicit_first_argument
    );

    bool is_computable_at_compile_time(
        iris::Expression const& expression,
        std::optional<iris::Type_reference> const& expression_type,
        Validate_expression_parameters const& parameters
    );

    bool is_computable_at_compile_time(
        iris::Module const& core_module,
        iris::compiler::Scope const& scope,
        iris::Statement const& statement,
        std::span<std::optional<Type_info> const> const expression_types,
        Declaration_database const& declaration_database
    );

    bool is_computable_at_compile_time(
        iris::Module const& core_module,
        iris::compiler::Scope const& scope,
        iris::Statement const& statement,
        iris::Expression_index const& expression_index,
        std::span<std::optional<Type_info> const> const expression_types,
        Declaration_database const& declaration_database
    );

    bool is_computable_at_compile_time(
        iris::Module const& core_module,
        iris::compiler::Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& expression,
        std::optional<iris::Type_reference> const& expression_type,
        std::span<std::optional<Type_info> const> const expression_types,
        Declaration_database const& declaration_database
    );

    Global_variable_declaration const* get_global_variable(
        std::string_view const current_module_name,
        iris::Expression const& expression,
        Declaration_database const& declaration_database
    );

    bool is_constant_global_variable(
        std::string_view const current_module_name,
        iris::Expression const& expression,
        Declaration_database const& declaration_database
    );

    bool is_mutable_global_variable(
        std::string_view const current_module_name,
        iris::Expression const& expression,
        Declaration_database const& declaration_database
    );

    bool is_macro_global_variable(
        std::string_view const current_module_name,
        iris::Expression const& expression,
        Declaration_database const& declaration_database
    );

    std::optional<iris::Source_range> get_statement_source_range(
        iris::Statement const& statement
    );

    std::optional<iris::Source_range> create_source_range_from_source_location(
        std::optional<iris::Source_location> const& source_location,
        std::uint32_t const count
    );

    std::optional<iris::Source_range> create_source_range_from_source_location(
        std::optional<iris::Source_range_location> const& source_location,
        std::uint32_t const count
    );

    std::optional<iris::Source_range> create_source_range_from_source_position(
        std::optional<iris::Source_position> const& source_position,
        std::uint32_t const count
    );

    struct Implicit_argument
    {
        Expression_index expression;
        bool take_address_of;
    };

    std::optional<Implicit_argument> get_implicit_first_call_argument(
        iris::Statement const& statement,
        iris::Call_expression const& expression,
        Scope const& scope,
        Declaration_database const& declaration_database
    );

    std::pmr::vector<Expression_index> get_call_aguments(
        iris::Call_expression const& expression,
        std::optional<Implicit_argument> const& implicit_first_argument,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );
}
