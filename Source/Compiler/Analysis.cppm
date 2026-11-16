export module iris.compiler.analysis;

import std;

import iris.core;
import iris.core.declarations;
import iris.compiler.diagnostic;

namespace iris::compiler
{
    export struct Variable
    {
        std::pmr::string name;
        iris::Type_reference type;
        bool is_mutable;
        bool is_compile_time;
        std::optional<iris::Source_position> source_position;
    };

    export Variable create_variable(
        std::pmr::string name,
        iris::Type_reference type,
        bool is_mutable,
        bool is_compile_time,
        std::optional<iris::Source_position> source_position
    );

    export Variable create_variable(
        std::pmr::string name,
        iris::Type_reference type,
        bool is_mutable,
        bool is_compile_time,
        std::optional<iris::Source_range> source_range
    );

    export using Block_expression_variant = std::variant<
        For_each_expression const*,
        For_loop_expression const*,
        Switch_expression const*,
        While_loop_expression const*
    >;

    export struct Scope_pipeline
    {
        Pipeline_expression const* pipeline;
        // How many variables were in scope where the pipeline was declared. Its stages read the
        // variables before this index; a later variable of the same name (a lambda parameter) hides
        // the one they mean.
        std::size_t visible_variable_count;
    };

    export struct Scope
    {
        std::pmr::vector<Variable> variables;
        std::pmr::vector<Block_expression_variant> blocks;
        // Pipelines declared so far in the enclosing function. They are compile-time recipes, not
        // variables, so they live in their own list.
        std::pmr::vector<Scope_pipeline> pipelines;
    };

    export void add_pipeline_to_scope(
        Scope& scope,
        Pipeline_expression const& pipeline
    );

    export Scope_pipeline const* find_scope_pipeline(
        Scope const& scope,
        std::string_view const name
    );

    export Pipeline_expression const* find_pipeline_from_scope(
        Scope const& scope,
        std::string_view const name
    );

    // The names that the stages of `pipeline` read from outside it: everything they reference except
    // the pipeline's own parameters and bindings, including what the pipelines they consume read.
    // `enclosing_pipelines` are the pipelines visible where `pipeline` is declared. The result also
    // holds names that are not variables, such as called functions; callers filter by what they need.
    export std::pmr::vector<std::pmr::string> get_pipeline_free_names(
        Pipeline_expression const& pipeline,
        std::span<Scope_pipeline const> const enclosing_pipelines
    );

    // The pipelines that were visible where `scope_pipeline` was declared: those before it in `scope`.
    export std::span<Scope_pipeline const> get_pipelines_visible_at_declaration(
        Scope const& scope,
        Scope_pipeline const& scope_pipeline
    );

    // When `expression` is a call whose callee names a pipeline in scope, returns that pipeline.
    export Pipeline_expression const* get_pipeline_from_call_expression(
        Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& expression
    );

    // The element type that a pipeline reads from an input of type `input_type`
    // (Array_slice::<T> or Constant_array::<T, N>), or nullopt if the type cannot feed a pipeline.
    export std::optional<iris::Type_reference> get_pipeline_input_element_type(
        iris::Declaration_database const& declaration_database,
        iris::Type_reference const& input_type
    );

    // The length of an input known at compile time: N for Constant_array::<T, N>, or the most it
    // can hold for Fixed_capacity_array::<T, N>. This is the capacity of an into_fixed() result.
    export std::optional<std::uint64_t> get_pipeline_input_static_capacity(
        iris::Declaration_database const& declaration_database,
        iris::Type_reference const& input_type
    );

    // The name of the output element: the last map binding or, with no map, the single parameter.
    export std::optional<std::string_view> get_pipeline_output_name(
        Pipeline_expression const& pipeline
    );

    // Adds the pipeline parameters to the scope.
    export void add_pipeline_parameters_to_scope(
        Scope& scope,
        Pipeline_expression const& pipeline
    );

    // Adds the binding of one stage (a map or enumerate name) to the scope, with the type that
    // analysis deduces for it. Does nothing for stages without a binding or whose type cannot be
    // deduced. Later stages see it; the stage itself and earlier ones do not.
    export void add_pipeline_stage_binding_to_scope(
        std::string_view const module_name,
        iris::Function_declaration const* function_declaration,
        Scope& scope,
        iris::Pipeline_stage const& stage,
        iris::Declaration_database const& declaration_database
    );

    // Adds the pipeline parameters to the scope, then each map binding in order, with the types
    // that analysis deduces for them. Bindings whose type cannot be deduced are skipped.
    export void add_pipeline_bindings_to_scope(
        std::string_view const module_name,
        iris::Function_declaration const* function_declaration,
        Scope& scope,
        Pipeline_expression const& pipeline,
        iris::Declaration_database const& declaration_database
    );

    export std::optional<iris::Type_reference> get_pipeline_output_type(
        std::string_view const module_name,
        iris::Function_declaration const* function_declaration,
        Scope const& scope,
        Pipeline_expression const& pipeline,
        iris::Declaration_database const& declaration_database
    );

    // `p(inputs).separate([scratch])` before a sink: the sink runs one loop per stage.
    export struct Pipeline_separate_call
    {
        // The `.separate(...)` call, which lives in the same statement.
        iris::Expression_index call;
        iris::Call_expression const* call_expression;
        // The buffer for elements that do not have the output type.
        std::optional<iris::Expression_index> scratch_argument;
    };

    // A value sink applied to a pipeline call: `p(inputs).<sink_name>(<arguments>)`.
    export struct Pipeline_sink_call
    {
        Pipeline_expression const* pipeline;
        std::string_view sink_name;
        // The object of the access: the pipeline call `p(inputs)`, or the pipeline name `p` when
        // the sink is written on the pipeline itself.
        iris::Expression_index pipeline_call;
        // The sink's own call, `.<sink>(arguments)`, which lives in the same statement.
        iris::Call_expression const* call_expression;
        // True for `p.<sink>(inputs)`, where the inputs are the sink's own arguments (in_place).
        bool is_on_pipeline_name;
        // The expressions that are the pipeline's inputs.
        std::span<iris::Expression_index const> input_arguments;
        // Set for `p(inputs).separate([scratch]).<sink>(...)`. `pipeline_call` is still `p(inputs)`.
        std::optional<Pipeline_separate_call> separate = std::nullopt;
    };

    // One stage of a separated pipeline: the element it leaves for the next stage, and the buffer that
    // element lives in. Elements of the output type live in the output buffer, others in the scratch.
    export struct Separated_pipeline_stage
    {
        std::string_view element_name;
        iris::Type_reference element_type;
        bool is_in_scratch;
    };

    export struct Separated_pipeline_layout
    {
        std::pmr::vector<Separated_pipeline_stage> stages;
        iris::Type_reference output_type;
        // The element type stored in the scratch buffer, when some stage needs it.
        std::optional<iris::Type_reference> scratch_type;
        // True when stages leave elements of more than one type other than the output type, which a
        // single scratch buffer cannot hold.
        bool has_several_scratch_types = false;
    };

    // Nullopt when the pipeline cannot be separated in a linear chain (an enumerate stage, several
    // inputs before the first map) or when a type cannot be deduced.
    export std::optional<Separated_pipeline_layout> get_separated_pipeline_layout(
        std::string_view const module_name,
        iris::Function_declaration const* function_declaration,
        Scope const& scope,
        Pipeline_expression const& pipeline,
        iris::Declaration_database const& declaration_database
    );

    // The names that `statement` reads and does not declare itself.
    export std::pmr::vector<std::pmr::string> get_statement_free_names(
        iris::Statement const& statement
    );

    // Every sink a pipeline can be consumed by, other than the for-each loop and `separate`.
    export std::span<std::string_view const> get_pipeline_sink_names();

    export bool is_pipeline_sink_name(std::string_view const name);

    // When `expression` is a call of the form `p(inputs).<member>(...)` or `p.<member>(...)` and `p` is
    // a pipeline in scope.
    // The member name is not checked here, so that validation can report unknown sinks.
    export std::optional<Pipeline_sink_call> get_pipeline_sink_call(
        Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& expression
    );

    export std::optional<iris::Type_reference> get_pipeline_sink_type(
        std::string_view const module_name,
        iris::Function_declaration const* function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        Pipeline_sink_call const& sink_call,
        iris::Declaration_database const& declaration_database
    );

    // The type that the lambda of `p(inputs).fold(init, lambda (accumulator, element) => ...)` is
    // used as: (type of init, output element type) -> type of init.
    export std::optional<iris::Type_reference> get_pipeline_fold_lambda_type(
        std::string_view const module_name,
        iris::Function_declaration const* function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        Pipeline_sink_call const& sink_call,
        iris::Declaration_database const& declaration_database
    );

    // Adds the variables that the top-level statements of a statement block declare, so that its
    // result can be typed.
    export void add_statement_block_variables_to_scope(
        std::string_view const module_name,
        iris::Function_declaration const* function_declaration,
        Scope& scope,
        std::span<iris::Statement const> const statements,
        iris::Declaration_database const& declaration_database
    );

    export struct Analysis_result
    {
        std::pmr::vector<Diagnostic> diagnostics;
    };

    export struct Analysis_options
    {
        bool validate = true;
    };

    export Analysis_result process_module(
        iris::Module& core_module,
        iris::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void process_declarations(
        Analysis_result& result,
        iris::Module& core_module,
        Module_declarations& declarations,
        Module_definitions& definitions,
        iris::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void process_function(
        Analysis_result& result,
        iris::Module& core_module,
        iris::Function_declaration& function_declaration,
        iris::Function_definition& function_definition,
        iris::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void process_block(
        Analysis_result& result,
        iris::Module& core_module,
        iris::Function_declaration const* function_declaration,
        Scope& scope,
        std::span<Statement> const statements,
        iris::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void process_statements(
        Analysis_result& result,
        iris::Module& core_module,
        iris::Function_declaration const* function_declaration,
        Scope& scope,
        std::span<Statement> const statements,
        iris::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void process_statement(
        Analysis_result& result,
        iris::Module& core_module,
        iris::Function_declaration const* function_declaration,
        Scope& scope,
        iris::Statement& statement,
        std::optional<iris::Type_reference> const& expected_statement_type,
        iris::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::optional<iris::Statement> process_expression(
        Analysis_result& result,
        iris::Module& core_module,
        iris::Function_declaration const* function_declaration,
        Scope& scope,
        iris::Statement& statement,
        iris::Expression& expression,
        std::size_t const expression_index,
        iris::Declaration_database& declaration_database,
        Analysis_options const& options,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export struct Type_info
    {
        iris::Type_reference type = {};
        bool is_mutable = false;
    };

    export Type_info create_type_info(iris::Type_reference type, bool is_mutable);
    export std::optional<Type_info> create_type_info(std::optional<iris::Type_reference> type, bool is_mutable);

    export std::pmr::string get_type_module_name(
        std::string_view const declaration_module_name,
        std::string_view const declaration_name
    );

    export bool is_module_visible_from(
        iris::Declaration_database const& declaration_database,
        std::string_view const current_module_name,
        std::string_view const declaration_module_name
    );

    export std::optional<std::pmr::string> find_implicit_function_module(
        iris::Declaration_database const& declaration_database,
        Declaration const& receiver_declaration,
        std::string_view const member_name
    );

    // `enclosing_pipelines` are the pipelines visible where the lambda is written. When the body
    // consumes one of them, the expansion reads what its stages read, so those names are captured too.
    export void compute_lambda_captures(
        iris::Lambda_expression& lambda_expression,
        std::pmr::vector<std::pmr::string> const& enclosing_names,
        std::span<Scope_pipeline const> const enclosing_pipelines = {}
    );

    // Resolves a type reference to the lambda signature it denotes: either an anonymous
    // Lambda_type written inline, or a named lambda reached through a Custom_type_reference.
    export std::optional<iris::Lambda_type> resolve_lambda_type(
        iris::Declaration_database const& declaration_database,
        std::optional<iris::Type_reference> const& type_reference
    );

    // The signature that a call expression's callee resolves to, whether it is a function,
    // a function pointer, or a lambda-typed value.
    export std::optional<iris::Function_type> get_function_type_to_call(
        std::string_view const module_name,
        Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& expression,
        iris::Declaration_database const& declaration_database
    );

    export std::optional<Type_info> get_expression_type_info(
        std::string_view const module_name,
        iris::Function_declaration const* const function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& expression,
        std::optional<iris::Type_reference> const& expected_expression_type,
        iris::Declaration_database const& declaration_database
    );

    export std::optional<iris::Type_reference> get_expression_type(
        std::string_view const module_name,
        iris::Function_declaration const* const function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        std::optional<iris::Type_reference> const& expected_statement_type,
        iris::Declaration_database const& declaration_database
    );

    export std::optional<iris::Type_reference> get_expression_type(
        std::string_view const module_name,
        iris::Function_declaration const* const function_declaration,
        Scope const& scope,
        iris::Statement const& statement,
        iris::Expression const& expression,
        std::optional<iris::Type_reference> const& expected_expression_type,
        Declaration_database const& declaration_database
    );

    export struct Deduced_instance_call
    {
        iris::Custom_type_reference custom_type_reference;
        iris::Function_constructor const& function_constructor;
        std::pmr::vector<Statement> arguments;
    };

    export std::optional<Deduced_instance_call> deduce_instance_call_arguments(
        iris::Declaration_database const& declaration_database,
        std::string_view const module_name,
        Scope const& scope,
        iris::Statement const& statement,
        iris::Call_expression const& call_expression,
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

    std::optional<iris::Type_reference> get_declaration_member_type(
        Declaration const& declaration,
        std::string_view const member_name
    );

    export struct Declaration_member_info
    {
        std::string_view member_name;
        iris::Type_reference member_type;
        std::optional<iris::Source_position> member_source_position;
    };

    export std::pmr::vector<Declaration_member_info> get_declaration_member_infos(
        Declaration const& declaration,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export void add_parameters_to_scope(
        Scope& scope,
        std::span<std::pmr::string const> const parameter_names,
        std::span<iris::Type_reference const> const parameter_types,
        std::optional<std::pmr::vector<Source_position>> const parameter_source_positions
    );

    export std::optional<Scope> calculate_scope(
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition const& function_definition,
        iris::Declaration_database const& declaration_database,
        iris::Source_position const& source_position
    );

    export Variable const* find_variable_from_scope(
        Scope const& scope,
        std::string_view const name
    );

    export template <typename Function>
    void visit_statements_using_scope(
        std::string_view const module_name,
        iris::Function_declaration const* const function_declaration,
        Scope& scope,
        std::span<iris::Statement const> const statements,
        iris::Declaration_database const& declaration_database,
        Function&& callback
    );

    // Visits the statements and the result of every statement block expression inside `statement`.
    // Statement blocks are not statements of their own, so the visitor reaches them through here.
    export template <typename Function>
    void visit_statement_block_expressions(
        std::string_view const module_name,
        iris::Function_declaration const* const function_declaration,
        Scope& scope,
        iris::Statement const& statement,
        iris::Declaration_database const& declaration_database,
        Function&& callback
    )
    {
        for (std::size_t index = 0; index < statement.expressions.size(); ++index)
        {
            if (!std::holds_alternative<iris::Statement_block_expression>(statement.expressions[index].data))
                continue;

            iris::Statement_block_expression const& block = std::get<iris::Statement_block_expression>(statement.expressions[index].data);

            visit_statements_using_scope(module_name, function_declaration, scope, block.statements, declaration_database, callback);

            Scope result_scope = scope;
            add_statement_block_variables_to_scope(module_name, function_declaration, result_scope, block.statements, declaration_database);
            callback(block.result, result_scope);
            visit_statement_block_expressions(module_name, function_declaration, result_scope, block.result, declaration_database, callback);
        }
    }

    export template <typename Function>
    void visit_statements_using_scope(
        std::string_view const module_name,
        iris::Function_declaration const* const function_declaration,
        Scope& scope,
        std::span<iris::Statement const> const statements,
        iris::Declaration_database const& declaration_database,
        Function&& callback
    )
    {
        std::size_t const initial_variable_count = scope.variables.size();
        std::size_t const initial_pipeline_count = scope.pipelines.size();

        auto const visit_statement = [&](iris::Statement const& statement) -> void
        {
            callback(statement, scope);
            visit_statement_block_expressions(module_name, function_declaration, scope, statement, declaration_database, callback);
        };

        for (iris::Statement const& statement : statements)
        {
            visit_statement(statement);

            if (!statement.expressions.empty())
            {
                iris::Expression const& expression = statement.expressions[0];

                if (std::holds_alternative<iris::Block_expression>(expression.data))
                {
                    iris::Block_expression const& block_expression = std::get<iris::Block_expression>(expression.data);
                    visit_statements_using_scope(module_name, function_declaration, scope, block_expression.statements, declaration_database, callback);
                }
                else if (std::holds_alternative<iris::If_expression>(expression.data))
                {
                    iris::If_expression const& if_expression = std::get<iris::If_expression>(expression.data);

                    for (iris::Condition_statement_pair const& pair : if_expression.series)
                    {
                        if (pair.condition.has_value())
                            visit_statement(pair.condition.value());

                        visit_statements_using_scope(module_name, function_declaration, scope, pair.then_statements, declaration_database, callback);
                    }
                }
                else if (std::holds_alternative<iris::For_loop_expression>(expression.data))
                {
                    iris::For_loop_expression const& for_loop_expression = std::get<iris::For_loop_expression>(expression.data);

                    std::optional<iris::Type_reference> const type_reference = get_expression_type(module_name, nullptr, scope, statement, statement.expressions[for_loop_expression.range_begin.expression_index], std::nullopt, declaration_database);
                    if (type_reference.has_value())
                    {
                        scope.variables.push_back(
                            create_variable(for_loop_expression.variable_name, type_reference.value(), true, false, expression.source_range)
                        );
                    }

                    visit_statement(for_loop_expression.range_end);
                    visit_statements_using_scope(module_name, function_declaration, scope, for_loop_expression.then_statements, declaration_database, callback);

                    if (type_reference.has_value())
                        scope.variables.pop_back();
                }
                else if (std::holds_alternative<iris::For_each_expression>(expression.data))
                {
                    iris::For_each_expression const& for_each_expression = std::get<iris::For_each_expression>(expression.data);

                    std::size_t const variable_count = scope.variables.size();

                    iris::Pipeline_expression const* const pipeline = get_pipeline_from_call_expression(scope, statement, statement.expressions[for_each_expression.range_expression.expression_index]);
                    if (pipeline != nullptr)
                    {
                        std::optional<iris::Type_reference> const type_reference = get_pipeline_output_type(module_name, function_declaration, scope, *pipeline, declaration_database);
                        if (type_reference.has_value())
                        {
                            scope.variables.push_back(
                                for_each_expression.variable_name_source_position.has_value() ?
                                create_variable(for_each_expression.variable_name, type_reference.value(), false, false, for_each_expression.variable_name_source_position) :
                                create_variable(for_each_expression.variable_name, type_reference.value(), false, false, expression.source_range)
                            );
                        }
                    }

                    visit_statements_using_scope(module_name, function_declaration, scope, for_each_expression.then_statements, declaration_database, callback);

                    scope.variables.erase(scope.variables.begin() + variable_count, scope.variables.end());
                }
                else if (std::holds_alternative<iris::Pipeline_expression>(expression.data))
                {
                    iris::Pipeline_expression const& pipeline_expression = std::get<iris::Pipeline_expression>(expression.data);

                    std::size_t const variable_count = scope.variables.size();
                    // Each stage sees the parameters and the bindings of the stages before it.
                    add_pipeline_parameters_to_scope(scope, pipeline_expression);
                    for (iris::Pipeline_stage const& stage : pipeline_expression.stages)
                    {
                        if (!stage.expression.expressions.empty())
                            visit_statement(stage.expression);

                        add_pipeline_stage_binding_to_scope(module_name, function_declaration, scope, stage, declaration_database);
                    }
                    scope.variables.erase(scope.variables.begin() + variable_count, scope.variables.end());

                    add_pipeline_to_scope(scope, pipeline_expression);
                }
                else if (std::holds_alternative<iris::Ternary_condition_expression>(expression.data))
                {
                    iris::Ternary_condition_expression const& ternary_condition_expression = std::get<iris::Ternary_condition_expression>(expression.data);

                    visit_statement(ternary_condition_expression.then_statement);
                    visit_statement(ternary_condition_expression.else_statement);
                }
                else if (std::holds_alternative<iris::Switch_expression>(expression.data))
                {
                    iris::Switch_expression const& switch_expression = std::get<iris::Switch_expression>(expression.data);

                    for (iris::Switch_case_expression_pair const& pair : switch_expression.cases)
                        visit_statements_using_scope(module_name, function_declaration, scope, pair.statements, declaration_database, callback);
                }
                else if (std::holds_alternative<iris::Variable_declaration_expression>(expression.data))
                {
                    iris::Variable_declaration_expression const& data = std::get<iris::Variable_declaration_expression>(expression.data);
                    std::optional<iris::Type_reference> const type_reference = get_expression_type(module_name, nullptr, scope, statement, statement.expressions[data.right_hand_side.expression_index], std::nullopt, declaration_database);
                    if (type_reference.has_value())
                        scope.variables.push_back(
                            create_variable(data.name, type_reference.value(), data.is_mutable, false, expression.source_range)
                        );
                }
                else if (std::holds_alternative<iris::Variable_declaration_with_type_expression>(expression.data))
                {
                    iris::Variable_declaration_with_type_expression const& data = std::get<iris::Variable_declaration_with_type_expression>(expression.data);
                    std::optional<iris::Type_reference> const type_reference = iris::get_variable_declaration_with_type_expression_type(statement, data);
                    if (type_reference.has_value())
                    {
                        scope.variables.push_back(
                            create_variable(data.name, std::move(type_reference.value()), data.is_mutable, false, expression.source_range)
                        );
                    }
                }
                else if (std::holds_alternative<iris::While_loop_expression>(expression.data))
                {
                    iris::While_loop_expression const& while_loop_expression = std::get<iris::While_loop_expression>(expression.data);

                    visit_statement(while_loop_expression.condition);

                    visit_statements_using_scope(module_name, function_declaration, scope, while_loop_expression.then_statements, declaration_database, callback);
                }
            }
        }

        if (initial_variable_count < scope.variables.size())
            scope.variables.erase(scope.variables.begin() + initial_variable_count, scope.variables.end());

        if (initial_pipeline_count < scope.pipelines.size())
            scope.pipelines.erase(scope.pipelines.begin() + initial_pipeline_count, scope.pipelines.end());
    }
}
