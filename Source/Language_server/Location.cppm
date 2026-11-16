module;

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <lsp/types.h>

export module iris.language_server.location;

import iris.compiler.analysis;
import iris.core;
import iris.core.declarations;

namespace iris::language_server
{
    export std::optional<Declaration> find_declaration_that_contains_source_position(
        Declaration_database const& declaration_database,
        std::string_view const& module_name,
        iris::Source_position const& source_position
    );

    export std::optional<iris::Function> find_function_that_contains_source_position(
        iris::Module const& core_module,
        iris::Source_position const& source_position
    );

    export std::optional<iris::Type_reference> find_type_that_contains_source_position(
        iris::Type_reference const& type,
        iris::Source_position const& source_position
    );

    export std::optional<Declaration> find_value_declaration_using_expression(
        Declaration_database const& declaration_database,
        iris::Module const& core_module,
        iris::Statement const& statement,
        iris::Expression const& expression
    );

    export iris::Enum_declaration const* find_enum_declaration_using_expression(
        Declaration_database const& declaration_database,
        iris::Module const& core_module,
        iris::Statement const& statement,
        iris::Expression const& expression
    );

    // Like iris::compiler::visit_statements_using_scope, but it also walks the body of every lambda
    // literal it meets, with the lambda's own parameters added to the scope. The compiler reaches a
    // lambda body through the function the lambda pass generates from it, which does not exist while
    // the file is still being edited, so the language server walks the body where it was written.
    export void visit_statements_using_scope_including_lambda_bodies(
        Declaration_database const& declaration_database,
        iris::Module const& core_module,
        iris::Function_declaration const* const function_declaration,
        iris::compiler::Scope& scope,
        std::span<iris::Statement const> const statements,
        std::function<void(iris::Statement const&, iris::compiler::Scope const&)> const& callback
    );

    export void visit_expressions_that_contain_position(
        Declaration_database const& declaration_database,
        iris::Module const& core_module,
        iris::Source_position const& source_position,
        std::function<bool(iris::Function_declaration const* function_declaration, iris::compiler::Scope const& scope, iris::Statement const& statement, iris::Expression const& expression)> const& visitor
    );

    // The source range of a name written at `position`, or nullopt when the position is unknown.
    export std::optional<iris::Source_range> create_name_source_range(
        std::optional<iris::Source_position> const& position,
        std::string_view const name
    );

    export std::string format_type_for_display(
        iris::Module const& core_module,
        iris::Type_reference const& type
    );

    // When `expression` names a pipeline in scope (`p` in `p(xs)` or `p.in_place(xs)`), returns it.
    // A visible variable of the same name wins, as it does for the compiler.
    export iris::compiler::Scope_pipeline const* find_pipeline_reference(
        iris::compiler::Scope const& scope,
        iris::Expression const& expression
    );

    // A pipeline or sink signature as hover and signature help show it. Every parameter label is a
    // substring of `label`.
    export struct Pipeline_signature
    {
        std::string label;
        std::vector<std::string> parameters;
    };

    // `pipeline p(value: Complex, weight: Float32) => Complex`, where the type after `=>` is the
    // output element type.
    export Pipeline_signature create_pipeline_signature(
        iris::Module const& core_module,
        Declaration_database const& declaration_database,
        iris::Function_declaration const* const function_declaration,
        iris::compiler::Scope const& scope,
        iris::Pipeline_expression const& pipeline
    );

    // `count() -> Uint64`, `into(out: Array_slice::<mutable U>) -> Array_slice::<mutable U>`, ...
    export std::optional<Pipeline_signature> create_pipeline_sink_signature(
        iris::Module const& core_module,
        Declaration_database const& declaration_database,
        iris::Function_declaration const* const function_declaration,
        iris::compiler::Scope const& scope,
        iris::Statement const& statement,
        iris::compiler::Pipeline_sink_call const& sink_call
    );
}
