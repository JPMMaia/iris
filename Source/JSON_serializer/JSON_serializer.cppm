module;

#include "Generics.h"

#include <cassert>

export module iris.json_serializer;

import std;

import iris.json_serializer.generated;
//import iris.json_serializer.generics;
import iris.common;
import iris.core;

namespace iris::json
{
    export std::optional<JSON> serialize_module(
        iris::Module const& core_module
    )
    {
        return to_json(core_module);
    }

    export std::optional<iris::Module> deserialize_module(
        JSON const& json
    )
    {
        iris::Module output{};
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
        iris::Module const& core_module
    )
    {
        std::optional<JSON> const data = serialize_module(
            core_module
        );
        if (!data.has_value())
            return false;

        std::string const dump = data->dump(4);
        iris::common::write_to_file(file_path, dump);
        return true;
    }

    export std::optional<iris::Module> read_module_from_file(
        std::filesystem::path const& file_path
    )
    {
        std::optional<JSON> const data = iris::common::get_file_contents(file_path);
        if (!data.has_value())
            return std::nullopt;

        std::optional<iris::Module> core_module = deserialize_module(
            data.value()
        );

        return core_module;
    }
}
