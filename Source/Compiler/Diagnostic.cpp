module;

#include <nlohmann/json.hpp>

module iris.compiler.diagnostic;

import std;

import iris.core;
import iris.json_serializer;
import iris.parser.parse_tree;

namespace iris::compiler
{
    Compile_error::Compile_error(
        std::string_view const message,
        std::optional<Source_position> const source_position,
        std::source_location const throw_site
    ) :
        std::runtime_error{ std::string{message} },
        source_position{ source_position },
        throw_site{ throw_site }
    {
    }

    namespace
    {
        struct Compilation_breadcrumb
        {
            std::string kind;
            std::string name;
        };

        std::vector<Compilation_breadcrumb>& get_compilation_breadcrumbs()
        {
            static thread_local std::vector<Compilation_breadcrumb> breadcrumbs;
            return breadcrumbs;
        }

        // Set while an exception is unwinding through the scopes. The trail is what the top-level
        // handler needs, and it would otherwise be popped away before that handler ever sees it.
        thread_local bool g_breadcrumbs_frozen = false;
    }

    Compilation_scope::Compilation_scope(std::string_view const kind, std::string_view const name)
    {
        if (g_breadcrumbs_frozen && std::uncaught_exceptions() == 0)
        {
            // Someone caught and handled the exception; the frozen trail is stale.
            g_breadcrumbs_frozen = false;
            get_compilation_breadcrumbs().clear();
        }

        get_compilation_breadcrumbs().push_back(Compilation_breadcrumb{ .kind = std::string{kind}, .name = std::string{name} });
    }

    Compilation_scope::~Compilation_scope()
    {
        if (std::uncaught_exceptions() > 0)
        {
            g_breadcrumbs_frozen = true;
            return;
        }

        if (g_breadcrumbs_frozen)
            return;

        std::vector<Compilation_breadcrumb>& breadcrumbs = get_compilation_breadcrumbs();
        if (!breadcrumbs.empty())
            breadcrumbs.pop_back();
    }

    std::pmr::string format_compilation_context(
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::string output{output_allocator};

        // Innermost first: the last thing the compiler started is the most likely culprit.
        std::vector<Compilation_breadcrumb> const& breadcrumbs = get_compilation_breadcrumbs();
        for (auto iterator = breadcrumbs.rbegin(); iterator != breadcrumbs.rend(); ++iterator)
        {
            output += std::format("  while {} '{}'\n", iterator->kind, iterator->name);
        }

        return output;
    }

    Diagnostic create_error_diagnostic(
        std::optional<std::filesystem::path> const source_file_path,
        std::optional<Source_range> const range,
        std::string_view const message
    )
    {
        return Diagnostic
        {
            .file_path = source_file_path,
            .range = range.has_value() ? range.value() : Source_range{},
            .source = Diagnostic_source::Compiler,
            .severity = Diagnostic_severity::Error,
            .message = std::pmr::string{message},
            .related_information = {},
        };
    }

    Diagnostic create_error_diagnostic_with_code(
        std::optional<std::filesystem::path> const source_file_path,
        std::optional<Source_range> const range,
        std::string_view const message,
        Diagnostic_code const code,
        Diagnostic_data data
    )
    {
        return Diagnostic
        {
            .file_path = source_file_path,
            .range = range.has_value() ? range.value() : Source_range{},
            .source = Diagnostic_source::Compiler,
            .severity = Diagnostic_severity::Error,
            .code = code,
            .message = std::pmr::string{message},
            .related_information = {},
            .data = std::move(data),
        };
    }

    Diagnostic create_warning_diagnostic(
        std::optional<std::filesystem::path> const source_file_path,
        std::optional<Source_range> const range,
        std::string_view const message
    )
    {
        return Diagnostic
        {
            .file_path = source_file_path,
            .range = range.has_value() ? range.value() : Source_range{},
            .source = Diagnostic_source::Compiler,
            .severity = Diagnostic_severity::Warning,
            .message = std::pmr::string{message},
            .related_information = {},
        };
    }

    static std::pmr::string create_parser_diagnostic_message(
        iris::parser::Parse_tree const& tree,
        iris::parser::Parse_node const& node,
        iris::Source_range const& range,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        if (iris::parser::is_error_node(node))
        {
            return std::pmr::string{"Unexpected token.", output_allocator};
        }
        else
        {
            std::string_view const node_value = iris::parser::get_node_value(tree, node);
            return std::pmr::string{
                std::format("Missing '{}'.", node_value),
                output_allocator
            };
        }
    }

    std::pmr::vector<Diagnostic> create_parser_diagnostics(
        std::filesystem::path const& source_file_path,
        iris::parser::Parse_tree const& parse_tree,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        iris::parser::Parse_node const& root_node = iris::parser::get_root_node(parse_tree);

        if (!iris::parser::has_errors(root_node))
            return std::pmr::vector<Diagnostic>{output_allocator};

        std::pmr::vector<iris::parser::Parse_node> const error_or_missing_nodes = iris::parser::get_error_or_missing_nodes(
            root_node,
            temporaries_allocator,
            temporaries_allocator
        );

        std::pmr::vector<Diagnostic> diagnostics{output_allocator};
        diagnostics.reserve(error_or_missing_nodes.size());

        for (iris::parser::Parse_node const& node : error_or_missing_nodes)
        {
            iris::Source_range const range = iris::parser::get_node_source_range(node);
            std::pmr::string message = create_parser_diagnostic_message(parse_tree, node, range, output_allocator);

            Diagnostic diagnostic
            {
                .file_path = source_file_path,
                .range = range,
                .source = Diagnostic_source::Parser,
                .severity = Diagnostic_severity::Error,
                .message = std::move(message),
                .related_information = {},
            };

            diagnostics.push_back(std::move(diagnostic));
        }

        return diagnostics;
    }

    std::pmr::string diagnostic_to_string(
        Diagnostic const& diagnostic,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        using String_stream = std::basic_stringstream<char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>>;

        String_stream output_stream{std::ios_base::in | std::ios_base::out, temporaries_allocator};

        output_stream << diagnostic;

        return std::pmr::string{output_stream.str(), output_allocator};
    }

    std::ostream& operator<<(std::ostream& output_stream, Diagnostic const& diagnostic)
    {
        if (diagnostic.file_path.has_value())
            output_stream << std::format("{}:{}:{}: ", diagnostic.file_path.value().generic_string(), diagnostic.range.start.line, diagnostic.range.start.column);
        else
            output_stream << std::format("({},{},{},{}): ", diagnostic.range.start.line, diagnostic.range.start.column, diagnostic.range.end.line, diagnostic.range.end.column);

        if (diagnostic.severity == Diagnostic_severity::Warning)
            output_stream << "warning";
        else
            output_stream << "error";

        if (diagnostic.code.has_value())
            output_stream << "(" << static_cast<int>(diagnostic.code.value()) << ")";

        output_stream << ": ";
        output_stream << diagnostic.message;

        return output_stream;
    }

    void sort_diagnostics(
        std::pmr::vector<Diagnostic>& diagnostics
    )
    {
        std::sort(
            diagnostics.begin(),
            diagnostics.end(),
            [](Diagnostic const& lhs, Diagnostic const& rhs)
            {
                if (lhs.file_path != rhs.file_path)
                    return lhs.file_path < rhs.file_path;

                if (lhs.range.start.line != rhs.range.start.line)
                    return lhs.range.start.line < rhs.range.start.line;

                if (lhs.range.start.column != rhs.range.start.column)
                    return lhs.range.start.column < rhs.range.start.column;

                return lhs.message < rhs.message;
            }
        );
    }

    Diagnostic_data create_diagnostic_mismatch_type_data(
        std::optional<iris::Type_reference> const& provided_type,
        std::optional<iris::Type_reference> const& expected_type
    )
    {
        std::pmr::string provided_type_json = provided_type.has_value() ? iris::json::write(provided_type.value()) : std::pmr::string{"null"};
        std::pmr::string expected_type_json = expected_type.has_value() ? iris::json::write(expected_type.value()) : std::pmr::string{"null"};

        nlohmann::ordered_json output;
        
        if (provided_type.has_value())
            output["provided_type"] = nlohmann::ordered_json::parse(iris::json::write(provided_type.value()));
        
        if (expected_type.has_value())
            output["expected_type"] = nlohmann::ordered_json::parse(iris::json::write(expected_type.value()));
        
        return std::pmr::string{output.dump()};
    }

    Diagnostic_mismatch_type_data read_diagnostic_mismatch_type_data(
        Diagnostic_data const& data
    )
    {
        Diagnostic_mismatch_type_data output = {};

        nlohmann::ordered_json input = nlohmann::ordered_json::parse(data);

        if (input.contains("provided_type"))
        {
            std::string const& provided_type_string = input["provided_type"].dump();
            output.provided_type = iris::json::read<iris::Type_reference>(provided_type_string.c_str());
        }

        if (input.contains("expected_type"))
        {
            std::string const& expected_type_string = input["expected_type"].dump();
            output.expected_type = iris::json::read<iris::Type_reference>(expected_type_string.c_str());
        }

        return output;
    }
}
