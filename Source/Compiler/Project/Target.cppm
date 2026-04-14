export module h.compiler.target;

import std;

namespace h::compiler
{
    export struct Target
    {
        std::pmr::string operating_system;
    };

    export Target get_default_target();
}
