module;

#include <memory_resource>
#include <span>
#include <string>
#include <string_view>

#include <lsp/types.h>

export module iris.language_server.core;

import iris.core;

namespace iris::language_server
{
    export lsp::Position to_lsp_position(
        iris::Source_position const& input
    );

    export lsp::Range to_lsp_range(
        iris::Source_range const& input
    );

    export iris::Source_position to_source_position(
        lsp::Position const& input
    );

    export iris::Source_range to_source_range(
        lsp::Range const& input
    );

    export std::pmr::u8string convert_to_utf_8_string(
        std::string_view const& input,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export bool compare_document_uris(
        lsp::DocumentUri const& left,
        lsp::DocumentUri const& right
    );

    export std::optional<lsp::PreviousResultId> find_previous_result_id(
        std::span<lsp::PreviousResultId const> const result_ids,
        lsp::DocumentUri const& document_uri
    );
}
