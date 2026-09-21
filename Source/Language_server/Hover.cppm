module;

#include <lsp/types.h>

export module iris.language_server.hover;

import iris.core;
import iris.core.declarations;
import iris.parser.parse_tree;

namespace iris::language_server
{
    export lsp::TextDocument_HoverResult compute_hover(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        lsp::Position const position
    );
}
