module;

#include <assert.h>

export module iris.binary_serializer.generics;

import std;

import iris.core;

namespace iris::binary_serializer
{
    export struct Serializer
    {
        std::pmr::vector<std::byte> data;
    };

    export struct Deserializer
    {
        std::span<std::byte const> data;
        std::size_t offset = 0;
    };

    export template <typename T>
    void serialize(
        Serializer& serializer,
        T const& data
    );

    export template <typename T>
    void deserialize(
        Deserializer& deserializer,
        T& data
    );

    export template <>
    void serialize(Serializer& serializer, std::pmr::string const& value);

    export template <>
    void deserialize(Deserializer& deserializer, std::pmr::string& value);

    export void write_data(
        Serializer& serializer,
        void const* const data,
        std::size_t const size_in_bytes
    )
    {
        std::size_t const offset = serializer.data.size();
        serializer.data.resize(offset + size_in_bytes);
        
        std::memcpy(serializer.data.data() + offset, data, size_in_bytes);
    }

    export void read_data(
        Deserializer& deserializer,
        void* const data,
        std::size_t const size_in_bytes
    )
    {
        if (deserializer.offset + size_in_bytes > deserializer.data.size())
        {
            assert(false);
            return;
        }

        std::memcpy(data, deserializer.data.data() + deserializer.offset, size_in_bytes);
        deserializer.offset += size_in_bytes;
    }

    export void write_bool(
        Serializer& serializer,
        bool const data
    )
    {
        std::byte const value = static_cast<std::byte>(data);
        write_data(serializer, &value, sizeof(value));
    }

    export bool read_bool(
        Deserializer& deserializer
    )
    {
        bool value = false;
        read_data(deserializer, &value, sizeof(value));
        return value;
    }

    export void write_uint64(
        Serializer& serializer,
        std::uint64_t const data
    )
    {
        write_data(serializer, &data, sizeof(data));
    }

    export std::uint64_t read_uint64(
        Deserializer& deserializer
    )
    {
        std::uint64_t data = 0;
        read_data(deserializer, &data, sizeof(data));
        return data;
    }

    export template <>
    void serialize(Serializer& serializer, std::filesystem::path const& value)
    {
        serialize(serializer, std::pmr::string{value.generic_string()});
    }

    export template <>
    void deserialize(Deserializer& deserializer, std::filesystem::path& value)
    {
        std::pmr::string string_value;
        deserialize(deserializer, string_value);
        value = string_value;
    }

    export template <typename T>
    void serialize(Serializer& serializer, std::optional<T> const& value)
    {
        if (value.has_value())
        {
            write_bool(serializer, true);
            serialize(serializer, value.value());
        }
        else
        {
            write_bool(serializer, false);
        }
    }

    export template <typename T>
    void deserialize(Deserializer& deserializer, std::optional<T>& value)
    {
        bool const has_value = read_bool(deserializer);

        if (has_value)
        {
            T v;
            deserialize(deserializer, v);
            value = v;
        }
    }

    export template <>
    void serialize(Serializer& serializer, std::pmr::string const& value)
    {
        write_uint64(serializer, value.size());
        write_data(serializer, value.data(), value.size());
    }

    export template <>
    void deserialize(Deserializer& deserializer, std::pmr::string& value)
    {
        std::uint64_t const size_in_bytes = read_uint64(deserializer);

        value.resize(size_in_bytes);
        read_data(deserializer, value.data(), value.size());
    }

    export template <typename... T>
    void serialize(Serializer& serializer, std::variant<T...> const& value)
    {
        write_uint64(serializer, value.index());

        auto const visitor = [&](auto const& data) -> void
        {
            serialize(serializer, data);
        };

        std::visit(visitor, value);
    }

    export template <typename... Ts>
    void deserialize(Deserializer& deserializer, std::variant<Ts...>& value)
    {
        std::uint64_t const index = read_uint64(deserializer);

        std::variant<Ts...> result;

        auto const construct = [&](auto I) -> std::variant<Ts...>
        {
            using T = std::tuple_element_t<I, std::tuple<Ts...>>;
            T v;
            deserialize(deserializer, v);
            return v;
        };

        using func_t = std::variant<Ts...> (Deserializer&);
        func_t* funcs[] =
        {
            [](Deserializer& deserializer) -> std::variant<Ts...>
            {
                Ts v;
                deserialize(deserializer, v);
                return v;
            }...
        };
        
        value = funcs[index](deserializer);
    }

    export template <typename T>
    void serialize(Serializer& serializer, std::pmr::deque<T> const& value)
    {
        write_uint64(serializer, value.size());

        for (std::size_t index = 0; index < value.size(); ++index)
            serialize(serializer, value[index]);
    }

    export template <typename T>
    void deserialize(Deserializer& deserializer, std::pmr::deque<T>& value)
    {
        std::uint64_t const size_in_bytes = read_uint64(deserializer);

        value.resize(size_in_bytes);

        for (std::size_t index = 0; index < value.size(); ++index)
        {
            T element;
            deserialize(deserializer, element);
            value[index] = std::move(element);
        }
    }

    export template <typename T>
    void serialize(Serializer& serializer, std::pmr::vector<T> const& value)
    {
        write_uint64(serializer, value.size());

        for (std::size_t index = 0; index < value.size(); ++index)
            serialize(serializer, value[index]);
    }

    export template <typename T>
    void deserialize(Deserializer& deserializer, std::pmr::vector<T>& value)
    {
        std::uint64_t const size_in_bytes = read_uint64(deserializer);

        value.resize(size_in_bytes);

        for (std::size_t index = 0; index < value.size(); ++index)
        {
            T element;
            deserialize(deserializer, element);
            value[index] = std::move(element);
        }
    }

    export template <typename T>
    void serialize(Serializer& serializer, T const& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable!");

        write_data(serializer, &value, sizeof(T));
    }

    export template <typename T>
    void deserialize(Deserializer& deserializer, T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable!");

        read_data(deserializer, &value, sizeof(T));
    }
}
