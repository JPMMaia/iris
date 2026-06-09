module;

#include <memory_resource>
#include <span>
#include <string>
#include <string_view>

#include <lsp/types.h>

module iris.language_server.core;

import iris.core;

namespace iris::language_server
{
    lsp::Position to_lsp_position(
        iris::Source_position const& input
    )
    {
        return lsp::Position
        {
            .line = input.line > 0 ? input.line - 1 : 0,
            .character = input.column > 0 ? input.column - 1 : 0,
        };
    }

    lsp::Range to_lsp_range(
        iris::Source_range const& input
    )
    {
        return lsp::Range
        {
            .start = to_lsp_position(input.start),
            .end = to_lsp_position(input.end),
        };
    }

    iris::Source_position to_source_position(
        lsp::Position const& input
    )
    {
        return iris::Source_position
        {
            .line = input.line + 1,
            .column = input.character + 1,
        };
    }

    iris::Source_range to_source_range(
        lsp::Range const& input
    )
    {
        return iris::Source_range
        {
            .start = to_source_position(input.start),
            .end = to_source_position(input.end),
        };
    }

    
    std::pmr::u8string convert_to_utf_8_string(
        std::string_view const& input,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::u8string output{output_allocator};
        output.resize(input.size());

        std::memcpy(output.data(), input.data(), output.size());

        return output;
    }

    bool compare_document_uris(
        lsp::DocumentUri const& left,
        lsp::DocumentUri const& right
    )
    {
        std::string_view const left_path = left.path();
        std::string_view const right_path = right.path();

        if (left_path.size() != right_path.size())
            return false;

        for (std::size_t index = 0; index < left_path.size(); ++index)
        {
            char const left_char = left_path[index];
            char const right_char = right_path[index];

            if (left_char == right_char)
                continue;

            if ((left_char == '\\' || left_char == '/') && (right_char == '\\' || right_char == '/'))
                continue;

            if (std::tolower(left_char) != std::tolower(right_char))
                return false;
        }
        
        return true;
    }

    std::optional<lsp::PreviousResultId> find_previous_result_id(
        std::span<lsp::PreviousResultId const> const result_ids,
        lsp::DocumentUri const& document_uri
    )
    {
        auto const location = std::find_if(
            result_ids.begin(),
            result_ids.end(),
            [&](lsp::PreviousResultId const& result_id) -> bool { return compare_document_uris(result_id.uri, document_uri); }
        );
        if (location == result_ids.end())
            return std::nullopt;

        return *location;
    }
}