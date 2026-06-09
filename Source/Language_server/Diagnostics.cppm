module;

#include <filesystem>
#include <memory_resource>
#include <optional>
#include <span>
#include <vector>

#include <lsp/types.h>

export module iris.language_server.diagnostics;

import iris.compiler.diagnostic;
import iris.core;
import iris.core.declarations;
import iris.parser.parse_tree;

namespace iris::language_server
{
    export struct Diagnostics_state
    {
        std::pmr::vector<iris::compiler::Diagnostic> diagnostics;
        std::optional<int> version;
        std::string result_id;
    };

    export std::pmr::vector<iris::compiler::Diagnostic> create_parser_diagnostics(
        std::filesystem::path const& source_file_path,
        iris::parser::Parse_tree const& parse_tree,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export lsp::WorkspaceFullDocumentDiagnosticReport create_full_document_diagnostics_report(
        lsp::DocumentUri const& document_uri,
        std::optional<int> const version,
        std::string_view const result_id,
        std::span<iris::compiler::Diagnostic const> const diagnostics
    );

    export lsp::WorkspaceUnchangedDocumentDiagnosticReport create_unchanged_document_diagnostics_report(
        lsp::DocumentUri const& document_uri,
        std::optional<int> const version,
        std::string_view const previous_result_id
    );

    export std::pmr::vector<lsp::WorkspaceDocumentDiagnosticReport> create_all_diagnostics(
        std::span<std::filesystem::path const> const core_module_source_file_paths,
        std::span<iris::Module const> const core_modules,
        std::span<std::optional<int> const> const core_module_versions,
        std::span<iris::parser::Parse_tree const> const core_module_parse_trees,
        std::span<Diagnostics_state> const core_module_diagnostics_states,
        std::span<lsp::PreviousResultId const> const previous_result_ids,
        std::span<iris::Module const> const header_modules,
        iris::Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );
    
    export lsp::DocumentDiagnosticReport create_document_diagnostics(
        lsp::DocumentUri const& document_uri,
        iris::Module const& core_module,
        std::optional<int> const version,
        iris::parser::Parse_tree const& core_module_parse_tree,
        Diagnostics_state& diagnostics_state,
        iris::Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::vector<lsp::Diagnostic> create_document_parser_diagnostics(
        std::filesystem::path const& source_file_path,
        iris::parser::Parse_tree const& parse_tree,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    lsp::Diagnostic to_lsp_diagnostic(
        iris::compiler::Diagnostic const& input
    );
}
