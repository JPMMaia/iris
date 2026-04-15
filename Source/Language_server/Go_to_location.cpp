module;

#include <filesystem>
#include <optional>
#include <variant>
#include <vector>

#include <lsp/types.h>

module iris.language_server.go_to_location;

import iris.compiler.analysis;
import iris.core;
import iris.core.declarations;
import iris.core.types;
import iris.language_server.core;
import iris.language_server.location;
import iris.parser.parse_tree;

namespace iris::language_server
{
    static lsp::TextDocument_DefinitionResult create_result(
        std::filesystem::path const& target_file_path,
        iris::Source_range const& target_range,
        iris::Source_range const& target_selection_range,
        bool const client_supports_definition_link
    )
    {
        if (client_supports_definition_link)
        {
            std::vector<lsp::DefinitionLink> links;

            links.push_back(
                lsp::LocationLink
                {
                    .targetUri = lsp::DocumentUri::fromPath(target_file_path.generic_string()),
                    .targetRange = to_lsp_range(target_range),
                    .targetSelectionRange = to_lsp_range(target_selection_range),
                    .originSelectionRange = std::nullopt,
                }
            );

            return links;
        }
        else
        {
            std::vector<lsp::Location> locations;

            locations.push_back(
                lsp::Location
                {
                    .uri = lsp::DocumentUri::fromPath(target_file_path.generic_string()),
                    .range =  to_lsp_range(target_range),
                }
            );

            return locations;
        }
    }

    static lsp::TextDocument_DefinitionResult create_result_from_declaration(
        iris::parser::Parse_tree const& parse_tree,
        Declaration const& declaration,
        bool const client_supports_definition_link
    )
    {
        std::string_view const declaration_name = get_declaration_name(declaration);
        std::optional<iris::Source_range_location> const declaration_location = get_declaration_source_location(declaration);
        if (!declaration_location.has_value() || !declaration_location->file_path.has_value())
            return nullptr;
        
        std::filesystem::path const& target_file_path = declaration_location->file_path.value();
        iris::Source_range const target_range = declaration_location->range;
        iris::Source_range const target_selection_range = create_sub_source_range(target_range, 0, declaration_name.size()).value();

        return create_result(
            target_file_path,
            target_range,
            target_selection_range,
            client_supports_definition_link
        );
    }

    static lsp::TextDocument_DefinitionResult create_result_from_type(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Type_reference const& type,
        bool const client_supports_definition_link
    )
    {
        std::optional<Declaration> const& declaration = find_declaration(declaration_database, type);
        if (!declaration.has_value())
            return nullptr;

        return create_result_from_declaration(
            parse_tree,
            declaration.value(),
            client_supports_definition_link
        );
    }

    static lsp::TextDocument_DefinitionResult create_result_from_variable(
        iris::Module const& core_module,
        iris::compiler::Variable const& variable,
        bool const client_supports_definition_link
    )
    {
        if (!core_module.source_file_path.has_value() || !variable.source_position.has_value())
            return nullptr;
        
        std::filesystem::path const& target_file_path = core_module.source_file_path.value();
        iris::Source_range const target_range = create_source_range(
            variable.source_position->line,
            variable.source_position->column,
            variable.source_position->line,
            variable.source_position->column + variable.name.size()
        );
        iris::Source_range const target_selection_range = target_range;

        return create_result(
            target_file_path,
            target_range,
            target_selection_range,
            client_supports_definition_link
        );
    }

    static lsp::TextDocument_DefinitionResult create_result_from_declaration_member(
        Declaration const& declaration,
        std::string_view const member_name,
        std::optional<iris::Source_position> const& member_source_position,
        bool const client_supports_definition_link
    )
    {
        if (!member_source_position.has_value())
            return nullptr;

        std::optional<iris::Source_range_location> const declaration_location = get_declaration_source_location(declaration);
        if (!declaration_location.has_value() || !declaration_location->file_path.has_value())
            return nullptr;

        std::filesystem::path const& target_file_path = declaration_location->file_path.value();
        iris::Source_range const target_range = create_source_range(
            member_source_position->line,
            member_source_position->column,
            member_source_position->line,
            member_source_position->column + member_name.size()
        );
        iris::Source_range const target_selection_range = target_range;    

        return create_result(
            target_file_path,
            target_range,
            target_selection_range,
            client_supports_definition_link
        );
    }

    lsp::TextDocument_DefinitionResult compute_go_to_definition(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        lsp::Position const position,
        bool const client_supports_definition_link
    )
    {
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        iris::Source_position const& source_position = to_source_position(position);

        std::optional<Declaration> const declaration_optional = find_declaration_that_contains_source_position(
            declaration_database,
            core_module.name,
            source_position
        );
        if (declaration_optional.has_value())
        {
            std::optional<lsp::TextDocument_DefinitionResult> result_optional = std::nullopt;

            auto const process_type = [&](iris::Type_reference const& type) -> bool
            {
                std::optional<iris::Type_reference> const inner_type = find_type_that_contains_source_position(
                    type,
                    source_position
                );
                if (inner_type.has_value())
                {
                    result_optional = create_result_from_type(
                        declaration_database,
                        parse_tree,
                        inner_type.value(),
                        client_supports_definition_link
                    );
                    return true;
                }
                
                return false;
            };

            auto const process_declaration = [&](auto const* const declaration) -> bool
            {
                return visit_type_references(
                    *declaration,
                    process_type
                );
            };

            std::visit(process_declaration, declaration_optional->data);
            if (result_optional.has_value())
                return result_optional.value();
        }

        std::optional<iris::Function> const function = find_function_that_contains_source_position(
            core_module,
            source_position
        );
        if (function.has_value())
        {
            std::optional<lsp::TextDocument_DefinitionResult> result_optional = std::nullopt;

            auto const process_type = [&](iris::Type_reference const& type) -> bool
            {
                std::optional<iris::Type_reference> const inner_type = find_type_that_contains_source_position(
                    type,
                    source_position
                );
                if (inner_type.has_value())
                {
                    result_optional = create_result_from_type(
                        declaration_database,
                        parse_tree,
                        inner_type.value(),
                        client_supports_definition_link
                    );
                    return true;
                }
                
                return false;
            };

            visit_type_references(
                function->definition->statements,
                process_type
            );

            if (result_optional.has_value())
                return result_optional.value();

            auto const process_statement = [&](iris::Statement const& statement, iris::compiler::Scope const& scope) -> bool
            {
                auto const process_expression = [&](iris::Expression const& expression, iris::Statement const& statement) -> bool
                {
                    if (!expression.source_range.has_value())
                        return false;

                    if (iris::range_contains_position_inclusive(expression.source_range.value(), source_position))
                    {
                        if (std::holds_alternative<iris::Access_expression>(expression.data))
                        {
                            iris::Access_expression const& access_expression = std::get<iris::Access_expression>(expression.data);

                            iris::Expression const& expression_to_access = statement.expressions[access_expression.expression.expression_index];

                            std::optional<iris::Type_reference> const expression_to_access_type = iris::compiler::get_expression_type(
                                core_module,
                                function->declaration,
                                scope,
                                statement,
                                expression_to_access,
                                std::nullopt,
                                declaration_database
                            );
                            if (expression_to_access_type.has_value())
                            {
                                std::optional<Declaration> const& declaration = find_declaration(declaration_database, expression_to_access_type.value());
                                if (declaration.has_value())
                                {
                                    std::pmr::vector<iris::compiler::Declaration_member_info> const member_infos = iris::compiler::get_declaration_member_infos(
                                        declaration.value(),
                                        temporaries_allocator
                                    );

                                    auto const location = std::find_if(
                                        member_infos.begin(),
                                        member_infos.end(),
                                        [&](iris::compiler::Declaration_member_info const& member_info) -> bool { return member_info.member_name == access_expression.member_name; }
                                    );
                                    if (location != member_infos.end())
                                    {
                                        iris::compiler::Declaration_member_info const& member_info = *location;

                                        result_optional = create_result_from_declaration_member(
                                            declaration.value(),
                                            member_info.member_name,
                                            member_info.member_source_position,
                                            client_supports_definition_link
                                        );
                                        return true;
                                    }
                                }
                            }

                            iris::Enum_declaration const* enum_declaration = find_enum_declaration_using_expression(
                                declaration_database,
                                core_module,
                                statement,
                                expression_to_access
                            );
                            if (enum_declaration != nullptr)
                            {
                                if (range_contains_position_inclusive(expression_to_access.source_range.value(), source_position))
                                {
                                    result_optional = create_result_from_declaration(parse_tree, Declaration{.data = enum_declaration}, client_supports_definition_link);
                                    return true;
                                }

                                auto const location = std::find_if(
                                    enum_declaration->values.begin(),
                                    enum_declaration->values.end(),
                                    [&](iris::Enum_value const& member) -> bool { return member.name == access_expression.member_name; }
                                );
                                if (location != enum_declaration->values.end())
                                {
                                    iris::Enum_value const& member = *location;

                                    if (member.source_location.has_value())
                                    {
                                        result_optional = create_result_from_declaration_member(
                                            Declaration{ .data = enum_declaration },
                                            member.name,
                                            iris::Source_position{member.source_location->line, member.source_location->column},
                                            client_supports_definition_link
                                        );
                                        return true;
                                    }
                                }
                            }

                            std::optional<Declaration> const declaration = find_value_declaration_using_expression(
                                declaration_database,
                                core_module,
                                statement,
                                expression
                            );
                            if (declaration.has_value())
                            {
                                result_optional = create_result_from_declaration(parse_tree, declaration.value(), client_supports_definition_link);
                                return true;
                            }

                            return true;
                        }
                        else if (std::holds_alternative<iris::Instantiate_expression>(expression.data))
                        {
                            iris::Instantiate_expression const& instantiate_expression = std::get<iris::Instantiate_expression>(expression.data);

                            auto const instantiate_member_location = std::find_if(
                                instantiate_expression.members.begin(),
                                instantiate_expression.members.end(),
                                [&](Instantiate_member_value_pair const& member) -> bool {
                                    std::optional<iris::Source_range> const member_name_source_range = create_sub_source_range(
                                        member.source_range,
                                        0,
                                        member.member_name.size()
                                    );
                                    return member_name_source_range.has_value() && range_contains_position_inclusive(member_name_source_range.value(), source_position);
                                }
                            );
                            if (instantiate_member_location != instantiate_expression.members.end())
                            {
                                Instantiate_member_value_pair const& member = *instantiate_member_location;

                                std::optional<iris::Type_reference> const type_to_instantiate = get_expression_type(
                                    core_module,
                                    function->declaration,
                                    scope,
                                    statement,
                                    expression,
                                    std::nullopt,
                                    declaration_database
                                );
                                if (type_to_instantiate.has_value())
                                {
                                    std::optional<Declaration> const& declaration = find_declaration(declaration_database, type_to_instantiate.value());
                                    if (declaration.has_value())
                                    {
                                        std::pmr::vector<iris::compiler::Declaration_member_info> const member_infos = iris::compiler::get_declaration_member_infos(
                                            declaration.value(),
                                            temporaries_allocator
                                        );

                                        auto const member_location = std::find_if(
                                            member_infos.begin(),
                                            member_infos.end(),
                                            [&](iris::compiler::Declaration_member_info const& member_info) -> bool { return member_info.member_name == member.member_name; }
                                        );
                                        if (member_location != member_infos.end() && member_location->member_source_position.has_value())
                                        {
                                            result_optional = create_result_from_declaration_member(
                                                declaration.value(),
                                                member.member_name,
                                                member_location->member_source_position.value(),
                                                client_supports_definition_link
                                            );
                                            return true;
                                        }
                                    }
                                }
                            }
                        }
                        else if (std::holds_alternative<iris::Variable_expression>(expression.data))
                        {
                            iris::Variable_expression const& variable_expression = std::get<iris::Variable_expression>(expression.data);

                            iris::compiler::Variable const* const variable = iris::compiler::find_variable_from_scope(scope, variable_expression.name);
                            if (variable != nullptr)
                            {
                                result_optional = create_result_from_variable(core_module, *variable, client_supports_definition_link);
                                return true;
                            }
                            
                            std::optional<Declaration> const declaration = find_declaration(declaration_database, core_module.name, variable_expression.name);
                            if (declaration.has_value())
                            {
                                result_optional = create_result_from_declaration(parse_tree, declaration.value(), client_supports_definition_link);
                                return true;
                            }
                        }
                    }

                    return false;
                };

                return visit_expressions(
                    statement,
                    process_expression
                );
            };

            iris::compiler::Scope scope = {};

            iris::compiler::add_parameters_to_scope(
                scope,
                function->declaration->input_parameter_names,
                function->declaration->type.input_parameter_types,
                function->declaration->input_parameter_source_positions
            );

            iris::compiler::visit_statements_using_scope(
                core_module,
                function->declaration,
                scope,
                function->definition->statements,
                declaration_database,
                process_statement
            );

            if (result_optional.has_value())
                return result_optional.value();
        }

        return nullptr;
    }
}
