#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

namespace iris::json
{
    using JSON = nlohmann::json;

    template <typename T>
    JSON to_json(
        T const& value
    );

    template <typename T>
    void from_json(
        JSON const& data,
        T& output
    );

    template <typename T>
    JSON to_json(
        std::pmr::deque<T> const& value
    );

    template <typename T>
    void from_json(
        JSON const& data,
        std::pmr::deque<T>& output
    );

    template <typename T>
    JSON to_json(
        std::pmr::vector<T> const& value
    );

    template <typename T>
    void from_json(
        JSON const& data,
        std::pmr::vector<T>& output
    );

    template <typename ...T>
    JSON to_json(
        std::variant<T...> const& value
    );

    template <typename ...T>
    void from_json(
        JSON const& data,
        std::variant<T...>& output
    );

    template <>
    inline JSON to_json(
        std::filesystem::path const& value
    )
    {
        return value.generic_string();
    }

    template <>
    inline void from_json(
        JSON const& data,
        std::filesystem::path& output
    )
    {
        output = data.get<std::string>();
    }

    template <>
    inline JSON to_json(
        std::pmr::string const& value
    )
    {
        return std::string{value};
    }

    template <>
    inline void from_json(
        JSON const& data,
        std::pmr::string& output
    )
    {
        output = std::pmr::string{data.get<std::string>()};
    }

    template <>
    inline JSON to_json(
        std::uint32_t const& value
    )
    {
        return value;
    }

    template <>
    inline void from_json(
        JSON const& data,
        std::uint32_t& output
    )
    {
        output = data.get<std::uint32_t>();
    }

    template <>
    inline JSON to_json(
        std::uint64_t const& value
    )
    {
        return value;
    }

    template <>
    inline void from_json(
        JSON const& data,
        std::uint64_t& output
    )
    {
        output = data.get<std::uint64_t>();
    }

    template <>
    inline JSON to_json(
        bool const& value
    )
    {
        return value;
    }

    template <>
    inline void from_json(
        JSON const& data,
        bool& output
    )
    {
        output = data.get<bool>();
    }

    template <typename T>
    JSON to_json(
        std::optional<T> const& value
    )
    {
        if (value.has_value())
            return to_json(value.value());
        return nullptr;
    }

    template <typename T>
    void from_json(
        JSON const& data,
        std::optional<T>& output
    )
    {
        if (data.is_null())
        {
            output = std::nullopt;
            return;
        }
            
        T value{};
        iris::json::from_json(data, value);
        output = value;
    }

    template <typename T>
    JSON to_json(
        std::pmr::deque<T> const& value
    )
    {
        JSON json;

        json["size"] = value.size();

        JSON array_json = JSON::array();
        for (std::size_t index = 0; index < value.size(); ++index)
            array_json.push_back(to_json(value[index]));
        json["elements"] = std::move(array_json);

        return json;
    }

    template <typename T>
    void from_json(
        JSON const& data,
        std::pmr::deque<T>& output
    )
    {
        JSON const& array_json = data.at("elements");

        for (JSON const& element_data : array_json)
        {
            T element{};
            iris::json::from_json(element_data, element);
            output.push_back(std::move(element));
        }
    }

    template <typename T>
    JSON to_json(
        std::pmr::vector<T> const& value
    )
    {
        JSON json;

        json["size"] = value.size();

        JSON array_json = JSON::array();
        for (std::size_t index = 0; index < value.size(); ++index)
            array_json.push_back(to_json(value[index]));
        json["elements"] = std::move(array_json);

        return json;
    }

    template <typename T>
    void from_json(
        JSON const& data,
        std::pmr::vector<T>& output
    )
    {
        JSON const& array_json = data.at("elements");

        output.reserve(array_json.size());
        for (JSON const& element_data : array_json)
        {
            T element{};
            iris::json::from_json(element_data, element);
            output.push_back(std::move(element));
        }
    }

    namespace detail
    {
        template <std::size_t Index, typename Variant>
        bool try_set_variant_from_json(
            std::size_t const target_index,
            JSON const& data,
            Variant& output
        )
        {
            if (Index != target_index)
                return false;

            std::variant_alternative_t<Index, Variant> value{};
            iris::json::from_json(data, value);
            output = std::move(value);
            return true;
        }

        template <typename Variant, std::size_t ...Indices>
        void from_json_variant(
            std::size_t const index,
            JSON const& data,
            Variant& output,
            std::index_sequence<Indices...>
        )
        {
            (try_set_variant_from_json<Indices>(index, data, output) || ...);
        }
    }

    template <typename ...T>
    JSON to_json(
        std::variant<T...> const& value
    )
    {
        JSON json;
        json["index"] = value.index();
        std::visit([&json](auto const& v)
        {
            json["value"] = to_json(v);
        }, value);
        return json;
    }

    template <typename ...T>
    void from_json(
        JSON const& data,
        std::variant<T...>& output
    )
    {
        std::size_t const index = data.at("index").get<std::size_t>();
        detail::from_json_variant(index, data.at("value"), output, std::index_sequence_for<T...>{});
    }
}
