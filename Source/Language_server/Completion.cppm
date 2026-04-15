module;

#include <span>

#include <lsp/types.h>

export module iris.language_server.completion;

import iris.compiler.artifact;
import iris.core;
import iris.core.declarations;
import iris.parser.parse_tree;

namespace iris::language_server
{
    export lsp::TextDocument_CompletionResult compute_completion(
        std::span<iris::compiler::Artifact const> const artifacts,
        std::span<iris::Module const> const header_modules,
        std::span<iris::Module const> const core_modules,
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        lsp::Position const position
    );
}
