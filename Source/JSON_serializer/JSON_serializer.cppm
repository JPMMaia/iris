module;

#include "Generics.h"

#include <cassert>

export module h.json_serializer;

import std;

import h.json_serializer.generated;
//import h.json_serializer.generics;
import h.common;
import h.core;

namespace h::json
{
    export std::optional<JSON> serialize_module(
        h::Module const& core_module
    )
    {
        return to_json(core_module);
    }

    export std::optional<h::Module> deserialize_module(
        std::string_view const data
    )
    {
        JSON const json = JSON::parse(data);

        h::Module output{};
        from_json(json, output);
        return output;
    }

    export template<typename T>
    std::optional<T> read_enum(
        std::string_view const data
    )
    {
        JSON const json = data;

        T output{};
        from_json(json, output);
        return output;
    }

    export template<typename T>
    std::optional<T> read(
        std::string_view const data
    )
    {
        JSON const json = JSON::parse(data);

        T output{};
        from_json(json, output);
        return output;
    }

    export template<typename T>
    std::pmr::string write(
        T const& data
    )
    {
        JSON const json = to_json(data);
        return std::pmr::string{json.dump()};
    }

    export bool write_module_to_file(
        std::filesystem::path const& file_path,
        h::Module const& core_module
    )
    {
        std::optional<JSON> const data = serialize_module(
            core_module
        );
        if (!data.has_value())
            return false;

        std::string const dump = data->dump(4);
        h::common::write_to_file(file_path, dump);
        return true;
    }

    export std::optional<h::Module> read_module_from_file(
        std::filesystem::path const& file_path
    )
    {
        std::optional<JSON> const data = h::common::get_file_contents(file_path);
        if (!data.has_value())
            return std::nullopt;

        std::optional<h::Module> core_module = deserialize_module(
            data.value()
        );

        return core_module;
    }
}
