export module iris.binary_serializer;

import std;

import iris.binary_serializer.generated;
import iris.binary_serializer.generics;
import iris.common;
import iris.core;

namespace iris::binary_serializer
{
    export std::optional<std::pmr::vector<std::byte>> serialize_module(
        iris::Module const& core_module,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        Serializer serializer
        {
            .data{temporaries_allocator}
        };
        serialize(serializer, core_module);

        return std::pmr::vector<std::byte>{std::move(serializer.data), output_allocator};
    }

    export std::optional<iris::Module> deserialize_module(
        std::span<std::byte const> const data
    )
    {
        Deserializer deserializer
        {
            .data = data,
            .offset = 0,
        };

        iris::Module core_module = {};
        deserialize(deserializer, core_module);

        return core_module;
    }

    export bool write_module_to_file(
        std::filesystem::path const& file_path,
        iris::Module const& core_module,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::optional<std::pmr::vector<std::byte>> const data = serialize_module(
            core_module,
            temporaries_allocator,
            temporaries_allocator
        );
        if (!data.has_value())
            return false;

        iris::common::write_binary_file(file_path, data.value());
        return true;
    }

    export std::optional<iris::Module> read_module_from_file(
        std::filesystem::path const& file_path
    )
    {
        std::optional<std::pmr::vector<std::byte>> const data = iris::common::read_binary_file(file_path);
        if (!data.has_value())
            return std::nullopt;

        std::optional<iris::Module> core_module = deserialize_module(
            data.value()
        );

        return core_module;
    }
}
