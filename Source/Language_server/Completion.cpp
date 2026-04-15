module;

#include <array>
#include <format>
#include <span>
#include <string>
#include <vector>

#include <lsp/types.h>

module iris.language_server.completion;

import iris.compiler.analysis;
import iris.compiler.artifact;
import iris.core;
import iris.core.declarations;
import iris.core.types;
import iris.language_server.core;
import iris.language_server.location;
import iris.parser.convertor;
import iris.parser.parse_tree;

namespace iris::language_server
{
    static std::optional<std::string> create_sort_string(
        std::string_view const label
    )
    {
        if (label.starts_with("_"))
            return std::format("~{}", label);

        return std::nullopt;
    }

    static lsp::CompletionItem create_completion_item(
        std::string_view const label,
        lsp::CompletionItemKindEnum const kind
    )
    {
        return lsp::CompletionItem
        {
            .label = std::string{label},
            .kind = kind,
            .sortText = create_sort_string(label),
        };
    }

    static std::pmr::vector<std::string_view> get_module_names(
        std::span<iris::Module const* const> const core_modules,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<std::string_view> module_names{output_allocator};
        module_names.reserve(core_modules.size());

        for (iris::Module const* core_module : core_modules)
        {
            if (core_module == nullptr)
                continue;

            module_names.push_back(core_module->name);
        }

        return module_names;
    }

    static void add_builtin_type_items(
        std::vector<lsp::CompletionItem>& items
    )
    {
        static constexpr std::array<std::string_view, 28> builtin_types
        {
            "Any_type",
            "Bool",
            "Byte",
            "C_bool",
            "C_char",
            "C_int",
            "C_long",
            "C_longdouble",
            "C_longlong",
            "C_schar",
            "C_short",
            "C_uchar",
            "C_uint",
            "C_ulong",
            "C_ulonglong",
            "C_ushort",
            "Float16",
            "Float32",
            "Float64",
            "Int16",
            "Int32",
            "Int64",
            "Int8",
            "String",
            "Uint16",
            "Uint32",
            "Uint64",
            "Uint8",
        };

        items.reserve(items.size() + builtin_types.size());

        for (std::string_view const type_name : builtin_types)
        {
            items.push_back(
                create_completion_item(type_name, lsp::CompletionItemKind::Keyword)
            );
        }
    }

    static void add_enum_member_items(
        std::vector<lsp::CompletionItem>& items,
        iris::Enum_declaration const& declaration
    )
    {
        items.reserve(items.size() + declaration.values.size());

        for (iris::Enum_value const& value : declaration.values)
        {
            items.push_back(
                create_completion_item(value.name, lsp::CompletionItemKind::EnumMember)
            );
        }
    }

    static void add_declaration_member_items(
        std::vector<lsp::CompletionItem>& items,
        Declaration const& declaration
    )
    {
        std::pmr::vector<iris::compiler::Declaration_member_info> const member_infos = iris::compiler::get_declaration_member_infos(declaration, {});
        items.reserve(items.size() + member_infos.size());
        
        for (iris::compiler::Declaration_member_info const& member_info : member_infos)
        {
            items.push_back(
                create_completion_item(member_info.member_name, lsp::CompletionItemKind::Field)
            );
        }
    }

    static void add_array_slice_member_items(
        std::vector<lsp::CompletionItem>& items,
        Array_slice_type const& type
    )
    {
        items.push_back(
            create_completion_item("data", lsp::CompletionItemKind::Field)
        );

        items.push_back(
            create_completion_item("length", lsp::CompletionItemKind::Field)
        );
    }

    static void add_scope_variable_items(
        std::vector<lsp::CompletionItem>& items,
        iris::compiler::Scope const& scope
    )
    {
        items.reserve(items.size() + scope.variables.size());

        for (iris::compiler::Variable const& variable : scope.variables)
        {
            items.push_back(
                create_completion_item(variable.name, lsp::CompletionItemKind::Variable)
            );
        }
    }

    static void add_import_module_name_items(
        std::vector<lsp::CompletionItem>& items,
        std::string_view const current_module_name,
        std::span<std::string_view const> const import_module_names
    )
    {
        items.reserve(items.size() + import_module_names.size());

        for (std::string_view const module_name : import_module_names)
        {
            if (module_name == current_module_name)
                continue;

            items.push_back(
                create_completion_item(module_name, lsp::CompletionItemKind::Module)
            );
        }
    }

    static void add_import_alias_items(
        std::vector<lsp::CompletionItem>& items,
        iris::Module const& core_module
    )
    {
        items.reserve(items.size() + core_module.dependencies.alias_imports.size());

        for (Import_module_with_alias const& import_module : core_module.dependencies.alias_imports)
        {
            items.push_back(
                create_completion_item(import_module.alias, lsp::CompletionItemKind::Module)
            );
        }
    }

    static void add_declaration_type_items(
        std::vector<lsp::CompletionItem>& items,
        Declaration_database const& declaration_database,
        std::string_view const module_name
    )
    {
        auto const process_declaration = [&](Declaration const& declaration) -> bool {

            if (std::holds_alternative<iris::Alias_type_declaration const*>(declaration.data))
            {
                iris::Alias_type_declaration const& data = *std::get<iris::Alias_type_declaration const*>(declaration.data);

                items.push_back(
                    create_completion_item(data.name, lsp::CompletionItemKind::TypeParameter)
                );
            }
            else if (std::holds_alternative<iris::Enum_declaration const*>(declaration.data))
            {
                iris::Enum_declaration const& data = *std::get<iris::Enum_declaration const*>(declaration.data);
                
                items.push_back(
                    create_completion_item(data.name, lsp::CompletionItemKind::Enum)
                );
            }
            else if (std::holds_alternative<iris::Struct_declaration const*>(declaration.data))
            {
                iris::Struct_declaration const& data = *std::get<iris::Struct_declaration const*>(declaration.data);

                items.push_back(
                    create_completion_item(data.name, lsp::CompletionItemKind::Struct)
                );
            }
            else if (std::holds_alternative<iris::Union_declaration const*>(declaration.data))
            {
                iris::Union_declaration const& data = *std::get<iris::Union_declaration const*>(declaration.data);

                items.push_back(
                    create_completion_item(data.name, lsp::CompletionItemKind::Struct)
                );
            }
            else if (std::holds_alternative<iris::Type_constructor const*>(declaration.data))
            {
                iris::Type_constructor const& data = *std::get<iris::Type_constructor const*>(declaration.data);

                items.push_back(
                    create_completion_item(data.name, lsp::CompletionItemKind::Constructor)
                );
            }

            return false;
        };

        visit_declarations(
            declaration_database,
            module_name,
            process_declaration
        );
    }

    static void add_declaration_value_items(
        std::vector<lsp::CompletionItem>& items,
        Declaration_database const& declaration_database,
        std::string_view const module_name
    )
    {
        auto const process_declaration = [&](Declaration const& declaration) -> bool {

            if (std::holds_alternative<iris::Enum_declaration const*>(declaration.data))
            {
                iris::Enum_declaration const& data = *std::get<iris::Enum_declaration const*>(declaration.data);
                
                items.push_back(
                    create_completion_item(data.name, lsp::CompletionItemKind::Enum)
                );
            }
            else if (std::holds_alternative<iris::Function_declaration const*>(declaration.data))
            {
                iris::Function_declaration const& data = *std::get<iris::Function_declaration const*>(declaration.data);

                items.push_back(
                    create_completion_item(data.name, lsp::CompletionItemKind::Function)
                );
            }
            else if (std::holds_alternative<iris::Global_variable_declaration const*>(declaration.data))
            {
                iris::Global_variable_declaration const& data = *std::get<iris::Global_variable_declaration const*>(declaration.data);

                items.push_back(
                    create_completion_item(data.name, data.global_type == iris::Global_variable_type::Macro ? lsp::CompletionItemKind::Constant : lsp::CompletionItemKind::Variable)
                );
            }
            else if (std::holds_alternative<iris::Function_constructor const*>(declaration.data))
            {
                iris::Function_constructor const& data = *std::get<iris::Function_constructor const*>(declaration.data);

                items.push_back(
                    create_completion_item(data.name, lsp::CompletionItemKind::Constructor)
                );
            }

            return false;
        };

        visit_declarations(
            declaration_database,
            module_name,
            process_declaration
        );
    }

    static lsp::CompletionList create_type_completion_list(
        Declaration_database const& declaration_database,
        iris::Module const& core_module
    )
    {
        std::vector<lsp::CompletionItem> items = {};
        add_builtin_type_items(items);
        add_import_alias_items(items, core_module);
        add_declaration_type_items(items, declaration_database, core_module.name);

        return lsp::CompletionList
        {
            .isIncomplete = false,
            .items = std::move(items),
            .itemDefaults = std::nullopt,
        };
    }

    static std::optional<lsp::CompletionList> create_module_type_completion_list(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        iris::parser::Parse_node const& node_before
    )
    {
        std::optional<iris::parser::Parse_node> const node_to_access = iris::parser::get_node_previous_sibling(node_before);
        if (!node_to_access.has_value())
            return std::nullopt;

        std::string_view const node_to_access_value = iris::parser::get_node_value(parse_tree, node_to_access.value());
        iris::Import_module_with_alias const* import_module = find_import_module_with_alias(
            core_module,
            node_to_access_value
        );
        if (import_module == nullptr)
            return std::nullopt;
        
        std::vector<lsp::CompletionItem> items = {};
        add_declaration_type_items(items, declaration_database, import_module->module_name);

        return lsp::CompletionList
        {
            .isIncomplete = false,
            .items = std::move(items),
            .itemDefaults = std::nullopt,
        };
    }

    static lsp::CompletionList create_value_completion_list(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        iris::Function_declaration const* const function_declaration,
        iris::Function_definition const* const function_definition,
        iris::parser::Parse_node const& node_before,
        iris::Source_position const& source_position
    )
    {
        std::vector<lsp::CompletionItem> items = {};
        add_import_alias_items(items, core_module);
        add_declaration_value_items(items, declaration_database, core_module.name);

        if (function_declaration != nullptr && function_definition != nullptr)
        {
            iris::Source_range const node_before_source_range = iris::parser::get_node_source_range(node_before);
            std::string_view const node_before_value = get_node_value(parse_tree, node_before);
            iris::Source_position const scope_source_position = 
                node_before_value.ends_with(";") ?
                iris::Source_position{ .line = node_before_source_range.end.line, .column = node_before_source_range.end.column - 1 } :
                node_before_source_range.end;

            std::optional<iris::compiler::Scope> const scope = iris::compiler::calculate_scope(
                core_module,
                *function_declaration,
                *function_definition,
                declaration_database,
                scope_source_position
            );

            if (scope.has_value())
                add_scope_variable_items(items, scope.value());
        }

        return lsp::CompletionList
        {
            .isIncomplete = false,
            .items = std::move(items),
            .itemDefaults = std::nullopt,
        };
    }

    static lsp::CompletionList create_declaration_member_completion_list(
        Declaration const& declaration
    )
    {
        std::vector<lsp::CompletionItem> items = {};

        add_declaration_member_items(
            items,
            declaration
        );

        return lsp::CompletionList
        {
            .isIncomplete = false,
            .items = std::move(items),
            .itemDefaults = std::nullopt,
        };
    }

    static iris::Source_position get_access_operator_source_position(
        std::string_view const operator_value,
        iris::Source_position const source_position_after_dot
    )
    {
        if (source_position_after_dot.column > 1)
        {
            if (source_position_after_dot.column > 1)
            {
                return iris::Source_position
                {
                    .line = source_position_after_dot.line,
                    .column = source_position_after_dot.column - static_cast<std::uint32_t>(operator_value.size())
                };
            }
        }

        return source_position_after_dot;
    }

    static std::optional<std::string_view> find_access_operator_before_source_position(
        iris::parser::Parse_tree const& parse_tree,
        iris::parser::Parse_node const& hint_node,
        iris::Source_position const source_position
    )
    {
        std::uint32_t const end_byte = iris::parser::calculate_byte(
            parse_tree,
            hint_node,
            source_position
        );

        if (end_byte == 0)
            return std::nullopt;

        std::uint32_t current_byte = end_byte;
        while (current_byte > 0)
        {
            current_byte -= 1;

            char8_t const character = parse_tree.text[current_byte];
            if (character == ' ' || character == '\t' || character == '\n' || character == '\r')
                continue;

            if (character == '.')
                return std::string_view{"."};

            if (character == '>' && current_byte > 0 && parse_tree.text[current_byte - 1] == '-')
                return std::string_view{"->"};

            return std::nullopt;
        }

        return std::nullopt;
    }

    static std::optional<lsp::CompletionList> create_access_value_completion_list_for_expression_type(
        Declaration_database const& declaration_database,
        iris::Type_reference const& expression_type
    )
    {
        if (is_array_slice_type_reference(expression_type))
        {
            std::vector<lsp::CompletionItem> items = {};    
            add_array_slice_member_items(items, std::get<Array_slice_type>(expression_type.data));

            return lsp::CompletionList
            {
                .isIncomplete = false,
                .items = std::move(items),
                .itemDefaults = std::nullopt,
            };
        }

        if (is_soa_array_type_reference(expression_type))
        {
            std::vector<lsp::CompletionItem> items = {};
            items.push_back(create_completion_item("data", lsp::CompletionItemKind::Field));
            items.push_back(create_completion_item("length", lsp::CompletionItemKind::Field));
            items.push_back(create_completion_item("view", lsp::CompletionItemKind::Method));

            return lsp::CompletionList
            {
                .isIncomplete = false,
                .items = std::move(items),
                .itemDefaults = std::nullopt,
            };
        }

        if (is_soa_array_view_type_reference(expression_type))
        {
            std::vector<lsp::CompletionItem> items = {};
            items.push_back(create_completion_item("data", lsp::CompletionItemKind::Field));
            items.push_back(create_completion_item("length", lsp::CompletionItemKind::Field));
            items.push_back(create_completion_item("start_index", lsp::CompletionItemKind::Field));
            items.push_back(create_completion_item("end_index", lsp::CompletionItemKind::Field));

            return lsp::CompletionList
            {
                .isIncomplete = false,
                .items = std::move(items),
                .itemDefaults = std::nullopt,
            };
        }

        std::optional<Declaration> const underlying_declaration_optional = find_underlying_declaration(
            declaration_database,
            expression_type
        );
        if (underlying_declaration_optional.has_value())
        {
            std::vector<lsp::CompletionItem> items = {};
            add_declaration_member_items(items, underlying_declaration_optional.value());

            return lsp::CompletionList
            {
                .isIncomplete = false,
                .items = std::move(items),
                .itemDefaults = std::nullopt,
            };
        }

        return std::nullopt;
    }

    static std::optional<lsp::CompletionList> create_access_value_completion_list(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        iris::Function_declaration const* const function_declaration,
        iris::Function_definition const* const function_definition,
        iris::parser::Parse_node const& hint_node,
        std::string_view const access_operator,
        iris::Source_position const source_position
    )
    {
        std::optional<iris::parser::Parse_node> const node_to_access = iris::parser::find_node_before_source_position(
            parse_tree,
            hint_node,
            get_access_operator_source_position(access_operator, source_position)
        );
        if (!node_to_access.has_value())
            return std::nullopt;

        std::string_view const node_to_access_value = iris::parser::get_node_value(parse_tree, node_to_access.value());
        iris::Import_module_with_alias const* import_module = find_import_module_with_alias(
            core_module,
            node_to_access_value
        );
        if (import_module != nullptr)
        {
            std::vector<lsp::CompletionItem> items = {};
            add_declaration_value_items(items, declaration_database, import_module->module_name);

            return lsp::CompletionList
            {
                .isIncomplete = false,
                .items = std::move(items),
                .itemDefaults = std::nullopt,
            };
        }

        iris::Statement access_statement;
        iris::parser::node_to_expression(
            access_statement,
            iris::parser::create_module_info(core_module),
            parse_tree,
            node_to_access.value(),
            {},
            {}
        );

        if (access_statement.expressions.empty())
            return std::nullopt;

        iris::Expression const& first_expression = access_statement.expressions[0];

        iris::Enum_declaration const* const enum_declaration = find_enum_declaration_using_expression(
            declaration_database,
            core_module,
            access_statement,
            first_expression
        );
        if (enum_declaration != nullptr)
        {
            std::vector<lsp::CompletionItem> items = {};
            add_enum_member_items(items, *enum_declaration);

            return lsp::CompletionList
            {
                .isIncomplete = false,
                .items = std::move(items),
                .itemDefaults = std::nullopt,
            };
        }

        if (function_declaration != nullptr && function_definition != nullptr)
        {
            iris::Source_position const scope_source_position = get_access_operator_source_position(
                access_operator,
                source_position
            );

            std::optional<iris::compiler::Scope> const scope = iris::compiler::calculate_scope(
                core_module,
                *function_declaration,
                *function_definition,
                declaration_database,
                scope_source_position
            );

            if (scope.has_value())
            {
                std::optional<iris::Type_reference> const expression_type = iris::compiler::get_expression_type(
                    core_module,
                    function_declaration,
                    scope.value(),
                    access_statement,
                    std::nullopt,
                    declaration_database
                );
                if (expression_type.has_value())
                {
                    bool const is_dereference_and_access = access_operator == "->";
                    if (is_dereference_and_access)
                    {
                        if (is_soa_array_type_reference(expression_type.value()))
                        {
                            iris::Soa_array_type const& soa_type = std::get<iris::Soa_array_type>(expression_type.value().data);
                            if (!soa_type.value_type.empty())
                            {
                                std::optional<Declaration> const element_declaration = find_underlying_declaration(
                                    declaration_database,
                                    soa_type.value_type.front()
                                );
                                if (element_declaration.has_value())
                                {
                                    std::vector<lsp::CompletionItem> items = {};
                                    add_declaration_member_items(items, element_declaration.value());

                                    return lsp::CompletionList
                                    {
                                        .isIncomplete = false,
                                        .items = std::move(items),
                                        .itemDefaults = std::nullopt,
                                    };
                                }
                            }
                            return std::nullopt;
                        }

                        if (is_soa_array_view_type_reference(expression_type.value()))
                        {
                            iris::Soa_array_view_type const& soa_view_type = std::get<iris::Soa_array_view_type>(expression_type.value().data);
                            if (!soa_view_type.value_type.empty())
                            {
                                std::optional<Declaration> const element_declaration = find_underlying_declaration(
                                    declaration_database,
                                    soa_view_type.value_type.front()
                                );
                                if (element_declaration.has_value())
                                {
                                    std::vector<lsp::CompletionItem> items = {};
                                    add_declaration_member_items(items, element_declaration.value());

                                    return lsp::CompletionList
                                    {
                                        .isIncomplete = false,
                                        .items = std::move(items),
                                        .itemDefaults = std::nullopt,
                                    };
                                }
                            }
                            return std::nullopt;
                        }

                        std::optional<iris::Type_reference> const value_type = remove_pointer(expression_type.value());
                        if (value_type.has_value())
                        {
                            return create_access_value_completion_list_for_expression_type(declaration_database, value_type.value());
                        }
                    }

                    return create_access_value_completion_list_for_expression_type(declaration_database, expression_type.value());
                }
            }
        }
        
        return std::nullopt;
    }

    static bool expects_type(
        iris::parser::Parse_tree const& parse_tree,
        iris::parser::Parse_node const& node_before
    )
    {
        std::string_view const node_before_value = iris::parser::get_node_value(parse_tree, node_before);

        if (node_before_value == "as")
        {
            return true;
        }
        else if (node_before_value == ":")
        {
            std::optional<iris::parser::Parse_node> const var_node = iris::parser::get_node_previous_sibling(node_before, 2);
            if (var_node.has_value())
            {
                std::string_view const var_value = iris::parser::get_node_value(parse_tree, var_node.value());
                if (var_value == "var" || var_value == "mutable")
                    return true;
            }
        }

        return false;
    }

    static bool expects_access_type(
        iris::parser::Parse_tree const& parse_tree,
        iris::parser::Parse_node const& node_before
    )
    {
        std::string_view const node_before_value = iris::parser::get_node_value(parse_tree, node_before);

        if (node_before_value == ".")
        {
            std::optional<iris::parser::Parse_node> const previous_sibling_node = iris::parser::get_node_previous_sibling(node_before, 2);
            if (previous_sibling_node.has_value())
            {
                std::string_view const previous_sibling_value = iris::parser::get_node_value(parse_tree, previous_sibling_node.value());
                if (previous_sibling_value == "as")
                {
                    return true;
                }
                else if (previous_sibling_value == ":")
                {
                    std::optional<iris::parser::Parse_node> const var_node = iris::parser::get_node_previous_sibling(previous_sibling_node.value(), 2);
                    if (var_node.has_value())
                    {
                        std::string_view const var_value = iris::parser::get_node_value(parse_tree, var_node.value());
                        if (var_value == "var" || var_value == "mutable")
                            return true;
                    }
                }
            }
        }

        return false;
    }

    static bool is_at_import_module_name(std::string_view const node_before_value)
    {
        return node_before_value == "import";
    }

    static bool is_at_instantiate_member(
        iris::parser::Parse_node const smallest_node,
        std::string_view const smallest_node_symbol,
        std::string_view const previous_sibling_symbol
    )
    {
        std::optional<iris::parser::Parse_node> const parent_node = iris::parser::get_parent_node(smallest_node);
        if (parent_node.has_value())
        {
            std::string_view const parent_node_symbol = iris::parser::get_node_symbol(parent_node.value());
            if (parent_node_symbol == "Expression_instantiate_members" && (previous_sibling_symbol == "{" || previous_sibling_symbol == ","))
                return true;    
        }

        return false;
    }

    lsp::TextDocument_CompletionResult compute_completion(
        std::span<iris::compiler::Artifact const> const artifacts,
        std::span<iris::Module const> const header_modules,
        std::span<iris::Module const> const core_modules,
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        lsp::Position const position
    )
    {
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        iris::Source_position const source_position = to_source_position(position);
        iris::parser::Parse_node const root_node = iris::parser::get_root_node(parse_tree);

        iris::parser::Parse_node const smallest_node = iris::parser::get_smallest_node_that_contains_position(
            root_node,
            source_position
        );
        std::string_view const smallest_node_symbol = iris::parser::get_node_symbol(smallest_node);
        iris::Source_range const smallest_node_range = iris::parser::get_node_source_range(smallest_node);

        std::optional<iris::parser::Parse_node> const previous_sibling = iris::parser::get_node_previous_sibling(smallest_node);
        std::string_view const previous_sibling_symbol = previous_sibling.has_value() ? iris::parser::get_node_symbol(previous_sibling.value()) : std::string_view{""};
        std::string_view const previous_sibling_value = previous_sibling.has_value() ? iris::parser::get_node_value(parse_tree, previous_sibling.value()) : std::string_view{""};
        iris::Source_range const previous_sibling_range = previous_sibling.has_value() ? iris::parser::get_node_source_range(previous_sibling.value()) : smallest_node_range;

        std::optional<iris::parser::Parse_node> const node_before = iris::parser::find_node_before_source_position(parse_tree, smallest_node, source_position);
        std::string_view const node_before_value = node_before.has_value() ? iris::parser::get_node_value(parse_tree, node_before.value()) : std::string_view{""};
        std::optional<std::string_view> access_operator =
            (node_before_value == "." || node_before_value == "->")
            ? std::optional<std::string_view>{node_before_value}
            : find_access_operator_before_source_position(parse_tree, root_node, source_position);

        if (is_at_import_module_name(node_before_value))
        {
            if (core_module.source_file_path.has_value())
            {
                std::optional<std::size_t> const artifact_index = iris::compiler::find_artifact_index_that_includes_source_file(
                    artifacts,
                    core_module.source_file_path.value(),
                    temporaries_allocator
                );
                if (artifact_index.has_value())
                {
                    iris::compiler::Artifact const& artifact = artifacts[artifact_index.value()];

                    std::pmr::vector<iris::Module const*> const artifact_modules_and_dependencies = iris::compiler::get_artifact_modules_and_dependencies(
                        artifact,
                        artifacts,
                        header_modules,
                        core_modules,
                        temporaries_allocator,
                        temporaries_allocator
                    );

                    std::pmr::vector<std::string_view> const import_module_names = get_module_names(artifact_modules_and_dependencies, temporaries_allocator);

                    std::vector<lsp::CompletionItem> items = {};
                    add_import_module_name_items(items, core_module.name, import_module_names);

                    return lsp::CompletionList
                    {
                        .isIncomplete = false,
                        .items = std::move(items),
                        .itemDefaults = std::nullopt,
                    };
                }
            }

            return lsp::CompletionList
            {
                .isIncomplete = false,
                .items = {},
                .itemDefaults = std::nullopt,
            };
        }

        if (is_at_instantiate_member(smallest_node, smallest_node_symbol, previous_sibling_symbol))
        {
            std::optional<lsp::CompletionList> completion_list = std::nullopt;

            auto const process_expression = [&](iris::Function_declaration const* const function_declaration, iris::compiler::Scope const& scope, iris::Statement const& statement, iris::Expression const& expression) -> bool
            {
                std::optional<iris::Type_reference> const expression_type = iris::compiler::get_expression_type(
                    core_module,
                    function_declaration,
                    scope,
                    statement,
                    expression,
                    std::nullopt,
                    declaration_database
                );
                if (expression_type.has_value())
                {
                    std::optional<Declaration> const declaration = find_underlying_declaration(
                        declaration_database,
                        expression_type.value()
                    );
                    if (declaration.has_value())
                    {
                        completion_list = create_declaration_member_completion_list(
                            declaration.value()
                        );
                    }

                    return true;
                }

                return false;
            };

            visit_expressions_that_contain_position(
                declaration_database,
                core_module,
                source_position,
                process_expression
            );

            if (completion_list.has_value())
                return completion_list.value();
        }

        std::optional<Declaration> const declaration_optional = find_declaration_that_contains_source_position(
            declaration_database,
            core_module.name,
            source_position
        );

        if (declaration_optional.has_value())
        {
            Declaration const& declaration = declaration_optional.value();

            if (std::holds_alternative<iris::Alias_type_declaration const*>(declaration.data))
            {
                if (node_before_value == "=")
                {
                    return create_type_completion_list(declaration_database, core_module);
                }
                else if (node_before_value == ".")
                {
                    std::optional<lsp::CompletionList> module_type_completion_list = create_module_type_completion_list(
                        declaration_database,
                        parse_tree,
                        core_module,
                        node_before.value()
                    );
                    if (module_type_completion_list.has_value())
                        return module_type_completion_list.value();
                }
                
                return lsp::CompletionList
                {
                    .isIncomplete = false,
                    .items = {},
                    .itemDefaults = std::nullopt,
                };
            }
            else if (std::holds_alternative<iris::Function_declaration const*>(declaration.data))
            {
                iris::Function_declaration const& function_declaration = *std::get<iris::Function_declaration const*>(declaration.data);

                bool const is_access_expression = previous_sibling_value.ends_with(".");
                bool const is_function_type_parameter =
                    (previous_sibling_value.ends_with(":") || is_access_expression) &&
                    (smallest_node_symbol == "," || smallest_node_symbol == ")");
                if (is_function_type_parameter)
                {
                    if (is_access_expression)
                    {
                        std::optional<lsp::CompletionList> module_type_completion_list = create_module_type_completion_list(
                            declaration_database,
                            parse_tree,
                            core_module,
                            node_before.value()
                        );
                        if (module_type_completion_list.has_value())
                        {
                            return module_type_completion_list.value();
                        }
                        else
                        {
                            return lsp::CompletionList
                            {
                                .isIncomplete = false,
                                .items = {},
                                .itemDefaults = std::nullopt,
                            };
                        }
                    }
                    else
                    {
                        return create_type_completion_list(declaration_database, core_module);
                    }
                }
            }
            else if (std::holds_alternative<iris::Struct_declaration const*>(declaration.data) || std::holds_alternative<iris::Union_declaration const*>(declaration.data))
            {
                if (node_before_value == ":")
                {
                    return create_type_completion_list(declaration_database, core_module);
                }
                else if (node_before_value == ".")
                {
                    std::optional<lsp::CompletionList> module_type_completion_list = create_module_type_completion_list(
                        declaration_database,
                        parse_tree,
                        core_module,
                        node_before.value()
                    );
                    if (module_type_completion_list.has_value())
                        return module_type_completion_list.value();
                }
            }
        }

        std::optional<iris::Function> const function = find_function_that_contains_source_position(
            core_module,
            source_position
        );

        if (function.has_value())
        {
            std::vector<lsp::CompletionItem> items = {};

            if (node_before.has_value() && expects_type(parse_tree, node_before.value()))
            {
                return create_type_completion_list(declaration_database, core_module);
            }
            else if (access_operator.has_value())
            {
                if (access_operator.value() == "." && node_before.has_value() && expects_access_type(parse_tree, node_before.value()))
                {
                    std::optional<lsp::CompletionList> module_type_completion_list = create_module_type_completion_list(
                        declaration_database,
                        parse_tree,
                        core_module,
                        node_before.value()
                    );
                    if (module_type_completion_list.has_value())
                        return module_type_completion_list.value();
                }
                else
                {
                    iris::parser::Parse_node const& access_hint_node = root_node;
                    std::optional<lsp::CompletionList> module_value_completion_list = create_access_value_completion_list(
                        declaration_database,
                        parse_tree,
                        core_module,
                        function->declaration,
                        function->definition,
                        access_hint_node,
                        access_operator.value(),
                        source_position
                    );
                    if (module_value_completion_list.has_value())
                        return module_value_completion_list.value();
                }
            }
            else if (!node_before.has_value())
            {
                return lsp::CompletionList
                {
                    .isIncomplete = false,
                    .items = {},
                    .itemDefaults = std::nullopt,
                };
            }
            else
            {
                return create_value_completion_list(
                    declaration_database,
                    parse_tree,
                    core_module,
                    function->declaration,
                    function->definition,
                    node_before.value(),
                    source_position
                );
            }
        }

        return lsp::CompletionList
        {
            .isIncomplete = false,
            .items = {},
            .itemDefaults = std::nullopt,
        };
    }
}
