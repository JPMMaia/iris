module;

#include <memory_resource>
#include <string>

module iris.compiler.target;

namespace iris::compiler
{
    Target get_default_target()
    {
        return Target
        {
            .operating_system = "windows"
        };
    }
}
