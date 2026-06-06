module;

#include <lsp/types.h>

export module iris.language_server.go_to_location;

import iris.core;
import iris.core.declarations;
import iris.parser.parse_tree;

namespace iris::language_server
{
    export lsp::TextDocument_DefinitionResult compute_go_to_definition(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        lsp::Position const position,
        bool const client_supports_definition_link
    );
}
