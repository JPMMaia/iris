module;

#include <cstdint>
#include <functional>
#include <memory_resource>
#include <optional>

#include <lsp/types.h>

export module iris.language_server.signature_help;

import iris.core;
import iris.core.declarations;
import iris.parser.parse_tree;

namespace iris::language_server
{
    export enum class Signature_help_kind
    {
        None,
        Function,
        Struct
    };

    export struct Signature_help_name
    {
        std::pmr::string module_name;
        std::pmr::string declaration_name;
    };

    export Signature_help_kind decide_signature_help_kind(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        lsp::Position const position
    );

    export std::optional<Signature_help_name> find_function_call_module_and_function_name(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        lsp::Position const position
    );

    export std::optional<std::uint32_t> find_function_call_active_parameter(
        iris::parser::Parse_tree const& parse_tree,
        lsp::Position const position
    );

    export lsp::TextDocument_SignatureHelpResult compute_function_signature_help(
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        std::uint32_t const active_parameter
    );

    export std::optional<Signature_help_name> find_instantiate_module_and_struct_name(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        lsp::Position const position
    );

    export std::optional<std::uint32_t> find_instantiate_active_member(
        Declaration_database const& declaration_database,
        iris::Module const& core_module,
        Signature_help_name const& struct_name,
        iris::parser::Parse_tree const& parse_tree,
        lsp::Position const position
    );

    export lsp::TextDocument_SignatureHelpResult compute_struct_signature_help(
        iris::Module const& core_module,
        iris::Struct_declaration const& struct_declaration,
        std::uint32_t const active_member
    );

    export lsp::TextDocument_SignatureHelpResult compute_signature_help(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        lsp::Position const position,
        std::function<void(lsp::LogMessageParams&&)> const& window_log_message
    );
}
