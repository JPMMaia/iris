module;

#include <span>

#include <lsp/types.h>

export module iris.language_server.code_action;

import iris.compiler.diagnostic;
import iris.core;
import iris.core.declarations;
import iris.parser.parse_tree;

namespace iris::language_server
{
    export lsp::TextDocument_CodeActionResult compute_code_actions(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        std::span<iris::compiler::Diagnostic const> const diagnostics,
        lsp::Range const range,
        lsp::CodeActionContext const& context
    );
}
