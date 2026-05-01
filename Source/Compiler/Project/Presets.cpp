module;

#include <nlohmann/json.hpp>

module iris.compiler.presets;

import std;

import iris.common;
import iris.compiler.expressions;

namespace iris::compiler
{
    static std::filesystem::path resolve_path(std::filesystem::path const& path, std::filesystem::path const& root)
    {
        if (path.is_absolute())
            return path.lexically_normal();

        return (root / path).lexically_normal();
    }

    static std::pmr::vector<std::filesystem::path> parse_path_array(
        nlohmann::json const& root,
        std::string_view const key,
        std::filesystem::path const& presets_directory
    )
    {
        if (!root.contains(key))
            return {};

        nlohmann::json const& value = root.at(key);
        if (!value.is_array())
            throw std::runtime_error(std::format("'{}' must be an array.", key));

        std::pmr::vector<std::filesystem::path> paths;
        std::pmr::set<std::pmr::string> seen;

        for (nlohmann::json const& element : value)
        {
            if (!element.is_string())
                throw std::runtime_error(std::format("'{}' must contain only strings.", key));

            std::filesystem::path const parsed = resolve_path(std::filesystem::path{ element.get<std::string>() }, presets_directory);
            std::pmr::string const normalized = std::pmr::string{parsed.generic_string()};

            if (seen.contains(normalized))
                continue;

            seen.insert(normalized);
            paths.push_back(parsed);
        }

        return paths;
    }

    static std::optional<std::filesystem::path> parse_build_directory(
        nlohmann::json const& root,
        std::filesystem::path const& presets_directory
    )
    {
        if (!root.contains("build_directory"))
            return std::nullopt;

        nlohmann::json const& value = root.at("build_directory");
        if (!value.is_string())
            throw std::runtime_error("'build_directory' must be a string.");

        return resolve_path(std::filesystem::path{ value.get<std::string>() }, presets_directory);
    }

    static std::optional<Contract_options> parse_function_contract_options(nlohmann::json const& root)
    {
        if (!root.contains("function_contracts"))
            return std::nullopt;

        nlohmann::json const& value = root.at("function_contracts");
        if (!value.is_string())
            throw std::runtime_error("'function_contracts' must be a string.");

        std::string_view const string = value.get_ref<std::string const&>();
        if (string == "disabled")
            return Contract_options::Disabled;

        if (string == "log_error_and_abort")
            return Contract_options::Log_error_and_abort;

        throw std::runtime_error("'function_contracts' must be either 'disabled' or 'log_error_and_abort'.");
    }

    static std::optional<bool> parse_output_llvm_ir(nlohmann::json const& root)
    {
        if (!root.contains("output_llvm_ir"))
            return std::nullopt;

        nlohmann::json const& value = root.at("output_llvm_ir");
        if (!value.is_boolean())
            throw std::runtime_error("'output_llvm_ir' must be a boolean.");

        return value.get<bool>();
    }

    static Environment_variables parse_environment_variables(nlohmann::json const& root)
    {
        if (!root.contains("environment_variables"))
            return {};

        nlohmann::json const& value = root.at("environment_variables");
        if (!value.is_object())
            throw std::runtime_error("'environment_variables' must be an object.");

        Environment_variables output;
        output.reserve(value.size());

        for (auto const& [key, item] : value.items())
        {
            if (!item.is_string())
                throw std::runtime_error(std::format("'environment_variables.{}' must be a string.", key));

            output.insert_or_assign(std::pmr::string{ key }, std::pmr::string{ item.get<std::string>() });
        }

        return output;
    }

    std::optional<Presets> try_get_presets(std::filesystem::path const& presets_file_path)
    {
        if (!std::filesystem::exists(presets_file_path))
            return std::nullopt;

        std::optional<std::pmr::string> const json_data = iris::common::get_file_contents(presets_file_path.c_str());
        if (!json_data.has_value())
            throw std::runtime_error(std::format("Failed to read contents of {}", presets_file_path.generic_string()));

        nlohmann::json const root = nlohmann::json::parse(json_data.value());
        if (!root.is_object())
            throw std::runtime_error("Presets root must be a JSON object.");

        std::filesystem::path const presets_directory = presets_file_path.parent_path();

        Presets presets
        {
            .build_directory_path = parse_build_directory(root, presets_directory),
            .repository_paths = parse_path_array(root, "repository_paths", presets_directory),
            .header_search_paths = parse_path_array(root, "header_search_paths", presets_directory),
            .function_contract_options = parse_function_contract_options(root),
            .output_llvm_ir = parse_output_llvm_ir(root),
            .environment_variables = parse_environment_variables(root),
        };

        return presets;
    }
}
