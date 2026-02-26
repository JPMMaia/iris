module;

#include <format>
#include <functional>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

module h.core.types;

import h.common;
import h.core;

namespace h
{
    Type_reference create_array_slice_type_reference(std::pmr::vector<Type_reference> element_type, bool const is_mutable)
    {
        return
        {
            .data = h::Array_slice_type
            {
                .element_type = std::move(element_type),
                .is_mutable = is_mutable,
            }
        };
    }

    bool is_array_slice_type_reference(Type_reference const& type)
    {
        return std::holds_alternative<Array_slice_type>(type.data);
    }

    Type_reference create_bool_type_reference()
    {
        return create_fundamental_type_type_reference(Fundamental_type::Bool);
    }

    bool is_bool(Type_reference const& type)
    {
        if (std::holds_alternative<Fundamental_type>(type.data))
        {
            Fundamental_type const data = std::get<Fundamental_type>(type.data);
            return data == Fundamental_type::Bool;
        }

        return false;
    }

    bool is_c_bool(Type_reference const& type)
    {
        if (std::holds_alternative<Fundamental_type>(type.data))
        {
            Fundamental_type const data = std::get<Fundamental_type>(type.data);
            return data == Fundamental_type::C_bool;
        }

        return false;
    }


    Type_reference create_builtin_type_reference(std::pmr::string value)
    {
        return Type_reference
        {
            .data = Builtin_type_reference
            {
                .value = std::move(value),
            }
        }; 
    }

    bool is_builtin_type_reference(Type_reference const& type)
    {
        return std::holds_alternative<Builtin_type_reference>(type.data);
    }


    Type_reference create_constant_array_type_reference(std::pmr::vector<Type_reference> value_type, std::uint64_t size)
    {
        return Type_reference
        {
            .data = Constant_array_type
            {
                .value_type = std::move(value_type),
                .size = size,
            }
        }; 
    }

    std::uint64_t get_constant_array_type_size(Type_reference const& type_reference)
    {
        return std::get<Constant_array_type>(type_reference.data).size;
    }

    bool is_constant_array_type_reference(Type_reference const& type)
    {
        return std::holds_alternative<Constant_array_type>(type.data);
    }


    Type_reference create_custom_type_reference(std::string_view const module_name, std::string_view const name)
    {
        return Type_reference
        {
            .data = Custom_type_reference
            {
                .module_reference =
                {
                    .name = std::pmr::string{ module_name },
                },
                .name = std::pmr::string{ name }
            }
        };
    }

    bool is_custom_type_reference(Type_reference const& type)
    {
        return std::holds_alternative<Custom_type_reference>(type.data);
    }

    Custom_type_reference fix_custom_type_reference(
        Custom_type_reference type,
        Module const& core_module
    )
    {
        if (type.module_reference.name.empty() || type.module_reference.name == core_module.name)
        {
            type.module_reference.name = core_module.name;
        }
        else
        {
            auto const location = std::find_if(
                core_module.dependencies.alias_imports.begin(),
                core_module.dependencies.alias_imports.end(),
                [&](Import_module_with_alias const& alias_import) { return alias_import.alias == type.module_reference.name; }
            );

            if (location == core_module.dependencies.alias_imports.end())
                h::common::print_message_and_exit(std::format("Could not find import alias '{}' in module '{}'", type.module_reference.name, core_module.name));

            type.module_reference.name = location->module_name;
        }

        return type;
    }

    Type_reference fix_custom_type_reference(
        Type_reference type,
        Module const& core_module
    )
    {
        if (std::holds_alternative<Custom_type_reference>(type.data))
        {
            type.data = fix_custom_type_reference(std::get<Custom_type_reference>(type.data), core_module);
        }

        return type;
    }

    void fix_custom_type_references(
        Module& core_module
    )
    {
        auto const process_type_reference = [&](std::string_view const declaration_name, Type_reference const& type_reference) -> bool
        {
            Type_reference* const reference = const_cast<Type_reference*>(&type_reference);
            *reference = fix_custom_type_reference(type_reference, core_module);
            return false;
        };

        visit_type_references_recursively_with_declaration_name(core_module, process_type_reference);
    }


    Type_reference create_function_type_type_reference(
        Function_type const& function_type,
        std::pmr::vector<std::pmr::string> input_parameter_names,
        std::pmr::vector<std::pmr::string> output_parameter_names
    )
    {
        return Type_reference
        {
            .data = h::Function_pointer_type
            {
                .type = function_type,
                .input_parameter_names = std::move(input_parameter_names),
                .output_parameter_names = std::move(output_parameter_names),
            }
        };
    }

    std::optional<Type_reference> get_function_output_type_reference(Function_type const& function_type, Module const& core_module)
    {
        if (function_type.output_parameter_types.empty())
            return std::nullopt;

        if (function_type.output_parameter_types.size() == 1)
            return function_type.output_parameter_types.front();

        // TODO function with multiple output arguments
        return std::nullopt;
    }

    std::optional<Type_reference> get_function_output_type_reference(Type_reference const& type, Module const& core_module)
    {
        if (std::holds_alternative<Function_pointer_type>(type.data))
        {
            Function_pointer_type const& function_pointer_type = std::get<Function_pointer_type>(type.data);
            return get_function_output_type_reference(function_pointer_type.type, core_module);
        }

        throw std::runtime_error{ "Type is not a function type!" };
    }

    bool is_function_pointer(Type_reference const& type)
    {
        return std::holds_alternative<Function_pointer_type>(type.data);
    }


    Type_reference create_fundamental_type_type_reference(Fundamental_type const value)
    {
        return Type_reference
        {
            .data = value
        };
    }

    bool is_byte(Type_reference const& type)
    {
        if (std::holds_alternative<Fundamental_type>(type.data))
        {
            Fundamental_type const data = std::get<Fundamental_type>(type.data);
            return (data == Fundamental_type::Byte);
        }

        return false;
    }

    bool is_floating_point(Type_reference const& type)
    {
        if (std::holds_alternative<Fundamental_type>(type.data))
        {
            Fundamental_type const data = std::get<Fundamental_type>(type.data);
            return (data == Fundamental_type::Float16) || (data == Fundamental_type::Float32) || (data == Fundamental_type::Float64);
        }

        return false;
    }

    bool is_any_type(Type_reference const& type)
    {
        if (std::holds_alternative<Fundamental_type>(type.data))
        {
            Fundamental_type const data = std::get<Fundamental_type>(type.data);
            return (data == Fundamental_type::Any_type);
        }

        return false;
    }

    Type_reference create_c_string_type_reference(bool const is_mutable)
    {
        return Type_reference
        {
            .data = Pointer_type
            {
                .element_type =
                {
                    Type_reference
                    {
                        .data = Fundamental_type::C_char
                    }
                },
                .is_mutable = is_mutable
            }
        };
    }

    bool is_c_string(Type_reference const& type_reference)
    {
        if (std::holds_alternative<Pointer_type>(type_reference.data))
        {
            Pointer_type const& pointer_type = std::get<Pointer_type>(type_reference.data);

            if (!pointer_type.element_type.empty())
            {
                Type_reference const& value_type = pointer_type.element_type[0];
                if (std::holds_alternative<Fundamental_type>(value_type.data))
                {
                    Fundamental_type const fundamental_type = std::get<Fundamental_type>(value_type.data);
                    if (fundamental_type == Fundamental_type::C_char)
                    {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    Type_reference create_integer_type_type_reference(std::uint32_t const number_of_bits, bool const is_signed)
    {
        return Type_reference
        {
            .data = Integer_type
            {
                .number_of_bits = number_of_bits,
                .is_signed = is_signed
            }
        };
    }

    bool is_integer(Fundamental_type const type)
    {
        switch (type)
        {
        case Fundamental_type::C_char:
        case Fundamental_type::C_schar:
        case Fundamental_type::C_uchar:
        case Fundamental_type::C_short:
        case Fundamental_type::C_ushort:
        case Fundamental_type::C_int:
        case Fundamental_type::C_uint:
        case Fundamental_type::C_long:
        case Fundamental_type::C_ulong:
        case Fundamental_type::C_longlong:
        case Fundamental_type::C_ulonglong:
            return true;
        default:
            return false;
        }
    }

    bool is_integer(Type_reference const& type)
    {
        return std::holds_alternative<Integer_type>(type.data)
            || (std::holds_alternative<Fundamental_type>(type.data) && is_integer(std::get<Fundamental_type>(type.data)));
    }

    bool is_signed_integer(Type_reference const& type)
    {
        if (std::holds_alternative<Integer_type>(type.data))
        {
            Integer_type const& data = std::get<Integer_type>(type.data);
            return data.is_signed;
        }
        else if (std::holds_alternative<Fundamental_type>(type.data))
        {
            Fundamental_type const& data = std::get<Fundamental_type>(type.data);

            switch (data)
            {
            case Fundamental_type::C_char:
                // TODO this is implementation defined
            case Fundamental_type::C_schar:
            case Fundamental_type::C_short:
            case Fundamental_type::C_int:
            case Fundamental_type::C_long:
            case Fundamental_type::C_longlong:
                return true;
            case Fundamental_type::C_uchar:
            case Fundamental_type::C_ushort:
            case Fundamental_type::C_uint:
            case Fundamental_type::C_ulong:
            case Fundamental_type::C_ulonglong:
                return false;
            default:
                return false;
            }
        }

        return false;
    }

    bool is_unsigned_integer(Type_reference const& type)
    {
        if (std::holds_alternative<Integer_type>(type.data))
        {
            Integer_type const& data = std::get<Integer_type>(type.data);
            return !data.is_signed;
        }
        else if (std::holds_alternative<Fundamental_type>(type.data))
        {
            Fundamental_type const& data = std::get<Fundamental_type>(type.data);

            switch (data)
            {
            case Fundamental_type::C_uchar:
            case Fundamental_type::C_ushort:
            case Fundamental_type::C_uint:
            case Fundamental_type::C_ulong:
            case Fundamental_type::C_ulonglong:
                return true;
            default:
                return false;
            }
        }

        return false;
    }

    bool is_number_or_c_number(Type_reference const& type)
    {
        if (is_integer(type))
            return true;
        
        if (is_floating_point(type))
            return true;

        if (std::holds_alternative<Fundamental_type>(type.data))
        {
            Fundamental_type const data = std::get<Fundamental_type>(type.data);
            switch (data)
            {
                case Fundamental_type::Float16:
                case Fundamental_type::Float32:
                case Fundamental_type::Float64:
                case Fundamental_type::C_char:
                case Fundamental_type::C_schar:
                case Fundamental_type::C_uchar:
                case Fundamental_type::C_short:
                case Fundamental_type::C_ushort:
                case Fundamental_type::C_int:
                case Fundamental_type::C_uint:
                case Fundamental_type::C_long:
                case Fundamental_type::C_ulong:
                case Fundamental_type::C_longlong:
                case Fundamental_type::C_ulonglong:
                case Fundamental_type::C_longdouble:
                    return true;
                default:
                    return false;
            }
        }

        return false;
    }

    Type_reference create_null_pointer_type_type_reference()
    {
        Null_pointer_type pointer_type
        {
        };

        return Type_reference
        {
            .data = std::move(pointer_type)
        };
    }

    bool is_null_pointer_type(Type_reference const& type)
    {
        return std::holds_alternative<Null_pointer_type>(type.data);
    }

    Type_reference create_pointer_type_type_reference(std::pmr::vector<Type_reference> element_type, bool const is_mutable)
    {
        Pointer_type pointer_type
        {
            .element_type = std::move(element_type),
            .is_mutable = is_mutable
        };

        return Type_reference
        {
            .data = std::move(pointer_type)
        };
    }

    std::optional<Type_reference> remove_pointer(Type_reference const& type)
    {
        if (std::holds_alternative<Pointer_type>(type.data))
        {
            Pointer_type const& pointer_type = std::get<Pointer_type>(type.data);
            if (pointer_type.element_type.empty())
                return {};

            return pointer_type.element_type.front();
        }

        throw std::runtime_error("Type is not a pointer type!");
    }

    bool is_pointer(Type_reference const& type)
    {
        return std::holds_alternative<Pointer_type>(type.data) || std::holds_alternative<Null_pointer_type>(type.data);
    }

    bool is_non_void_pointer(Type_reference const& type)
    {
        if (std::holds_alternative<Pointer_type>(type.data))
        {
            Pointer_type const& pointer_type = std::get<Pointer_type>(type.data);
            return !pointer_type.element_type.empty();
        }

        return false;
    }

    std::optional<Type_reference> get_element_or_pointee_type(Type_reference const& type)
    {
        if (std::holds_alternative<Array_slice_type>(type.data))
        {
            Array_slice_type const& array_slice_type = std::get<Array_slice_type>(type.data);
            if (array_slice_type.element_type.empty())
                return std::nullopt;

            return array_slice_type.element_type.front();
        }
        else if (std::holds_alternative<Constant_array_type>(type.data))
        {
            Constant_array_type const& constant_array_type = std::get<Constant_array_type>(type.data);
            if (constant_array_type.value_type.empty())
                return std::nullopt;

            return constant_array_type.value_type.front();
        }
        else if (std::holds_alternative<Pointer_type>(type.data))
        {
            Pointer_type const& pointer_type = std::get<Pointer_type>(type.data);
            if (pointer_type.element_type.empty())
                return std::nullopt;

            return pointer_type.element_type.front();
        }

        return std::nullopt;
    }

    std::optional<std::string_view> get_type_module_name(Type_reference const& type)
    {
        if (std::holds_alternative<h::Custom_type_reference>(type.data))
        {
            h::Custom_type_reference const& data = std::get<h::Custom_type_reference>(type.data);
            return data.module_reference.name;
        }
        else if (std::holds_alternative<h::Type_instance>(type.data))
        {
            h::Type_instance const& data = std::get<h::Type_instance>(type.data);
            return data.type_constructor.module_reference.name;
        }

        return std::nullopt;
    }

    h::Struct_declaration create_array_slice_type_struct_declaration(std::pmr::vector<Type_reference> const& element_type)
    {
        h::Type_reference const uint64_type
        {
            .data = h::Integer_type
            {
                .number_of_bits = 64,
                .is_signed = false
            }
        };

        return h::Struct_declaration
        {
            .name = "Generic_array_slice",
            .member_types = {
                h::create_pointer_type_type_reference(element_type, false),
                h::create_integer_type_type_reference(64, false)
            },
            .member_names = {
                "data",
                "length"
            },
            .member_bit_fields = {
                std::nullopt,
                std::nullopt
            },
            .member_default_values = {
                h::Statement{ 
                    .expressions = {
                        h::Expression{ .data = h::Null_pointer_expression{} }
                    }
                },
                h::Statement{
                    .expressions = {
                        h::Expression{
                            .data = h::Constant_expression {
                                .type = uint64_type,
                                .data = "0"
                            }
                        }
                    }
                }
            },
            .is_packed = false,
            .is_literal = false,
        };
    }
}
