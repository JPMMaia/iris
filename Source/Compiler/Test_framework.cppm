export module iris.compiler.test_framework;

import std;

import iris.core;

namespace iris::compiler
{
    export std::pmr::string get_test_module_name(
        std::string_view const artifact_name
    );

    export std::optional<iris::Module> create_test_module(
        std::string_view const artifact_name,
        std::span<iris::Module const* const> const core_modules,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export iris::Function_pointer_type create_test_check_function_pointer_type();
    export iris::Function_declaration create_test_check_function_declaration();
}
