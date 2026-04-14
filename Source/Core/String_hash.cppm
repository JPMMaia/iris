export module h.core.string_hash;

import std;

namespace h
{
    export struct String_hash
    {
        using is_transparent = void;
        
        std::size_t operator()(std::string_view const value) const noexcept
        {
            return std::hash<std::string_view>{}(value);
        }
    };

    export struct String_equal
    {
        using is_transparent = void;
        
        bool operator()(std::string_view const lhs, std::string_view const rhs) const noexcept
        {
            return lhs == rhs;
        }
    };
}
