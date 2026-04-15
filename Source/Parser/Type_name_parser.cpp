module iris.parser.type_name_parser;

import std;

import iris.core;
import iris.core.types;

namespace iris::parser
{
    static std::optional<Type_reference> parse_integer_type_name(
        std::string_view const type_name
    )
    {
        if (type_name.starts_with("Int"))
        {
            std::string_view const number_of_bits_string = type_name.substr(3);
            std::uint32_t const number_of_bits = parse_number_of_bits(number_of_bits_string);
            return create_integer_type_type_reference(number_of_bits, true);
        }
        else if (type_name.starts_with("Uint"))
        {
            std::string_view const number_of_bits_string = type_name.substr(4);
            std::uint32_t const number_of_bits = parse_number_of_bits(number_of_bits_string);
            return create_integer_type_type_reference(number_of_bits, false);
        }
        else
        {
            return std::nullopt;
        }
    }

    static std::optional<Type_reference> parse_decimal_type_name(
        std::string_view const type_name
    )
    {
        if (!type_name.starts_with("Decimal"))
            return std::nullopt;

        std::string_view const scale_string = type_name.substr(7); // "Decimal" is 7 characters
        if (scale_string.empty() || scale_string.size() > 2)
            return std::nullopt;

        char buffer[3] = { '\0', '\0', '\0' };
        for (std::size_t index = 0; index < scale_string.size(); ++index)
            buffer[index] = scale_string[index];

        int const scale = std::atoi(buffer);
        if (scale < 1 || scale > 18)
            return std::nullopt;

        return create_decimal_type_reference(static_cast<std::uint32_t>(scale));
    }

    static std::optional<Fundamental_type> parse_fundamental_type_name(
        std::string_view const type_name
    )
    {   
        if (type_name == "Bool")
            return iris::Fundamental_type::Bool;
        if (type_name == "Byte")
            return iris::Fundamental_type::Byte;
        if (type_name == "Float16")
            return iris::Fundamental_type::Float16;
        if (type_name == "Float32")
            return iris::Fundamental_type::Float32;
        if (type_name == "Float64")
            return iris::Fundamental_type::Float64;
        if (type_name == "String")
            return iris::Fundamental_type::String;
        if (type_name == "Any_type")
            return iris::Fundamental_type::Any_type;
        if (type_name == "C_bool")
            return iris::Fundamental_type::C_bool;
        if (type_name == "C_char")
            return iris::Fundamental_type::C_char;
        if (type_name == "C_schar")
            return iris::Fundamental_type::C_schar;
        if (type_name == "C_uchar")
            return iris::Fundamental_type::C_uchar;
        if (type_name == "C_short")
            return iris::Fundamental_type::C_short;
        if (type_name == "C_ushort")
            return iris::Fundamental_type::C_ushort;
        if (type_name == "C_int")
            return iris::Fundamental_type::C_int;
        if (type_name == "C_uint")
            return iris::Fundamental_type::C_uint;
        if (type_name == "C_long")
            return iris::Fundamental_type::C_long;
        if (type_name == "C_ulong")
            return iris::Fundamental_type::C_ulong;
        if (type_name == "C_longlong")
            return iris::Fundamental_type::C_longlong;
        if (type_name == "C_ulonglong")
            return iris::Fundamental_type::C_ulonglong;
        if (type_name == "C_longdouble")
            return iris::Fundamental_type::C_longdouble;
        else
            return std::nullopt;
    }

    std::optional<Type_reference> parse_type_name(
        std::string_view const module_name,
        std::string_view const type_name,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::optional<Type_reference> const integer_type = parse_integer_type_name(type_name);
        if (integer_type.has_value())
            return integer_type.value();

        std::optional<Type_reference> const decimal_type = parse_decimal_type_name(type_name);
        if (decimal_type.has_value())
            return decimal_type.value();

        std::optional<Fundamental_type> const fundamental_type = parse_fundamental_type_name(type_name);
        if (fundamental_type.has_value())
            return create_fundamental_type_type_reference(fundamental_type.value());
        
        if (type_name == "void")
            return std::nullopt;
        
        return create_custom_type_reference(module_name, type_name);
    }

    std::uint32_t parse_number_of_bits(
        std::string_view const value
    )
    {
        if (value.empty())
            return 32u;

        if (value.size() > 2)
            return 64u;

        char buffer[3] = { '\0', '\0', '\0' };
        for (std::size_t index = 0; index < value.size(); ++index)
            buffer[index] = value[index];

        int const number_of_bits = std::atoi(buffer);
        if (number_of_bits == 0)
            return 32u;

        return static_cast<std::uint32_t>(number_of_bits);
    }
}
