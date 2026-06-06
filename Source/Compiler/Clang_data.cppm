export module iris.compiler.clang_data;

import std;

namespace iris::compiler
{    
    export struct Clang_data;
    export using Clang_data_pointer = std::unique_ptr<Clang_data, void(*)(Clang_data*)>;

    export struct Clang_module_declarations;

    export struct Clang_declaration_database;

    export struct Clang_context;
    export using Clang_context_pointer = std::unique_ptr<Clang_context, void(*)(Clang_context*)>;

    export struct Clang_module_data;
    export using Clang_module_data_pointer = std::unique_ptr<Clang_module_data, void(*)(Clang_module_data*)>;
}
