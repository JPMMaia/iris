module;

#include <cctype>
#include <filesystem>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

module h.parser.convertor;

import h.common;
import h.core;
import h.core.types;
import h.parser.parse_tree;
import h.parser.parser;
import h.parser.type_name_parser;

namespace h::parser
{
    Module_info create_module_info(
        h::Module const& core_module
    )
    {
        return
        {
            .module_name = core_module.name,
            .source_file_path = core_module.source_file_path,
            .alias_imports = core_module.dependencies.alias_imports,
        };
    }

    std::pmr::string create_string(
        std::string_view const value,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        return std::pmr::string{value, output_allocator};
    }

    Source_location source_position_to_source_location(
        std::optional<std::filesystem::path> const& source_file_path,
        Source_position const& source_position
    )
    {
        return
        {
            .file_path = source_file_path,
            .line = source_position.line,
            .column = source_position.column,
        };
    }

    Source_range_location source_range_to_source_location(
        std::optional<std::filesystem::path> const& source_file_path,
        Source_range const& source_range
    )
    {
        return
        {
            .file_path = source_file_path,
            .range = source_range,
        };
    }

    Source_range_location get_declaration_source_range(
        std::optional<std::filesystem::path> const& source_file_path,
        Parse_node const declaration_node,
        std::optional<Parse_node> const& name_node
    )
    {
        h::Source_range const range = get_node_source_range(declaration_node);
        h::Source_position const start =
            name_node.has_value() ?
            get_node_start_source_position(name_node.value()) :
            range.start;

        return
        {
            .file_path = source_file_path,
            .range = {
                .start = start,
                .end = range.end,
            },
        };
    }

    std::string_view get_number_suffix(
        std::string_view const value
    )
    {
        std::size_t const start = value.starts_with("0x") ? 2 : 0;

        for (std::size_t index = start; index < value.size(); ++index)
        {
            char const character = value[index];

            if (std::isalpha(character) && character != '.')
                return value.substr(index);
        }

        return "";
    }

    std::string_view get_string_content(
        std::string_view const value
    )
    {
        if (value.size() < 3)
            return value;

        return value.substr(1, value.size() - 2);
    }

    std::string_view get_string_suffix(
        std::string_view const value
    )
    {
        auto const location = value.rfind("\"");
        return value.substr(location + 1);
    }

    std::uint64_t parse_uint64(
        std::string_view const value
    )
    {
        constexpr std::size_t maximum_size = 21;
        char buffer[maximum_size];

        std::size_t size = std::min(maximum_size - 1, value.size());

        for (std::size_t index = 0; index < size; ++index)
            buffer[index] = value[index];
        
        buffer[size] = '\0';

        char* end = nullptr;
        unsigned long long number = std::strtoull(buffer, &end, 10);
        return static_cast<std::uint64_t>(number);
    }

    std::pmr::string encode_comment(
        std::string_view const value,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::string buffer{temporaries_allocator};

        std::size_t index = 0;
        while (index < value.size())
        {
            std::size_t const start_index = value.find("//", index);
            std::size_t const end_index = value.find("\n", index + 2);

            std::size_t content_end_index = end_index;
            if (content_end_index == std::string_view::npos)
                content_end_index = value.size();
            if (value[content_end_index - 1] == '\r')
                content_end_index -= 1;

            std::size_t const content_start_index = start_index + 2;
            
            std::string_view const content = value.substr(content_start_index, content_end_index - content_start_index);
            if (index > 0)
                buffer += "\n";
            buffer += content;

            if (end_index == std::string_view::npos)
                break;

            index = end_index + 1;
        }

        return std::pmr::string{std::move(buffer), output_allocator};
    }

    static std::optional<std::pmr::string> extract_comments_from_node(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::string_view const comment = get_node_value(tree, node);
        if (comment.empty())
            return std::nullopt;
        
        std::pmr::string const encoded_comment = encode_comment(comment, output_allocator, temporaries_allocator);
        
        return encoded_comment;
    }

    static std::optional<std::string_view> get_module_name(
        Parse_tree const& tree,
        Parse_node const& node
    )
    {
        std::optional<Parse_node> const module_head = get_child_node(tree, node, 0);
        if (!module_head.has_value())
            return std::nullopt;

        std::optional<Parse_node> const module_declaration = get_child_node(tree, module_head.value(), 0);
        if (!module_declaration.has_value())
            return std::nullopt;

        std::optional<Parse_node> const module_name = get_child_node(tree, module_declaration.value(), "Module_name");
        if (!module_name.has_value())
            return std::nullopt;

        return get_node_value(tree, module_name.value());
    }

    template<typename Parameter_t>
    void replace_custom_type_reference_by_parameter_type(
        Module_info const& module_info,
        std::span<Statement const> const statements,
        std::span<Parameter_t const> const parameters
    )
    {
        auto const replace_by_parameter_type = [&](h::Type_reference const& type_reference) -> bool
        {
            if (std::holds_alternative<h::Custom_type_reference>(type_reference.data))
            {
                h::Custom_type_reference const& value = std::get<h::Custom_type_reference>(type_reference.data);
                if (value.module_reference.name == module_info.module_name)
                {
                    auto const location = std::find_if(
                        parameters.begin(),
                        parameters.end(),
                        [&value](Parameter_t const& parameter) -> bool
                        {
                            return parameter.name == value.name;
                        }
                    );
                    if (location != parameters.end())
                    {
                        h::Type_reference* mutable_type_reference = const_cast<h::Type_reference*>(&type_reference);
                        mutable_type_reference->data = h::Parameter_type{ .name = value.name };
                    }
                }
            }

            return false;
        };

        h::visit_type_references_recursively(statements, replace_by_parameter_type);
    }

    std::optional<h::Module> parse_and_convert_to_module(
        std::string_view const source,
        std::optional<std::filesystem::path> const& source_file_path,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::u8string_view const utf_8_source{reinterpret_cast<char8_t const*>(source.data()), source.size()};

        Parser parser = create_parser();
        Parse_tree tree = parse(parser, std::pmr::u8string{utf_8_source, temporaries_allocator});
        
        Parse_node const root = get_root_node(tree);

        std::optional<h::Module> const converted_module = parse_node_to_module(
            tree,
            root,
            source_file_path,
            output_allocator,
            temporaries_allocator
        );

        destroy_tree(std::move(tree));
        destroy_parser(std::move(parser));

        return converted_module;
    }

    std::optional<h::Module> parse_and_convert_to_module(
        std::filesystem::path const& source_file_path,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::optional<std::pmr::string> const file_contents = h::common::get_file_contents(source_file_path);
        if (!file_contents.has_value())
            return std::nullopt;
        
        std::string_view const source = file_contents.value();

        return parse_and_convert_to_module(source, source_file_path, output_allocator, temporaries_allocator);
    }

    std::optional<h::Module> parse_node_to_module(
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::filesystem::path> const& source_file_path,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::optional<Parse_node> const module_head_node = get_child_node(tree, node, 0);
        if (!module_head_node.has_value())
            return std::nullopt;

        std::optional<Parse_node> const module_declaration_node = get_child_node(tree, module_head_node.value(), 0);
        if (!module_declaration_node.has_value())
            return std::nullopt;

        std::optional<std::string_view> const module_name = get_module_name(tree, node);
        if (!module_name.has_value())
            return std::nullopt;

        h::Module output = {};
        output.name = module_name.value();
        output.source_file_path = source_file_path;

        std::optional<Parse_node> const comment_node = get_child_node(tree, module_declaration_node.value(), "Comment");
        if (comment_node.has_value())
        {
            output.comment = extract_comments_from_node(tree, comment_node.value(), output_allocator, temporaries_allocator);
        }

        output.dependencies.alias_imports = create_import_modules(
            tree,
            module_head_node,
            output_allocator,
            temporaries_allocator
        );

        Module_info const module_info = create_module_info(output);

        std::pmr::vector<Parse_node> const child_nodes = get_child_nodes(tree, node, temporaries_allocator);
        for (std::size_t child_index = 1; child_index < child_nodes.size(); ++child_index)
        {
            Parse_node const declaration_node = child_nodes[child_index];
            node_to_declaration(output, module_info, tree, declaration_node, output_allocator, temporaries_allocator);
        }

        return output;
    }

    std::pmr::vector<Import_module_with_alias> create_import_modules(
        Parse_tree const& tree,
        std::optional<Parse_node> const& module_head_node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<Import_module_with_alias> output{output_allocator};
        
        if (!module_head_node.has_value())
            return output;

        std::pmr::vector<Parse_node> const child_nodes = get_child_nodes(tree, module_head_node.value(), temporaries_allocator);
        if (child_nodes.size() <= 1)
            return output;

        output.reserve(child_nodes.size() - 1);

        for (std::size_t child_index = 1; child_index < child_nodes.size(); ++child_index)
        {
            Parse_node const child_node = child_nodes[child_index];

            std::optional<Import_module_with_alias> import_alias = node_to_import_module_with_alias(
                tree,
                child_node,
                output_allocator
            );

            if (import_alias.has_value())
                output.push_back(std::move(import_alias.value()));
        }

        return output;
    }

    std::optional<Import_module_with_alias> node_to_import_module_with_alias(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::optional<Parse_node> const import_name_node = get_child_node(tree, node, "Import_name");
        if (!import_name_node.has_value())
            return std::nullopt;

        std::optional<Parse_node> const import_alias_node = get_child_node(tree, node, "Import_alias");
        if (!import_alias_node.has_value())
            return std::nullopt;

        std::string_view const import_name = get_node_value(tree, import_name_node.value());
        std::string_view const import_alias = get_node_value(tree, import_alias_node.value());

        Source_range const source_range = get_node_source_range(node);

        return Import_module_with_alias
        {
            .module_name = create_string(import_name, output_allocator),
            .alias = create_string(import_alias, output_allocator),
            .usages = {},
            .source_range = source_range,
        };
    }

    enum class Declaration_type
    {
        Alias,
        Enum,
        Global_variable,
        Struct,
        Union,
        Function,
        Type_constructor,
        Function_constructor
    };

    static bool is_export_declaration(Parse_tree const& tree, Parse_node const& node)
    {
        std::optional<Parse_node> const export_node = get_child_node(tree, node, "export");
        return export_node.has_value();
    }

    struct Declaration_attributes
    {
        std::optional<std::string_view> unique_name = std::nullopt;
        bool is_test = false;
    };

    Declaration_attributes get_declaration_attributes(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        Declaration_attributes attributes = {};

        std::pmr::vector<Parse_node> const attribute_nodes = get_child_nodes(tree, node, "Declaration_attribute", temporaries_allocator);
        for (Parse_node const& attribute_node : attribute_nodes)
        {
            std::optional<Parse_node> const name_node = get_child_node(tree, attribute_node, 0);
            if (!name_node.has_value())
                continue;

            std::pmr::vector<Parse_node> const argument_nodes = get_child_nodes(tree, attribute_node, "Generic_expression", temporaries_allocator);

            std::string_view const name = get_node_value(tree, name_node.value());
            if (name == "@unique_name")
            {
                if (argument_nodes.size() == 1)
                {
                    Parse_node const unique_name_node = argument_nodes[0];

                    attributes.unique_name = get_string_content(get_node_value(tree, unique_name_node));
                }
            }
            else if (name == "@test")
            {
                attributes.is_test = true;
            }
        }

        return attributes;
    }

    void node_to_declaration(
        h::Module& core_module,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::optional<Parse_node> const comment_node = get_child_node(tree, node, "Comment");
        std::optional<std::pmr::string> comment = std::nullopt;
        if (comment_node.has_value())
        {
            comment = extract_comments_from_node(tree, comment_node.value(), temporaries_allocator, temporaries_allocator);
        }

        Declaration_attributes const declaration_attributes = get_declaration_attributes(tree, node, temporaries_allocator);

        bool const is_export = is_export_declaration(tree, node);

        std::optional<Parse_node> const declaration_value_node = get_last_child_node(tree, node);
        if (!declaration_value_node.has_value())
            return;

        std::string_view const declaration_type = get_node_symbol(declaration_value_node.value());

        if (declaration_type == "Alias")
        {
            Alias_type_declaration declaration = node_to_alias_type_declaration(module_info, tree, declaration_value_node.value(), declaration_attributes.unique_name, comment, output_allocator, temporaries_allocator);
            
            if (is_export)
                core_module.export_declarations.alias_type_declarations.push_back(std::move(declaration));
            else
                core_module.internal_declarations.alias_type_declarations.push_back(std::move(declaration));
        }
        else if (declaration_type == "Enum")
        {
            Enum_declaration declaration = node_to_enum_declaration(module_info, tree, declaration_value_node.value(), declaration_attributes.unique_name, comment, output_allocator, temporaries_allocator);

            if (is_export)
                core_module.export_declarations.enum_declarations.push_back(std::move(declaration));
            else
                core_module.internal_declarations.enum_declarations.push_back(std::move(declaration));
        }
        else if (declaration_type == "Function")
        {
            std::optional<Parse_node> function_declaration_node = get_child_node(tree, declaration_value_node.value(), 0);
            if (!function_declaration_node.has_value())
                return;

            Function_declaration function_declaration = node_to_function_declaration(
                module_info,
                tree,
                function_declaration_node.value(),
                is_export ? h::Linkage::External : h::Linkage::Private,
                declaration_attributes.unique_name,
                declaration_attributes.is_test,
                comment,
                output_allocator,
                temporaries_allocator
            );

            std::optional<Parse_node> function_definition_node = get_child_node(tree, declaration_value_node.value(), 1);
            if (function_definition_node.has_value() && get_node_symbol(*function_definition_node) == "Function_definition")
            {
                Function_definition function_definition = node_to_function_definition(
                    module_info,
                    tree,
                    function_definition_node.value(),
                    function_declaration.name,
                    output_allocator,
                    temporaries_allocator
                );
    
                core_module.definitions.function_definitions.push_back(std::move(function_definition));
            }

            if (is_export)
                core_module.export_declarations.function_declarations.push_back(std::move(function_declaration));
            else
                core_module.internal_declarations.function_declarations.push_back(std::move(function_declaration));
        }
        else if (declaration_type == "Function_constructor")
        {
            Function_constructor declaration = node_to_function_constructor_declaration(module_info, tree, declaration_value_node.value(), comment, output_allocator, temporaries_allocator);

            if (is_export)
                core_module.export_declarations.function_constructors.push_back(std::move(declaration));
            else
                core_module.internal_declarations.function_constructors.push_back(std::move(declaration));
        }
        else if (declaration_type == "Global_variable")
        {
            Global_variable_declaration declaration = node_to_global_variable_declaration(module_info, tree, declaration_value_node.value(), declaration_attributes.unique_name, comment, output_allocator, temporaries_allocator);

            if (is_export)
                core_module.export_declarations.global_variable_declarations.push_back(std::move(declaration));
            else
                core_module.internal_declarations.global_variable_declarations.push_back(std::move(declaration));
        }
        else if (declaration_type == "Struct")
        {
            Struct_declaration declaration = node_to_struct_declaration(module_info, tree, declaration_value_node.value(), declaration_attributes.unique_name, comment, output_allocator, temporaries_allocator);

            if (is_export)
                core_module.export_declarations.struct_declarations.push_back(std::move(declaration));
            else
                core_module.internal_declarations.struct_declarations.push_back(std::move(declaration));
        }
        else if (declaration_type == "Type_constructor")
        {
            Type_constructor declaration = node_to_type_constructor_declaration(module_info, tree, declaration_value_node.value(), comment, output_allocator, temporaries_allocator);

            if (is_export)
                core_module.export_declarations.type_constructors.push_back(std::move(declaration));
            else
                core_module.internal_declarations.type_constructors.push_back(std::move(declaration));
        }
        else if (declaration_type == "Union")
        {
            Union_declaration declaration = node_to_union_declaration(module_info, tree, declaration_value_node.value(), declaration_attributes.unique_name, comment, output_allocator, temporaries_allocator);

            if (is_export)
                core_module.export_declarations.union_declarations.push_back(std::move(declaration));
            else
                core_module.internal_declarations.union_declarations.push_back(std::move(declaration));
        }
    }

    std::pmr::vector<h::Type_reference> parameter_nodes_to_type_references(
        Module_info const& module_info,
        Parse_tree const& tree,
        std::span<Parse_node const> const nodes,
        bool const is_variadic,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<h::Type_reference> output{output_allocator};

        if (nodes.empty())
            return output;

        std::size_t const count = 
            is_variadic ?
            nodes.size() - 1 :
            nodes.size();

        if (count == 0)
            return output;

        output.resize(count, h::Type_reference{});

        for (std::size_t index = 0; index < count; ++index)
        {
            Parse_node const& parameter_node = nodes[index];
            
            std::optional<Parse_node> const parameter_type_node = get_child_node(tree, parameter_node, 2);
            if (parameter_type_node.has_value())
            {
                std::optional<Parse_node> const type_node = get_child_node(tree, parameter_type_node.value(), 0);
                if (type_node.has_value())
                {
                    std::optional<h::Type_reference> type = node_to_type_reference(module_info, tree, type_node.value(), output_allocator, temporaries_allocator);
                    if (type.has_value())
                        output[index] = std::move(type.value());
                }
            }
        }

        return output;
    }

    std::pmr::vector<std::pmr::string> parameter_nodes_to_names(
        Parse_tree const& tree,
        std::span<Parse_node const> const nodes,
        bool const is_variadic,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<std::pmr::string> output{output_allocator};

        if (nodes.empty())
            return output;

        std::size_t const count = 
            is_variadic ?
            nodes.size() - 1 :
            nodes.size();

        if (count == 0)
            return output;

        output.resize(count, std::pmr::string{output_allocator});

        for (std::size_t index = 0; index < count; ++index)
        {
            Parse_node const& parameter_node = nodes[index];
            
            std::optional<Parse_node> const name_node = get_child_node(tree, parameter_node, 0);
            if (name_node.has_value())
            {
                output[index] = create_string(get_node_value(tree, name_node.value()), output_allocator);
            }
        }

        return output;
    }

    std::optional<h::Type_reference> node_to_type_reference(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& child,
        std::string_view const type_choice,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Source_range const source_range = get_node_source_range(child);

        if (type_choice == "Type_name")
        {
            std::optional<Parse_node> const type_name_node = get_child_node(tree, child, 0);
            if (!type_name_node.has_value())
                return {};

            std::string_view const type_name = get_node_value(tree, type_name_node.value());

            if (type_name == "Void")
                return {};

            if (type_name == "Type")
                return create_builtin_type_reference(create_string("Type", output_allocator));

            std::optional<h::Type_reference> type_reference = parse_type_name(
                module_info.module_name,
                type_name,
                output_allocator
            );
            if (!type_reference.has_value())
                return std::nullopt;
            
            type_reference->source_range = source_range;
            return type_reference;
        }
        else if (type_choice == "Module_type")
        {
            h::Custom_type_reference type;

            std::optional<Parse_node> const alias_name_node = get_child_node(tree, child, "Module_type_module_name");
            if (alias_name_node.has_value())
            {
                std::string_view const alias_name = get_node_value(tree, alias_name_node.value());

                auto const is_alias = [&](Import_module_with_alias const& import_module) -> bool
                {
                    return import_module.alias == alias_name;
                };

                auto const location = std::find_if(
                    module_info.alias_imports.begin(),
                    module_info.alias_imports.end(),
                    is_alias
                );

                if (location != module_info.alias_imports.end())
                {
                    Import_module_with_alias const& import_module = *location;
                    type.module_reference.name = create_string(import_module.module_name, output_allocator);
                }
            }

            std::optional<Parse_node> const type_name_node = get_child_node(tree, child, "Module_type_type_name");
            if (type_name_node.has_value())
            {
                type.name = create_string(get_node_value(tree, type_name_node.value()), output_allocator);
            }

            return h::Type_reference{ .data = std::move(type), .source_range = source_range };
        }
        else if (type_choice == "Pointer_type")
        {
            std::optional<Parse_node> const mutable_node = get_child_node(tree, child, "mutable");
            bool const is_mutable = mutable_node.has_value();
            
            h::Pointer_type output =
            {
                .element_type = std::pmr::vector<h::Type_reference>{output_allocator},
                .is_mutable = is_mutable,
            };

            std::optional<Parse_node> const type_node = get_child_node(tree, child, "Type");
            if (type_node.has_value())
            {
                std::optional<h::Type_reference> type = node_to_type_reference(
                    module_info,
                    tree,
                    type_node.value(),
                    output_allocator,
                    temporaries_allocator
                );

                if (type.has_value())
                {
                    output.element_type.emplace_back(std::move(type.value()));
                }
            }

            return h::Type_reference{ .data = std::move(output), .source_range = source_range };
        }
        else if (type_choice == "Array_slice_type")
        {
            h::Array_slice_type output = {};

            std::optional<Parse_node> const mutable_node = get_child_node(tree, child, "mutable");
            output.is_mutable = mutable_node.has_value();

            std::optional<Parse_node> const element_type_node = get_child_node(tree, child, "Type");
            if (element_type_node.has_value())
            {
                std::optional<h::Type_reference> element_type = node_to_type_reference(
                    module_info,
                    tree,
                    element_type_node.value(),
                    output_allocator,
                    temporaries_allocator
                );

                if (element_type.has_value())
                {
                    output.element_type = std::pmr::vector<h::Type_reference>{output_allocator};
                    output.element_type.emplace_back(std::move(element_type.value()));
                }
            }

            return h::Type_reference{ .data = std::move(output), .source_range = source_range };
        }
        else if (type_choice == "Constant_array_type")
        {
            h::Constant_array_type output = {};

            std::optional<Parse_node> const value_type_node = get_child_node(tree, child, 2);
            if (value_type_node.has_value())
            {
                std::optional<h::Type_reference> value_type = node_to_type_reference(
                    module_info,
                    tree,
                    value_type_node.value(),
                    output_allocator,
                    temporaries_allocator
                );

                if (value_type.has_value())
                {
                    output.value_type = std::pmr::vector<h::Type_reference>{output_allocator};
                    output.value_type.emplace_back(std::move(value_type.value()));
                }
            }

            std::optional<Parse_node> const length_node = get_child_node(tree, child, 4);
            if (length_node.has_value())
            {
                std::string_view const length_string = get_node_value(tree, length_node.value());
                std::uint64_t const length = parse_uint64(length_string);
                output.size = length;
            }

            return h::Type_reference{ .data = std::move(output), .source_range = source_range };
        }
        else if (type_choice == "Function_pointer_type")
        {
            std::pmr::vector<Parse_node> const input_parameter_nodes = get_child_nodes_of_parent(tree, child, "Function_pointer_type_input_parameters", "Function_parameter", temporaries_allocator);
            bool const variadic = is_variadic(tree, input_parameter_nodes);

            std::pmr::vector<Parse_node> const output_parameter_nodes = get_child_nodes_of_parent(tree, child, "Function_pointer_type_output_parameters", "Function_parameter", temporaries_allocator);

            Function_pointer_type output = {};
            output.type.is_variadic = variadic;

            output.type.input_parameter_types = parameter_nodes_to_type_references(
                module_info,
                tree,
                input_parameter_nodes,
                variadic,
                output_allocator,
                temporaries_allocator
            );
            output.input_parameter_names = parameter_nodes_to_names(
                tree,
                input_parameter_nodes,
                variadic,
                output_allocator,
                temporaries_allocator
            );

            output.type.output_parameter_types = parameter_nodes_to_type_references(
                module_info,
                tree,
                output_parameter_nodes,
                false,
                output_allocator,
                temporaries_allocator
            );
            output.output_parameter_names = parameter_nodes_to_names(
                tree,
                output_parameter_nodes,
                false,
                output_allocator,
                temporaries_allocator
            );

            return h::Type_reference{ .data = std::move(output), .source_range = source_range };
        }
        else if (type_choice == "Type_instance_type")
        {
            h::Type_instance output = {};

            std::optional<Parse_node> const left_hand_side_node = get_child_node(tree, child, 0);
            if (left_hand_side_node.has_value())
            {
                std::string_view const left_hand_side_type_choice = get_node_symbol(left_hand_side_node.value());

                std::optional<h::Type_reference> left_hand_side = node_to_type_reference(
                    module_info,
                    tree,
                    left_hand_side_node.value(),
                    left_hand_side_type_choice,
                    output_allocator,
                    temporaries_allocator
                );

                if (left_hand_side.has_value() && std::holds_alternative<h::Custom_type_reference>(left_hand_side.value().data))
                {
                    output.type_constructor = std::move(std::get<h::Custom_type_reference>(left_hand_side.value().data));
                }
            }
            
            std::pmr::vector<Parse_node> const argument_nodes = get_child_nodes_of_parent(tree, child, "Type_instance_type_parameters", "Expression_instance_call_parameter", temporaries_allocator);
            output.arguments = node_to_block(module_info, tree, argument_nodes, output_allocator, temporaries_allocator);
            
            return h::Type_reference{ .data = std::move(output), .source_range = source_range };
        }

        return std::nullopt;
    }

    std::optional<h::Type_reference> node_to_type_reference(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {   
        std::optional<Parse_node> const child = get_child_node(tree, node, 0);
        if (!child.has_value())
            return {};

        std::string_view const type_choice = get_node_symbol(child.value());

        return node_to_type_reference(
            module_info,
            tree,
            child.value(),
            type_choice,
            output_allocator,
            temporaries_allocator
        );
    }

    h::Alias_type_declaration node_to_alias_type_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::string_view> const unique_name,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Alias_type_declaration output;
        
        std::optional<Parse_node> const name_node = get_child_node(tree, node, 1);
        if (name_node.has_value())
        {
            output.name = create_string(get_node_value(tree, name_node.value()), output_allocator);
        }
        
        output.source_location = get_declaration_source_range(module_info.source_file_path, node, name_node);

        if (unique_name.has_value())
            output.unique_name = create_string(unique_name.value(), output_allocator);

        if (comment.has_value())
        {
            output.comment = create_string(comment.value(), output_allocator);
        }

        std::optional<Parse_node> const alias_type_node = get_child_node(tree, node, 3);
        if (alias_type_node.has_value())
        {
            std::optional<Parse_node> const type_node = get_child_node(tree, alias_type_node.value(), 0);
            if (type_node.has_value())
            {
                std::optional<h::Type_reference> type = node_to_type_reference(module_info, tree, type_node.value(), output_allocator, temporaries_allocator);
                if (type.has_value())
                {
                    output.type = std::pmr::vector<h::Type_reference>{{std::move(type.value())}, output_allocator};
                }
            }
        }

        return output;
    }

    h::Enum_declaration node_to_enum_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::string_view> const unique_name,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Enum_declaration output;
        
        std::optional<Parse_node> const name_node = get_child_node(tree, node, 1);
        if (name_node.has_value())
        {
            output.name = create_string(get_node_value(tree, name_node.value()), output_allocator);
        }

        output.source_location = get_declaration_source_range(module_info.source_file_path, node, name_node);

        if (unique_name.has_value())
            output.unique_name = create_string(unique_name.value(), output_allocator);

        if (comment.has_value())
        {
            output.comment = create_string(comment.value(), output_allocator);
        }

        std::optional<Parse_node> const values_node = get_child_node(tree, node, "Enum_values");
        if (values_node.has_value())
        {
            std::pmr::vector<Parse_node> const value_nodes = get_child_nodes(tree, values_node.value(), temporaries_allocator);

            output.values.resize(value_nodes.size() - 2, h::Enum_value{});

            for (std::size_t index = 2; index < value_nodes.size(); ++index)
            {
                Parse_node const& value_node = value_nodes[index - 1];
                
                h::Enum_value enum_value{};
                
                std::optional<Parse_node> const value_name_node = get_child_node(tree, value_node, "Enum_value_name");
                if (value_name_node.has_value())
                {
                    enum_value.name = create_string(get_node_value(tree, value_name_node.value()), output_allocator);
                    enum_value.source_location = source_position_to_source_location(module_info.source_file_path, get_node_start_source_position(value_name_node.value()));
                }

                std::optional<Parse_node> const value_value_node = get_child_node(tree, value_node, "Generic_expression");
                if (value_value_node.has_value())
                {
                    enum_value.value = node_to_statement(module_info, tree, value_value_node.value(), output_allocator, temporaries_allocator);
                }

                std::optional<Parse_node> const comment_node = get_child_node(tree, value_node, "Comment");
                if (comment_node.has_value())
                {
                    std::optional<std::pmr::string> comment = extract_comments_from_node(
                        tree,
                        comment_node.value(),
                        output_allocator,
                        temporaries_allocator
                    );
        
                    if (comment.has_value())
                    {
                        enum_value.comment = comment.value();
                    }
                }

                output.values[index - 2] = std::move(enum_value);
            }
        }

        return output;
    }

    h::Global_variable_declaration node_to_global_variable_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::string_view> const unique_name,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Global_variable_declaration output = {};

        std::optional<Parse_node> const global_type_node = get_child_node(tree, node, 0);
        if (global_type_node.has_value())
        {
            std::string_view const type_value = get_node_value(tree, global_type_node.value());
            if (type_value == "mutable")
                output.global_type = h::Global_variable_type::Mutable;
            else if (type_value == "macro")
                output.global_type = h::Global_variable_type::Macro;
            else
                output.global_type = h::Global_variable_type::Constant;
        }
        
        std::optional<Parse_node> const name_node = get_child_node(tree, node, 1);
        if (name_node.has_value())
        {
            output.name = create_string(get_node_value(tree, name_node.value()), output_allocator);
        }
        
        output.source_location = get_declaration_source_range(module_info.source_file_path, node, name_node);

        if (unique_name.has_value())
            output.unique_name = create_string(unique_name.value(), output_allocator);

        if (comment.has_value())
        {
            output.comment = create_string(comment.value(), output_allocator);
        }

        std::optional<Parse_node> const type_node = get_child_node(tree, node, "Type");
        if (type_node.has_value())
        {
            output.type = node_to_type_reference(module_info, tree, type_node.value(), output_allocator, temporaries_allocator);
        }

        std::optional<Parse_node> const initial_value_node = get_child_node(tree, node, "Generic_expression_or_instantiate");
        if (initial_value_node.has_value())
        {
            output.initial_value = node_to_statement(module_info, tree, initial_value_node.value(), output_allocator, temporaries_allocator);
        }

        return output;
    }

    h::Function_condition node_to_function_condition(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Function_condition output{};
        
        std::optional<Parse_node> const name_node = get_child_node(tree, node, 1);
        if (name_node.has_value())
        {
            std::string_view const string = get_node_value(tree, name_node.value());
            if (string.size() > 2)
            {
                std::string_view const content = string.substr(1, string.size() - 2);
                output.description = create_string(content, output_allocator);
            }
        }

        std::optional<Parse_node> const condition_node = get_child_node(tree, node, 3);
        if (condition_node.has_value())
        {
            output.condition = node_to_statement(module_info, tree, condition_node.value(), output_allocator, temporaries_allocator);
        }

        output.source_range = get_node_source_range(node);

        return output;
    }

    static void add_function_parameters(
        std::pmr::vector<std::pmr::string>& parameter_names,
        std::pmr::vector<Type_reference>& parameter_types,
        std::pmr::vector<Source_position>& parameter_source_positions,
        Module_info const& module_info,
        Parse_tree const& tree,
        std::span<Parse_node const> const parameter_nodes,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        for (Parse_node const& parameter_node : parameter_nodes)
        {   
            std::optional<Parse_node> const parameter_name_node = get_child_node(tree, parameter_node, "Function_parameter_name");
            std::pmr::string parameter_name = 
                parameter_name_node.has_value() ?
                create_string(get_node_value(tree, parameter_name_node.value()), output_allocator) :
                create_string("", output_allocator);
            
            parameter_names.push_back(std::move(parameter_name));

            std::optional<Parse_node> const parameter_type_node = get_child_node(tree, parameter_node, "Function_parameter_type");
            std::optional<Parse_node> const type_node = parameter_type_node.has_value() ? get_child_node(tree, parameter_type_node.value(), 0) : std::nullopt;
            std::optional<Type_reference> parameter_type = 
                type_node.has_value() ?
                node_to_type_reference(module_info, tree, type_node.value(), output_allocator, temporaries_allocator) :
                std::nullopt;

            if (parameter_type.has_value())
                parameter_types.push_back(std::move(parameter_type.value()));
            else
                parameter_types.push_back({});

            Source_position const source_position = get_node_start_source_position(parameter_node);
            parameter_source_positions.push_back(source_position);
        }
    }

    bool is_variadic(
        Parse_tree const& tree,
        std::span<Parse_node const> const parameter_nodes
    )
    {
        if (parameter_nodes.empty())
            return false;

        Parse_node const& last_node = parameter_nodes.back();
        std::string_view const last_node_value = get_node_value(tree, last_node);
        return last_node_value == "...";
    }

    h::Function_declaration node_to_function_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        h::Linkage const linkage,
        std::optional<std::string_view> const& unique_name,
        bool const is_test,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Function_declaration output = {};
        output.is_test = is_test;
        
        std::optional<Parse_node> const name_node = get_child_node(tree, node, "Function_name");
        if (name_node.has_value())
        {
            output.name = create_string(get_node_value(tree, name_node.value()), output_allocator);
        }

        output.source_location = get_declaration_source_range(module_info.source_file_path, node, name_node);

        if (unique_name.has_value())
            output.unique_name = create_string(unique_name.value(), output_allocator);
        
        output.linkage = linkage;

        if (comment.has_value())
        {
            output.comment = create_string(comment.value(), output_allocator);
        }

        std::pmr::vector<Parse_node> const input_parameter_nodes = get_child_nodes_of_parent(tree, node, "Function_input_parameters", "Function_parameter", temporaries_allocator);
        output.input_parameter_source_positions = std::pmr::vector<Source_position>{};
        output.type.is_variadic = is_variadic(tree, input_parameter_nodes);
        std::span<Parse_node const> const input_parameter_nodes_without_variadic = 
            output.type.is_variadic ?
            std::span<Parse_node const>{input_parameter_nodes.begin(), input_parameter_nodes.end() - 1} :
            std::span<Parse_node const>{input_parameter_nodes};
        add_function_parameters(
            output.input_parameter_names,
            output.type.input_parameter_types,
            output.input_parameter_source_positions.value(),
            module_info,
            tree,
            input_parameter_nodes_without_variadic,
            output_allocator,
            temporaries_allocator
        );

        std::pmr::vector<Parse_node> const output_parameter_nodes = get_child_nodes_of_parent(tree, node, "Function_output_parameters", "Function_parameter", temporaries_allocator);
        output.output_parameter_source_positions = std::pmr::vector<Source_position>{};
        add_function_parameters(
            output.output_parameter_names,
            output.type.output_parameter_types,
            output.output_parameter_source_positions.value(),
            module_info,
            tree,
            output_parameter_nodes,
            output_allocator,
            temporaries_allocator
        );
        
        std::pmr::vector<Parse_node> const preconditions_node = get_child_nodes(tree, node, "Function_precondition", temporaries_allocator);
        output.preconditions = std::pmr::vector<h::Function_condition>{output_allocator};
        output.preconditions.resize(preconditions_node.size(), h::Function_condition{});
        for (std::size_t index = 0; index < preconditions_node.size(); ++index)
        {
            Parse_node const& condition_node = preconditions_node[index];
            output.preconditions[index] = node_to_function_condition(module_info, tree, condition_node, output_allocator, temporaries_allocator);
        }

        std::pmr::vector<Parse_node> const postconditions_node = get_child_nodes(tree, node, "Function_postcondition", temporaries_allocator);
        output.postconditions = std::pmr::vector<h::Function_condition>{output_allocator};
        output.postconditions.resize(postconditions_node.size(), h::Function_condition{});
        for (std::size_t index = 0; index < postconditions_node.size(); ++index)
        {
            Parse_node const& condition_node = postconditions_node[index];
            output.postconditions[index] = node_to_function_condition(module_info, tree, condition_node, output_allocator, temporaries_allocator);
        }
        
        return output;
    }

    h::Function_definition node_to_function_definition(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::string_view const function_name,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Function_definition output;
        output.name = create_string(function_name, output_allocator);

        output.source_location = get_declaration_source_range(module_info.source_file_path, node, std::nullopt);

        std::optional<Parse_node> const block_node = get_last_child_node(tree, node);
        if (block_node.has_value())
        {
            output.statements = node_to_block(
                module_info,
                tree,
                block_node.value(),
                output_allocator,
                temporaries_allocator
            );
        }

        return output;
    }

    h::Function_constructor node_to_function_constructor_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Function_constructor output = {};
        
        std::optional<Parse_node> const name_node = get_child_node(tree, node, 1);
        if (name_node.has_value())
        {
            output.name = create_string(get_node_value(tree, name_node.value()), output_allocator);
        }

        output.source_location = get_declaration_source_range(module_info.source_file_path, node, name_node);

        if (comment.has_value())
        {
            output.comment = create_string(comment.value(), output_allocator);
        }
        
        std::pmr::vector<Parse_node> const parameter_nodes = get_child_nodes_of_parent(tree, node, "Function_constructor_parameters", "Function_parameter", temporaries_allocator);
        output.parameters = std::pmr::vector<Function_constructor_parameter>{output_allocator};
        output.parameters.resize(parameter_nodes.size(), Function_constructor_parameter{});
        for (std::size_t index = 0; index < parameter_nodes.size(); ++index)
        {
            Parse_node const& parameter_node = parameter_nodes[index];
            std::optional<Parse_node> const parameter_name_node = get_child_node(tree, parameter_node, 0);
            if (parameter_name_node.has_value())
            {
                output.parameters[index].name = create_string(get_node_value(tree, parameter_name_node.value()), output_allocator);
            }

            std::optional<Parse_node> const parameter_type_node = get_child_node(tree, parameter_node, 2);
            if (parameter_type_node.has_value())
            {
                std::optional<Parse_node> const type_node = get_child_node(tree, parameter_type_node.value(), 0);
                if (type_node.has_value())
                {
                    std::optional<h::Type_reference> type = node_to_type_reference(module_info, tree, type_node.value(), output_allocator, temporaries_allocator);
                    if (type.has_value())
                        output.parameters[index].type = std::move(type.value());
                }
            }
        }

        std::optional<Parse_node> const block_node = get_last_child_node(tree, node);
        if (block_node.has_value())
        {
            output.statements = node_to_block(
                module_info,
                tree,
                block_node.value(),
                output_allocator,
                temporaries_allocator
            );

            replace_custom_type_reference_by_parameter_type<Function_constructor_parameter>(
                module_info,
                output.statements,
                output.parameters
            );
        }
        
        return output;
    }

    h::Struct_declaration node_to_struct_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::string_view> const unique_name,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Struct_declaration output;
        output.is_packed = false;
        output.is_literal = false;
        
        std::optional<Parse_node> const name_node = get_child_node(tree, node, "Struct_name");
        if (name_node.has_value())
        {
            output.name = create_string(get_node_value(tree, name_node.value()), output_allocator);
        }

        output.source_location = get_declaration_source_range(module_info.source_file_path, node, name_node);

        if (unique_name.has_value())
            output.unique_name = create_string(unique_name.value(), output_allocator);

        if (comment.has_value())
        {
            output.comment = create_string(comment.value(), output_allocator);
        }

        std::pmr::vector<Indexed_comment> member_comments{temporaries_allocator};

        std::optional<Parse_node> const members_node = get_last_child_node(tree, node);
        if (members_node.has_value())
        {
            std::pmr::vector<Parse_node> const member_nodes = get_child_nodes(tree, members_node.value(), temporaries_allocator);
            if (member_nodes.size() > 2)
            {
                std::size_t const member_count = member_nodes.size() - 2;

                output.member_names = std::pmr::vector<std::pmr::string>{output_allocator};
                output.member_names.resize(member_count, std::pmr::string{});

                output.member_types = std::pmr::vector<h::Type_reference>{output_allocator};
                output.member_types.resize(member_count, h::Type_reference{});

                output.member_bit_fields = std::pmr::vector<std::optional<std::uint32_t>>{output_allocator};
                output.member_bit_fields.resize(member_count, std::nullopt);

                output.member_default_values = std::pmr::vector<h::Statement>{output_allocator};
                output.member_default_values.resize(member_count, h::Statement{});

                output.member_source_positions = std::pmr::vector<Source_position>{output_allocator};
                output.member_source_positions->resize(member_count, h::Source_position{output.source_location->range.start.line, output.source_location->range.start.column});

                for (std::size_t index = 2; index < member_nodes.size(); ++index)
                {
                    Parse_node const& member_node = member_nodes[index - 1];
                    std::size_t const member_index = index - 2;
                    
                    std::optional<Parse_node> const member_name_node = get_child_node(tree, member_node, "Struct_member_name");
                    if (member_name_node.has_value())
                    {
                        output.member_names[member_index] = create_string(get_node_value(tree, member_name_node.value()), output_allocator);
                        output.member_source_positions.value()[member_index] = get_node_start_source_position(member_name_node.value());
                    }

                    std::optional<Parse_node> const member_type_node = get_child_node(tree, member_node, "Struct_member_type");
                    if (member_type_node.has_value())
                    {
                        std::optional<Parse_node> const type_node = get_child_node(tree, member_type_node.value(), 0);
                        if (type_node.has_value())
                        {
                            std::optional<Type_reference> type = node_to_type_reference(module_info, tree, type_node.value(), output_allocator, temporaries_allocator);
                            if (type.has_value())
                            {
                                output.member_types[member_index] = std::move(type.value());
                            }
                        }
                    }

                    std::optional<Parse_node> const member_bit_field_node = get_child_node(tree, member_node, "Integer_without_suffix");
                    if (member_bit_field_node.has_value())
                    {
                        std::string_view const bits_string = get_node_value(tree, member_bit_field_node.value());
                        std::uint64_t const bits = parse_uint64(bits_string);

                        output.member_bit_fields[member_index] = static_cast<std::uint32_t>(bits);
                    }

                    std::optional<Parse_node> const default_value_node = get_child_node(tree, member_node, "Generic_expression_or_instantiate");
                    if (default_value_node.has_value())
                    {
                        output.member_default_values[member_index] = node_to_statement(module_info, tree, default_value_node.value(), output_allocator, temporaries_allocator);
                    }

                    std::optional<Parse_node> const member_comment_node = get_child_node(tree, member_node, "Comment");
                    if (member_comment_node.has_value())
                    {
                        std::pmr::string comments = encode_comment(
                            get_node_value(tree, member_comment_node.value()),
                            output_allocator,
                            temporaries_allocator
                        );

                        member_comments.push_back(
                            Indexed_comment
                            {
                                .index = member_index,
                                .comment = std::move(comments)
                            }
                        );
                    }
                }
            }
        }

        output.member_comments = std::pmr::vector<Indexed_comment>{std::move(member_comments), output_allocator};

        return output;
    }

    h::Type_constructor node_to_type_constructor_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Type_constructor output = {};
        
        std::optional<Parse_node> const name_node = get_child_node(tree, node, 1);
        if (name_node.has_value())
        {
            output.name = create_string(get_node_value(tree, name_node.value()), output_allocator);
        }

        output.source_location = get_declaration_source_range(module_info.source_file_path, node, name_node);

        if (comment.has_value())
        {
            output.comment = create_string(comment.value(), output_allocator);
        }
        
        std::pmr::vector<Parse_node> const parameter_nodes = get_child_nodes_of_parent(tree, node, "Type_constructor_parameters", "Function_parameter", temporaries_allocator);
        output.parameters = std::pmr::vector<Type_constructor_parameter>{output_allocator};
        output.parameters.resize(parameter_nodes.size(), Type_constructor_parameter{});
        for (std::size_t index = 0; index < parameter_nodes.size(); ++index)
        {
            Parse_node const& parameter_node = parameter_nodes[index];
            std::optional<Parse_node> const parameter_name_node = get_child_node(tree, parameter_node, 0);
            if (parameter_name_node.has_value())
            {
                output.parameters[index].name = create_string(get_node_value(tree, parameter_name_node.value()), output_allocator);
            }

            std::optional<Parse_node> const parameter_type_node = get_child_node(tree, parameter_node, 2);
            if (parameter_type_node.has_value())
            {
                std::optional<Parse_node> const type_node = get_child_node(tree, parameter_type_node.value(), 0);
                if (type_node.has_value())
                {
                    std::optional<h::Type_reference> type = node_to_type_reference(module_info, tree, type_node.value(), output_allocator, temporaries_allocator);
                    if (type.has_value())
                        output.parameters[index].type = std::move(type.value());
                }
            }
        }

        std::optional<Parse_node> const block_node = get_last_child_node(tree, node);
        if (block_node.has_value())
        {
            output.statements = node_to_block(
                module_info,
                tree,
                block_node.value(),
                output_allocator,
                temporaries_allocator
            );

            replace_custom_type_reference_by_parameter_type<Type_constructor_parameter>(
                module_info,
                output.statements,
                output.parameters
            );
        }
        
        return output;
    }

    h::Union_declaration node_to_union_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::string_view> const unique_name,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Union_declaration output = {};
        
        std::optional<Parse_node> const name_node = get_child_node(tree, node, 1);
        if (name_node.has_value())
        {
            output.name = create_string(get_node_value(tree, name_node.value()), output_allocator);
        }

        output.source_location = get_declaration_source_range(module_info.source_file_path, node, name_node);

        if (unique_name.has_value())
            output.unique_name = create_string(unique_name.value(), output_allocator);
        
        if (comment.has_value())
        {
            output.comment = create_string(comment.value(), output_allocator);
        }

        std::pmr::vector<Indexed_comment> member_comments{temporaries_allocator};

        std::optional<Parse_node> const members_node = get_child_node(tree, node, 2);
        if (members_node.has_value())
        {
            std::pmr::vector<Parse_node> const member_nodes = get_child_nodes(tree, members_node.value(), temporaries_allocator);
            if (member_nodes.size() > 2)
            {
                std::size_t const member_count = member_nodes.size() - 2;

                output.member_names = std::pmr::vector<std::pmr::string>{output_allocator};
                output.member_names.resize(member_count, std::pmr::string{});

                output.member_types = std::pmr::vector<h::Type_reference>{output_allocator};
                output.member_types.resize(member_count, h::Type_reference{});

                output.member_source_positions = std::pmr::vector<Source_position>{output_allocator};
                output.member_source_positions->resize(member_count, h::Source_position{output.source_location->range.start.line, output.source_location->range.start.column});

                for (std::size_t index = 2; index < member_nodes.size(); ++index)
                {
                    Parse_node const& member_node = member_nodes[index - 1];
                    std::size_t const member_index = index - 2;
                    
                    std::optional<Parse_node> const member_name_node = get_child_node(tree, member_node, "Union_member_name");
                    if (member_name_node.has_value())
                    {
                        output.member_names[member_index] = create_string(get_node_value(tree, member_name_node.value()), output_allocator);
                        output.member_source_positions.value()[member_index] = get_node_start_source_position(member_name_node.value());
                    }

                    std::optional<Parse_node> const member_type_node = get_child_node(tree, member_node, "Union_member_type");
                    if (member_type_node.has_value())
                    {
                        std::optional<Parse_node> const type_node = get_child_node(tree, member_type_node.value(), 0);
                        if (type_node.has_value())
                        {
                            std::optional<Type_reference> type = node_to_type_reference(module_info, tree, type_node.value(), output_allocator, temporaries_allocator);
                            if (type.has_value())
                            {
                                output.member_types[member_index] = std::move(type.value());
                            }
                        }
                    }

                    std::optional<Parse_node> const member_comment_node = get_child_node(tree, member_node, "Comment");
                    if (member_comment_node.has_value())
                    {
                        std::pmr::string comments = encode_comment(
                            get_node_value(tree, member_comment_node.value()),
                            output_allocator,
                            temporaries_allocator
                        );

                        member_comments.push_back(
                            Indexed_comment
                            {
                                .index = member_index,
                                .comment = std::move(comments)
                            }
                        );
                    }
                }
            }
        }

        output.member_comments = std::pmr::vector<Indexed_comment>{std::move(member_comments), output_allocator};

        return output;
    }

    h::Statement node_to_statement(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Statement output;
        output.expressions = std::pmr::vector<h::Expression>{temporaries_allocator};
        
        std::optional<Parse_node> const expression_node = get_child_node(tree, node, 0);
        if (expression_node.has_value())
        {
            node_to_expression(output, module_info, tree, expression_node.value(), output_allocator, temporaries_allocator);
        }

        std::pmr::vector<h::Expression> final_expressions{std::move(output.expressions), output_allocator};
        output.expressions = std::move(final_expressions);
        
        return output;
    }

    static std::optional<Parse_node> get_non_generic_expression_node(
        Parse_tree const& tree,
        Parse_node const& node
    )
    {
        std::string_view const symbol = get_node_symbol(node);
        
        if (symbol == "Generic_expression" || symbol == "Generic_expression_or_instantiate")
        {
            std::optional<Parse_node> const child = get_child_node(tree, node, 0);
            if (!child.has_value())
                return std::nullopt;

            return get_non_generic_expression_node(tree, child.value());
        }
        else if (symbol == "Identifier" || symbol == "Variable_name" || symbol == "Expression_access_member_name")
        {
            std::optional<Parse_node> const parent = get_parent_node(node);
            if (!parent.has_value())
                return std::nullopt;
            
            return get_non_generic_expression_node(tree, parent.value());
        }
        else
        {
            return node;
        }
    }

    h::Expression_index node_to_expression(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Expression expression;

        expression.source_range = get_node_source_range(node);
            
        std::optional<Parse_node> const expression_node_optional = get_non_generic_expression_node(tree, node);
        if (!expression_node_optional.has_value())
            return {.expression_index = static_cast<std::uint64_t>(-1)};

        Parse_node const& expression_node = expression_node_optional.value();
        std::string_view const expression_type = get_node_symbol(expression_node);

        std::size_t const expression_index = statement.expressions.size();
        statement.expressions.push_back({});

        if (expression_type == "Expression_access")
        {
            expression.data = node_to_expression_access(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_access_array")
        {
            expression.data = node_to_expression_access_array(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_assert")
        {
            expression.data = node_to_expression_assert(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_assignment")
        {
            expression.data = node_to_expression_assignment(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_binary")
        {
            expression.data = node_to_expression_binary(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_block")
        {
            expression.data = node_to_expression_block(module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_break")
        {
            expression.data = node_to_expression_break(tree, expression_node);
        }
        else if (expression_type == "Expression_call")
        {
            expression.data = node_to_expression_call(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_cast")
        {
            expression.data = node_to_expression_cast(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_comment")
        {
            expression.data = node_to_expression_comment(tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_constant")
        {
            expression.data = node_to_expression_constant(tree, expression_node, output_allocator);
        }
        else if (expression_type == "Expression_create_array")
        {
            expression.data = node_to_expression_constant_array(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_continue")
        {
            expression.data = node_to_expression_continue(tree, expression_node);
        }
        else if (expression_type == "Expression_defer")
        {
            expression.data = node_to_expression_defer(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_dereference_and_access")
        {
            expression.data = node_to_expression_dereference_and_access(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_for_loop")
        {
            expression.data = node_to_expression_for_loop(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_function")
        {
            expression.data = node_to_expression_function(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_if")
        {
            expression.data = node_to_expression_if(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_instance_call")
        {
            expression.data = node_to_expression_instance_call(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_instantiate")
        {
            expression.data = node_to_expression_instantiate(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_null_pointer")
        {
            expression.data = node_to_expression_null_pointer();
        }
        else if (expression_type == "Expression_parenthesis")
        {
            expression.data = node_to_expression_parenthesis(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_reflection_call")
        {
            expression.data = node_to_expression_reflection(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_return")
        {
            expression.data = node_to_expression_return(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_struct")
        {
            expression.data = node_to_expression_struct(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_switch")
        {
            expression.data = node_to_expression_switch(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_ternary_condition")
        {
            expression.data = node_to_expression_ternary_condition(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_type")
        {
            expression.data = node_to_expression_type(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_unary")
        {
            expression.data = node_to_expression_unary(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_variable_declaration")
        {
            expression.data = node_to_expression_variable_declaration(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_variable_declaration_with_type")
        {
            expression.data = node_to_expression_variable_declaration_with_type(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else if (expression_type == "Expression_variable")
        {
            expression.data = node_to_expression_variable(tree, expression_node, output_allocator);
        }
        else if (expression_type == "Expression_while_loop")
        {
            expression.data = node_to_expression_while_loop(statement, module_info, tree, expression_node, output_allocator, temporaries_allocator);
        }
        else
        {
            statement.expressions.pop_back();
            return {.expression_index = static_cast<std::uint64_t>(-1)};
        }

        statement.expressions[expression_index] = std::move(expression);
        
        return {.expression_index = expression_index };
    }

    std::optional<h::Binary_operation> get_assignment_operation(std::string_view const operation)
    {
        if (operation == "=") return std::nullopt;
        if (operation == "+=") return h::Binary_operation::Add;
        if (operation == "-=") return h::Binary_operation::Subtract;
        if (operation == "*=") return h::Binary_operation::Multiply;
        if (operation == "/=") return h::Binary_operation::Divide;
        if (operation == "%=") return h::Binary_operation::Modulus;
        if (operation == "&=") return h::Binary_operation::Bitwise_and;
        if (operation == "|=") return h::Binary_operation::Bitwise_or;
        if (operation == "^=") return h::Binary_operation::Bitwise_xor;
        if (operation == "<<=") return h::Binary_operation::Bit_shift_left;
        if (operation == ">>=") return h::Binary_operation::Bit_shift_right;
        
        return std::nullopt;
    }

    h::Binary_operation get_binary_operation(std::string_view const operation)
    {
        if (operation == "+") return h::Binary_operation::Add;
        if (operation == "-") return h::Binary_operation::Subtract;
        if (operation == "*") return h::Binary_operation::Multiply;
        if (operation == "/") return h::Binary_operation::Divide;
        if (operation == "%") return h::Binary_operation::Modulus;
        if (operation == "==") return h::Binary_operation::Equal;
        if (operation == "!=") return h::Binary_operation::Not_equal;
        if (operation == "<") return h::Binary_operation::Less_than;
        if (operation == "<=") return h::Binary_operation::Less_than_or_equal_to;
        if (operation == ">") return h::Binary_operation::Greater_than;
        if (operation == ">=") return h::Binary_operation::Greater_than_or_equal_to;
        if (operation == "&&") return h::Binary_operation::Logical_and;
        if (operation == "||") return h::Binary_operation::Logical_or;
        if (operation == "&") return h::Binary_operation::Bitwise_and;
        if (operation == "|") return h::Binary_operation::Bitwise_or;
        if (operation == "^") return h::Binary_operation::Bitwise_xor;
        if (operation == "<<") return h::Binary_operation::Bit_shift_left;
        if (operation == ">>") return h::Binary_operation::Bit_shift_right;
        if (operation == "has") return h::Binary_operation::Has;

        return h::Binary_operation::Add;
    }

    h::Unary_operation get_unary_operation(std::string_view const operation)
    {
        if (operation == "!") return h::Unary_operation::Not;
        if (operation == "~") return h::Unary_operation::Bitwise_not;
        if (operation == "-") return h::Unary_operation::Minus;
        if (operation == "++") return h::Unary_operation::Pre_increment;
        if (operation == "--") return h::Unary_operation::Pre_decrement;
        if (operation == "*") return h::Unary_operation::Indirection;
        if (operation == "&") return h::Unary_operation::Address_of;

        return h::Unary_operation::Not;
    }

    std::pmr::vector<h::Statement> node_to_block(
        Module_info const& module_info,
        Parse_tree const& tree,
        std::span<Parse_node const> const statement_nodes,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::size_t const statement_count = statement_nodes.size();
        
        std::pmr::vector<h::Statement> output{output_allocator};
        output.resize(statement_count);

        for (std::size_t statement_index = 0; statement_index < statement_count; ++statement_index)
        {
            Parse_node const& statement_node = statement_nodes[statement_index];

            Statement statement = node_to_statement(module_info, tree, statement_node, output_allocator, temporaries_allocator);
            output[statement_index] = std::move(statement);
        }
        
        return output;
    }

    std::pmr::vector<h::Statement> node_to_block(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<h::Statement> output;

        std::pmr::vector<Parse_node> const child_nodes = get_named_child_nodes(tree, node, temporaries_allocator);

        return node_to_block(
            module_info,
            tree,
            child_nodes,
            output_allocator,
            temporaries_allocator
        );
    }

    h::Access_expression node_to_expression_access(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Access_expression output = {};

        std::optional<Parse_node> const left_hand_side = get_child_node(tree, node, "Generic_expression");
        if (left_hand_side.has_value())
        {
            output.expression = node_to_expression(
                statement,
                module_info,
                tree,
                left_hand_side.value(),
                output_allocator,
                temporaries_allocator
            );
        }
        
        std::optional<Parse_node> const member_name = get_child_node(tree, node, "Expression_access_member_name");
        if (member_name.has_value())
            output.member_name = create_string(get_node_value(tree, member_name.value()), output_allocator);
        
        return output;
    }

    h::Access_array_expression node_to_expression_access_array(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Access_array_expression output = {};
        
        std::optional<Parse_node> const array_node = get_child_node(tree, node, 0);
        if (array_node.has_value())
        {
            output.expression = node_to_expression(statement, module_info, tree, array_node.value(), output_allocator, temporaries_allocator);
        }
        
        std::optional<Parse_node> const index_node = get_child_node(tree, node, 2);
        if (index_node.has_value())
        {
            output.index = node_to_expression(statement, module_info, tree, index_node.value(), output_allocator, temporaries_allocator);
        }
        
        return output;
    }

    h::Assert_expression node_to_expression_assert(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Assert_expression output;
        
        std::optional<Parse_node> const message_node = get_child_node(tree, node, "String");
        if (message_node.has_value())
        {
            std::string_view const message = get_node_value(tree, message_node.value());
            output.message = create_string(message.substr(1, message.size() - 2), output_allocator);
        }
        
        std::optional<Parse_node> const statement_node = get_child_node(tree, node, "Generic_expression");
        if (statement_node.has_value())
        {
            output.statement = node_to_statement(module_info, tree, statement_node.value(), output_allocator, temporaries_allocator);
        }
        
        return output;
    }

    h::Assignment_expression node_to_expression_assignment(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Assignment_expression output;
        
        std::optional<Parse_node> const left_hand_side = get_child_node(tree, node, 0);
        if (left_hand_side.has_value())
        {
            output.left_hand_side = node_to_expression(statement, module_info,tree, left_hand_side.value(), output_allocator, temporaries_allocator);
        }
        
        std::optional<Parse_node> const right_hand_side = get_child_node(tree, node, 2);
        if (right_hand_side.has_value())
        {
            output.right_hand_side = node_to_expression(statement, module_info,tree, right_hand_side.value(), output_allocator, temporaries_allocator);
        }
        
        std::optional<Parse_node> const operation = get_child_node(tree, node, 1);
        if (operation.has_value())
        {
            output.additional_operation = get_assignment_operation(get_node_value(tree, operation.value()));
        }
        
        return output;
    }

    h::Binary_expression node_to_expression_binary(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Binary_expression output;
        
        std::optional<Parse_node> const left_node = get_child_node(tree, node, 0);
        if (left_node.has_value())
        {
            output.left_hand_side = node_to_expression(statement, module_info, tree, left_node.value(), output_allocator, temporaries_allocator);
        }
        
        std::optional<Parse_node> const right_node = get_child_node(tree, node, 2);
        if (right_node.has_value())
        {
            output.right_hand_side = node_to_expression(statement, module_info, tree, right_node.value(), output_allocator, temporaries_allocator);
        }
        
        std::optional<Parse_node> const operation_node = get_child_node(tree, node, 1);
        if (operation_node.has_value())
        {
            output.operation = get_binary_operation(get_node_value(tree, operation_node.value()));
        }
        
        return output;
    }

    h::Block_expression node_to_expression_block(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Block_expression output;

        output.statements = node_to_block(
            module_info,
            tree,
            node,
            output_allocator,
            temporaries_allocator
        );

        return output;
    }

    h::Break_expression node_to_expression_break(
        Parse_tree const& tree,
        Parse_node const& node
    )
    {
        h::Break_expression output = {};
        
        std::optional<Parse_node> const count_node = get_child_node(tree, node, "Expression_break_loop_count");
        if (count_node.has_value())
        {
            std::string_view const count_string = get_node_value(tree, count_node.value());
            std::uint64_t const count = parse_uint64(count_string);
            output.loop_count = count;
        }
        
        return output;
    }

    h::Call_expression node_to_expression_call(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Call_expression output = {};
        
        std::optional<Parse_node> const function = get_child_node(tree, node, 0);
        if (function.has_value())
            output.expression = node_to_expression(statement, module_info, tree, function.value(), output_allocator, temporaries_allocator);
        
        std::pmr::vector<Parse_node> const argument_nodes = get_child_nodes_of_parent(tree, node, "Expression_call_arguments", "Generic_expression_or_instantiate", temporaries_allocator);

        output.arguments.resize(argument_nodes.size());

        for (std::size_t argument_index = 0; argument_index < argument_nodes.size(); ++argument_index)
        {
            Parse_node const& argument_node = argument_nodes[argument_index];

            Expression_index const argument = node_to_expression(statement, module_info, tree, argument_node, output_allocator, temporaries_allocator);
            output.arguments[argument_index] = argument;
        }
        
        return output;
    }

    h::Cast_expression node_to_expression_cast(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Cast_expression output;
        
        std::optional<Parse_node> const source_node = get_child_node(tree, node, 0);
        if (source_node.has_value())
        {
            output.source = node_to_expression(statement, module_info, tree, source_node.value(), output_allocator, temporaries_allocator);
        }
        
        std::optional<Parse_node> const destination_type_node = get_child_node(tree, node, 2);
        if (destination_type_node.has_value())
        {
            std::optional<Parse_node> const type_node = get_child_node(tree, destination_type_node.value(), 0);
            if (type_node.has_value())
            {
                std::optional<h::Type_reference> type = node_to_type_reference(module_info, tree, type_node.value(), output_allocator, temporaries_allocator);
                if (type.has_value())
                {
                    output.destination_type = std::move(type.value());
                }
            }
        }

        output.cast_type = h::Cast_type::Numeric;
        
        return output;
    }

    h::Comment_expression node_to_expression_comment(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Comment_expression output;
        
        std::optional<Parse_node> const comment_node = get_child_node(tree, node, 0);
        if (comment_node.has_value())
        {
            std::optional<std::pmr::string> comment = extract_comments_from_node(
                tree,
                comment_node.value(),
                output_allocator,
                temporaries_allocator
            );

            if (comment.has_value())
            {
                output.comment = std::move(comment.value());
            }
        }
        
        return output;
    }

    h::Constant_expression node_to_expression_constant(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {        
        std::optional<Parse_node> choice_node = get_child_node(tree, node, 0);
        if (!choice_node.has_value())
            return {};

        std::string_view const choice = get_node_symbol(choice_node.value());
        std::string_view const value = get_node_value(tree, node);

        if (choice == "Boolean")
        {
            return 
            {
                .type = create_fundamental_type_type_reference(h::Fundamental_type::Bool),
                .data = create_string(value, output_allocator),
            };
        }
        else if (choice == "Number")
        {
            std::string_view const suffix = get_number_suffix(value);
            if (suffix.empty())
            {
                return 
                {
                    .type = create_integer_type_type_reference(32, true),
                    .data = create_string(value, output_allocator),
                };
            }
            
            std::string_view const value_without_suffix = value.substr(0, value.size() - suffix.size());
            
            char const first_character = suffix[0];

            if (first_character == 'i' || first_character == 'u')
            {
                bool const is_signed = first_character == 'i';
                std::uint32_t const number_of_bits = parse_number_of_bits(suffix.substr(1));
                return 
                {
                    .type = create_integer_type_type_reference(number_of_bits, is_signed),
                    .data = create_string(value_without_suffix, output_allocator),
                };
            }

            if (first_character == 'f')
            {
                std::uint32_t const number_of_bits = parse_number_of_bits(suffix.substr(1));

                if (number_of_bits == 16)
                {
                    return 
                    {
                        .type = create_fundamental_type_type_reference(h::Fundamental_type::Float16),
                        .data = create_string(value_without_suffix, output_allocator),
                    };
                }
                else if (number_of_bits == 64)
                {
                    return 
                    {
                        .type = create_fundamental_type_type_reference(h::Fundamental_type::Float64),
                        .data = create_string(value_without_suffix, output_allocator),
                    };
                }
                else
                {
                    return 
                    {
                        .type = create_fundamental_type_type_reference(h::Fundamental_type::Float32),
                        .data = create_string(value_without_suffix, output_allocator),
                    };
                }
            }

            if (first_character == 'c')
            {
                if (suffix == "cc")
                {
                    return 
                    {
                        .type = create_fundamental_type_type_reference(h::Fundamental_type::C_char),
                        .data = create_string(value_without_suffix, output_allocator),
                    };
                }
                else if (suffix == "cs")
                {
                    return 
                    {
                        .type = create_fundamental_type_type_reference(h::Fundamental_type::C_short),
                        .data = create_string(value_without_suffix, output_allocator),
                    };
                }
                else if (suffix == "ci")
                {
                    return 
                    {
                        .type = create_fundamental_type_type_reference(h::Fundamental_type::C_int),
                        .data = create_string(value_without_suffix, output_allocator),
                    };
                }
                else if (suffix == "cl")
                {
                    return 
                    {
                        .type = create_fundamental_type_type_reference(h::Fundamental_type::C_long),
                        .data = create_string(value_without_suffix, output_allocator),
                    };
                }
                else if (suffix == "cll")
                {
                    return 
                    {
                        .type = create_fundamental_type_type_reference(h::Fundamental_type::C_longlong),
                        .data = create_string(value_without_suffix, output_allocator),
                    };
                }
                else if (suffix == "cuc")
                {
                    return 
                    {
                        .type = create_fundamental_type_type_reference(h::Fundamental_type::C_uchar),
                        .data = create_string(value_without_suffix, output_allocator),
                    };
                }
                else if (suffix == "cus")
                {
                    return 
                    {
                        .type = create_fundamental_type_type_reference(h::Fundamental_type::C_ushort),
                        .data = create_string(value_without_suffix, output_allocator),
                    };
                }
                else if (suffix == "cui")
                {
                    return 
                    {
                        .type = create_fundamental_type_type_reference(h::Fundamental_type::C_uint),
                        .data = create_string(value_without_suffix, output_allocator),
                    };
                }
                else if (suffix == "cul")
                {
                    return 
                    {
                        .type = create_fundamental_type_type_reference(h::Fundamental_type::C_ulong),
                        .data = create_string(value_without_suffix, output_allocator),
                    };
                }
                else if (suffix == "cull")
                {
                    return 
                    {
                        .type = create_fundamental_type_type_reference(h::Fundamental_type::C_ulonglong),
                        .data = create_string(value_without_suffix, output_allocator),
                    };
                }
                else if (suffix == "cb")
                {
                    return 
                    {
                        .type = create_fundamental_type_type_reference(h::Fundamental_type::C_bool),
                        .data = create_string(value_without_suffix, output_allocator),
                    };
                }
            }
        }
        else if (choice == "String")
        {
            std::string_view const suffix = get_string_suffix(value);

            std::string_view const value_without_suffix = value.substr(0, value.size() - suffix.size());
            std::string_view const value_without_quotes = value_without_suffix.substr(1, value_without_suffix.size() - 2);

            if (suffix == "c")
            {
                return 
                {
                    .type = create_c_string_type_reference(false),
                    .data = create_string(value_without_quotes, output_allocator),
                };
            }
            else
            {
                return 
                {
                    .type = create_fundamental_type_type_reference(h::Fundamental_type::String),
                    .data = create_string(value_without_quotes, output_allocator),
                };
            }
        }
        
        return {};
    }

    h::Constant_array_expression node_to_expression_constant_array(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Constant_array_expression output = {};
        
        std::pmr::vector<Parse_node> const elements = get_child_nodes(tree, node, "Generic_expression_or_instantiate", temporaries_allocator);

        output.array_data = std::pmr::vector<Statement>{output_allocator};
        output.array_data.resize(elements.size(), h::Statement{});

        for (std::size_t index = 0; index < elements.size(); ++index)
        {
            Parse_node const& element_node = elements[index];
            output.array_data[index] = node_to_statement(module_info, tree, element_node, output_allocator, temporaries_allocator);
        }

        return output;
    }

    h::Continue_expression node_to_expression_continue(
        Parse_tree const& tree,
        Parse_node const& node
    )
    {
        return h::Continue_expression{};
    }

    h::Defer_expression node_to_expression_defer(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Defer_expression output{};
        
        std::optional<Parse_node> const expression_node = get_child_node(tree, node, 1);
        if (expression_node.has_value())
        {
            output.expression_to_defer = node_to_expression(statement, module_info, tree, expression_node.value(), output_allocator, temporaries_allocator);
        }
        
        return output;
    }

    h::Dereference_and_access_expression node_to_expression_dereference_and_access(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Dereference_and_access_expression output{};
        
        std::optional<Parse_node> const expression_node = get_child_node(tree, node, 0);
        if (expression_node.has_value())
        {
            output.expression = node_to_expression(statement, module_info, tree, expression_node.value(), output_allocator, temporaries_allocator);
        }

        std::optional<Parse_node> const member_node = get_child_node(tree, node, 2);
        if (member_node.has_value())
        {
            output.member_name = create_string(get_node_value(tree, member_node.value()), output_allocator);
        }
        
        return output;
    }

    h::For_loop_expression node_to_expression_for_loop(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::For_loop_expression output;

        std::optional<Parse_node> const head_node = get_child_node(tree, node, 0);
        if (head_node.has_value())
        {
            std::optional<Parse_node> const variable_node = get_child_node(tree, head_node.value(), 1);
            if (variable_node.has_value())
            {
                output.variable_name = create_string(get_node_value(tree, variable_node.value()), output_allocator);
            }

            std::optional<Parse_node> const range_begin_node = get_child_node(tree, head_node.value(), 3);
            if (range_begin_node.has_value())
            {
                std::optional<Parse_node> const number_expression_node = get_child_node(tree, range_begin_node.value(), 0);
                if (number_expression_node.has_value())
                {
                    std::optional<Parse_node> const number_node = get_child_node(tree, number_expression_node.value(), 0);
                    if (number_node.has_value())
                    {
                        output.range_begin = node_to_expression(statement, module_info, tree, number_node.value(), output_allocator, temporaries_allocator);
                    }
                }
            }

            std::optional<Parse_node> const range_end_node = get_child_node(tree, head_node.value(), 5);
            if (range_end_node.has_value())
            {
                std::optional<Parse_node> const number_node = get_child_node(tree, range_end_node.value(), 0);
                if (number_node.has_value())
                {
                    output.range_end = node_to_statement(module_info, tree, number_node.value(), output_allocator, temporaries_allocator);
                }
            }

            std::optional<Parse_node> const step_node = get_child_node(tree, head_node.value(), "Expression_for_loop_step");
            if (step_node.has_value())
            {
                std::optional<Parse_node> const step_number_node = get_child_node(tree, step_node.value(), 1);
                if (step_number_node.has_value())
                {
                    std::optional<Parse_node> const number_node = get_child_node(tree, step_number_node.value(), 0);
                    if (number_node.has_value())
                    {
                        output.step_by = node_to_expression(statement, module_info, tree, number_node.value(), output_allocator, temporaries_allocator);
                    }
                }
            }

            std::optional<Parse_node> const reverse_node = get_child_node(tree, head_node.value(), "Expression_for_loop_reverse");
            bool const is_reverse = reverse_node.has_value();
            output.range_comparison_operation = is_reverse ? h::Binary_operation::Greater_than : h::Binary_operation::Less_than;
        }

        std::optional<Parse_node> const block_node = get_child_node(tree, node, 1);
        if (block_node.has_value())
        {
            output.then_statements = node_to_block(module_info, tree, block_node.value(), output_allocator, temporaries_allocator);
        }

        return output;
    }

    h::Function_expression node_to_expression_function(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Function_expression output = {};

        output.declaration = node_to_function_declaration(
            module_info,
            tree,
            node,
            h::Linkage::Private,
            std::nullopt,
            false,
            std::nullopt,
            output_allocator,
            temporaries_allocator
        );

        output.definition = node_to_function_definition(
            module_info,
            tree,
            node,
            "",
            output_allocator,
            temporaries_allocator
        );

        return output;
    }

    h::If_expression node_to_expression_if(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {        
        std::pmr::vector<Condition_statement_pair> series{temporaries_allocator};
        
        std::optional<Parse_node> current_node = node;
        while (current_node.has_value())
        {
            h::Condition_statement_pair serie;

            std::string_view const current_node_value = get_node_symbol(current_node.value());

            std::optional<Parse_node> const condition_node =
                current_node_value == "Expression_if" ?
                get_child_node(tree, current_node.value(), "Generic_expression") :
                std::nullopt;
            if (condition_node.has_value())
            {
                serie.condition = node_to_statement(module_info, tree, condition_node.value(), output_allocator, temporaries_allocator);
            }

            std::optional<Parse_node> const statements_node =
                current_node_value == "Expression_if_statements" ?
                current_node :
                get_child_node(tree, current_node.value(), "Expression_if_statements");
            if (statements_node.has_value())
            {
                serie.then_statements = node_to_block(module_info, tree, statements_node.value(), output_allocator, temporaries_allocator);
                serie.block_source_range = get_node_source_range(statements_node.value());
            }

            series.push_back(std::move(serie));

            current_node = get_child_node(tree, current_node.value(), "Expression_if_else");
            if (!current_node.has_value())
                break;

            current_node = get_last_child_node(
                tree,
                current_node.value()
            );
        }

        h::If_expression output
        {
            .series = std::pmr::vector<Condition_statement_pair>{std::move(series), output_allocator},
        };
        return output;
    }

    h::Instance_call_expression node_to_expression_instance_call(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Instance_call_expression output = {};
        
        std::optional<Parse_node> const left_hand_side_node = get_child_node(tree, node, 0);
        if (left_hand_side_node.has_value())
        {
            output.left_hand_side = node_to_expression(statement, module_info, tree, left_hand_side_node.value(), output_allocator, temporaries_allocator);
        }
        
        std::pmr::vector<Parse_node> const argument_nodes = get_child_nodes(tree, node, "Expression_instance_call_parameter", temporaries_allocator);
        output.arguments = node_to_block(module_info, tree, argument_nodes, output_allocator, temporaries_allocator);
        
        return output;
    }

    h::Instantiate_expression_type calculate_instantiate_expression_type(
        Parse_tree const& tree,
        Parse_node const& node
    )
    {
        std::optional<Parse_node> const mode_node = get_child_node(tree, node, 0);

        if (!mode_node.has_value())
            return h::Instantiate_expression_type::Default;

        std::string_view const mode_value = get_node_value(tree, mode_node.value());

        if (mode_value == "explicit")
            return h::Instantiate_expression_type::Explicit;
        else if (mode_value == "uninitialized")
            return h::Instantiate_expression_type::Uninitialized;
        else if (mode_value == "zero_initialized")
            return h::Instantiate_expression_type::Zero_initialized;
        else
            return h::Instantiate_expression_type::Default;
    }

    h::Instantiate_expression node_to_expression_instantiate(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Instantiate_expression output;

        output.type = calculate_instantiate_expression_type(tree, node);
        
        std::pmr::vector<Parse_node> const member_nodes = get_child_nodes_of_parent(tree, node, "Expression_instantiate_members", "Expression_instantiate_member", temporaries_allocator);
        for (Parse_node const& member_node : member_nodes)
        {
            h::Instantiate_member_value_pair pair{};
            
            std::optional<Parse_node> const name_node = get_child_node(tree, member_node, "Expression_instantiate_member_name");
            if (name_node.has_value())
            {
                pair.member_name = create_string(get_node_value(tree, name_node.value()), output_allocator);
            }

            pair.source_range = get_node_source_range(member_node);
            
            std::optional<Parse_node> const value_node = get_child_node(tree, member_node, "Generic_expression_or_instantiate");
            if (value_node.has_value())
            {
                pair.value = node_to_expression(statement, module_info, tree, value_node.value(), output_allocator, temporaries_allocator);
            }
            
            output.members.push_back(std::move(pair));
        }
        
        return output;
    }

    h::Null_pointer_expression node_to_expression_null_pointer()
    {
        return h::Null_pointer_expression{};
    }

    h::Parenthesis_expression node_to_expression_parenthesis(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Parenthesis_expression output;
        
        std::optional<Parse_node> const expression_node = get_child_node(tree, node, "Generic_expression");
        if (expression_node.has_value())
        {
            output.expression = node_to_expression(statement, module_info, tree, expression_node.value(), output_allocator, temporaries_allocator);
        }
        
        return output;
    }

    h::Reflection_expression node_to_expression_reflection(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Reflection_expression output = {};
        
        std::optional<Parse_node> const name_node = get_child_node(tree, node, 0);
        if (name_node.has_value())
        {
            std::string_view const identifier = get_node_value(tree, name_node.value());
            std::string_view const name = identifier.substr(1);
            output.name = create_string(name, output_allocator);
        }

        std::optional<Parse_node> const argument_types_node = get_child_node(tree, node, "Expression_reflection_call_type_arguments");
        if (argument_types_node.has_value())
        {
            std::pmr::vector<Parse_node> const argument_type_nodes = get_child_nodes(tree, argument_types_node.value(), "Type", temporaries_allocator);
            
            output.type_arguments.resize(argument_type_nodes.size());

            for (std::size_t index = 0; index < argument_type_nodes.size(); ++index)
            {
                std::optional<h::Type_reference> type = node_to_type_reference(module_info, tree, argument_type_nodes[index], output_allocator, temporaries_allocator);
                if (type.has_value())
                {
                    output.type_arguments[index] = std::move(type.value());
                }
            }
        }

        std::pmr::vector<Parse_node> const argument_nodes = get_child_nodes_of_parent(tree, node, "Expression_call_arguments", "Generic_expression_or_instantiate", temporaries_allocator);

        output.arguments.resize(argument_nodes.size());

        for (std::size_t argument_index = 0; argument_index < argument_nodes.size(); ++argument_index)
        {
            Parse_node const& argument_node = argument_nodes[argument_index];

            Expression_index const argument = node_to_expression(statement, module_info, tree, argument_node, output_allocator, temporaries_allocator);
            output.arguments[argument_index] = argument;
        }
        
        return output;
    }

    h::Return_expression node_to_expression_return(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::optional<Parse_node> const value_node = get_child_node(tree, node, "Generic_expression_or_instantiate");
        if (!value_node.has_value())
            return {};

        h::Return_expression output;
        output.expression = node_to_expression(statement, module_info, tree, value_node.value(), output_allocator, temporaries_allocator);
        return output;
    }

    h::Struct_expression node_to_expression_struct(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Struct_expression output = {};

        output.declaration = node_to_struct_declaration(
            module_info,
            tree,
            node,
            std::nullopt,
            std::nullopt,
            output_allocator,
            temporaries_allocator
        );

        return output;
    }

    h::Switch_case_expression_pair node_to_expression_switch_case(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Switch_case_expression_pair output{};

        std::optional<Parse_node> const case_value = get_child_node(tree, node, "Expression_switch_case_value");
        if (case_value.has_value())
        {
            std::optional<Parse_node> const expression_node = get_child_node(tree, case_value.value(), 0);
            if (expression_node.has_value())
            {
                output.case_value = node_to_expression(statement, module_info, tree, expression_node.value(), output_allocator, temporaries_allocator);
            }
        }

        std::pmr::vector<Parse_node> const child_nodes = get_child_nodes(tree, node, temporaries_allocator);
        std::span<Parse_node const> const statement_nodes = 
            case_value.has_value() ?
            std::span<Parse_node const>{child_nodes.begin() + 3, child_nodes.end()} :
            std::span<Parse_node const>{child_nodes.begin() + 2, child_nodes.end()};

        if (!statement_nodes.empty())
        {
            output.statements = std::pmr::vector<h::Statement>{output_allocator};
            output.statements.resize(statement_nodes.size(), h::Statement{});

            for (std::size_t index = 0; index < statement_nodes.size(); ++index)
            {
                output.statements[index] = node_to_statement(module_info, tree, statement_nodes[index], output_allocator, temporaries_allocator);
            }
        }

        return output;
    }

    h::Switch_expression node_to_expression_switch(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Switch_expression output{};

        std::optional<Parse_node> const value_node = get_child_node(tree, node, 1);
        if (value_node.has_value())
        {
            output.value = node_to_expression(statement, module_info, tree, value_node.value(), output_allocator, temporaries_allocator);
        }

        std::optional<Parse_node> const cases_node = get_child_node(tree, node, 2);
        if (cases_node.has_value())
        {
            std::pmr::vector<Parse_node> const child_nodes = get_child_nodes(tree, cases_node.value(), temporaries_allocator);
            if (child_nodes.size() > 2)
            {
                std::span<Parse_node const> const case_nodes{ child_nodes.begin() + 1, child_nodes.end() - 1 };

                output.cases = std::pmr::vector<h::Switch_case_expression_pair>{output_allocator};
                output.cases.resize(case_nodes.size(), h::Switch_case_expression_pair{});

                for (std::size_t index = 0; index < case_nodes.size(); ++index)
                {
                    Parse_node const& case_node = case_nodes[index];
                    
                    h::Switch_case_expression_pair case_pair = node_to_expression_switch_case(statement, module_info, tree, case_node, output_allocator, temporaries_allocator);
                    output.cases[index] = std::move(case_pair);
                }
            }
        }

        return output;
    }

    h::Ternary_condition_expression node_to_expression_ternary_condition(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Ternary_condition_expression output{};
        
        std::optional<Parse_node> const condition = get_child_node(tree, node, 0);
        if (condition.has_value())
        {
            output.condition = node_to_expression(statement, module_info, tree, condition.value(), output_allocator, temporaries_allocator);
        }
        
        std::optional<Parse_node> const then_node = get_child_node(tree, node, 2);
        if (then_node.has_value())
        {
            output.then_statement = node_to_statement(module_info, tree, then_node.value(), output_allocator, temporaries_allocator);
        }
        
        std::optional<Parse_node> const else_node = get_child_node(tree, node, 4);
        if (else_node.has_value())
        {
            output.else_statement = node_to_statement(module_info, tree, else_node.value(), output_allocator, temporaries_allocator);
        }
        
        return output;
    }

    h::Type_expression node_to_expression_type(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Type_expression output = {};
        
        std::optional<Parse_node> const type_node = get_child_node(tree, node, 0);
        if (type_node.has_value())
        {
            std::optional<h::Type_reference> type = node_to_type_reference(module_info, tree, type_node.value(), output_allocator, temporaries_allocator);
            if (type.has_value())
            {
                output.type = std::move(type.value());
            }
        }
        
        return output;
    }

    h::Unary_expression node_to_expression_unary(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Unary_expression output;
        
        std::optional<Parse_node> const operand_node = get_child_node(tree, node, "Generic_expression");
        if (operand_node.has_value())
        {
            output.expression = node_to_expression(statement, module_info, tree, operand_node.value(), output_allocator, temporaries_allocator);
        }
        
        std::optional<Parse_node> const operation_node = get_child_node(tree, node, "Expression_unary_symbol");
        if (operation_node.has_value())
        {
            output.operation = get_unary_operation(get_node_value(tree, operation_node.value()));
        }
        
        return output;
    }

    h::Variable_expression node_to_expression_variable(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        h::Variable_expression output;
        
        std::optional<Parse_node> const name_node = get_child_node(tree, node, "Variable_name");
        if (name_node.has_value())
        {
            std::string_view const name = get_node_value(tree, name_node.value());
            output.name = create_string(name, output_allocator);
        }
        
        return output;
    }

    h::Variable_declaration_expression node_to_expression_variable_declaration(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Variable_declaration_expression output;
        
        std::optional<Parse_node> const name = get_child_node(tree, node, "Variable_name");
        if (name.has_value())
        {
            output.name = create_string(get_node_value(tree, name.value()), output_allocator);
        }
        
        std::optional<Parse_node> const mutability = get_child_node(tree, node, "Expression_variable_mutability");
        if (mutability.has_value())
        {
            output.is_mutable = get_node_value(tree, mutability.value()) == "mutable";
        }
        
        std::optional<Parse_node> const right_hand_side_node = get_child_node(tree, node, "Generic_expression");
        if (right_hand_side_node.has_value())
        {
            output.right_hand_side = node_to_expression(statement, module_info, tree, right_hand_side_node.value(), output_allocator, temporaries_allocator);
        }
        
        return output;
    }

    h::Variable_declaration_with_type_expression node_to_expression_variable_declaration_with_type(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Variable_declaration_with_type_expression output = {};
        
        std::optional<Parse_node> const name = get_child_node(tree, node, "Variable_name");
        if (name.has_value())
        {
            output.name = create_string(get_node_value(tree, name.value()), output_allocator);
        }
        
        std::optional<Parse_node> const mutability = get_child_node(tree, node, "Expression_variable_mutability");
        if (mutability.has_value())
        {
            output.is_mutable = get_node_value(tree, mutability.value()) == "mutable";
        }
        
        std::optional<Parse_node> const declaration_type_node = get_child_node(tree, node, "Expression_variable_declaration_type");
        if (declaration_type_node.has_value())
        {
            std::optional<Parse_node> const type_node = get_child_node(tree, declaration_type_node.value(), 0);
            if (type_node.has_value())
            {
                std::optional<h::Type_reference> type = node_to_type_reference(module_info, tree, type_node.value(), output_allocator, temporaries_allocator);
                if (type.has_value())
                    output.type = std::move(type.value());
            }
        }
        
        std::optional<Parse_node> const right_hand_side_node = get_child_node(tree, node, "Generic_expression_or_instantiate");
        if (right_hand_side_node.has_value())
        {
            output.right_hand_side = node_to_expression(statement, module_info, tree, right_hand_side_node.value(), output_allocator, temporaries_allocator);
        }
        
        return output;
    }

    h::While_loop_expression node_to_expression_while_loop(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::While_loop_expression output;

        std::optional<Parse_node> const condition = get_child_node(tree, node, 1);
        if (condition.has_value())
        {
            output.condition = node_to_statement(module_info, tree, condition.value(), output_allocator, temporaries_allocator);
        }

        std::optional<Parse_node> const statements = get_child_node(tree, node, 2);
        if (statements.has_value())
        {
            output.then_statements = node_to_block(module_info, tree, statements.value(), output_allocator, temporaries_allocator);
        }

        return output;
    }
}
