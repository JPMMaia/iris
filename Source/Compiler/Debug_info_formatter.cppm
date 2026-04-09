module;

#include <cstdint>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>

export module h.compiler.debug_info_formatter;

import h.core;
import h.core.declarations;

namespace h::compiler
{
    export std::pmr::string format_debug_primitive_type_name(
        h::Fundamental_type type,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export std::pmr::string format_debug_primitive_type_name(
        h::Integer_type const& type,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export struct Soa_debug_member_info
    {
        std::pmr::string name;
        std::pmr::string type_name;
    };

    export std::pmr::string format_debug_type_name(
        h::Module const& core_module,
        h::Type_reference const& type_reference,
        h::Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::string format_debug_type_name(
        h::Module const& core_module,
        h::Type_reference const& type_reference,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::string create_soa_array_debug_type_name(
        h::Module const& core_module,
        h::Soa_array_type const& type,
        std::span<Soa_debug_member_info const> member_infos,
        h::Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::string create_soa_array_debug_type_name(
        h::Module const& core_module,
        h::Soa_array_type const& type,
        std::span<Soa_debug_member_info const> member_infos,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );
}
