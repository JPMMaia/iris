module iris.compiler.common;

import std;
import llvm;

import iris.core;
import iris.core.declarations;
import iris.core.hash;
import iris.compiler.diagnostic;

namespace iris::compiler
{
    std::string_view to_string_view(llvm::StringRef const string)
    {
        return std::string_view{ string.data(), string.size() };
    }

    std::pmr::string unescape_string_literal(
        std::string_view const value,
        std::optional<Source_position> const& source_position,
        std::pmr::polymorphic_allocator<char> const& output_allocator
    )
    {
        std::pmr::string output{ output_allocator };
        output.reserve(value.size());

        // A single left to right pass. Anything that already consumed a character is appended
        // immediately, so a replacement can never be rescanned and no index arithmetic is needed.
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            char const character = value[index];

            if (character != '\\')
            {
                output.push_back(character);
                continue;
            }

            if (index + 1 == value.size())
                throw Compile_error{ "String literal ends with an incomplete escape sequence.", source_position };

            char const escaped = value[index + 1];
            ++index;

            switch (escaped)
            {
            case '\\': output.push_back('\\'); break;
            case '"': output.push_back('"'); break;
            case '\'': output.push_back('\''); break;
            case 'n': output.push_back('\n'); break;
            case 't': output.push_back('\t'); break;
            case 'r': output.push_back('\r'); break;
            case '0': output.push_back('\0'); break;
            default:
                throw Compile_error{
                    std::format("Unknown escape sequence '\\{}' in string literal.", escaped),
                    source_position
                };
            }
        }

        return output;
    }

    std::string mangle_name(
        std::string_view const module_name,
        std::string_view const declaration_name,
        std::optional<std::string_view> const unique_name
    )
    {
        if (unique_name.has_value())
            return std::string{ *unique_name };

        std::pmr::string module_name_prefix{module_name};
        std::replace(module_name_prefix.begin(), module_name_prefix.end(), '.', '_');

        return std::format("{}_{}", module_name_prefix, declaration_name);
    }

    std::string mangle_name(
        iris::Declaration_database const& declaration_database,
        std::string_view const module_name,
        std::string_view const declaration_name
    )
    {
        std::optional<iris::Declaration> const declaration = find_declaration(declaration_database, module_name, declaration_name);

        if (declaration.has_value())
        {
            std::optional<std::string_view> const unique_name = get_declaration_unique_name(declaration.value());
            return mangle_name(module_name, declaration_name, unique_name);
        }

        return mangle_name(module_name, declaration_name, std::nullopt);
    }

    std::string mangle_name(
        Module const& core_module,
        std::string_view const declaration_name,
        std::optional<std::string_view> const unique_name
    )
    {
        return mangle_name(core_module.name, declaration_name, unique_name);
    }

    std::string mangle_function_name(
        Module const& core_module,
        std::string_view const declaration_name
    )
    {
        std::optional<Function_declaration const*> function_declaration = find_function_declaration(core_module, declaration_name);
        if (!function_declaration.has_value())
            return mangle_name(core_module, declaration_name, std::nullopt);

        std::optional<std::pmr::string> const& unique_name = function_declaration.value()->unique_name;
        return mangle_name(core_module, declaration_name, unique_name);
    }

    std::pmr::string get_lambda_environment_struct_name(std::string_view const generated_function_name)
    {
        return std::pmr::string{ std::format("{}__environment", generated_function_name) };
    }

    std::string mangle_struct_name(
        Module const& core_module,
        std::string_view const declaration_name
    )
    {
        std::optional<Struct_declaration const*> struct_declaration = find_struct_declaration(core_module, declaration_name);
        if (!struct_declaration.has_value())
            return mangle_name(core_module, declaration_name, std::nullopt);

        std::optional<std::pmr::string> const& unique_name = struct_declaration.value()->unique_name;
        return mangle_name(core_module, declaration_name, unique_name);
    }

    std::string mangle_union_name(
        Module const& core_module,
        std::string_view const declaration_name
    )
    {
        std::optional<Union_declaration const*> union_declaration = find_union_declaration(core_module, declaration_name);
        if (!union_declaration.has_value())
            return mangle_name(core_module, declaration_name, std::nullopt);

        std::optional<std::pmr::string> const& unique_name = union_declaration.value()->unique_name;
        return mangle_name(core_module, declaration_name, unique_name);
    }

    llvm::Function* get_llvm_function(
        std::string_view const module_name,
        llvm::Module& llvm_module,
        std::string_view const name,
        std::optional<std::string_view> const unique_name
    )
    {
        std::string const mangled_name = mangle_name(module_name, name, unique_name);
        llvm::Function* const llvm_function = llvm_module.getFunction(mangled_name);
        return llvm_function;
    }

    llvm::Function* get_llvm_function(
        Module const& core_module,
        llvm::Module& llvm_module,
        std::string_view const name
    )
    {
        std::optional<Function_declaration const*> function_declaration = find_function_declaration(core_module, name);
        if (!function_declaration.has_value())
            return nullptr;

        std::optional<std::pmr::string> const& unique_name = function_declaration.value()->unique_name;

        return get_llvm_function(core_module.name, llvm_module, name, unique_name);
    }

    iris::Module const* get_module(
        std::string_view const module_name,
        iris::Module const& core_module,
        std::pmr::unordered_map<std::pmr::string, iris::Module> const& core_module_dependencies
    )
    {
        if (core_module.name == module_name)
            return &core_module;

        // TODO this allocates memory unnecessarily
        auto const location = core_module_dependencies.find(std::pmr::string{module_name});
        if (location != core_module_dependencies.end())
            return &location->second;

        return nullptr;
    }

    llvm::GlobalValue::LinkageTypes to_linkage(
        Linkage const linkage,
        bool const is_test
    )
    {
        if (is_test)
            return llvm::GlobalValue::LinkageTypes::ExternalLinkage;

        switch (linkage)
        {
        case Linkage::External:
            return llvm::GlobalValue::LinkageTypes::ExternalLinkage;
        case Linkage::Private:
        default:
            return llvm::GlobalValue::LinkageTypes::PrivateLinkage;
        }
    }
}
