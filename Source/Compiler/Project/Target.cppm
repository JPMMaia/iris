export module iris.compiler.target;

import std;

namespace iris::compiler
{
    export struct Target
    {
        std::pmr::string operating_system;
    };

    export Target get_default_target();
}
