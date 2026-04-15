export module iris.compiler.debug_info_formatter;

import std;

import iris.compiler.types;
import iris.core;
import iris.core.declarations;

namespace iris::compiler
{
    export std::pmr::string format_debug_primitive_type_name(
        iris::Fundamental_type type,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export std::pmr::string format_debug_primitive_type_name(
        iris::Integer_type const& type,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export struct Soa_debug_member_info
    {
        std::pmr::string name;
        std::pmr::string type_name;
    };

    export std::pmr::string format_debug_type_name(
        iris::Module const& core_module,
        iris::Type_reference const& type_reference,
        Debug_type_database const& debug_type_database,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::string create_soa_array_debug_type_name(
        iris::Module const& core_module,
        iris::Soa_array_type const& type,
        Debug_type_database const& debug_type_database,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::string create_soa_array_view_debug_type_name(
        iris::Module const& core_module,
        iris::Soa_array_view_type const& type,
        Debug_type_database const& debug_type_database,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );
}
