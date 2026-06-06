export module iris.compiler.presets;

import std;

import iris.compiler.expressions;

namespace iris::compiler
{
    export using Environment_variables = std::pmr::unordered_map<std::pmr::string, std::pmr::string>;

    export struct Presets
    {
        std::optional<std::filesystem::path> build_directory_path;
        std::pmr::vector<std::filesystem::path> repository_paths;
        std::pmr::vector<std::filesystem::path> header_search_paths;
        std::optional<Contract_options> function_contract_options;
        std::optional<bool> output_llvm_ir;
        Environment_variables environment_variables;
    };

    export std::optional<Presets> try_get_presets(std::filesystem::path const& presets_file_path);
}
