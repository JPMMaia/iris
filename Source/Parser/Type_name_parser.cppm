export module iris.parser.type_name_parser;

import std;

import iris.core;

namespace iris::parser
{
    export std::optional<Type_reference> parse_type_name(
        std::string_view const module_name,
        std::string_view const type_name,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export std::uint32_t parse_number_of_bits(
        std::string_view const suffix
    );
}
