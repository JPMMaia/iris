export module iris.compiler.diagnostic;

import std;

import iris.core;
import iris.parser.parse_tree;

namespace iris::compiler
{
    export enum class Diagnostic_severity
    {
        Warning,
        Error,
        Information,
        Hint,
    };

    export enum class Diagnostic_source
    {
        Parser,
        Compiler
    };

    export enum class Diagnostic_code
    {
        Type_mismatch = 0,
        Soa_element_type_not_a_struct = 1,
    };

    export struct Diagnostic_related_information
    {
        friend bool operator==(Diagnostic_related_information const& lhs, Diagnostic_related_information const& rhs) = default;
    };

    export using Diagnostic_data = std::pmr::string;

    export struct Diagnostic
    {
        std::optional<std::filesystem::path> file_path = {};
        Source_range range = {};
        Diagnostic_source source = {};
        Diagnostic_severity severity = {};
        std::optional<Diagnostic_code> code = std::nullopt;
        std::pmr::string message = {};
        Diagnostic_related_information related_information = {};
        Diagnostic_data data = {};

        friend bool operator==(Diagnostic const& lhs, Diagnostic const& rhs)
        {
            return lhs.file_path == rhs.file_path &&
                   lhs.range == rhs.range &&
                   lhs.source == rhs.source &&
                   lhs.severity == rhs.severity &&
                   lhs.code == rhs.code &&
                   lhs.message == rhs.message &&
                   lhs.related_information == rhs.related_information;
        }
    };

    

    export struct Compile_error : std::runtime_error
    {
        Compile_error(
            std::string_view message,
            std::optional<Source_position> source_position,
            std::source_location throw_site = std::source_location::current()
        );

        std::optional<Source_position> source_position;
        std::source_location throw_site;
        std::optional<std::filesystem::path> file_path;
    };

    export struct Compilation_scope
    {
        Compilation_scope(std::string_view kind, std::string_view name);
        Compilation_scope(Compilation_scope const&) = delete;
        Compilation_scope& operator=(Compilation_scope const&) = delete;
        ~Compilation_scope();
    };

    export std::pmr::string format_compilation_context(
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    export Diagnostic create_error_diagnostic(
        std::optional<std::filesystem::path> const source_file_path,
        std::optional<Source_range> const range,
        std::string_view const message
    );

    export Diagnostic create_error_diagnostic_with_code(
        std::optional<std::filesystem::path> const source_file_path,
        std::optional<Source_range> const range,
        std::string_view const message,
        Diagnostic_code const code,
        Diagnostic_data data
    );

    export Diagnostic create_warning_diagnostic(
        std::optional<std::filesystem::path> const source_file_path,
        std::optional<Source_range> const range,
        std::string_view const message
    );

    export std::pmr::vector<Diagnostic> create_parser_diagnostics(
        std::filesystem::path const& source_file_path,
        iris::parser::Parse_tree const& parse_tree,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::pmr::string diagnostic_to_string(
        Diagnostic const& diagnostic,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::ostream& operator<<(
        std::ostream& output_stream,
        Diagnostic const& diagnostic
    );

    export void sort_diagnostics(
        std::pmr::vector<Diagnostic>& diagnostics
    );

    
    export struct Diagnostic_mismatch_type_data
    {
        std::optional<iris::Type_reference> provided_type;
        std::optional<iris::Type_reference> expected_type;
    };

    export Diagnostic_data create_diagnostic_mismatch_type_data(
        std::optional<iris::Type_reference> const& provided_type,
        std::optional<iris::Type_reference> const& expected_type
    );

    export Diagnostic_mismatch_type_data read_diagnostic_mismatch_type_data(
        Diagnostic_data const& data
    );
}
