export module iris.compiler.import_usages_pass;

import std;

import iris.core;

namespace iris::compiler
{
    export void add_import_usages(
        iris::Module& core_module,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );
}
