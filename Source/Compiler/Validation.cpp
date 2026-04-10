module;

#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <memory_resource>
#include <optional>
#include <span>
#include <unordered_set>
#include <variant>
#include <vector>

module h.compiler.validation;

import h.compiler.analysis;
import h.compiler.diagnostic;
import h.core;
import h.core.declarations;
import h.core.formatter;
import h.core.types;

namespace h::compiler
{
    h::compiler::Diagnostic create_error_diagnostic(
        std::optional<std::filesystem::path> const source_file_path,
        std::optional<Source_range> const range,
        std::string_view const message
    )
    {
        return h::compiler::Diagnostic
        {
            .file_path = source_file_path,
            .range = range.has_value() ? range.value() : Source_range{},
            .source = Diagnostic_source::Compiler,
            .severity = Diagnostic_severity::Error,
            .message = std::pmr::string{message},
            .related_information = {},
        };
    }

    h::compiler::Diagnostic create_error_diagnostic_with_code(
        std::optional<std::filesystem::path> const source_file_path,
        std::optional<Source_range> const range,
        std::string_view const message,
        Diagnostic_code const code,
        Diagnostic_data data
    )
    {
        return h::compiler::Diagnostic
        {
            .file_path = source_file_path,
            .range = range.has_value() ? range.value() : Source_range{},
            .source = Diagnostic_source::Compiler,
            .severity = Diagnostic_severity::Error,
            .code = code,
            .message = std::pmr::string{message},
            .related_information = {},
            .data = std::move(data),
        };
    }

    h::compiler::Diagnostic create_warning_diagnostic(
        std::optional<std::filesystem::path> const source_file_path,
        std::optional<Source_range> const range,
        std::string_view const message
    )
    {
        return h::compiler::Diagnostic
        {
            .file_path = source_file_path,
            .range = range.has_value() ? range.value() : Source_range{},
            .source = Diagnostic_source::Compiler,
            .severity = Diagnostic_severity::Warning,
            .message = std::pmr::string{message},
            .related_information = {},
        };
    }

    std::optional<std::string_view> find_type_unique_name(
        Declaration_database const& declaration_database,
        h::Type_reference const& type
    )
    {
        if (std::holds_alternative<h::Custom_type_reference>(type.data))
        {
            std::optional<Declaration> const declaration_optional = find_declaration(declaration_database, type);
            if (declaration_optional.has_value())
            {
                Declaration const& declaration = declaration_optional.value();

                if (std::holds_alternative<Alias_type_declaration const*>(declaration.data))
                {
                    Alias_type_declaration const& value = *std::get<Alias_type_declaration const*>(declaration.data);
                    return value.unique_name;
                }
                else if (std::holds_alternative<Enum_declaration const*>(declaration.data))
                {
                    Enum_declaration const& value = *std::get<Enum_declaration const*>(declaration.data);
                    return value.unique_name;
                }
                else if (std::holds_alternative<Forward_declaration const*>(declaration.data))
                {
                    Forward_declaration const& value = *std::get<Forward_declaration const*>(declaration.data);
                    return value.unique_name;
                }
                else if (std::holds_alternative<Function_declaration const*>(declaration.data))
                {
                    Function_declaration const& value = *std::get<Function_declaration const*>(declaration.data);
                    return value.unique_name;
                }
                else if (std::holds_alternative<Global_variable_declaration const*>(declaration.data))
                {
                    Global_variable_declaration const& value = *std::get<Global_variable_declaration const*>(declaration.data);
                    return value.unique_name;
                }
                else if (std::holds_alternative<Struct_declaration const*>(declaration.data))
                {
                    Struct_declaration const& value = *std::get<Struct_declaration const*>(declaration.data);
                    return value.unique_name;
                }
                else if (std::holds_alternative<Union_declaration const*>(declaration.data))
                {
                    Union_declaration const& value = *std::get<Union_declaration const*>(declaration.data);
                    return value.unique_name;
                }
            }
        }

        return std::nullopt;
    }

    bool are_compatible_types(
        Declaration_database const& declaration_database,
        std::optional<h::Type_reference> const& first,
        std::optional<h::Type_reference> const& second
    )
    {
        if (!first.has_value() || !second.has_value())
            return false;

        std::optional<h::Type_reference> const first_underlying_type = get_underlying_type(declaration_database, first.value());
        if (!first_underlying_type.has_value())
            return false;

        std::optional<h::Type_reference> const second_underlying_type = get_underlying_type(declaration_database, second.value());
        if (!second_underlying_type.has_value())
            return false;

        {
            std::optional<std::string_view> const first_unique_name = find_type_unique_name(declaration_database, first_underlying_type.value());
            if (first_unique_name.has_value())
            {
                std::optional<std::string_view> const second_unique_name = find_type_unique_name(declaration_database, second_underlying_type.value());
                if (second_unique_name.has_value())
                {
                    return first_unique_name.value() == second_unique_name.value();
                }
            }
        }
        
        if (is_pointer(first_underlying_type.value()) && is_null_pointer_type(second_underlying_type.value()))
            return true;

        if (is_null_pointer_type(first_underlying_type.value()) && is_pointer(second_underlying_type.value()))
            return true;

        if (is_function_pointer(first_underlying_type.value()) && is_null_pointer_type(second_underlying_type.value()))
            return true;

        if (is_null_pointer_type(first_underlying_type.value()) && is_function_pointer(second_underlying_type.value()))
            return true;

        if (is_function_pointer(first_underlying_type.value()) && is_function_pointer(second_underlying_type.value()))
        {
            h::Function_pointer_type const& first_pointer_type = std::get<h::Function_pointer_type>(first_underlying_type->data);
            h::Function_pointer_type const& second_pointer_type = std::get<h::Function_pointer_type>(second_underlying_type->data);
            return first_pointer_type.type == second_pointer_type.type;
        }

        return first == second;
    }

    bool can_assign_type(
        Declaration_database const& declaration_database,
        std::optional<h::Type_reference> const& destination,
        std::optional<h::Type_reference> const& source
    )
    {
        if (!destination.has_value() || !source.has_value())
            return false;

        std::optional<h::Type_reference> const destination_underlying_type = get_underlying_type(declaration_database, destination.value());
        if (!destination_underlying_type.has_value())
            return false;
        h::Type_reference const& destination_type = destination_underlying_type.value();

        std::optional<h::Type_reference> const source_underlying_type = get_underlying_type(declaration_database, source.value());
        if (!source_underlying_type.has_value())
            return false;
        h::Type_reference const& source_type = source_underlying_type.value();

        {
            std::optional<std::string_view> const destination_unique_name = find_type_unique_name(declaration_database, destination_type);
            if (destination_unique_name.has_value())
            {
                std::optional<std::string_view> const source_unique_name = find_type_unique_name(declaration_database, source_type);
                if (source_unique_name.has_value())
                {
                    return destination_unique_name.value() == source_unique_name.value();
                }
            }
        }

        if (is_pointer(destination_type))
        {
            if (is_null_pointer_type(source_type))
                return true;

            if (!is_pointer(source_type))
                return false;

            h::Pointer_type const& destination_pointer_type = std::get<h::Pointer_type>(destination_type.data);
            h::Pointer_type const& source_pointer_type = std::get<h::Pointer_type>(source_type.data);

            if (destination_pointer_type.is_mutable && !source_pointer_type.is_mutable)
                return false;

            if (destination_pointer_type.element_type.empty())
                return true;

            if (source_pointer_type.element_type.empty())
                return false;

            return can_assign_type(declaration_database, destination_pointer_type.element_type[0], source_pointer_type.element_type[0]);
        }

        if (is_function_pointer(destination_type) && is_null_pointer_type(source_type))
            return true;

        if (std::holds_alternative<h::Array_slice_type>(destination_type.data))
        {
            h::Array_slice_type const& destination_array_slice_type = std::get<h::Array_slice_type>(destination_type.data);

            if (std::holds_alternative<h::Array_slice_type>(source_type.data))
            {
                h::Array_slice_type const& source_array_slice_type = std::get<h::Array_slice_type>(source_type.data);

                if (destination_array_slice_type.is_mutable && !source_array_slice_type.is_mutable)
                    return false;

                if (destination_array_slice_type.element_type.empty() && source_array_slice_type.element_type.empty())
                    return true;

                if (destination_array_slice_type.element_type.empty() || source_array_slice_type.element_type.empty())
                    return false;

                return can_assign_type(declaration_database, destination_array_slice_type.element_type[0], source_array_slice_type.element_type[0]);
            }
            else if (std::holds_alternative<h::Constant_array_type>(source_type.data))
            {
                h::Constant_array_type const& constant_array_type = std::get<h::Constant_array_type>(source_type.data);

                if (destination_array_slice_type.element_type.empty() && constant_array_type.value_type.empty())
                    return true;

                if (destination_array_slice_type.element_type.empty() || constant_array_type.value_type.empty())
                    return false;

                return can_assign_type(declaration_database, destination_array_slice_type.element_type[0], constant_array_type.value_type[0]);
            }
        }

        if (is_any_type(destination_type))
            return true;

        return are_compatible_types(declaration_database, destination, source);
    }

    std::array<std::string_view, 14> get_reserved_keywords()
    {
        return
        {
            "Byte",
            "Int8",
            "Int16",
            "Int32",
            "Int64",
            "Uint8",
            "Uint16",
            "Uint32",
            "Uint64",
            "Float16",
            "Float32",
            "Float64",
            "true",
            "false",
        };
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_module(
        h::Module const& core_module,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        // TODO validate module name

        {
            std::pmr::vector<h::compiler::Diagnostic> const diagnostics = validate_imports(
                core_module,
                declaration_database,
                temporaries_allocator
            );
            if (!diagnostics.empty())
                return diagnostics;
        }

        {
            std::pmr::vector<h::compiler::Diagnostic> const diagnostics = validate_type_references(
                core_module,
                declaration_database,
                temporaries_allocator
            );
            if (!diagnostics.empty())
                return diagnostics;
        }

        {
            std::pmr::vector<h::compiler::Diagnostic> const diagnostics = validate_declarations(
                core_module,
                declaration_database,
                temporaries_allocator
            );
            if (!diagnostics.empty())
                return diagnostics;
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_imports(
        h::Module const& core_module,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<h::compiler::Diagnostic> diagnostics{temporaries_allocator};

        std::pmr::unordered_set<std::string_view> all_names{temporaries_allocator};

        for (Import_module_with_alias const& import_module : core_module.dependencies.alias_imports)
        {
            if (all_names.contains(import_module.alias))
            {
                diagnostics.push_back(
                    create_error_diagnostic(
                        core_module.source_file_path,
                        create_sub_source_range(
                            import_module.source_range,
                            11 + import_module.module_name.size(),
                            import_module.alias.size()
                        ),
                        std::format("Duplicate import alias '{}'.", import_module.alias)
                    )
                );
            }
            else
            {
                all_names.insert(import_module.alias);
            }

            auto const location = declaration_database.map.find(import_module.module_name);
            if (location == declaration_database.map.end())
            {
                diagnostics.push_back(
                    create_error_diagnostic(
                        core_module.source_file_path,
                        create_sub_source_range(
                            import_module.source_range,
                            7,
                            import_module.module_name.size()
                        ),
                        std::format("Cannot find module '{}'.", import_module.module_name)
                    )
                );
            }
        }

        return diagnostics;
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_type_references(
        h::Module const& core_module,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<h::compiler::Diagnostic> diagnostics{temporaries_allocator};

        auto const process_type_reference = [&](
            std::string_view const declaration_name,
            h::Type_reference const& type
        ) -> bool
        {
            std::pmr::vector<h::compiler::Diagnostic> const current_diagnostics = validate_type_reference(
                core_module,
                type,
                declaration_database,
                temporaries_allocator
            );

            if (!current_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), current_diagnostics.begin(), current_diagnostics.end());

            return false;
        };

        visit_type_references_recursively_with_declaration_name(core_module, process_type_reference);

        return diagnostics;
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_soa_array_type(
        h::Module const& core_module,
        h::Type_reference const& type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        (void)temporaries_allocator;

        h::Soa_array_type const& soa_array_type = std::get<h::Soa_array_type>(type.data);
        if (soa_array_type.value_type.empty())
            return {};

        h::Type_reference const& element_type = soa_array_type.value_type.front();

        if (!is_custom_type_reference(element_type))
        {
            return
            {
                create_error_diagnostic_with_code(
                    core_module.source_file_path,
                    element_type.source_range.has_value() ? element_type.source_range : type.source_range,
                    "Soa_array element type must be a struct type.",
                    Diagnostic_code::Soa_element_type_not_a_struct,
                    {}
                )
            };
        }

        std::optional<Declaration> const declaration = find_declaration(
            declaration_database,
            element_type
        );

        if (!declaration.has_value())
            return {};

        if (!std::holds_alternative<Struct_declaration const*>(declaration.value().data))
        {
            return
            {
                create_error_diagnostic_with_code(
                    core_module.source_file_path,
                    element_type.source_range.has_value() ? element_type.source_range : type.source_range,
                    "Soa_array element type must be a struct type.",
                    Diagnostic_code::Soa_element_type_not_a_struct,
                    {}
                )
            };
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_type_reference(
        h::Module const& core_module,
        h::Type_reference const& type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        if (is_custom_type_reference(type))
        {   
            return validate_custom_type_reference(
                core_module,
                type,
                declaration_database,
                temporaries_allocator
            );
        }
        else if (std::holds_alternative<h::Integer_type>(type.data))
        {
            return validate_integer_type(
                core_module,
                type,
                declaration_database,
                temporaries_allocator
            );
        }
        else if (std::holds_alternative<h::Soa_array_type>(type.data))
        {
            return validate_soa_array_type(
                core_module,
                type,
                declaration_database,
                temporaries_allocator
            );
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_custom_type_reference(
        h::Module const& core_module,
        h::Type_reference const& type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::optional<Declaration> const declaration = find_declaration(
            declaration_database,
            type
        );

        if (!declaration.has_value())
        {
            std::pmr::string const type_name = h::format_type_reference(core_module, type, temporaries_allocator, temporaries_allocator);

            return
            {
                create_error_diagnostic(
                    core_module.source_file_path,
                    type.source_range,
                    std::format("Type '{}' does not exist.", type_name)
                )
            };
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_integer_type(
        h::Module const& core_module,
        h::Type_reference const& type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        h::Integer_type const& integer_type = std::get<h::Integer_type>(type.data);

        if (integer_type.number_of_bits != 8 && integer_type.number_of_bits != 16 && integer_type.number_of_bits != 32 && integer_type.number_of_bits != 64)
        {
            std::pmr::string const type_name = h::format_type_reference(core_module, type, temporaries_allocator, temporaries_allocator);

            return
            {
                create_error_diagnostic(
                    core_module.source_file_path,
                    type.source_range,
                    std::format("Type '{}' does not exist.", type_name)
                )
            };
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_declarations(
        h::Module const& core_module,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<h::compiler::Diagnostic> diagnostics{temporaries_allocator};

        std::pmr::unordered_set<std::string_view> all_names{temporaries_allocator};

        std::array<std::string_view, 14> const reserved_keywords = get_reserved_keywords();

        auto const process_declaration_name = [&](
            std::string_view const name,
            std::optional<Source_range_location> const& source_location
        ) -> void
        {
            if (all_names.contains(name))
            {
                diagnostics.push_back(
                    create_error_diagnostic(
                        core_module.source_file_path,
                        create_source_range_from_source_location(source_location, name.size()),
                        std::format("Duplicate declaration name '{}'.", name)
                    )
                );
            }
            else
            {
                all_names.insert(name);
            }

            auto const location = std::find(reserved_keywords.begin(), reserved_keywords.end(), name);
            if (location != reserved_keywords.end())
            {
                diagnostics.push_back(
                    create_error_diagnostic(
                        core_module.source_file_path,
                        create_source_range_from_source_location(source_location, name.size()),
                        std::format("Invalid declaration name '{}' which is a reserved keyword.", name)
                    )
                );
            }
        };

        for (Alias_type_declaration const& declaration : core_module.export_declarations.alias_type_declarations)
        {
            process_declaration_name(declaration.name, declaration.source_location);
        }

        for (Alias_type_declaration const& declaration : core_module.internal_declarations.alias_type_declarations)
        {
            process_declaration_name(declaration.name, declaration.source_location);
        }

        for (Enum_declaration const& declaration : core_module.export_declarations.enum_declarations)
        {
            process_declaration_name(declaration.name, declaration.source_location);

            std::pmr::vector<h::compiler::Diagnostic> declaration_diagnostics = validate_enum_declaration(
                core_module,
                declaration,
                declaration_database,
                temporaries_allocator
            );
            
            if (!declaration_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), declaration_diagnostics.begin(), declaration_diagnostics.end());
        }

        for (Enum_declaration const& declaration : core_module.internal_declarations.enum_declarations)
        {
            process_declaration_name(declaration.name, declaration.source_location);

            std::pmr::vector<h::compiler::Diagnostic> const declaration_diagnostics = validate_enum_declaration(
                core_module,
                declaration,
                declaration_database,
                temporaries_allocator
            );
            
            if (!declaration_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), declaration_diagnostics.begin(), declaration_diagnostics.end());
        }

        for (Global_variable_declaration const& declaration : core_module.export_declarations.global_variable_declarations)
        {
            process_declaration_name(declaration.name, declaration.source_location);

            std::pmr::vector<h::compiler::Diagnostic> const declaration_diagnostics = validate_global_variable_declaration(
                core_module,
                declaration,
                declaration_database,
                temporaries_allocator
            );
            
            if (!declaration_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), declaration_diagnostics.begin(), declaration_diagnostics.end());
        }

        for (Global_variable_declaration const& declaration : core_module.internal_declarations.global_variable_declarations)
        {
            process_declaration_name(declaration.name, declaration.source_location);

            std::pmr::vector<h::compiler::Diagnostic> const declaration_diagnostics = validate_global_variable_declaration(
                core_module,
                declaration,
                declaration_database,
                temporaries_allocator
            );
            
            if (!declaration_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), declaration_diagnostics.begin(), declaration_diagnostics.end());
        }

        for (Struct_declaration const& declaration : core_module.export_declarations.struct_declarations)
        {
            process_declaration_name(declaration.name, declaration.source_location);

            std::pmr::vector<h::compiler::Diagnostic> const declaration_diagnostics = validate_struct_declaration(
                core_module,
                declaration,
                declaration_database,
                temporaries_allocator
            );

            if (!declaration_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), declaration_diagnostics.begin(), declaration_diagnostics.end());
        }

        for (Struct_declaration const& declaration : core_module.internal_declarations.struct_declarations)
        {
            process_declaration_name(declaration.name, declaration.source_location);

            std::pmr::vector<h::compiler::Diagnostic> const declaration_diagnostics = validate_struct_declaration(
                core_module,
                declaration,
                declaration_database,
                temporaries_allocator
            );

            if (!declaration_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), declaration_diagnostics.begin(), declaration_diagnostics.end());
        }

        for (Union_declaration const& declaration : core_module.export_declarations.union_declarations)
        {
            process_declaration_name(declaration.name, declaration.source_location);

            std::pmr::vector<h::compiler::Diagnostic> const declaration_diagnostics = validate_union_declaration(
                core_module,
                declaration,
                declaration_database,
                temporaries_allocator
            );

            if (!declaration_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), declaration_diagnostics.begin(), declaration_diagnostics.end());
        }

        for (Union_declaration const& declaration : core_module.internal_declarations.union_declarations)
        {
            process_declaration_name(declaration.name, declaration.source_location);

            std::pmr::vector<h::compiler::Diagnostic> const declaration_diagnostics = validate_union_declaration(
                core_module,
                declaration,
                declaration_database,
                temporaries_allocator
            );

            if (!declaration_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), declaration_diagnostics.begin(), declaration_diagnostics.end());
        }

        for (Function_declaration const& declaration : core_module.export_declarations.function_declarations)
        {
            process_declaration_name(declaration.name, declaration.source_location);

            std::optional<Function_definition const*> const definition = find_function_definition(core_module, declaration.name);

            std::pmr::vector<h::compiler::Diagnostic> const function_diagnostics = validate_function(
                core_module,
                declaration,
                definition.has_value() ? definition.value() : nullptr,
                declaration_database,
                temporaries_allocator
            );

            if (!function_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), function_diagnostics.begin(), function_diagnostics.end());
        }

        for (Function_declaration const& declaration : core_module.internal_declarations.function_declarations)
        {
            process_declaration_name(declaration.name, declaration.source_location);

            std::optional<Function_definition const*> const definition = find_function_definition(core_module, declaration.name);

            std::pmr::vector<h::compiler::Diagnostic> const function_diagnostics = validate_function(
                core_module,
                declaration,
                definition.has_value() ? definition.value() : nullptr,
                declaration_database,
                temporaries_allocator
            );

            if (!function_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), function_diagnostics.begin(), function_diagnostics.end());
        }

        for (Function_constructor const& declaration : core_module.export_declarations.function_constructors)
        {
            process_declaration_name(declaration.name, declaration.source_location);
        }

        for (Function_constructor const& declaration : core_module.internal_declarations.function_constructors)
        {
            process_declaration_name(declaration.name, declaration.source_location);
        }

        for (Type_constructor const& declaration : core_module.export_declarations.type_constructors)
        {
            process_declaration_name(declaration.name, declaration.source_location);
        }

        for (Type_constructor const& declaration : core_module.internal_declarations.type_constructors)
        {
            process_declaration_name(declaration.name, declaration.source_location);
        }

        sort_diagnostics(diagnostics);
        return diagnostics;
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_enum_declaration(
        h::Module const& core_module,
        h::Enum_declaration const& declaration,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<h::compiler::Diagnostic> diagnostics{temporaries_allocator};

        std::pmr::unordered_set<std::string_view> all_names{temporaries_allocator};

        Scope scope;

        Type_reference const int32_type = create_integer_type_type_reference(32, true);

        for (Enum_value const& value : declaration.values)
        {
            if (all_names.contains(value.name))
            {
                diagnostics.push_back(
                    create_error_diagnostic(
                        core_module.source_file_path,
                        create_source_range_from_source_location(value.source_location, value.name.size()),
                        std::format("Duplicate enum value name '{}.{}'.", declaration.name, value.name)
                    )
                );
            }
            else
            {
                all_names.insert(value.name);
            }

            if (value.value.has_value())
            {
                h::Statement const& statement = value.value.value();

                std::pmr::vector<std::optional<Type_info>> const expression_types = calculate_expression_type_infos_of_statement(
                    core_module,
                    nullptr,
                    scope,
                    statement,
                    std::nullopt,
                    declaration_database,
                    temporaries_allocator
                );

                bool const is_compile_time = is_computable_at_compile_time(
                    core_module,
                    scope,
                    statement,
                    expression_types,
                    declaration_database
                );

                if (!is_compile_time)
                {
                    diagnostics.push_back(
                        create_error_diagnostic(
                            core_module.source_file_path,
                            get_statement_source_range(statement),
                            std::format("The value of '{}.{}' must be computable at compile-time.", declaration.name, value.name)
                        )
                    );
                    continue;
                }

                std::optional<Type_reference> const type = get_expression_type(
                    core_module,
                    nullptr,
                    scope,
                    statement,
                    std::nullopt,
                    declaration_database
                );
                
                if (!type.has_value() || type.value() != int32_type)
                {
                    diagnostics.push_back(
                        create_error_diagnostic(
                            core_module.source_file_path,
                            get_statement_source_range(statement),
                            std::format("Enum value '{}.{}' must be a Int32 type.", declaration.name, value.name)
                        )
                    );
                }
            }

            scope.variables.push_back(
                create_variable(value.name, int32_type, false, true, value.source_location.has_value() ? std::optional<h::Source_position>{h::Source_position{value.source_location->line, value.source_location->column}} : std::optional<h::Source_position>{std::nullopt})
            );
        }

        return diagnostics;
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_global_variable_declaration(
        h::Module const& core_module,
        h::Global_variable_declaration const& declaration,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<std::optional<Type_info>> const expression_types = calculate_expression_type_infos_of_statement(
            core_module,
            nullptr,
            {},
            declaration.initial_value,
            declaration.type,
            declaration_database,
            temporaries_allocator
        );

        bool const is_compile_time = is_computable_at_compile_time(
            core_module,
            {},
            declaration.initial_value,
            expression_types,
            declaration_database
        );

        if (!is_compile_time)
        {
            return
            {
                create_error_diagnostic(
                    core_module.source_file_path,
                    get_statement_source_range(declaration.initial_value),
                    std::format("The value of '{}' must be computable at compile-time.", declaration.name)
                )
            };
        }

        if (declaration.type.has_value())
        {
            std::optional<h::Type_reference> const& type_reference = get_expression_type_from_type_info(expression_types, 0);

            if (!are_compatible_types(declaration_database, declaration.type, type_reference))
            {
                std::pmr::string const provided_type_name = h::format_type_reference(core_module, type_reference, temporaries_allocator, temporaries_allocator);
                std::pmr::string const expected_type_name = h::format_type_reference(core_module, declaration.type, temporaries_allocator, temporaries_allocator);

                return
                {
                    create_error_diagnostic_with_code(
                        core_module.source_file_path,
                        get_statement_source_range(declaration.initial_value),
                        std::format("Expression type '{}' does not match expected type '{}'.", provided_type_name, expected_type_name),
                        Diagnostic_code::Type_mismatch,
                        create_diagnostic_mismatch_type_data(type_reference, declaration.type)
                    )
                };
            }
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_struct_declaration(
        h::Module const& core_module,
        h::Struct_declaration const& declaration,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<h::compiler::Diagnostic> diagnostics{temporaries_allocator};

        std::pmr::unordered_set<std::string_view> all_names{temporaries_allocator};

        for (std::size_t member_index = 0; member_index < declaration.member_names.size(); ++member_index)
        {
            std::string_view const member_name = declaration.member_names[member_index];

            std::optional<Source_position> const member_source_position =
                declaration.member_source_positions.has_value() ?
                declaration.member_source_positions.value()[member_index] :
                std::optional<Source_position>{std::nullopt};

            if (all_names.contains(member_name))
            {
                diagnostics.push_back(
                    create_error_diagnostic(
                        core_module.source_file_path,
                        create_source_range_from_source_position(member_source_position, member_name.size()),
                        std::format("Duplicate struct member name '{}.{}'.", declaration.name, member_name)
                    )
                );
            }
            else
            {
                all_names.insert(member_name);
            }

            h::Type_reference const& member_type = declaration.member_types[member_index];
            h::Statement const& member_default_value = declaration.member_default_values[member_index];

            std::pmr::vector<std::optional<Type_info>> const expression_types = calculate_expression_type_infos_of_statement(
                core_module,
                nullptr,
                {},
                member_default_value,
                member_type,
                declaration_database,
                temporaries_allocator
            );

            bool const is_compile_time = is_computable_at_compile_time(
                core_module,
                {},
                member_default_value,
                expression_types,
                declaration_database
            );

            if (!is_compile_time)
            {
                diagnostics.push_back(
                    create_error_diagnostic(
                        core_module.source_file_path,
                        get_statement_source_range(member_default_value),
                        std::format("The value of '{}.{}' must be computable at compile-time.", declaration.name, member_name)
                    )
                );
                continue;
            }

            std::optional<Type_reference> const& default_value_type = get_expression_type_from_type_info(expression_types, 0);

            if (!are_compatible_types(declaration_database, default_value_type, member_type))
            {
                std::pmr::string const provided_type_name = h::format_type_reference(core_module, default_value_type, temporaries_allocator, temporaries_allocator);
                std::pmr::string const expected_type_name = h::format_type_reference(core_module, member_type, temporaries_allocator, temporaries_allocator);

                diagnostics.push_back(
                    create_error_diagnostic_with_code(
                        core_module.source_file_path,
                        get_statement_source_range(member_default_value),
                        std::format(
                            "Expression type '{}' does not match expected type '{}'.",
                            provided_type_name,
                            expected_type_name
                        ),
                        Diagnostic_code::Type_mismatch,
                        create_diagnostic_mismatch_type_data(default_value_type, member_type)
                    )
                );
            }
        }

        return diagnostics;
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_union_declaration(
        h::Module const& core_module,
        h::Union_declaration const& declaration,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<h::compiler::Diagnostic> diagnostics{temporaries_allocator};

        std::pmr::unordered_set<std::string_view> all_names{temporaries_allocator};

        for (std::size_t member_index = 0; member_index < declaration.member_names.size(); ++member_index)
        {
            std::string_view const member_name = declaration.member_names[member_index];

            std::optional<Source_position> const member_source_position =
                declaration.member_source_positions.has_value() ?
                declaration.member_source_positions.value()[member_index] :
                std::optional<Source_position>{std::nullopt};

            if (all_names.contains(member_name))
            {
                diagnostics.push_back(
                    create_error_diagnostic(
                        core_module.source_file_path,
                        create_source_range_from_source_position(member_source_position, member_name.size()),
                        std::format("Duplicate union member name '{}.{}'.", declaration.name, member_name)
                    )
                );
            }
            else
            {
                all_names.insert(member_name);
            }
        }

        return diagnostics;
    }

    static std::pmr::vector<h::compiler::Diagnostic> create_function_missing_return_diagnostic(
        h::Module const& core_module,
        std::string_view const function_name,
        std::optional<Source_range> const& source_range
    )
    {
        return
        {
            create_error_diagnostic(
                core_module.source_file_path,
                source_range,
                std::format("'{}.{}': not all control paths return a value.", core_module.name, function_name)
            )
        };
    }

    static std::pmr::vector<h::compiler::Diagnostic> validate_function_return_expressions_with_statements(
        h::Module const& core_module,
        std::string_view const function_name,
        std::span<h::Statement const> const statements,
        std::optional<Source_range> const source_range
    );

    static std::pmr::vector<h::compiler::Diagnostic> validate_function_return_expressions_with_statement_expression(
        h::Module const& core_module,
        std::string_view const function_name,
        h::Statement const& last_statement,
        h::Expression const& last_statement_expression,
        std::optional<Source_range> const source_range
    )
    {
        if (std::holds_alternative<h::Return_expression>(last_statement_expression.data))
        {
            return {};
        }
        else if (std::holds_alternative<h::Block_expression>(last_statement_expression.data))
        {
            h::Block_expression const& block_expression = std::get<h::Block_expression>(last_statement_expression.data);
            return validate_function_return_expressions_with_statements(core_module, function_name, block_expression.statements, source_range);
        }
        else if (std::holds_alternative<h::Compile_time_expression>(last_statement_expression.data))
        {
            h::Compile_time_expression const& compile_time_expression = std::get<h::Compile_time_expression>(last_statement_expression.data);
            h::Expression const& right_side_expression = last_statement.expressions[compile_time_expression.expression.expression_index];
            return validate_function_return_expressions_with_statement_expression(core_module, function_name, last_statement, right_side_expression, source_range);
        }
        else if (std::holds_alternative<h::If_expression>(last_statement_expression.data))
        {
            h::If_expression const& if_expression = std::get<h::If_expression>(last_statement_expression.data);

            for (h::Condition_statement_pair const& serie : if_expression.series)
            {
                std::pmr::vector<h::compiler::Diagnostic> const serie_diagnostics = validate_function_return_expressions_with_statements(core_module, function_name, serie.then_statements, source_range);
                if (!serie_diagnostics.empty())
                    return serie_diagnostics;
            }

            bool const has_else_clause = !if_expression.series.empty() && !if_expression.series.back().condition.has_value();
            if (!has_else_clause)
                return create_function_missing_return_diagnostic(core_module, function_name, source_range);

            return {};
        }
        else if (std::holds_alternative<h::Switch_expression>(last_statement_expression.data))
        {
            h::Switch_expression const& switch_expression = std::get<h::Switch_expression>(last_statement_expression.data);

            for (h::Switch_case_expression_pair const& switch_case : switch_expression.cases)
            {
                std::pmr::vector<h::compiler::Diagnostic> const switch_case_diagnostics = validate_function_return_expressions_with_statements(core_module, function_name, switch_case.statements, source_range);
                if (!switch_case_diagnostics.empty())
                    return switch_case_diagnostics;
            }

            return {};
        }
        else
        {
            return create_function_missing_return_diagnostic(core_module, function_name, source_range);
        }
    }

    static std::pmr::vector<h::compiler::Diagnostic> validate_function_return_expressions_with_statements(
        h::Module const& core_module,
        std::string_view const function_name,
        std::span<h::Statement const> const statements,
        std::optional<Source_range> const source_range
    )
    {
        if (statements.empty())
            return create_function_missing_return_diagnostic(core_module, function_name, source_range);

        h::Statement const& last_statement = statements[statements.size() - 1];
        if (last_statement.expressions.empty())
            return create_function_missing_return_diagnostic(core_module, function_name, source_range);

        h::Expression const& last_statement_expression = last_statement.expressions[0];
        return validate_function_return_expressions_with_statement_expression(core_module, function_name, last_statement, last_statement_expression, source_range);
    }

    static std::pmr::vector<h::compiler::Diagnostic> validate_function_return_expressions(
        h::Module const& core_module,
        h::Function_declaration const& declaration,
        h::Function_definition const& definition
    )
    {
        if (declaration.type.output_parameter_types.empty())
            return {};

        std::optional<h::Source_range> const source_range = declaration.source_location.has_value() ? declaration.source_location->range : std::optional<h::Source_range>{std::nullopt};
        return validate_function_return_expressions_with_statements(core_module, declaration.name, definition.statements, source_range);
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_function(
        h::Module const& core_module,
        h::Function_declaration const& declaration,
        h::Function_definition const* const definition,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<h::compiler::Diagnostic> diagnostics{temporaries_allocator};

        // TODO validate parameters

        {
            Scope scope
            {
                .variables{temporaries_allocator}
            };

            add_parameters_to_scope(
                scope,
                declaration.input_parameter_names,
                declaration.type.input_parameter_types,
                declaration.input_parameter_source_positions
            );

            std::pmr::vector<h::compiler::Diagnostic> pre_condition_diagnostics = validate_function_contracts(
                core_module,
                declaration,
                scope,
                declaration.preconditions,
                declaration_database,
                temporaries_allocator
            );
            if (!pre_condition_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), pre_condition_diagnostics.begin(), pre_condition_diagnostics.end());

            add_parameters_to_scope(
                scope,
                declaration.output_parameter_names,
                declaration.type.output_parameter_types,
                declaration.output_parameter_source_positions
            );

            std::pmr::vector<h::compiler::Diagnostic> post_condition_diagnostics = validate_function_contracts(
                core_module,
                declaration,
                scope,
                declaration.postconditions,
                declaration_database,
                temporaries_allocator
            );
            if (!post_condition_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), post_condition_diagnostics.begin(), post_condition_diagnostics.end());
        }

        if (definition != nullptr)
        {
            Scope scope
            {
                .variables{temporaries_allocator}
            };

            add_parameters_to_scope(
                scope,
                declaration.input_parameter_names,
                declaration.type.input_parameter_types,
                declaration.input_parameter_source_positions
            );

            std::pmr::vector<h::compiler::Diagnostic> const definition_diagnostics = validate_statements(
                core_module,
                &declaration,
                scope,
                definition->statements,
                declaration_database,
                temporaries_allocator
            );
            if (!definition_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), definition_diagnostics.begin(), definition_diagnostics.end());

            std::pmr::vector<h::compiler::Diagnostic> function_missing_return_diagnostics = validate_function_return_expressions(
                core_module,
                declaration,
                *definition
            );
            if (!function_missing_return_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), function_missing_return_diagnostics.begin(), function_missing_return_diagnostics.end());
        }

        return diagnostics;
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_function_contracts(
        h::Module const& core_module,
        Function_declaration const& function_declaration,
        h::compiler::Scope const& scope,
        std::span<h::Function_condition const> const function_conditions,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<h::compiler::Diagnostic> diagnostics{temporaries_allocator};

        for (h::Function_condition const& function_condition : function_conditions)
        {
            std::pmr::vector<h::compiler::Diagnostic> statement_diagnostics = validate_statement(
                core_module,
                &function_declaration,
                scope,
                function_condition.condition,
                create_bool_type_reference(),
                declaration_database,
                temporaries_allocator
            );
            if (!statement_diagnostics.empty())
            {
                diagnostics.insert(diagnostics.end(), statement_diagnostics.begin(), statement_diagnostics.end());
                continue;
            }

            std::optional<h::Type_reference> const condition_type_optional = get_expression_type(
                core_module,
                &function_declaration,
                scope,
                function_condition.condition,
                std::nullopt,
                declaration_database
            );

            if (!condition_type_optional.has_value() || (!is_bool(condition_type_optional.value()) && !is_c_bool(condition_type_optional.value())))
            {
                std::pmr::string const provided_type_name = h::format_type_reference(core_module, condition_type_optional, temporaries_allocator, temporaries_allocator);

                diagnostics.push_back(
                    create_error_diagnostic(
                        core_module.source_file_path,
                        get_statement_source_range(function_condition.condition),
                        std::format("Expression type '{}' does not match expected type 'Bool'.", provided_type_name)
                    )
                );
            }

            auto const process_expression = [&](h::Expression const& expression, h::Statement const& statement) -> bool
            {
                bool const is_mutable_global_constant = is_mutable_global_variable(
                    core_module.name,
                    expression,
                    declaration_database
                );
                if (is_mutable_global_constant)
                {
                    diagnostics.push_back(
                        create_error_diagnostic(
                            core_module.source_file_path,
                            expression.source_range,
                            "Cannot use mutable global variable in function preconditions and postconditions. Consider making the global constant."
                        )
                    );
                }

                return false;
            };

            visit_expressions(
                function_condition.condition,
                process_expression
            );
        }

        return diagnostics;
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_statements(
        h::Module const& core_module,
        Function_declaration const* const function_declaration,
        Scope const& scope,
        std::span<h::Statement const> const statements,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<h::compiler::Diagnostic> diagnostics{temporaries_allocator};

        Scope new_scope = scope;

        for (std::size_t statement_index = 0; statement_index < statements.size(); ++statement_index)
        {
            h::Statement const& statement = statements[statement_index];

            std::pmr::vector<h::compiler::Diagnostic> const statement_diagnostics = validate_statement(
                core_module,
                function_declaration,
                new_scope,
                statement,
                std::nullopt,
                declaration_database,
                temporaries_allocator
            );
            if (!statement_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), statement_diagnostics.begin(), statement_diagnostics.end());

            if (!statement.expressions.empty())
            {
                h::Expression const& expression = statement.expressions[0];
                
                if (std::holds_alternative<h::Variable_declaration_expression>(expression.data))
                {
                    h::Variable_declaration_expression const& variable_declaration = std::get<h::Variable_declaration_expression>(expression.data);

                    std::optional<h::Type_reference> variable_type = get_expression_type(
                        core_module,
                        function_declaration,
                        new_scope,
                        statement,
                        statement.expressions[variable_declaration.right_hand_side.expression_index],
                        std::nullopt,
                        declaration_database
                    );

                    if (variable_type.has_value())
                    {
                        new_scope.variables.push_back(
                            create_variable(variable_declaration.name, std::move(variable_type.value()), variable_declaration.is_mutable, false, expression.source_range)
                        );
                    }
                }
                else if (std::holds_alternative<h::Variable_declaration_with_type_expression>(expression.data))
                {
                    h::Variable_declaration_with_type_expression const& variable_declaration_with_type = std::get<h::Variable_declaration_with_type_expression>(expression.data);

                    std::optional<h::Type_reference> const variable_type = h::get_variable_declaration_with_type_expression_type(statement, variable_declaration_with_type);
                    if (variable_type.has_value())
                    {
                        new_scope.variables.push_back(
                            create_variable(variable_declaration_with_type.name, std::move(variable_type.value()), variable_declaration_with_type.is_mutable, false, expression.source_range)
                        );
                    }
                }
            }
        }

        return diagnostics;
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_statement(
        h::Module const& core_module,
        Function_declaration const* const function_declaration,
        Scope const& scope,
        h::Statement const& statement,
        std::optional<h::Type_reference> const& expected_statement_type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<std::optional<Type_info>> const expression_types = calculate_expression_type_infos_of_statement(
            core_module,
            function_declaration,
            scope,
            statement,
            expected_statement_type,
            declaration_database,
            temporaries_allocator
        );

        Validate_expression_parameters parameters
        {
            .core_module = core_module,
            .function_declaration = function_declaration,
            .scope = scope,
            .statement = statement,
            .expected_statement_type = expected_statement_type,
            .expression_types = expression_types,
            .expression_index = 0,
            .declaration_database = declaration_database,
            .temporaries_allocator = temporaries_allocator
        };

        for (std::size_t index = 0; index < statement.expressions.size(); ++index)
        {
            std::size_t const expression_index = statement.expressions.size() - 1 - index;
            parameters.expression_index = expression_index;

            std::pmr::vector<h::compiler::Diagnostic> diagnostics = validate_expression(
                parameters
            );

            if (!diagnostics.empty())
                return diagnostics;
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_expression(
        Validate_expression_parameters const& parameters
    )
    {
        h::Expression const& expression = parameters.statement.expressions[parameters.expression_index];

        if (std::holds_alternative<h::Access_expression>(expression.data))
        {
            h::Access_expression const& value = std::get<h::Access_expression>(expression.data);
            return validate_access_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Assert_expression>(expression.data))
        {
            h::Assert_expression const& value = std::get<h::Assert_expression>(expression.data);
            return validate_assert_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Assignment_expression>(expression.data))
        {
            h::Assignment_expression const& value = std::get<h::Assignment_expression>(expression.data);
            return validate_assignment_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Binary_expression>(expression.data))
        {
            h::Binary_expression const& value = std::get<h::Binary_expression>(expression.data);
            return validate_binary_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Block_expression>(expression.data))
        {
            h::Block_expression const& value = std::get<h::Block_expression>(expression.data);
            return validate_block_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Break_expression>(expression.data))
        {
            h::Break_expression const& value = std::get<h::Break_expression>(expression.data);
            return validate_break_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Call_expression>(expression.data))
        {
            h::Call_expression const& value = std::get<h::Call_expression>(expression.data);
            return validate_call_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Cast_expression>(expression.data))
        {
            h::Cast_expression const& value = std::get<h::Cast_expression>(expression.data);
            return validate_cast_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Continue_expression>(expression.data))
        {
            h::Continue_expression const& value = std::get<h::Continue_expression>(expression.data);
            return validate_continue_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::For_loop_expression>(expression.data))
        {
            h::For_loop_expression const& value = std::get<h::For_loop_expression>(expression.data);
            return validate_for_loop_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::If_expression>(expression.data))
        {
            h::If_expression const& value = std::get<h::If_expression>(expression.data);
            return validate_if_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Instantiate_expression>(expression.data))
        {
            h::Instantiate_expression const& value = std::get<h::Instantiate_expression>(expression.data);
            return validate_instantiate_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Reflection_expression>(expression.data))
        {
            h::Reflection_expression const& value = std::get<h::Reflection_expression>(expression.data);
            return validate_reflection_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Return_expression>(expression.data))
        {
            h::Return_expression const& value = std::get<h::Return_expression>(expression.data);
            return validate_return_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Switch_expression>(expression.data))
        {
            h::Switch_expression const& value = std::get<h::Switch_expression>(expression.data);
            return validate_switch_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Ternary_condition_expression>(expression.data))
        {
            h::Ternary_condition_expression const& value = std::get<h::Ternary_condition_expression>(expression.data);
            return validate_ternary_condition_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Unary_expression>(expression.data))
        {
            h::Unary_expression const& value = std::get<h::Unary_expression>(expression.data);
            return validate_unary_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Variable_declaration_expression>(expression.data))
        {
            h::Variable_declaration_expression const& value = std::get<h::Variable_declaration_expression>(expression.data);
            return validate_variable_declaration_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Variable_declaration_with_type_expression>(expression.data))
        {
            h::Variable_declaration_with_type_expression const& value = std::get<h::Variable_declaration_with_type_expression>(expression.data);
            return validate_variable_declaration_with_type_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::Variable_expression>(expression.data))
        {
            h::Variable_expression const& value = std::get<h::Variable_expression>(expression.data);
            return validate_variable_expression(parameters, value, expression.source_range);
        }
        else if (std::holds_alternative<h::While_loop_expression>(expression.data))
        {
            h::While_loop_expression const& value = std::get<h::While_loop_expression>(expression.data);
            return validate_while_loop_expression(parameters, value, expression.source_range);
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_access_expression(
        Validate_expression_parameters const& parameters,
        h::Access_expression const& access_expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        std::optional<h::Type_reference> const& left_hand_side_type = get_expression_type_from_type_info(parameters.expression_types, access_expression.expression);

        if (left_hand_side_type.has_value())
        {
            std::optional<Declaration> const declaration_optional = h::find_underlying_declaration(
                parameters.declaration_database,
                left_hand_side_type.value()
            );
            if (declaration_optional.has_value())
            {
                Declaration const& declaration = declaration_optional.value();

                std::pmr::vector<Declaration_member_info> const member_infos = get_declaration_member_infos(
                    declaration,
                    parameters.temporaries_allocator
                );

                auto const member_location = std::find_if(
                    member_infos.begin(),
                    member_infos.end(),
                    [&](Declaration_member_info const& member_info) -> bool { return member_info.member_name == access_expression.member_name; }
                );
                if (member_location == member_infos.end())
                {
                    std::optional<Declaration> const function_declaration = find_underlying_declaration(
                        parameters.declaration_database,
                        declaration.module_name,
                        access_expression.member_name
                    );
                    if (!function_declaration.has_value() || (!std::holds_alternative<h::Function_declaration const*>(function_declaration->data) && !std::holds_alternative<h::Function_constructor const*>(function_declaration->data)))
                    {
                        std::pmr::string const type_full_name = h::format_type_reference(parameters.core_module, left_hand_side_type.value(), parameters.temporaries_allocator, parameters.temporaries_allocator);

                        return
                        {
                            create_error_diagnostic(
                                parameters.core_module.source_file_path,
                                source_range,
                                std::format(
                                    "Member '{}' does not exist in the type '{}'.",
                                    access_expression.member_name,
                                    type_full_name
                                )
                            )
                        };
                    }
                }
            }

            if (std::holds_alternative<h::Array_slice_type>(left_hand_side_type->data))
            {
                if (access_expression.member_name != "data" && access_expression.member_name != "length")
                {
                    std::pmr::string const type_full_name = h::format_type_reference(parameters.core_module, left_hand_side_type.value(), parameters.temporaries_allocator, parameters.temporaries_allocator);

                    return
                    {
                        create_error_diagnostic(
                            parameters.core_module.source_file_path,
                            source_range,
                            std::format(
                                "Member '{}' does not exist in the type '{}'.",
                                access_expression.member_name,
                                type_full_name
                            )
                        )
                    };
                }
            }
            else if (std::holds_alternative<h::Soa_array_type>(left_hand_side_type->data))
            {
                if (access_expression.member_name != "data" && access_expression.member_name != "length")
                {
                    std::pmr::string const type_full_name = h::format_type_reference(parameters.core_module, left_hand_side_type.value(), parameters.temporaries_allocator, parameters.temporaries_allocator);

                    return
                    {
                        create_error_diagnostic(
                            parameters.core_module.source_file_path,
                            source_range,
                            std::format(
                                "Member '{}' does not exist in the type '{}'.",
                                access_expression.member_name,
                                type_full_name
                            )
                        )
                    };
                }
            }
        }
        else
        {
            h::Expression const& left_hand_side_expression = parameters.statement.expressions[access_expression.expression.expression_index];

            if (std::holds_alternative<h::Variable_expression>(left_hand_side_expression.data))
            {
                h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(left_hand_side_expression.data);

                // Try enum:
                {
                    std::optional<Declaration> const declaration_optional = find_underlying_declaration(
                        parameters.declaration_database,
                        parameters.core_module.name,
                        variable_expression.name
                    );
                    if (declaration_optional.has_value())
                    {
                        if (std::holds_alternative<h::Enum_declaration const*>(declaration_optional->data))
                        {
                            h::Enum_declaration const& enum_declaration = *std::get<h::Enum_declaration const*>(declaration_optional->data);

                            auto const location = std::find_if(
                                enum_declaration.values.begin(),
                                enum_declaration.values.end(),
                                [&](h::Enum_value const& enum_value) -> bool { return enum_value.name == access_expression.member_name; }
                            );
                            if (location == enum_declaration.values.end())
                            {
                                return
                                {
                                    create_error_diagnostic(
                                        parameters.core_module.source_file_path,
                                        source_range,
                                        std::format(
                                            "Member '{}' does not exist in the type '{}'.",
                                            access_expression.member_name,
                                            enum_declaration.name
                                        )
                                    )
                                };
                            }
                        }
                    }
                }

                // Check declaration inside imported module:
                {
                    Import_module_with_alias const* const import_alias = find_import_module_with_alias(
                        parameters.core_module,
                        variable_expression.name
                    );
                    if (import_alias != nullptr)
                    {
                        std::optional<Declaration> const declaration_optional = find_underlying_declaration(
                            parameters.declaration_database,
                            import_alias->module_name,
                            access_expression.member_name
                        );

                        if (!declaration_optional.has_value())
                        {
                            return
                            {
                                create_error_diagnostic(
                                    parameters.core_module.source_file_path,
                                    source_range,
                                    std::format(
                                        "Declaration '{}' does not exist in the module '{}' (alias '{}').",
                                        access_expression.member_name,
                                        import_alias->module_name,
                                        variable_expression.name
                                    )
                                )
                            };
                        }

                        if (!declaration_optional->is_export)
                        {
                            return
                            {
                                create_error_diagnostic(
                                    parameters.core_module.source_file_path,
                                    source_range,
                                    std::format(
                                        "'{}.{}' (alias '{}') is not marked with export.",
                                        import_alias->module_name,
                                        access_expression.member_name,
                                        variable_expression.name
                                    )
                                )
                            };
                        }
                    }
                }
            }
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_assert_expression(
        Validate_expression_parameters const& parameters,
        h::Assert_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        std::optional<h::Type_reference> const condition_type_optional = get_expression_type(
            parameters.core_module,
            parameters.function_declaration,
            parameters.scope,
            expression.statement,
            std::nullopt,
            parameters.declaration_database
        );

        if (!condition_type_optional.has_value() || (!is_bool(condition_type_optional.value()) && !is_c_bool(condition_type_optional.value())))
        {
            std::pmr::string const provided_type_name = h::format_type_reference(parameters.core_module, condition_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);

            return 
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    get_statement_source_range(expression.statement),
                    std::format(
                        "Expression type '{}' does not match expected type 'Bool'.",
                        provided_type_name
                    )
                )
            };
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_binary_operation(
        Validate_expression_parameters const& parameters,
        h::Expression_index const left_hand_side,
        h::Expression_index const right_hand_side,
        h::Binary_operation const operation,
        std::optional<h::Source_range> const& source_range
    )
    {
        std::optional<h::Type_reference> const& left_hand_side_type_optional = get_expression_type_from_type_info(parameters.expression_types, left_hand_side);
        std::optional<h::Type_reference> const& right_hand_side_type_optional = get_expression_type_from_type_info(parameters.expression_types, right_hand_side);

        if (!left_hand_side_type_optional.has_value() || !right_hand_side_type_optional.has_value())
            return {};

        std::optional<h::Type_reference> const type_optional = get_underlying_type(parameters.declaration_database, left_hand_side_type_optional.value());
        if (!type_optional.has_value())
            return {};
        
        h::Type_reference const& type = type_optional.value();

        if (is_bit_shift_binary_operation(operation))
        {
            if (!is_integer(type) && !is_byte(type))
            {
                return
                {
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        source_range,
                        std::format(
                            "Binary operation '{}' can only be applied to integers or bytes.",
                            h::binary_operation_symbol_to_string(operation)
                        )
                    )
                };
            }
        }
        else if (is_bitwise_binary_operation(operation))
        {
            if (!is_integer(type) && !is_byte(type) && !is_enum_type(parameters.declaration_database, type))
            {
                return
                {
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        source_range,
                        std::format(
                            "Binary operation '{}' can only be applied to integers, bytes or enums.",
                            h::binary_operation_symbol_to_string(operation)
                        )
                    )
                };
            }
        }
        else if (is_equality_binary_operation(operation))
        {
            if (is_pointer(type) || is_null_pointer_type(type))
            {
                h::Type_reference const& right_hand_side_type = right_hand_side_type_optional.value();

                if (!is_pointer(right_hand_side_type) && !is_null_pointer_type(right_hand_side_type))
                {
                    return
                    {
                        create_error_diagnostic(
                            parameters.core_module.source_file_path,
                            source_range,
                            std::format(
                                "Binary operation '{}' can only be applied to numbers, bytes or booleans.",
                                h::binary_operation_symbol_to_string(operation)
                            )
                        )
                    };
                }
            }
            else if (!is_integer(type) && !is_floating_point(type) && !is_byte(type) && !is_bool(type) && !is_c_bool(type) && !is_enum_type(parameters.declaration_database, type))
            {
                return
                {
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        source_range,
                        std::format(
                            "Binary operation '{}' can only be applied to numbers, bytes, booleans or enums.",
                            h::binary_operation_symbol_to_string(operation)
                        )
                    )
                };
            }
        }
        else if (is_comparison_binary_operation(operation))
        {
            if (!is_integer(type) && !is_floating_point(type) && !is_decimal(type))
            {
                return
                {
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        source_range,
                        std::format(
                            "Binary operation '{}' can only be applied to numeric types.",
                            h::binary_operation_symbol_to_string(operation)
                        )
                    )
                };
            }
        }
        else if (is_logical_binary_operation(operation))
        {
            if (!is_bool(type) && !is_c_bool(type))
            {
                return
                {
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        source_range,
                        std::format(
                            "Binary operation '{}' can only be applied to a boolean value.",
                            h::binary_operation_symbol_to_string(operation)
                        )
                    )
                };
            }
        }
        else if (is_numeric_binary_operation(operation))
        {
            if (!is_integer(type) && !is_floating_point(type) && !is_decimal(type))
            {
                return
                {
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        source_range,
                        std::format(
                            "Binary operation '{}' can only be applied to numeric types.",
                            h::binary_operation_symbol_to_string(operation)
                        )
                    )
                };
            }
        }
        else if (operation == h::Binary_operation::Has)
        {
            if (!is_enum_type(parameters.declaration_database, type))
            {
                return
                {
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        source_range,
                        std::format(
                            "Binary operation 'has' can only be applied to enum values.",
                            h::binary_operation_symbol_to_string(operation)
                        )
                    )
                };
            }
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_assignment_expression(
        Validate_expression_parameters const& parameters,
        h::Assignment_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        std::optional<h::Type_reference> const& left_hand_side_type_optional = get_expression_type_from_type_info(parameters.expression_types, expression.left_hand_side);
        std::optional<h::Type_reference> const& right_hand_side_type_optional = get_expression_type_from_type_info(parameters.expression_types, expression.right_hand_side);
        
        if (!can_assign_type(parameters.declaration_database, left_hand_side_type_optional, right_hand_side_type_optional))
        {
            h::Expression const& right_hand_side_expression = parameters.statement.expressions[expression.right_hand_side.expression_index];
            std::pmr::string const left_hand_side_type_name = h::format_type_reference(parameters.core_module, left_hand_side_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);
            std::pmr::string const right_hand_side_type_name = h::format_type_reference(parameters.core_module, right_hand_side_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);

            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    right_hand_side_expression.source_range,
                    std::format(
                        "Expected type is '{}' but got '{}'.",
                        left_hand_side_type_name,
                        right_hand_side_type_name
                    )
                )
            };
        }

        std::optional<Type_info> left_hand_side_type_info = parameters.expression_types[expression.left_hand_side.expression_index];
        if (left_hand_side_type_info.has_value() && !left_hand_side_type_info->is_mutable)
        {
            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    source_range,
                    "Cannot modify non-mutable value."
                )
            };
        }

        if (expression.additional_operation.has_value())
        {
            std::pmr::vector<h::compiler::Diagnostic> diagnostics = validate_binary_operation(
                parameters,
                expression.left_hand_side,
                expression.right_hand_side,
                expression.additional_operation.value(),
                source_range
            );

            if (!diagnostics.empty())
                return diagnostics;
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_binary_expression(
        Validate_expression_parameters const& parameters,
        h::Binary_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        std::optional<h::Type_reference> const& left_hand_side_type_optional = get_expression_type_from_type_info(parameters.expression_types, expression.left_hand_side);
        std::optional<h::Type_reference> const& right_hand_side_type_optional = get_expression_type_from_type_info(parameters.expression_types, expression.right_hand_side);
        
        if (!are_compatible_types(parameters.declaration_database, left_hand_side_type_optional, right_hand_side_type_optional))
        {
            std::pmr::string const left_hand_side_type_name = h::format_type_reference(parameters.core_module, left_hand_side_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);
            std::pmr::string const right_hand_side_type_name = h::format_type_reference(parameters.core_module, right_hand_side_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);

            return
            {
                create_error_diagnostic_with_code(
                    parameters.core_module.source_file_path,
                    source_range,
                    std::format(
                        "Binary expression requires both operands to be of the same type. Left side type '{}' does not match right hand side type '{}'.",
                        left_hand_side_type_name,
                        right_hand_side_type_name
                    ),
                    Diagnostic_code::Type_mismatch,
                    create_diagnostic_mismatch_type_data(left_hand_side_type_optional, right_hand_side_type_optional)
                )
            };
        }

        return validate_binary_operation(
            parameters,
            expression.left_hand_side,
            expression.right_hand_side,
            expression.operation,
            source_range
        );
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_block_expression(
        Validate_expression_parameters const& parameters,
        h::Block_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        return validate_statements(
            parameters.core_module,
            parameters.function_declaration,
            parameters.scope,
            expression.statements,
            parameters.declaration_database,
            parameters.temporaries_allocator
        );
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_break_expression(
        Validate_expression_parameters const& parameters,
        h::Break_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        std::size_t const block_count = parameters.scope.blocks.size();
        if (block_count == 0)
        {
            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    source_range,
                    "Expression 'break' can only be placed inside for loops, while loops and switch cases."
                )
            };
        }

        if (block_count < expression.loop_count)
        {
            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    create_sub_source_range(source_range, 6, 1),
                    std::format(
                        "Expression 'break' loop count of {} is invalid.",
                        expression.loop_count
                    )
                )
            };
        }


        return {};
    }

    std::optional<std::pair<std::string_view, h::Instance_call_expression>> find_builtin_instance_call_expression(
        h::Statement const& statement,
        h::Call_expression const& expression
    )
    {
        h::Expression const& left_side_expression = statement.expressions[expression.expression.expression_index];

        if (std::holds_alternative<h::Instance_call_expression>(left_side_expression.data))
        {
            h::Instance_call_expression const& instance_call_expression = std::get<h::Instance_call_expression>(left_side_expression.data);

            h::Expression const& instance_call_left_expression = statement.expressions[instance_call_expression.left_hand_side.expression_index];

            if (std::holds_alternative<h::Variable_expression>(instance_call_left_expression.data))
            {
                h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(instance_call_left_expression.data);

                if (variable_expression.name == "create_stack_array_uninitialized")
                {
                    return std::pair<std::string_view, h::Instance_call_expression>{"create_stack_array_uninitialized", instance_call_expression};
                }
                else if (variable_expression.name == "reinterpret_as")
                {
                    return std::pair<std::string_view, h::Instance_call_expression>{"reinterpret_as", instance_call_expression};
                }
            }
        }

        return std::nullopt;
    }

    h::Function_constructor const* find_function_constructor_using_call_expression(
        std::string_view const current_module_name,
        Declaration_database const& declaration_database,
        Scope const& scope,
        h::Statement const& statement,
        h::Call_expression const& expression
    )
    {
        h::Expression const& left_side_expression = statement.expressions[expression.expression.expression_index];

        if (std::holds_alternative<h::Access_expression>(left_side_expression.data))
        {
            h::Access_expression const& access_expression = std::get<h::Access_expression>(left_side_expression.data);

            h::Expression const& access_left_side_expression = statement.expressions[access_expression.expression.expression_index];

            if (std::holds_alternative<h::Variable_expression>(access_left_side_expression.data))
            {
                h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(access_left_side_expression.data);
                // Left side can be a module alias or a variable name

                Variable const* const variable = find_variable_from_scope(
                    scope,
                    variable_expression.name
                );
                if (variable != nullptr)
                {
                    std::optional<std::string_view> const module_name = get_type_module_name(variable->type);
                    if (module_name.has_value())
                    {
                        std::optional<Declaration> const declaration = find_underlying_declaration(
                            declaration_database,
                            module_name.value(),
                            access_expression.member_name
                        );
                        if (declaration.has_value())
                        {
                            if (std::holds_alternative<h::Function_constructor const*>(declaration->data))
                                return std::get<h::Function_constructor const*>(declaration->data);
                        }
                    }

                    return nullptr;
                }

                // TODO import alias
            }
        }
        else if (std::holds_alternative<h::Variable_expression>(left_side_expression.data))
        {
            h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(left_side_expression.data);

            std::optional<Declaration> const declaration = find_underlying_declaration(declaration_database, current_module_name, variable_expression.name);
            if (declaration.has_value())
            {
                if (std::holds_alternative<h::Function_constructor const*>(declaration->data))
                    return std::get<h::Function_constructor const*>(declaration->data);
            }
        }

        return nullptr;
    }

    std::optional<h::Function_pointer_type> get_function_pointer_type_from_callable(
        Validate_expression_parameters const& parameters,
        h::Call_expression const& expression,
        std::optional<h::Type_reference> const& callable_type
    )
    {
        if (!callable_type.has_value())
            return std::nullopt;

        if (is_function_pointer(callable_type.value()))
            return std::get<h::Function_pointer_type>(callable_type->data);

        if (is_builtin_type_reference(callable_type.value()))
        {
            h::Builtin_type_reference const& builtin_type_reference = std::get<h::Builtin_type_reference>(callable_type->data);
            if (builtin_type_reference.value == "check")
            {
                h::Function_type function_type
                {
                    .input_parameter_types = { h::create_bool_type_reference() },
                    .output_parameter_types = {},
                    .is_variadic = false,
                };

                return h::Function_pointer_type
                {
                    .type = std::move(function_type),
                    .input_parameter_names = { "condition" },
                    .output_parameter_names = {}
                };
            }
            else if (builtin_type_reference.value == "create_array_slice_from_pointer")
            {
                std::pmr::vector<h::Type_reference> element_type;

                if (expression.arguments.size() > 0)
                {
                    std::optional<Type_info> const first_argument_type_info = get_expression_type_info(parameters.core_module, nullptr, parameters.scope, parameters.statement, parameters.statement.expressions[expression.arguments[0].expression_index], std::nullopt, parameters.declaration_database);
                    if (first_argument_type_info.has_value() && std::holds_alternative<h::Pointer_type>(first_argument_type_info->type.data))
                    {
                        std::optional<Type_reference> value_type = remove_pointer(first_argument_type_info->type);
                        if (value_type.has_value())
                            element_type.push_back(std::move(value_type.value()));
                    }
                }

                h::Function_type function_type
                {
                    .input_parameter_types = {
                        create_pointer_type_type_reference(element_type, false),
                        create_integer_type_type_reference(64, false)
                    },
                    .output_parameter_types = {
                        create_array_slice_type_reference(element_type, true)
                    },
                    .is_variadic = false,
                };

                return h::Function_pointer_type
                {
                    .type = std::move(function_type),
                    .input_parameter_names = {"data", "length"},
                    .output_parameter_names = {"array"}
                };
            }
            else if (builtin_type_reference.value == "offset_pointer")
            {
                h::Type_reference pointer_type;

                if (expression.arguments.size() > 0)
                {
                    std::optional<Type_info> first_argument_type_info = get_expression_type_info(parameters.core_module, nullptr, parameters.scope, parameters.statement, parameters.statement.expressions[expression.arguments[0].expression_index], std::nullopt, parameters.declaration_database);
                    if (first_argument_type_info.has_value() && std::holds_alternative<h::Pointer_type>(first_argument_type_info->type.data))
                        pointer_type = std::move(first_argument_type_info->type);
                }

                h::Function_type function_type
                {
                    .input_parameter_types = {
                        pointer_type,
                        create_integer_type_type_reference(64, true)
                    },
                    .output_parameter_types = {
                        pointer_type
                    },
                    .is_variadic = false,
                };

                return h::Function_pointer_type
                {
                    .type = std::move(function_type),
                    .input_parameter_names = {"pointer", "offset"},
                    .output_parameter_names = {"result"}
                };
            }
        }

        return std::nullopt;
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_call_expression(
        Validate_expression_parameters const& parameters,
        h::Call_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        std::optional<h::Type_reference> const& callable_type_optional = get_expression_type_from_type_info(parameters.expression_types, expression.expression);

        if (callable_type_optional.has_value() && is_builtin_type_reference(callable_type_optional.value()))
        {
            h::Builtin_type_reference const& builtin_type_reference = std::get<h::Builtin_type_reference>(callable_type_optional->data);
            if (builtin_type_reference.value == "create_array_slice_from_pointer")
            {
                if (expression.arguments.size() > 1)
                {
                    std::optional<h::Type_reference> const& first_argument_type_optional = get_expression_type_from_type_info(parameters.expression_types, expression.arguments[0]);

                    if (!first_argument_type_optional.has_value() || is_null_pointer_type(first_argument_type_optional.value()))
                    {
                        h::Expression const& first_argument_expression = parameters.statement.expressions[expression.arguments[0].expression_index];
                        std::pmr::string const provided_type_name = h::format_type_reference(parameters.core_module, first_argument_type_optional.value(), parameters.temporaries_allocator, parameters.temporaries_allocator);

                        return
                        {
                            create_error_diagnostic(
                                parameters.core_module.source_file_path,
                                first_argument_expression.source_range,
                                std::format(
                                    "Cannot pass '{}' as first argument to 'create_array_slice_from_pointer'.",
                                    provided_type_name
                                )
                            )
                        };
                    }
                }
            }
            else if (builtin_type_reference.value == "create_stack_array_uninitialized")
            {
                return
                {
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        source_range,
                        std::format(
                            "Function expects {} type arguments, but {} were provided.",
                            1,
                            0
                        )
                    )
                };
            }
        }
        
        {
            std::optional<std::pair<std::string_view, h::Instance_call_expression>> const builtin_instance_call = find_builtin_instance_call_expression(
                parameters.statement,
                expression
            );

            if (builtin_instance_call.has_value())
            {
                if (builtin_instance_call->first == "create_stack_array_uninitialized")
                {
                    h::Instance_call_expression const& instance_call_expression = builtin_instance_call->second;

                    if (instance_call_expression.arguments.size() != 1)
                    {
                        return
                        {
                            create_error_diagnostic(
                                parameters.core_module.source_file_path,
                                source_range,
                                std::format(
                                    "Function expects {} type arguments, but {} were provided.",
                                    1,
                                    instance_call_expression.arguments.size()
                                )
                            )
                        };
                    }
                }
                else if (builtin_instance_call->first == "reinterpret_as")
                {
                    h::Instance_call_expression const& instance_call_expression = builtin_instance_call->second;

                    if (instance_call_expression.arguments.size() != 1)
                    {
                        return
                        {
                            create_error_diagnostic(
                                parameters.core_module.source_file_path,
                                source_range,
                                std::format(
                                    "Function expects {} type arguments, but {} were provided.",
                                    1,
                                    instance_call_expression.arguments.size()
                                )
                            )
                        };
                    }
                }
            }
        }

        Function_constructor const* const function_constructor = find_function_constructor_using_call_expression(
            parameters.core_module.name,
            parameters.declaration_database,
            parameters.scope,
            parameters.statement,
            expression
        );

        if (function_constructor != nullptr)
        {
            std::optional<Deduced_instance_call> const deduced_instance_call = deduce_instance_call_arguments(
                parameters.declaration_database,
                parameters.core_module,
                parameters.scope,
                parameters.statement,
                expression,
                parameters.temporaries_allocator
            );

            if (!deduced_instance_call.has_value())
            {
                return
                {
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        source_range,
                        std::format(
                            "Cannot deduce arguments of implicit call of function constructor '{}'.",
                            function_constructor->name
                        )
                    )
                };
            }

            return {};
        }

        const std::optional<h::Function_pointer_type> function_pointer_type_optional = get_function_pointer_type_from_callable(
            parameters,
            expression,
            callable_type_optional
        );

        if (!function_pointer_type_optional.has_value())
        {
            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    source_range,
                    "Expression does not evaluate to a callable expression."
                )
            };
        }

        h::Function_pointer_type const& function_pointer_type = function_pointer_type_optional.value();

        std::optional<Implicit_argument> const implicit_first_argument = get_implicit_first_call_argument(
            parameters.statement,
            expression,
            parameters.scope,
            parameters.declaration_database
        );

        std::pmr::vector<Expression_index> const call_arguments = get_call_aguments(
            expression,
            implicit_first_argument,
            parameters.temporaries_allocator
        );

        if (function_pointer_type.type.is_variadic)
        {
            if (call_arguments.size() < function_pointer_type.type.input_parameter_types.size())
            {
                return
                {
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        source_range,
                        std::format(
                            "Function expects at least {} arguments, but {} were provided.",
                            function_pointer_type.type.input_parameter_types.size(),
                            call_arguments.size()
                        )
                    )
                };  
            }
        }
        else if (call_arguments.size() != function_pointer_type.type.input_parameter_types.size())
        {
            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    source_range,
                    std::format(
                        "Function expects {} arguments, but {} were provided.",
                        function_pointer_type.type.input_parameter_types.size(),
                        call_arguments.size()
                    )
                )
            };
        }

        std::pmr::vector<Diagnostic> diagnostics{parameters.temporaries_allocator};

        for (std::size_t argument_index = 0; argument_index < function_pointer_type.type.input_parameter_types.size(); ++argument_index)
        {
            std::uint64_t const expression_index = call_arguments[argument_index].expression_index;
            bool const take_address_of = argument_index == 0 && implicit_first_argument.has_value() && implicit_first_argument->take_address_of;
            std::optional<h::Type_reference> const& argument_type_optional = get_expression_type_from_type_info_from_call_arguments(parameters.expression_types, expression_index, take_address_of);
            
            h::Type_reference const& parameter_type = function_pointer_type.type.input_parameter_types[argument_index];

            if (!can_assign_type(parameters.declaration_database, parameter_type, argument_type_optional))
            {
                std::optional<Source_range> const argument_source_range = parameters.statement.expressions[expression_index].source_range;
                std::pmr::string const provided_type_name = h::format_type_reference(parameters.core_module, argument_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);
                std::pmr::string const expected_type_name = h::format_type_reference(parameters.core_module, parameter_type, parameters.temporaries_allocator, parameters.temporaries_allocator);

                diagnostics.push_back(
                    {
                        create_error_diagnostic(
                            parameters.core_module.source_file_path,
                            argument_source_range,
                            std::format(
                                "Argument {} type is '{}' but '{}' was provided.",
                                argument_index,
                                expected_type_name,
                                provided_type_name
                            )
                        )
                    }
                );
            }
        }

        return diagnostics;
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_cast_expression(
        Validate_expression_parameters const& parameters,
        h::Cast_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        std::optional<h::Type_reference> const& source_type_optional = get_expression_type_from_type_info(parameters.expression_types, expression.source);
        std::optional<Type_reference> const& underlying_destination_type = get_underlying_type(parameters.declaration_database, expression.destination_type);

        if (!source_type_optional.has_value() || !underlying_destination_type.has_value())
        {
            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    source_range,
                    std::format(
                        "Cannot apply numeric cast from '{}' to '{}'.",
                        h::format_type_reference(parameters.core_module, source_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator),
                        h::format_type_reference(parameters.core_module, underlying_destination_type, parameters.temporaries_allocator, parameters.temporaries_allocator)
                    )
                )
            };
        }
        
        std::optional<Type_reference> const& underlying_source_type = get_underlying_type(parameters.declaration_database, source_type_optional.value());

        bool const is_source_numeric = 
            underlying_source_type.has_value() && 
            (is_number_or_c_number(underlying_source_type.value()) || is_enum_type(parameters.declaration_database, underlying_source_type.value()) || is_bool(underlying_destination_type.value()) || is_c_bool(underlying_destination_type.value()));
        
        bool const is_destination_numeric =
            underlying_destination_type.has_value() &&
            (is_number_or_c_number(underlying_destination_type.value()) || is_enum_type(parameters.declaration_database, underlying_destination_type.value()) || is_bool(underlying_destination_type.value()) || is_c_bool(underlying_destination_type.value()));

        if (!is_source_numeric || !is_destination_numeric)
        {
            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    source_range,
                    std::format(
                        "Cannot apply numeric cast from '{}' to '{}'.",
                        h::format_type_reference(parameters.core_module, underlying_source_type, parameters.temporaries_allocator, parameters.temporaries_allocator),
                        h::format_type_reference(parameters.core_module, underlying_destination_type, parameters.temporaries_allocator, parameters.temporaries_allocator)
                    )
                )
            };
        }

        if (source_type_optional.value() == expression.destination_type)
        {
            return
            {
                create_warning_diagnostic(
                    parameters.core_module.source_file_path,
                    source_range,
                    std::format(
                        "Numeric cast from '{}' to '{}'.",
                        h::format_type_reference(parameters.core_module, source_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator),
                        h::format_type_reference(parameters.core_module, expression.destination_type, parameters.temporaries_allocator, parameters.temporaries_allocator)
                    )
                )
            };
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_continue_expression(
        Validate_expression_parameters const& parameters,
        h::Continue_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        auto const is_loop_block = [](Block_expression_variant const& block) -> bool {
            return std::holds_alternative<h::For_loop_expression const*>(block) ||
                   std::holds_alternative<h::While_loop_expression const*>(block);
        };

        auto const location = std::find_if(
            parameters.scope.blocks.begin(),
            parameters.scope.blocks.end(),
            is_loop_block
        );
        if (location == parameters.scope.blocks.end())
        {
            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    source_range,
                    "Expression 'continue' can only be placed inside for loops and while loops."
                )
            };
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_for_loop_expression(
        Validate_expression_parameters const& parameters,
        h::For_loop_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        std::optional<h::Type_reference> const& range_begin_type_optional = get_expression_type_from_type_info(parameters.expression_types, expression.range_begin);

        if (!range_begin_type_optional.has_value() || (!is_integer(range_begin_type_optional.value()) && !is_floating_point(range_begin_type_optional.value())))
        {
            h::Expression const& range_begin_expression = parameters.statement.expressions[expression.range_begin.expression_index];
            std::pmr::string const provided_type_name = h::format_type_reference(parameters.core_module, range_begin_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);

            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    range_begin_expression.source_range,
                    std::format(
                        "For loop range begin type '{}' is not a number.",
                        provided_type_name
                    )
                )
            };
        }

        Scope new_scope = parameters.scope;
        new_scope.variables.push_back(
            create_variable(expression.variable_name, range_begin_type_optional.value(), true, false, source_range)
        );
        new_scope.blocks.push_back(&expression);

        {
            std::pmr::vector<h::compiler::Diagnostic> const range_end_statement_diagnostics = validate_statement(
                parameters.core_module,
                parameters.function_declaration,
                new_scope,
                expression.range_end,
                std::nullopt,
                parameters.declaration_database,
                parameters.temporaries_allocator
            );
            if (!range_end_statement_diagnostics.empty())
                return range_end_statement_diagnostics;
        }

        std::optional<h::Type_reference> const range_end_type_optional = get_expression_type(
            parameters.core_module,
            parameters.function_declaration,
            new_scope,
            expression.range_end,
            std::nullopt,
            parameters.declaration_database
        );

        if (!are_compatible_types(parameters.declaration_database, range_begin_type_optional, range_end_type_optional))
        {
            std::pmr::string const provided_type_name = h::format_type_reference(parameters.core_module, range_end_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);
            std::pmr::string const expected_type_name = h::format_type_reference(parameters.core_module, range_begin_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);

            return
            {
                create_error_diagnostic_with_code(
                    parameters.core_module.source_file_path,
                    get_statement_source_range(expression.range_end),
                    std::format(
                        "For loop range end type '{}' does not match range begin type '{}'.",
                        provided_type_name,
                        expected_type_name
                    ),
                    Diagnostic_code::Type_mismatch,
                    create_diagnostic_mismatch_type_data(range_end_type_optional, range_begin_type_optional)
                )
            };
        }

        if (expression.step_by.has_value())
        {
            std::optional<h::Type_reference> const& step_by_type_optional = get_expression_type_from_type_info(parameters.expression_types, expression.step_by.value());

            if (!are_compatible_types(parameters.declaration_database, range_begin_type_optional, step_by_type_optional))
            {
                h::Expression const& step_by_expression = parameters.statement.expressions[expression.step_by->expression_index];
                std::pmr::string const provided_type_name = h::format_type_reference(parameters.core_module, step_by_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);
                std::pmr::string const expected_type_name = h::format_type_reference(parameters.core_module, range_begin_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);

                return
                {
                    create_error_diagnostic_with_code(
                        parameters.core_module.source_file_path,
                        step_by_expression.source_range,
                        std::format(
                            "For loop step_by type '{}' does not match range begin type '{}'.",
                            provided_type_name,
                            expected_type_name
                        ),
                        Diagnostic_code::Type_mismatch,
                        create_diagnostic_mismatch_type_data(step_by_type_optional, range_begin_type_optional)
                    )
                };
            }
        }

        {
            std::pmr::vector<h::compiler::Diagnostic> const statements_diagnostics = validate_statements(
                parameters.core_module,
                parameters.function_declaration,
                new_scope,
                expression.then_statements,
                parameters.declaration_database,
                parameters.temporaries_allocator
            );
            if (!statements_diagnostics.empty())
                return statements_diagnostics;
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_if_expression(
        Validate_expression_parameters const& parameters,
        h::If_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        std::pmr::vector<h::compiler::Diagnostic> diagnostics{parameters.temporaries_allocator};

        for (Condition_statement_pair const& pair : expression.series)
        {
            if (pair.condition.has_value())
            {
                std::pmr::vector<h::compiler::Diagnostic> const condition_diagnostics = validate_statement(
                    parameters.core_module,
                    parameters.function_declaration,
                    parameters.scope,
                    pair.condition.value(),
                    std::nullopt,
                    parameters.declaration_database,
                    parameters.temporaries_allocator
                );
                if (!condition_diagnostics.empty())
                    diagnostics.insert(diagnostics.end(), condition_diagnostics.begin(), condition_diagnostics.end());

                if (condition_diagnostics.empty())
                {
                    std::optional<h::Type_reference> const condition_type_optional = get_expression_type(
                        parameters.core_module,
                        parameters.function_declaration,
                        parameters.scope,
                        pair.condition.value(),
                        std::nullopt,
                        parameters.declaration_database
                    );

                    if (!condition_type_optional.has_value() || (!is_bool(condition_type_optional.value()) && !is_c_bool(condition_type_optional.value())))
                    {
                        std::pmr::string const provided_type_name = h::format_type_reference(parameters.core_module, condition_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);

                        diagnostics.push_back(
                            create_error_diagnostic(
                                parameters.core_module.source_file_path,
                                get_statement_source_range(pair.condition.value()),
                                std::format(
                                    "Expression type '{}' does not match expected type 'Bool'.",
                                    provided_type_name
                                )
                            )
                        );
                    }
                }

                std::pmr::vector<h::compiler::Diagnostic> const then_diagnostics = validate_statements(
                    parameters.core_module,
                    parameters.function_declaration,
                    parameters.scope,
                    pair.then_statements,
                    parameters.declaration_database,
                    parameters.temporaries_allocator
                );
                if (!then_diagnostics.empty())
                    diagnostics.insert(diagnostics.end(), then_diagnostics.begin(), then_diagnostics.end());
            }
        }

        return diagnostics;
    }

    std::optional<Declaration> find_declaration_to_instantiate(
        Declaration_database const& declaration_database,
        h::Type_reference const& type_to_instantiate,
        std::pmr::vector<h::Struct_declaration>& temporary_storage
    )
    {
        if (std::holds_alternative<h::Array_slice_type>(type_to_instantiate.data))
        {
            h::Array_slice_type const& array_slice = std::get<h::Array_slice_type>(type_to_instantiate.data);
            temporary_storage.push_back(create_array_slice_type_struct_declaration(array_slice.element_type));
            return Declaration{ .data = &temporary_storage[0], .module_name = "H.Builtin", .is_export = true };
        }

        if (std::holds_alternative<h::Soa_array_type>(type_to_instantiate.data))
        {
            h::Soa_array_type const& soa_array_type = std::get<h::Soa_array_type>(type_to_instantiate.data);
            if (soa_array_type.value_type.empty())
                return std::nullopt;

            return find_underlying_declaration(
                declaration_database,
                soa_array_type.value_type.front()
            );
        }

        std::optional<Declaration> const declaration_optional = find_underlying_declaration(
            declaration_database,
            type_to_instantiate
        );
        if (declaration_optional.has_value())
            return declaration_optional.value();

        return std::nullopt;
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_instantiate_expression(
        Validate_expression_parameters const& parameters,
        h::Instantiate_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        for (std::size_t member_index = 1; member_index < expression.members.size(); ++member_index)
        {
            h::Instantiate_member_value_pair const& pair = expression.members[member_index];

            auto const duplicate_location = std::find_if(
                expression.members.begin(),
                expression.members.begin() + member_index,
                [&](h::Instantiate_member_value_pair const& other_pair) -> bool
                {
                    return pair.member_name == other_pair.member_name;
                }
            );

            if (duplicate_location != expression.members.begin() + member_index)
            {
                return
                {
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        create_sub_source_range(pair.source_range, 0, pair.member_name.size()),
                        std::format(
                            "Duplicate instantiate member '{}'.",
                            pair.member_name
                        )
                    )
                };
            }
        }

        std::optional<h::Type_reference> const type_to_instantiate = get_expression_type_from_type_info(parameters.expression_types, parameters.expression_index);
        if (!type_to_instantiate.has_value())
        {
            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    source_range,
                    "Could not deduce type to instantiate."
                )
            };
        }

        if (std::holds_alternative<h::Soa_array_type>(type_to_instantiate->data) && expression.type == Instantiate_expression_type::Explicit)
        {
            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    source_range,
                    "Explicit Soa_array brace initialization is not implemented. Use [ ... ] initializer syntax."
                )
            };
        }

        std::pmr::vector<h::Struct_declaration> temporary_storage{parameters.temporaries_allocator};
        std::optional<Declaration> const declaration_optional = find_declaration_to_instantiate(
            parameters.declaration_database,
            type_to_instantiate.value(),
            temporary_storage
        );
        if (!declaration_optional.has_value())
        {
            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    source_range,
                    "Could not find declaration of type to instantiate."
                )
            };
        }

        Declaration const& declaration = declaration_optional.value();

        std::pmr::vector<Declaration_member_info> const member_infos = get_declaration_member_infos(declaration, parameters.temporaries_allocator);

        std::size_t previous_original_index = 0;

        for (std::size_t member_index = 0; member_index < expression.members.size(); ++member_index)
        {
            h::Instantiate_member_value_pair const& pair = expression.members[member_index];

            auto const location = std::find_if(
                member_infos.begin(),
                member_infos.end(),
                [&](Declaration_member_info const& member_info) -> bool
                {
                    return pair.member_name == member_info.member_name;
                }
            );

            if (location == member_infos.end())
            {
                return
                {
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        create_sub_source_range(pair.source_range, 0, pair.member_name.size()),
                        std::format(
                            "'{}.{}' does not exist.",
                            h::format_type_reference(parameters.core_module, type_to_instantiate, parameters.temporaries_allocator, parameters.temporaries_allocator),
                            pair.member_name
                        )
                    )
                };
            }

            h::Type_reference const& member_type = location->member_type;

            h::Expression const& member_value_expression = parameters.statement.expressions[pair.value.expression_index];
            std::optional<h::Type_reference> const assigned_value_type = get_expression_type(
                parameters.core_module,
                parameters.function_declaration,
                parameters.scope,
                parameters.statement,
                member_value_expression,
                member_type,
                parameters.declaration_database
            );

            if (!can_assign_type(parameters.declaration_database, member_type, assigned_value_type))
            {
                std::pmr::string const provided_type_name = h::format_type_reference(parameters.core_module, assigned_value_type, parameters.temporaries_allocator, parameters.temporaries_allocator);
                std::pmr::string const expected_type_name = h::format_type_reference(parameters.core_module, member_type, parameters.temporaries_allocator, parameters.temporaries_allocator);

                return
                {
                    create_error_diagnostic_with_code(
                        parameters.core_module.source_file_path,
                        member_value_expression.source_range,
                        std::format(
                            "Cannot assign value of type '{}' to member '{}.{}' of type '{}'.",
                            provided_type_name,
                            h::format_type_reference(parameters.core_module, type_to_instantiate, parameters.temporaries_allocator, parameters.temporaries_allocator),
                            pair.member_name,
                            expected_type_name
                        ),
                        Diagnostic_code::Type_mismatch,
                        create_diagnostic_mismatch_type_data(assigned_value_type, member_type)
                    )
                };
            }

            std::size_t const original_index = std::distance(member_infos.begin(), location);
            if (member_index > 0)
            {
                if (original_index < previous_original_index)
                {
                    return
                    {
                        create_error_diagnostic(
                            parameters.core_module.source_file_path,
                            source_range,
                            "Instantiate members are not sorted. They must appear in the order they were declarated in the struct declaration."
                        )
                    };
                }
            }
            previous_original_index = original_index;
        }

        if (expression.type == Instantiate_expression_type::Explicit)
        {
            if (expression.members.size() != member_infos.size())
            {
                std::pmr::vector<h::compiler::Diagnostic> diagnostics{parameters.temporaries_allocator};

                for (std::size_t member_index = 0; member_index < member_infos.size(); ++member_index)
                {
                    Declaration_member_info const& member_info = member_infos[member_index];

                    auto const location = std::find_if(
                        expression.members.begin(),
                        expression.members.end(),
                        [&](h::Instantiate_member_value_pair const& pair) -> bool
                        {
                            return pair.member_name == member_info.member_name;
                        }
                    );

                    if (location == expression.members.end())
                    {
                        diagnostics.push_back(
                            create_error_diagnostic(
                                parameters.core_module.source_file_path,
                                source_range,
                                std::format(
                                    "'{}.{}' is not set. Explicit instantiate expression requires all members to be set.",
                                    h::format_type_reference(parameters.core_module, type_to_instantiate, parameters.temporaries_allocator, parameters.temporaries_allocator),
                                    member_info.member_name
                                )
                            )
                        );
                    }
                }

                return diagnostics;
            }
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_reflection_expression(
        Validate_expression_parameters const& parameters,
        h::Reflection_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        if (expression.name == "size_of" || expression.name == "alignment_of")
        {
            if (expression.type_arguments.size() != 1)
            {
                return
                {
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        source_range,
                        std::format("@{} requires only 1 type argument.", expression.name)
                    )
                };
            }

            if (expression.arguments.size() != 0)
            {
                return
                {
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        source_range,
                        std::format("@{} does not have any parameters.", expression.name)
                    )
                };
            }
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_return_expression(
        Validate_expression_parameters const& parameters,
        h::Return_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        if (parameters.function_declaration == nullptr)
            return {};

        std::optional<h::Type_reference> const expected_type = 
            !parameters.function_declaration->type.output_parameter_types.empty() ?
            get_underlying_type(parameters.declaration_database, parameters.function_declaration->type.output_parameter_types[0]) :
            std::optional<h::Type_reference>{std::nullopt};

        if (expression.expression.has_value())
        {
            std::optional<h::Type_reference> const& provided_type = get_expression_type_from_type_info(parameters.expression_types, expression.expression.value());
            std::optional<h::Type_reference> const underlying_provided_type = provided_type.has_value() ? get_underlying_type(parameters.declaration_database, provided_type.value()) : std::optional<h::Type_reference>{};

            if (!are_compatible_types(parameters.declaration_database, underlying_provided_type, expected_type))
            {
                std::pmr::string const provided_type_name = h::format_type_reference(parameters.core_module, underlying_provided_type, parameters.temporaries_allocator, parameters.temporaries_allocator);
                std::pmr::string const expected_type_name = h::format_type_reference(parameters.core_module, expected_type, parameters.temporaries_allocator, parameters.temporaries_allocator);

                return
                {
                    create_error_diagnostic_with_code(
                        parameters.core_module.source_file_path,
                        source_range,
                        std::format(
                            "Function '{}' expects a return value of type '{}', but '{}' was provided.",
                            parameters.function_declaration->name,
                            expected_type_name,
                            provided_type_name
                        ),
                        Diagnostic_code::Type_mismatch,
                        create_diagnostic_mismatch_type_data(provided_type, expected_type)
                    )
                };
            }
        }
        else
        {
            if (parameters.function_declaration != nullptr)
            {
                if (!parameters.function_declaration->type.output_parameter_types.empty())
                {
                    std::pmr::string const expected_type_name = h::format_type_reference(parameters.core_module, expected_type, parameters.temporaries_allocator, parameters.temporaries_allocator);

                    return {
                        create_error_diagnostic(
                            parameters.core_module.source_file_path,
                            source_range,
                            std::format(
                                "Function '{}' expects a return value of type '{}', but none was provided.",
                                parameters.function_declaration->name,
                                expected_type_name
                            )
                        )
                    };
                }
            }
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_switch_expression(
        Validate_expression_parameters const& parameters,
        h::Switch_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        std::optional<Type_reference> const type_optional = get_expression_type_from_type_info(parameters.expression_types, expression.value);

        if (!type_optional.has_value() || (!is_enum_type(parameters.declaration_database, type_optional.value()) && !is_integer(type_optional.value())))
        {
            h::Expression const& value_expression = parameters.statement.expressions[expression.value.expression_index];
            std::pmr::string const provided_type_name = h::format_type_reference(parameters.core_module, type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);

            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    value_expression.source_range,
                    std::format(
                        "Switch condition type is '{}' but expected an integer or an enum value.",
                        provided_type_name
                    )
                )
            };
        }

        std::pmr::vector<h::compiler::Diagnostic> diagnostics{parameters.temporaries_allocator};

        std::size_t default_case_count = 0;

        Scope new_scope = parameters.scope;
        new_scope.blocks.push_back(&expression);

        for (Switch_case_expression_pair const& pair : expression.cases)
        {
            if (!pair.case_value.has_value())
            {
                default_case_count += 1;

                if (default_case_count > 1)
                {
                    return
                    {
                        create_error_diagnostic(
                            parameters.core_module.source_file_path,
                            source_range,
                            "Switch expression cannot have more than one default case."
                        )
                    };
                }

                continue;
            }

            h::Expression const& case_value_expression = parameters.statement.expressions[pair.case_value->expression_index];
            std::optional<h::Type_reference> const case_value_type_optional = get_expression_type_from_type_info(parameters.expression_types, pair.case_value.value());

            if (!are_compatible_types(parameters.declaration_database, type_optional, case_value_type_optional))
            {
                std::pmr::string const expected_type_name = h::format_type_reference(parameters.core_module, type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);
                std::pmr::string const provided_type_name = h::format_type_reference(parameters.core_module, case_value_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);

                diagnostics.push_back(
                    create_error_diagnostic_with_code(
                        parameters.core_module.source_file_path,
                        case_value_expression.source_range,
                        std::format(
                            "Switch case value type '{}' does not match switch condition type '{}'.",
                            provided_type_name,
                            expected_type_name
                        ),
                        Diagnostic_code::Type_mismatch,
                        create_diagnostic_mismatch_type_data(case_value_type_optional, type_optional)
                    )
                );
            }
            else if (!is_computable_at_compile_time(case_value_expression, case_value_type_optional, parameters))
            {
                diagnostics.push_back(
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        case_value_expression.source_range,
                        "Switch case expression must be computable at compile-time, and evaluate to an integer or an enum value."
                    )
                );
            }

            {
                std::pmr::vector<h::compiler::Diagnostic> const statements_diagnostics = validate_statements(
                    parameters.core_module,
                    parameters.function_declaration,
                    new_scope,
                    pair.statements,
                    parameters.declaration_database,
                    parameters.temporaries_allocator
                );
                if (!statements_diagnostics.empty())
                    diagnostics.insert(diagnostics.end(), statements_diagnostics.begin(), statements_diagnostics.end());
            }
        }

        return diagnostics;
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_ternary_condition_expression(
        Validate_expression_parameters const& parameters,
        h::Ternary_condition_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        std::optional<h::Type_reference> const& condition_type_optional = get_expression_type_from_type_info(parameters.expression_types, expression.condition);

        if (!condition_type_optional.has_value() || (!is_bool(condition_type_optional.value()) && !is_c_bool(condition_type_optional.value())))
        {
            h::Expression const& condition_expression = parameters.statement.expressions[expression.condition.expression_index];
            std::pmr::string const provided_type_name = h::format_type_reference(parameters.core_module, condition_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);

            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    condition_expression.source_range,
                    std::format(
                        "Expression type '{}' does not match expected type 'Bool'.",
                        provided_type_name
                    )
                )
            };
        }

        {
            std::pmr::vector<h::compiler::Diagnostic> diagnostics{parameters.temporaries_allocator};

            std::pmr::vector<h::compiler::Diagnostic> const then_statement_diagnostics = validate_statement(
                parameters.core_module,
                parameters.function_declaration,
                parameters.scope,
                expression.then_statement,
                std::nullopt,
                parameters.declaration_database,
                parameters.temporaries_allocator
            );
            if (!then_statement_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), then_statement_diagnostics.begin(), then_statement_diagnostics.end());

            std::pmr::vector<h::compiler::Diagnostic> const else_statement_diagnostics = validate_statement(
                parameters.core_module,
                parameters.function_declaration,
                parameters.scope,
                expression.else_statement,
                std::nullopt,
                parameters.declaration_database,
                parameters.temporaries_allocator
            );
            if (!else_statement_diagnostics.empty())
                diagnostics.insert(diagnostics.end(), else_statement_diagnostics.begin(), else_statement_diagnostics.end());

            if (!diagnostics.empty())
                return diagnostics;
        }

        std::optional<h::Type_reference> const then_type_optional = get_expression_type(
            parameters.core_module,
            parameters.function_declaration,
            parameters.scope,
            expression.then_statement,
            std::nullopt,
            parameters.declaration_database
        );

        std::optional<h::Type_reference> const else_type_optional = get_expression_type(
            parameters.core_module,
            parameters.function_declaration,
            parameters.scope,
            expression.else_statement,
            std::nullopt,
            parameters.declaration_database
        );

        if (!are_compatible_types(parameters.declaration_database, then_type_optional, else_type_optional))
        {
            std::pmr::string const then_type_name = h::format_type_reference(parameters.core_module, then_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);
            std::pmr::string const else_type_name = h::format_type_reference(parameters.core_module, else_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);

            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    source_range,
                    std::format(
                        "Ternary condition expression requires both branches to be of the same type. Left side type '{}' does not match right side type '{}'.",
                        then_type_name,
                        else_type_name
                    )
                )
            };
        }

        return {};
    }

    static bool can_take_address_of_expression(
        h::Statement const& statement,
        h::Expression const& expression
    )
    {
        if (std::holds_alternative<h::Access_expression>(expression.data))
        {
            h::Access_expression const& access_expression = std::get<h::Access_expression>(expression.data);
            h::Expression const& left_expression = statement.expressions[access_expression.expression.expression_index];
            return can_take_address_of_expression(statement, left_expression);
        }
        else if (std::holds_alternative<h::Access_array_expression>(expression.data))
        {
            h::Access_array_expression const& access_expression = std::get<h::Access_array_expression>(expression.data);
            h::Expression const& left_expression = statement.expressions[access_expression.expression.expression_index];
            return can_take_address_of_expression(statement, left_expression);
        }
        else if (std::holds_alternative<h::Dereference_and_access_expression>(expression.data))
        {
            h::Dereference_and_access_expression const& access_expression = std::get<h::Dereference_and_access_expression>(expression.data);
            h::Expression const& left_expression = statement.expressions[access_expression.expression.expression_index];
            return can_take_address_of_expression(statement, left_expression);
        }
        else if (std::holds_alternative<h::Variable_expression>(expression.data))
        {
            return true;
        }

        return false;
    }

    static bool is_constant_1(h::Expression const& expression)
    {
        if (std::holds_alternative<h::Constant_expression>(expression.data))
        {
            h::Constant_expression const& constant_expression = std::get<h::Constant_expression>(expression.data);
            return constant_expression.data == "1";
        }

        return false;
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_unary_expression(
        Validate_expression_parameters const& parameters,
        h::Unary_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        std::optional<h::Type_reference> const& operand_type_optional = get_expression_type_from_type_info(parameters.expression_types, expression.expression);

        if (!operand_type_optional.has_value())
            return {}; // TODO error

        std::optional<h::Type_reference> const type_optional = get_underlying_type(parameters.declaration_database, operand_type_optional.value());
        if (!type_optional.has_value())
            return {}; // TODO error
        
        h::Type_reference const& type = type_optional.value();

        switch (expression.operation)
        {
            case Unary_operation::Not:
            {
                if (!is_bool(type) && !is_c_bool(type))
                {
                    return
                    {
                        create_error_diagnostic(
                            parameters.core_module.source_file_path,
                            create_sub_source_range(source_range, 0, 1),
                            std::format(
                                "Cannot apply unary operation '{}' to expression.",
                                h::unary_operation_symbol_to_string(expression.operation)
                            )
                        )
                    };
                }
                break;
            }
            case Unary_operation::Bitwise_not:
            {
                if (!is_integer(type) && !is_byte(type))
                {
                    return
                    {
                        create_error_diagnostic(
                            parameters.core_module.source_file_path,
                            create_sub_source_range(source_range, 0, 1),
                            std::format(
                                "Cannot apply unary operation '{}' to expression.",
                                h::unary_operation_symbol_to_string(expression.operation)
                            )
                        )
                    };
                }
                break;
            }
            case Unary_operation::Minus:
            {
                if (!is_integer(type) && !is_floating_point(type) && !is_decimal(type))
                {
                    return
                    {
                        create_error_diagnostic(
                            parameters.core_module.source_file_path,
                            create_sub_source_range(source_range, 0, 1),
                            std::format(
                                "Cannot apply unary operation '{}' to expression.",
                                h::unary_operation_symbol_to_string(expression.operation)
                            )
                        )
                    };
                }
                else if (is_unsigned_integer(type))
                {
                    h::Expression const& operand_expression = parameters.statement.expressions[expression.expression.expression_index];
                    if (!is_constant_1(operand_expression))
                    {
                        return
                        {
                            create_error_diagnostic(
                                parameters.core_module.source_file_path,
                                create_sub_source_range(source_range, 0, 1),
                                std::format(
                                    "Cannot apply unary operation '-' to unsigned integer.",
                                    h::unary_operation_symbol_to_string(expression.operation)
                                )
                            )
                        };
                    }
                }
                break;
            }
            case Unary_operation::Indirection:
            {
                if (!is_non_void_pointer(type))
                {
                    return
                    {
                        create_error_diagnostic(
                            parameters.core_module.source_file_path,
                            create_sub_source_range(source_range, 0, 1),
                            std::format(
                                "Cannot apply unary operation '{}' to expression.",
                                h::unary_operation_symbol_to_string(expression.operation)
                            )
                        )
                    };
                }
                break;
            }
            case Unary_operation::Address_of:
            {
                Expression const& operand_expression = parameters.statement.expressions[expression.expression.expression_index];
                bool const can_take_adress = can_take_address_of_expression(parameters.statement, operand_expression);
                if (!can_take_adress)
                {
                    return
                    {
                        create_error_diagnostic(
                            parameters.core_module.source_file_path,
                            create_sub_source_range(source_range, 0, 1),
                            std::format(
                                "Cannot apply unary operation '{}' to expression.",
                                h::unary_operation_symbol_to_string(expression.operation)
                            )
                        )
                    };
                }
                else if (is_macro_global_variable(parameters.core_module.name, operand_expression, parameters.declaration_database))
                {
                    return
                    {
                        create_error_diagnostic(
                            parameters.core_module.source_file_path,
                            create_sub_source_range(source_range, 0, 1),
                            "Cannot take address of a global macro."
                        )
                    };
                }
                break;
            }
            default:
                break;
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_variable_declaration_expression(
        Validate_expression_parameters const& parameters,
        h::Variable_declaration_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        Variable const* const variable = find_variable_from_scope(
            parameters.scope,
            expression.name
        );
        if (variable != nullptr)
        {
            std::optional<h::Source_range> const& name_source_range = create_sub_source_range(
                source_range,
                expression.is_mutable ? 8 : 4,
                expression.name.size()
            );

            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    name_source_range,
                    std::format("Duplicate variable name '{}'.", expression.name)
                )
            };
        }

        std::optional<h::Type_reference> const& type_optional = get_expression_type_from_type_info(parameters.expression_types, expression.right_hand_side);
        if (!type_optional.has_value())
        {
            h::Expression const& right_hand_side = parameters.statement.expressions[expression.right_hand_side.expression_index];

            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    right_hand_side.source_range,
                    std::format("Cannot assign expression of type 'void' to variable '{}'.", expression.name)
                )
            };
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_variable_declaration_with_type_expression(
        Validate_expression_parameters const& parameters,
        h::Variable_declaration_with_type_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        Variable const* const variable = find_variable_from_scope(
            parameters.scope,
            expression.name
        );
        if (variable != nullptr)
        {
            std::optional<h::Source_range> const& name_source_range = create_sub_source_range(
                source_range,
                expression.is_mutable ? 8 : 4,
                expression.name.size()
            );

            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    name_source_range,
                    std::format("Duplicate variable name '{}'.", expression.name)
                )
            };
        }
        
        h::Expression const& right_hand_side = parameters.statement.expressions[expression.right_hand_side.expression_index];
        std::optional<h::Type_reference> const type_optional = h::get_variable_declaration_with_type_expression_type(parameters.statement, expression);
        if (!type_optional.has_value())
        {
            return
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    source_range,
                    std::format("Invalid declared type expression for variable '{}'.", expression.name)
                )
            };
        }

        h::Type_reference const& type = type_optional.value();

        if (
            std::holds_alternative<h::Instantiate_expression>(right_hand_side.data) &&
            !std::holds_alternative<h::Array_slice_type>(type.data) &&
            !std::holds_alternative<h::Soa_array_type>(type.data)
        )
        {
            std::optional<Declaration> const declaration_optional = find_underlying_declaration(parameters.declaration_database, type);
            if (!declaration_optional.has_value() || (!std::holds_alternative<h::Struct_declaration const*>(declaration_optional->data) && !std::holds_alternative<h::Union_declaration const*>(declaration_optional->data) && !std::holds_alternative<h::Type_constructor const*>(declaration_optional->data)))
            {
                return
                {
                    create_error_diagnostic(
                        parameters.core_module.source_file_path,
                        right_hand_side.source_range,
                        std::format(
                            "Cannot assign expression of type '{}' to variable '{}'. Expected struct or union type.",
                            h::format_type_reference(parameters.core_module, type, parameters.temporaries_allocator, parameters.temporaries_allocator),
                            expression.name
                        )
                    )
                };
            }
        }
        else
        {
            std::optional<h::Type_reference> const& right_hand_side_type = get_expression_type(
                parameters.core_module,
                parameters.function_declaration,
                parameters.scope,
                parameters.statement,
                right_hand_side,
                type,
                parameters.declaration_database
            );

            if (!can_assign_type(parameters.declaration_database, type, right_hand_side_type))
            {
                std::pmr::string const expected_type_name = h::format_type_reference(parameters.core_module, type, parameters.temporaries_allocator, parameters.temporaries_allocator);
                std::pmr::string const provided_type_name = h::format_type_reference(parameters.core_module, right_hand_side_type, parameters.temporaries_allocator, parameters.temporaries_allocator);

                return
                {
                    create_error_diagnostic_with_code(
                        parameters.core_module.source_file_path,
                        right_hand_side.source_range,
                        std::format(
                            "Expression type '{}' does not match expected type '{}'.",
                            provided_type_name,
                            expected_type_name
                        ),
                        Diagnostic_code::Type_mismatch,
                        create_diagnostic_mismatch_type_data(right_hand_side_type, type)
                    )
                };
            }
        }

        return {};
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_variable_expression(
        Validate_expression_parameters const& parameters,
        h::Variable_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        Variable const* const variable = find_variable_from_scope(
            parameters.scope,
            expression.name
        );
        if (variable != nullptr)
            return {};

        std::optional<Declaration> const declaration_optional = find_declaration(
            parameters.declaration_database,
            parameters.core_module.name,
            expression.name
        );
        if (declaration_optional.has_value())
            return {};

        Import_module_with_alias const* const import_alias = find_import_module_with_alias(
            parameters.core_module,
            expression.name
        );
        if (import_alias != nullptr)
            return {};

        if (is_builtin_function_name(expression.name))
            return {};

        return
        {
            create_error_diagnostic(
                parameters.core_module.source_file_path,
                source_range,
                std::format("Variable '{}' does not exist.", expression.name)
            )
        };
    }

    std::pmr::vector<h::compiler::Diagnostic> validate_while_loop_expression(
        Validate_expression_parameters const& parameters,
        h::While_loop_expression const& expression,
        std::optional<h::Source_range> const& source_range
    )
    {
        {
            std::pmr::vector<h::compiler::Diagnostic> const condition_statement_diagnostics = validate_statement(
                parameters.core_module,
                parameters.function_declaration,
                parameters.scope,
                expression.condition,
                std::nullopt,
                parameters.declaration_database,
                parameters.temporaries_allocator
            );
            if (!condition_statement_diagnostics.empty())
                return condition_statement_diagnostics;
        }

        std::optional<h::Type_reference> const condition_type_optional = get_expression_type(
            parameters.core_module,
            parameters.function_declaration,
            parameters.scope,
            expression.condition,
            std::nullopt,
            parameters.declaration_database
        );

        if (!condition_type_optional.has_value() || (!is_bool(condition_type_optional.value()) && !is_c_bool(condition_type_optional.value())))
        {
            std::pmr::string const provided_type_name = h::format_type_reference(parameters.core_module, condition_type_optional, parameters.temporaries_allocator, parameters.temporaries_allocator);

            return 
            {
                create_error_diagnostic(
                    parameters.core_module.source_file_path,
                    get_statement_source_range(expression.condition),
                    std::format(
                        "Expression type '{}' does not match expected type 'Bool'.",
                        provided_type_name
                    )
                )
            };
        }

        {
            Scope new_scope = parameters.scope;
            new_scope.blocks.push_back(&expression);

            std::pmr::vector<h::compiler::Diagnostic> const statements_diagnostics = validate_statements(
                parameters.core_module,
                parameters.function_declaration,
                new_scope,
                expression.then_statements,
                parameters.declaration_database,
                parameters.temporaries_allocator
            );
            if (!statements_diagnostics.empty())
                return statements_diagnostics;
        }

        return {};
    }

    static std::optional<h::Type_reference> get_expected_expression_type(
        h::Statement const& statement,
        std::optional<h::Type_reference> const expected_statement_type,
        std::size_t const expression_index
    )
    {
        if (expected_statement_type.has_value())
            return expected_statement_type.value();

        if (expression_index >= statement.expressions.size())
            return std::nullopt;

        if (expression_index == 1)
        {
            h::Expression const& expression = statement.expressions[0];
            if (std::holds_alternative<h::Variable_declaration_with_type_expression>(expression.data))
            {
                h::Variable_declaration_with_type_expression const& data = std::get<h::Variable_declaration_with_type_expression>(expression.data);
                return h::get_variable_declaration_with_type_expression_type(statement, data);
            }
        }

        return std::nullopt;
    }

    std::pmr::vector<std::optional<Type_info>> calculate_expression_type_infos_of_statement(
        h::Module const& core_module,
        h::Function_declaration const* const function_declaration,
        Scope const& scope,
        h::Statement const& statement,
        std::optional<h::Type_reference> const expected_statement_type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<std::optional<Type_info>> expression_types{temporaries_allocator};
        expression_types.resize(statement.expressions.size(), std::nullopt);

        for (std::size_t expression_index = 0; expression_index < statement.expressions.size(); ++expression_index)
        {
            h::Expression const& expression = statement.expressions[expression_index];
            std::optional<h::Type_reference> const expected_expression_type = get_expected_expression_type(statement, expected_statement_type, expression_index);
            
            expression_types[expression_index] = get_expression_type_info(
                core_module,
                function_declaration,
                scope,
                statement,
                expression,
                expected_expression_type,
                declaration_database
            );
        }

        return expression_types;
    }

    std::pmr::vector<std::optional<h::Type_reference>> calculate_expression_types_of_statement(
        h::Module const& core_module,
        h::Function_declaration const* const function_declaration,
        Scope const& scope,
        h::Statement const& statement,
        std::optional<h::Type_reference> const expected_statement_type,
        Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<std::optional<h::Type_reference>> expression_types{temporaries_allocator};
        expression_types.resize(statement.expressions.size(), std::nullopt);

        for (std::size_t expression_index = 0; expression_index < statement.expressions.size(); ++expression_index)
        {
            h::Expression const& expression = statement.expressions[expression_index];
            
            expression_types[expression_index] = get_expression_type(
                core_module,
                function_declaration,
                scope,
                statement,
                expression,
                expected_statement_type,
                declaration_database
            );
        }

        return expression_types;
    }

    std::optional<h::Type_reference> get_expression_type_from_type_info(
        std::span<std::optional<Type_info> const> const type_infos,
        std::uint64_t const expression_index
    )
    {
        if (expression_index >= type_infos.size())
            return std::nullopt;

        std::optional<Type_info> const& type_info = type_infos[expression_index];
        if (!type_info.has_value())
            return std::nullopt;
        
        return type_info->type;
    }

    std::optional<h::Type_reference> get_expression_type_from_type_info(
        std::span<std::optional<Type_info> const> const type_infos,
        h::Expression_index const expression_index
    )
    {
        return get_expression_type_from_type_info(type_infos, expression_index.expression_index);
    }

    std::optional<h::Type_reference> get_expression_type_from_type_info_from_call_arguments(
        std::span<std::optional<Type_info> const> const type_infos,
        std::uint64_t const expression_index,
        bool const take_address_of
    )
    {
        if (expression_index >= type_infos.size())
            return std::nullopt;

        std::optional<Type_info> const& type_info = type_infos[expression_index];
        if (!type_info.has_value())
            return std::nullopt;
        
        if (take_address_of)
            return create_pointer_type_type_reference({type_info->type}, type_info->is_mutable);
        
        return type_info->type;
    }


    bool is_computable_at_compile_time(
        h::Expression const& expression,
        std::optional<h::Type_reference> const& expression_type,
        Validate_expression_parameters const& parameters
    )
    {
        return is_computable_at_compile_time(
            parameters.core_module,
            parameters.scope,
            parameters.statement,
            expression,
            expression_type,
            parameters.expression_types,
            parameters.declaration_database
        );
    }

    bool is_computable_at_compile_time(
        h::Module const& core_module,
        h::compiler::Scope const& scope,
        h::Statement const& statement,
        std::span<std::optional<Type_info> const> const expression_types,
        Declaration_database const& declaration_database
    )
    {
        if (statement.expressions.empty())
            return true;

        h::Expression const& expression = statement.expressions[0];
        std::optional<Type_info> const& expression_type = expression_types[0];

        return is_computable_at_compile_time(
            core_module,
            scope,
            statement,
            expression,
            expression_type.has_value() ? std::optional<h::Type_reference>{expression_type->type} : std::optional<h::Type_reference>{std::nullopt},
            expression_types,
            declaration_database
        );
    }

    bool is_computable_at_compile_time(
        h::Module const& core_module,
        h::compiler::Scope const& scope,
        h::Statement const& statement,
        h::Expression_index const& expression_index,
        std::span<std::optional<Type_info> const> const expression_types,
        Declaration_database const& declaration_database
    )
    {
        h::Expression const& expression = statement.expressions[expression_index.expression_index];
        std::optional<Type_info> const& expression_type = expression_types[expression_index.expression_index];

        return is_computable_at_compile_time(
            core_module,
            scope,
            statement,
            expression,
            expression_type.has_value() ? std::optional<h::Type_reference>{expression_type->type} : std::optional<h::Type_reference>{std::nullopt},
            expression_types,
            declaration_database
        );
    }

    static bool is_declaration_value_computable_at_compile_time(Declaration const& declaration)
    {
        if (std::holds_alternative<h::Enum_declaration const*>(declaration.data))
        {
            return true;
        }
        else if (std::holds_alternative<h::Global_variable_declaration const*>(declaration.data))
        {
            h::Global_variable_declaration const& global_variable_declaration = *std::get<h::Global_variable_declaration const*>(declaration.data);
            return global_variable_declaration.global_type != Global_variable_type::Mutable;
        }
        else if (std::holds_alternative<h::Function_declaration const*>(declaration.data))
        {
            return true;
        }

        return false;
    }

    bool is_computable_at_compile_time(
        h::Module const& core_module,
        h::compiler::Scope const& scope,
        h::Statement const& statement,
        h::Expression const& expression,
        std::optional<h::Type_reference> const& expression_type,
        std::span<std::optional<Type_info> const> const expression_types,
        Declaration_database const& declaration_database
    )
    {
        if (std::holds_alternative<h::Access_expression>(expression.data))
        {
            h::Access_expression const& access_expression = std::get<h::Access_expression>(expression.data);

            if (expression_type.has_value())
            {
                if (is_enum_type(declaration_database, expression_type.value()))
                    return true;

                h::Expression const& left_hand_side = statement.expressions[access_expression.expression.expression_index];

                if (std::holds_alternative<h::Variable_expression>(left_hand_side.data))
                {
                    h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(left_hand_side.data);

                    std::optional<Declaration> const declaration = find_declaration_using_import_alias(
                        declaration_database,
                        core_module,
                        variable_expression.name,
                        access_expression.member_name
                    );
                    if (declaration.has_value())
                        return is_declaration_value_computable_at_compile_time(declaration.value());
                }
            }
        }
        else if (std::holds_alternative<h::Binary_expression>(expression.data))
        {
            h::Binary_expression const& binary_expression = std::get<h::Binary_expression>(expression.data);

            bool const is_lhs_compile_time = is_computable_at_compile_time(
                core_module,
                scope,
                statement,
                binary_expression.left_hand_side,
                expression_types,
                declaration_database
            );
            if (!is_lhs_compile_time)
                return false;

            bool const is_rhs_compile_time = is_computable_at_compile_time(
                core_module,
                scope,
                statement,
                binary_expression.right_hand_side,
                expression_types,
                declaration_database
            );
            if (!is_rhs_compile_time)
                return false;

            return true;
        }
        else if (std::holds_alternative<h::Cast_expression>(expression.data))
        {
            return true;
        }
        else if (std::holds_alternative<h::Constant_expression>(expression.data))
        {
            return true;
        }
        else if (std::holds_alternative<h::Constant_array_expression>(expression.data))
        {
            h::Constant_array_expression const& constant_array_expression = std::get<h::Constant_array_expression>(expression.data);

            for (h::Statement const& element : constant_array_expression.array_data)
            {
                bool const is_compile_time = is_computable_at_compile_time(
                    core_module,
                    scope,
                    element,
                    expression_types,
                    declaration_database
                );
                if (!is_compile_time)
                    return false;
            }

            return true;
        }
        else if (std::holds_alternative<h::Null_pointer_expression>(expression.data))
        {
            return true;
        }
        else if (std::holds_alternative<h::Instantiate_expression>(expression.data))
        {
            h::Instantiate_expression const& instantiate_expression = std::get<h::Instantiate_expression>(expression.data);

            for (h::Instantiate_member_value_pair const& pair : instantiate_expression.members)
            {
                bool const is_compile_time = is_computable_at_compile_time(
                    core_module,
                    scope,
                    statement,
                    pair.value,
                    expression_types,
                    declaration_database
                );

                if (!is_compile_time)
                    return false;
            }

            return true;
        }
        else if (std::holds_alternative<h::Unary_expression>(expression.data))
        {
            h::Unary_expression const& unary_expression = std::get<h::Unary_expression>(expression.data);

            switch (unary_expression.operation)
            {
                case h::Unary_operation::Pre_increment:
                case h::Unary_operation::Post_increment:
                case h::Unary_operation::Pre_decrement:
                case h::Unary_operation::Post_decrement:
                case h::Unary_operation::Indirection:
                case h::Unary_operation::Address_of:
                    return false;
                default:
                    break;
            }

            return is_computable_at_compile_time(
                core_module,
                scope,
                statement,
                statement.expressions[unary_expression.expression.expression_index],
                expression_type,
                expression_types,
                declaration_database
            );
        }
        else if (std::holds_alternative<h::Variable_expression>(expression.data))
        {
            h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(expression.data);
            Variable const* const variable = find_variable_from_scope(scope, variable_expression.name);
            if (variable != nullptr)
                return variable->is_compile_time;

            std::optional<Declaration> const declaration = find_declaration(
                declaration_database,
                core_module.name,
                variable_expression.name
            );

            if (declaration.has_value())
                return is_declaration_value_computable_at_compile_time(declaration.value());
            
            return false;
        }

        return false;
    }

    Global_variable_declaration const* get_global_variable(
        std::string_view const current_module_name,
        h::Expression const& expression,
        Declaration_database const& declaration_database
    )
    {
        // TODO can also be access expression

        if (std::holds_alternative<h::Variable_expression>(expression.data))
        {
            h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(expression.data);

            std::optional<Declaration> const declaration = find_underlying_declaration(
                declaration_database,
                current_module_name,
                variable_expression.name
            );
            if (!declaration.has_value())
                return nullptr;

            if (std::holds_alternative<Global_variable_declaration const*>(declaration->data))
            {
                return std::get<Global_variable_declaration const*>(declaration->data);
            }
        }

        return nullptr;
    }

    bool is_constant_global_variable(
        std::string_view const current_module_name,
        h::Expression const& expression,
        Declaration_database const& declaration_database
    )
    {
        Global_variable_declaration const* const global_variable = get_global_variable(
            current_module_name,
            expression,
            declaration_database
        );
        if (global_variable == nullptr)
            return false;

        return global_variable->global_type == h::Global_variable_type::Constant;
    }

    bool is_mutable_global_variable(
        std::string_view const current_module_name,
        h::Expression const& expression,
        Declaration_database const& declaration_database
    )
    {
        Global_variable_declaration const* const global_variable = get_global_variable(
            current_module_name,
            expression,
            declaration_database
        );
        if (global_variable == nullptr)
            return false;

        return global_variable->global_type == h::Global_variable_type::Mutable;
    }

    bool is_macro_global_variable(
        std::string_view const current_module_name,
        h::Expression const& expression,
        Declaration_database const& declaration_database
    )
    {
        Global_variable_declaration const* const global_variable = get_global_variable(
            current_module_name,
            expression,
            declaration_database
        );
        if (global_variable == nullptr)
            return false;

        return global_variable->global_type == h::Global_variable_type::Macro;
    }

    std::optional<h::Source_range> get_statement_source_range(
        h::Statement const& statement
    )
    {
        if (statement.expressions.empty())
            return std::nullopt;

        h::Expression const& first_expression = statement.expressions.front();
        return first_expression.source_range;
    }

    std::optional<h::Source_range> create_source_range_from_source_location(
        std::optional<h::Source_location> const& source_location,
        std::uint32_t const count
    )
    {
        if (!source_location.has_value())
            return std::nullopt;

        return h::Source_range
        {
            .start =
            {
                .line = source_location->line,
                .column = source_location->column
            },
            .end = 
            {
                .line = source_location->line,
                .column = source_location->column + count
            }
        };
    }

    std::optional<h::Source_range> create_source_range_from_source_location(
        std::optional<h::Source_range_location> const& source_location,
        std::uint32_t const count
    )
    {
        if (!source_location.has_value())
            return std::nullopt;

        return h::Source_range
        {
            .start =
            {
                .line = source_location->range.start.line,
                .column = source_location->range.start.column
            },
            .end = 
            {
                .line = source_location->range.start.line,
                .column = source_location->range.start.column + count
            }
        };
    }

    std::optional<h::Source_range> create_source_range_from_source_position(
        std::optional<h::Source_position> const& source_position,
        std::uint32_t const count
    )
    {
        if (!source_position.has_value())
            return std::nullopt;

        return h::Source_range
        {
            .start =
            {
                .line = source_position->line,
                .column = source_position->column
            },
            .end = 
            {
                .line = source_position->line,
                .column = source_position->column + count
            }
        };
    }

    std::optional<Expression_index> get_implicit_first_call_argument_auxiliary(
        h::Expression_index const& left_side_access_expression_index,
        h::Expression const& left_side_access_expression,
        std::string_view const access_expression_member_name,
        bool const dereference,
        Scope const& scope,
        Declaration_database const& declaration_database
    )
    {
        if (std::holds_alternative<h::Variable_expression>(left_side_access_expression.data))
        {
            h::Variable_expression const& variable_expression = std::get<h::Variable_expression>(left_side_access_expression.data);

            Variable const* const variable = find_variable_from_scope(
                scope,
                variable_expression.name
            );
            if (variable != nullptr)
            {
                std::optional<Type_reference> declaration_type_optional =
                    dereference ?
                    remove_pointer(variable->type) :
                    std::optional<Type_reference>{variable->type};

                if (declaration_type_optional.has_value())
                {
                    std::optional<Declaration> const declaration = find_underlying_declaration(
                        declaration_database,
                        declaration_type_optional.value()
                    );
                    if (declaration.has_value())
                    {
                        if (std::holds_alternative<Struct_declaration const*>(declaration->data))
                        {
                            Struct_declaration const& struct_declaration = *std::get<Struct_declaration const*>(declaration->data);

                            auto const member_location = std::find(
                                struct_declaration.member_names.begin(),
                                struct_declaration.member_names.end(),
                                access_expression_member_name
                            );

                            if (member_location != struct_declaration.member_names.end())
                                return std::nullopt;

                            return left_side_access_expression_index;
                        }    
                    }
                }
            }
        }

        return std::nullopt;
    }

    std::optional<Implicit_argument> get_implicit_first_call_argument(
        h::Statement const& statement,
        h::Call_expression const& expression,
        Scope const& scope,
        Declaration_database const& declaration_database
    )
    {
        h::Expression const& left_side_expression = statement.expressions[expression.expression.expression_index];

        if (std::holds_alternative<h::Access_expression>(left_side_expression.data))
        {
            h::Access_expression const& access_expression = std::get<h::Access_expression>(left_side_expression.data);
            h::Expression const& left_side_access_expression = statement.expressions[access_expression.expression.expression_index];

            std::optional<Expression_index> expression_index = get_implicit_first_call_argument_auxiliary(
                access_expression.expression,
                left_side_access_expression,
                access_expression.member_name,
                false,
                scope,
                declaration_database
            );
            if (expression_index.has_value())
            {
                return Implicit_argument
                {
                    .expression = expression_index.value(),
                    .take_address_of = true
                };
            }
        }
        else if (std::holds_alternative<h::Dereference_and_access_expression>(left_side_expression.data))
        {
            h::Dereference_and_access_expression const& access_expression = std::get<h::Dereference_and_access_expression>(left_side_expression.data);
            h::Expression const& left_side_access_expression = statement.expressions[access_expression.expression.expression_index];

            std::optional<Expression_index> expression_index = get_implicit_first_call_argument_auxiliary(
                access_expression.expression,
                left_side_access_expression,
                access_expression.member_name,
                true,
                scope,
                declaration_database
            );
            if (expression_index.has_value())
            {
                return Implicit_argument
                {
                    .expression = expression_index.value(),
                    .take_address_of = false
                };
            }
        }

        return std::nullopt;
    }

    std::pmr::vector<Expression_index> get_call_aguments(
        h::Call_expression const& expression,
        std::optional<Implicit_argument> const& implicit_first_argument,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        if (!implicit_first_argument.has_value())
            return std::pmr::vector<Expression_index>{expression.arguments, output_allocator};

        std::pmr::vector<Expression_index> output{output_allocator};
        output.reserve(1 + expression.arguments.size());

        output.push_back(implicit_first_argument->expression);
        output.insert(output.end(), expression.arguments.begin(), expression.arguments.end());

        return output;
    }
}
