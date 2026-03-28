module;

#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

export module h.compiler.analysis;

import h.core;
import h.core.declarations;
import h.compiler.diagnostic;

namespace h::compiler
{
    export struct Variable
    {
        std::pmr::string name;
        h::Type_reference type;
        bool is_mutable;
        bool is_compile_time;
        std::optional<h::Source_position> source_position;
    };

    export Variable create_variable(
        std::pmr::string name,
        h::Type_reference type,
        bool is_mutable,
        bool is_compile_time,
        std::optional<h::Source_position> source_position
    );

    export Variable create_variable(
        std::pmr::string name,
        h::Type_reference type,
        bool is_mutable,
        bool is_compile_time,
        std::optional<h::Source_range> source_range
    );

    export using Block_expression_variant = std::variant<
        For_loop_expression const*,
        Switch_expression const*,
        While_loop_expression const*
    >;

    export struct Scope
    {
        std::pmr::vector<Variable> variables;
        std::pmr::vector<Block_expression_variant> blocks;
    };

    export struct Analysis_result
    {
        std::pmr::vector<Diagnostic> diagnostics;
    };

    export struct Analysis_options
    {
        bool validate = true;
    };

    export Analysis_result process_module(
        h::Module& core_module,
        h::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void process_declarations(
        Analysis_result& result,
        h::Module& core_module,
        Module_declarations& declarations,
        Module_definitions& definitions,
        h::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void process_function(
        Analysis_result& result,
        h::Module& core_module,
        h::Function_declaration& function_declaration,
        h::Function_definition& function_definition,
        h::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void process_block(
        Analysis_result& result,
        h::Module& core_module,
        h::Function_declaration const* function_declaration,
        Scope& scope,
        std::span<Statement> const statements,
        h::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void process_statements(
        Analysis_result& result,
        h::Module& core_module,
        h::Function_declaration const* function_declaration,
        Scope& scope,
        std::span<Statement> const statements,
        h::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void process_statement(
        Analysis_result& result,
        h::Module& core_module,
        h::Function_declaration const* function_declaration,
        Scope& scope,
        h::Statement& statement,
        std::optional<h::Type_reference> const& expected_statement_type,
        h::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::optional<h::Statement> process_expression(
        Analysis_result& result,
        h::Module& core_module,
        h::Function_declaration const* function_declaration,
        Scope& scope,
        h::Statement& statement,
        h::Expression& expression,
        std::size_t const expression_index,
        h::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export struct Type_info
    {
        h::Type_reference type = {};
        bool is_mutable = false;
    };

    export Type_info create_type_info(h::Type_reference type, bool is_mutable);
    export std::optional<Type_info> create_type_info(std::optional<h::Type_reference> type, bool is_mutable);

    export std::optional<Type_info> get_expression_type_info(
        h::Module const& core_module,
        h::Function_declaration const* const function_declaration,
        Scope const& scope,
        h::Statement const& statement,
        h::Expression const& expression,
        std::optional<h::Type_reference> const& expected_expression_type,
        h::Declaration_database const& declaration_database
    );

    export std::optional<h::Type_reference> get_expression_type(
        h::Module const& core_module,
        h::Function_declaration const* const function_declaration,
        Scope const& scope,
        h::Statement const& statement,
        std::optional<h::Type_reference> const& expected_statement_type,
        h::Declaration_database const& declaration_database
    );

    export std::optional<h::Type_reference> get_expression_type(
        h::Module const& core_module,
        h::Function_declaration const* const function_declaration,
        Scope const& scope,
        h::Statement const& statement,
        h::Expression const& expression,
        std::optional<h::Type_reference> const& expected_expression_type,
        Declaration_database const& declaration_database
    );

    export struct Deduced_instance_call
    {
        h::Custom_type_reference custom_type_reference;
        h::Function_constructor const& function_constructor;
        std::pmr::vector<Statement> arguments;
    };

    export std::optional<Deduced_instance_call> deduce_instance_call_arguments(
        h::Declaration_database const& declaration_database,
        h::Module const& core_module,
        Scope const& scope,
        h::Statement const& statement,
        h::Call_expression const& call_expression,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<Function_expression const*> get_all_possible_function_expressions(
        Function_constructor const& function_constructor,
        std::size_t const argument_count,
        std::pmr::polymorphic_allocator<> const& allocator
    );

    std::pmr::vector<Statement> create_statements_from_type_references(
        std::span<std::optional<Type_reference> const> const type_references,
        std::pmr::polymorphic_allocator<> const& allocator
    );

    std::optional<h::Type_reference> get_declaration_member_type(
        Declaration const& declaration,
        std::string_view const member_name
    );

    export struct Declaration_member_info
    {
        std::string_view member_name;
        h::Type_reference member_type;
        std::optional<h::Source_position> member_source_position;
    };

    export std::pmr::vector<Declaration_member_info> get_declaration_member_infos(
        Declaration const& declaration,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export void add_import_usage(
        h::Module& core_module,
        std::string_view const alias,
        std::string_view const usage
    );

    export void add_parameters_to_scope(
        Scope& scope,
        std::span<std::pmr::string const> const parameter_names,
        std::span<h::Type_reference const> const parameter_types,
        std::optional<std::pmr::vector<Source_position>> const parameter_source_positions
    );

    export std::optional<Scope> calculate_scope(
        h::Module const& core_module,
        h::Function_declaration const& function_declaration,
        h::Function_definition const& function_definition,
        h::Declaration_database const& declaration_database,
        h::Source_position const& source_position
    );

    export Variable const* find_variable_from_scope(
        Scope const& scope,
        std::string_view const name
    );

    export template <typename Function>
    void visit_statements_using_scope(
        h::Module const& core_module,
        h::Function_declaration const* const function_declaration,
        Scope& scope,
        std::span<h::Statement const> const statements,
        h::Declaration_database const& declaration_database,
        Function&& callback
    )
    {
        std::size_t const initial_variable_count = scope.variables.size();

        for (h::Statement const& statement : statements)
        {
            callback(statement, scope);

            if (!statement.expressions.empty())
            {
                h::Expression const& expression = statement.expressions[0];

                if (std::holds_alternative<h::Block_expression>(expression.data))
                {
                    h::Block_expression const& block_expression = std::get<h::Block_expression>(expression.data);
                    visit_statements_using_scope(core_module, function_declaration, scope, block_expression.statements, declaration_database, callback);
                }
                else if (std::holds_alternative<h::If_expression>(expression.data))
                {
                    h::If_expression const& if_expression = std::get<h::If_expression>(expression.data);

                    for (h::Condition_statement_pair const& pair : if_expression.series)
                    {
                        if (pair.condition.has_value())
                            callback(pair.condition.value(), scope);

                        visit_statements_using_scope(core_module, function_declaration, scope, pair.then_statements, declaration_database, callback);
                    }
                }
                else if (std::holds_alternative<h::For_loop_expression>(expression.data))
                {
                    h::For_loop_expression const& for_loop_expression = std::get<h::For_loop_expression>(expression.data);

                    std::optional<h::Type_reference> const type_reference = get_expression_type(core_module, nullptr, scope, statement, statement.expressions[for_loop_expression.range_begin.expression_index], std::nullopt, declaration_database);
                    if (type_reference.has_value())
                    {
                        scope.variables.push_back(
                            create_variable(for_loop_expression.variable_name, type_reference.value(), true, false, expression.source_range)
                        );
                    }

                    callback(for_loop_expression.range_end, scope);
                    visit_statements_using_scope(core_module, function_declaration, scope, for_loop_expression.then_statements, declaration_database, callback);

                    if (type_reference.has_value())
                        scope.variables.pop_back();
                }
                else if (std::holds_alternative<h::Ternary_condition_expression>(expression.data))
                {
                    h::Ternary_condition_expression const& ternary_condition_expression = std::get<h::Ternary_condition_expression>(expression.data);

                    callback(ternary_condition_expression.then_statement, scope);
                    callback(ternary_condition_expression.else_statement, scope);
                }
                else if (std::holds_alternative<h::Switch_expression>(expression.data))
                {
                    h::Switch_expression const& switch_expression = std::get<h::Switch_expression>(expression.data);

                    for (h::Switch_case_expression_pair const& pair : switch_expression.cases)
                        visit_statements_using_scope(core_module, function_declaration, scope, pair.statements, declaration_database, callback);
                }
                else if (std::holds_alternative<h::Variable_declaration_expression>(expression.data))
                {
                    h::Variable_declaration_expression const& data = std::get<h::Variable_declaration_expression>(expression.data);
                    std::optional<h::Type_reference> const type_reference = get_expression_type(core_module, nullptr, scope, statement, statement.expressions[data.right_hand_side.expression_index], std::nullopt, declaration_database);
                    if (type_reference.has_value())
                        scope.variables.push_back(
                            create_variable(data.name, type_reference.value(), data.is_mutable, false, expression.source_range)
                        );
                }
                else if (std::holds_alternative<h::Variable_declaration_with_type_expression>(expression.data))
                {
                    h::Variable_declaration_with_type_expression const& data = std::get<h::Variable_declaration_with_type_expression>(expression.data);
                    scope.variables.push_back(
                        create_variable(data.name, data.type, data.is_mutable, false, expression.source_range)
                    );
                }
                else if (std::holds_alternative<h::While_loop_expression>(expression.data))
                {
                    h::While_loop_expression const& while_loop_expression = std::get<h::While_loop_expression>(expression.data);

                    visit_statements_using_scope(core_module, function_declaration, scope, while_loop_expression.then_statements, declaration_database, callback);
                }
            }
        }

        if (initial_variable_count < scope.variables.size())
            scope.variables.erase(scope.variables.begin() + initial_variable_count, scope.variables.end());
    }
}
