module;

#include <functional>
#include <optional>
#include <span>
#include <string_view>

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
}
