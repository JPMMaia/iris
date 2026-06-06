module;

#include <memory_resource>
#include <vector>

#include <lsp/types.h>

export module iris.language_server.inlay_hints;

import iris.core;
import iris.core.declarations;

namespace iris::language_server
{
    export std::pmr::vector<lsp::InlayHint> create_function_inlay_hints(
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        iris::Function_definition const& function_definition,
        iris::Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    void create_inlay_hint_variable_type_label_aux(
        std::vector<lsp::InlayHintLabelPart>& parts,
        iris::Module const& core_module,
        iris::Declaration_database const& declaration_database,
        std::optional<iris::Type_reference> const& type_optional,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::vector<lsp::InlayHintLabelPart> create_inlay_hint_variable_type_label(
        iris::Module const& core_module,
        iris::Declaration_database const& declaration_database,
        iris::Type_reference const& type,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );
}
