module;

#include <lsp/types.h>

module iris.language_server.hover;

namespace iris::language_server
{
    lsp::TextDocument_HoverResult compute_hover(
        Declaration_database const& declaration_database,
        iris::parser::Parse_tree const& parse_tree,
        iris::Module const& core_module,
        lsp::Position const& position
    )
    {
        return nullptr;
    }
}
