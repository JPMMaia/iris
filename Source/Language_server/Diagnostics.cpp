module;

#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <lsp/types.h>

module iris.language_server.diagnostics;

import iris.compiler;
import iris.compiler.analysis;
import iris.compiler.diagnostic;
import iris.compiler.validation;
import iris.core;
import iris.core.declarations;
import iris.language_server.core;
import iris.parser.parse_tree;

namespace iris::language_server
{
    static std::pmr::string create_parser_diagnostic_message(
        iris::parser::Parse_tree const& tree,
        iris::parser::Parse_node const& node,
        iris::Source_range const& range,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        if (iris::parser::is_error_node(node))
        {
            return std::pmr::string{
                std::format("Unexpected token at line {}, column {}.", range.start.line, range.start.column),
                output_allocator
            };
        }
        else
        {
            std::string_view const node_value = iris::parser::get_node_value(tree, node);
            return std::pmr::string{
                std::format("Missing '{}' at line {}, column {}.", node_value, range.start.line, range.start.column),
                output_allocator
            };
        }
    }

    std::pmr::vector<iris::compiler::Diagnostic> create_parser_diagnostics(
        std::filesystem::path const& source_file_path,
        iris::parser::Parse_tree const& parse_tree,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        iris::parser::Parse_node const& root_node = iris::parser::get_root_node(parse_tree);

        if (iris::parser::has_errors(root_node))
        {
            std::pmr::vector<iris::parser::Parse_node> const error_or_missing_nodes = iris::parser::get_error_or_missing_nodes(
                root_node,
                temporaries_allocator,
                temporaries_allocator
            );

            std::pmr::vector<iris::compiler::Diagnostic> diagnostics{output_allocator};
            diagnostics.reserve(error_or_missing_nodes.size());

            for (iris::parser::Parse_node const& node : error_or_missing_nodes)
            {
                iris::Source_range const range = iris::parser::get_node_source_range(node);
                std::pmr::string message = create_parser_diagnostic_message(parse_tree, node, range, output_allocator);

                iris::compiler::Diagnostic diagnostic
                {
                    .file_path = source_file_path,
                    .range = range,
                    .source = iris::compiler::Diagnostic_source::Parser,
                    .severity = iris::compiler::Diagnostic_severity::Error,
                    .message = std::move(message),
                    .related_information = {},
                };

                diagnostics.push_back(std::move(diagnostic));
            }

            return diagnostics;
        }

        return {};
    }

    lsp::WorkspaceFullDocumentDiagnosticReport create_full_document_diagnostics_report(
        lsp::DocumentUri const& document_uri,
        std::optional<int> const version,
        std::string_view const result_id,
        std::span<iris::compiler::Diagnostic const> const diagnostics
    )
    {
        lsp::WorkspaceFullDocumentDiagnosticReport document_report = {};
        document_report.uri = document_uri;
        document_report.resultId = std::string{result_id};

        if (version.has_value())
            document_report.version = version.value();

        document_report.items = {};

        for (iris::compiler::Diagnostic const& core_diagnostic : diagnostics)
        {
            if (!core_diagnostic.file_path.has_value())
                continue;

            if (compare_document_uris(lsp::DocumentUri::fromPath(core_diagnostic.file_path->generic_string()), document_uri))
            {
                lsp::Diagnostic lsp_diagnostic = to_lsp_diagnostic(core_diagnostic);

                document_report.items.push_back(std::move(lsp_diagnostic));
            }
        }

        return document_report;
    }

    lsp::WorkspaceUnchangedDocumentDiagnosticReport create_unchanged_document_diagnostics_report(
        lsp::DocumentUri const& document_uri,
        std::optional<int> const version,
        std::string_view const previous_result_id
    )
    {
        lsp::WorkspaceUnchangedDocumentDiagnosticReport item = {};
        item.resultId = std::string{ previous_result_id };
        item.uri = document_uri;
        if (version.has_value())
            item.version = version.value();

        return item;
    }

    static bool is_any_dependency_dirty(
        iris::Module const& core_module,
        std::span<iris::Module const> const core_modules,
        std::span<std::optional<int> const> const core_module_versions,
        std::span<Diagnostics_state const> const core_module_diagnostics_states
    )
    {
        for (iris::Import_module_with_alias const& alias : core_module.dependencies.alias_imports)
        {
            for (std::size_t index = 0; index < core_modules.size(); ++index)
            {
                iris::Module const& dependency = core_modules[index];

                if (alias.module_name == dependency.name)
                {
                    std::optional<int> const dependency_version = core_module_versions[index];
                    Diagnostics_state const& dependency_diagnostics_state = core_module_diagnostics_states[index];
                    if (dependency_diagnostics_state.version != dependency_version)
                        return true;
                }
            }
        }

        return false;
    }

    static std::string generate_new_result_id(
        std::string_view const previous_result_id
    )
    {
        char* end = nullptr;
        unsigned long long value = std::strtoull(previous_result_id.data(), &end, 10);
        value += 1;
        return std::string{std::to_string(value)};
    }

    std::pmr::vector<lsp::WorkspaceDocumentDiagnosticReport> create_all_diagnostics(
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
    )
    {
        std::pmr::vector<lsp::WorkspaceDocumentDiagnosticReport> items{output_allocator};
        items.reserve(core_module_source_file_paths.size());

        for (std::size_t core_module_index = 0; core_module_index < core_module_source_file_paths.size(); ++core_module_index)
        {
            std::filesystem::path const& source_file_path = core_module_source_file_paths[core_module_index];
            std::optional<int> const version = core_module_versions[core_module_index];
            Diagnostics_state& diagnostics_state = core_module_diagnostics_states[core_module_index];
            iris::Module const& core_module = core_modules[core_module_index];

            bool const is_core_module_dirty = diagnostics_state.version != version;
            bool const is_any_core_module_dependency_dirty = is_any_dependency_dirty(core_module, core_modules, core_module_versions, core_module_diagnostics_states);
            bool const are_diagnostics_dirty = is_core_module_dirty || is_any_core_module_dependency_dirty;

            lsp::DocumentUri const document_uri = lsp::DocumentUri::fromPath(source_file_path.generic_string());
            std::optional<lsp::PreviousResultId> const previous_result_id = find_previous_result_id(
                previous_result_ids,
                document_uri
            );
            lsp::DocumentUri const& found_document_uri = previous_result_id.has_value() ? previous_result_id->uri : document_uri;

            if (!are_diagnostics_dirty)
            {
                if (previous_result_id.has_value() && previous_result_id->value == diagnostics_state.result_id)
                {
                    lsp::WorkspaceUnchangedDocumentDiagnosticReport item = create_unchanged_document_diagnostics_report(
                        found_document_uri,
                        version,
                        diagnostics_state.result_id
                    );
                    items.push_back(item);

                    continue;
                }
            }

            diagnostics_state.version = version;
            diagnostics_state.result_id = generate_new_result_id(diagnostics_state.result_id);

            std::pmr::vector<iris::compiler::Diagnostic> parser_diagnostics = create_parser_diagnostics(
                source_file_path,
                core_module_parse_trees[core_module_index],
                temporaries_allocator,
                temporaries_allocator
            );

            if (!parser_diagnostics.empty())
            {
                lsp::WorkspaceFullDocumentDiagnosticReport item = create_full_document_diagnostics_report(
                    found_document_uri,
                    version,
                    diagnostics_state.result_id,
                    parser_diagnostics
                );
                diagnostics_state.diagnostics = std::move(parser_diagnostics);

                items.push_back(std::move(item));
            }
            else
            {
                std::pmr::vector<iris::compiler::Diagnostic> compiler_diagnostics = iris::compiler::validate_module(
                    core_module,
                    declaration_database,
                    temporaries_allocator
                );

                lsp::WorkspaceFullDocumentDiagnosticReport item = create_full_document_diagnostics_report(
                    found_document_uri,
                    version,
                    diagnostics_state.result_id,
                    compiler_diagnostics
                );
                diagnostics_state.diagnostics = std::move(compiler_diagnostics);

                items.push_back(std::move(item));
            }
        }

        return items;
    }

    lsp::DocumentDiagnosticReport create_document_diagnostics(
        lsp::DocumentUri const& document_uri,
        iris::Module const& core_module,
        std::optional<int> const version,
        iris::parser::Parse_tree const& core_module_parse_tree,
        Diagnostics_state& diagnostics_state,
        iris::Declaration_database const& declaration_database,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        diagnostics_state.version = version;
        diagnostics_state.result_id = generate_new_result_id(diagnostics_state.result_id);

        std::pmr::vector<iris::compiler::Diagnostic> parser_diagnostics = create_parser_diagnostics(
            document_uri.path(),
            core_module_parse_tree,
            output_allocator,
            temporaries_allocator
        );

        if (!parser_diagnostics.empty())
        {
            lsp::WorkspaceFullDocumentDiagnosticReport workspace_report = create_full_document_diagnostics_report(
                document_uri,
                version,
                diagnostics_state.result_id,
                parser_diagnostics
            );
            diagnostics_state.diagnostics = std::move(parser_diagnostics);

            lsp::RelatedFullDocumentDiagnosticReport full_report = {};
            full_report.kind = workspace_report.kind;
            full_report.items = std::move(workspace_report.items);
            full_report.resultId = std::move(workspace_report.resultId);
            return full_report;
        }
        else
        {
            std::pmr::vector<iris::compiler::Diagnostic> compiler_diagnostics = iris::compiler::validate_module(
                core_module,
                declaration_database,
                temporaries_allocator
            );

            lsp::WorkspaceFullDocumentDiagnosticReport workspace_report = create_full_document_diagnostics_report(
                document_uri,
                version,
                diagnostics_state.result_id,
                std::span<iris::compiler::Diagnostic const>{compiler_diagnostics}
            );
            diagnostics_state.diagnostics = std::move(compiler_diagnostics);

            lsp::RelatedFullDocumentDiagnosticReport full_report = {};
            full_report.kind = workspace_report.kind;
            full_report.items = std::move(workspace_report.items);
            full_report.resultId = std::move(workspace_report.resultId);
            return full_report;
        }
    }

    static std::vector<lsp::Diagnostic> convert_to_lsp_diagnostics(
        std::span<iris::compiler::Diagnostic const> const diagnostics
    )
    {
        std::vector<lsp::Diagnostic> lsp_diagnostics;
        lsp_diagnostics.reserve(diagnostics.size());
        for (iris::compiler::Diagnostic const& diagnostic : diagnostics)
            lsp_diagnostics.push_back(to_lsp_diagnostic(diagnostic));
        return lsp_diagnostics;
    }

    std::vector<lsp::Diagnostic> create_document_parser_diagnostics(
        std::filesystem::path const& source_file_path,
        iris::parser::Parse_tree const& parse_tree,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<iris::compiler::Diagnostic> const parser_diagnostics = create_parser_diagnostics(source_file_path, parse_tree, temporaries_allocator, temporaries_allocator);
        return convert_to_lsp_diagnostics(parser_diagnostics);
    }

    lsp::DiagnosticSeverity to_lsp_diagnostic_severity(
        iris::compiler::Diagnostic_severity const input
    )
    {
        switch (input)
        {
            case iris::compiler::Diagnostic_severity::Warning:
                return lsp::DiagnosticSeverity::Warning;
            case iris::compiler::Diagnostic_severity::Error:
                return lsp::DiagnosticSeverity::Error;
            case iris::compiler::Diagnostic_severity::Information:
                return lsp::DiagnosticSeverity::Information;
            case iris::compiler::Diagnostic_severity::Hint:
                return lsp::DiagnosticSeverity::Hint;
            default:
                return lsp::DiagnosticSeverity::Error;
        }
    }

    lsp::Opt<lsp::OneOf<int, lsp::String>> to_lsp_diagnostic_code(
        std::optional<iris::compiler::Diagnostic_code> const code
    )
    {
        if (!code.has_value())
            return std::nullopt;

        return static_cast<int>(code.value());
    }

    lsp::String diagnostic_source_to_string(
        iris::compiler::Diagnostic_source const source
    )
    {
        switch (source)
        {
            case iris::compiler::Diagnostic_source::Parser:
                return "Parser";
            case iris::compiler::Diagnostic_source::Compiler:
            default:
                return "Compiler";
        }
    }

    lsp::Diagnostic to_lsp_diagnostic(
        iris::compiler::Diagnostic const& input
    )
    {
        return lsp::Diagnostic
        {
            .range = to_lsp_range(input.range),
            .message = lsp::String{input.message},
            .severity = to_lsp_diagnostic_severity(input.severity),
            .code = to_lsp_diagnostic_code(input.code),
            .source = diagnostic_source_to_string(input.source),
            .data = !input.data.empty() ? lsp::json::parse(input.data) : nullptr,
        };
    }
}
