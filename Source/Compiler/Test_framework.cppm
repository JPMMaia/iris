export module h.compiler.test_framework;

import std;

import h.core;

namespace h::compiler
{
    export std::pmr::string get_test_module_name(
        std::string_view const artifact_name
    );

    export std::optional<h::Module> create_test_module(
        std::string_view const artifact_name,
        std::span<h::Module const* const> const core_modules,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export h::Function_pointer_type create_test_check_function_pointer_type();
    export h::Function_declaration create_test_check_function_declaration();
}
