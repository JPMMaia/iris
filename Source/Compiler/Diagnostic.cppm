export module iris.compiler.diagnostic;

import std;

import iris.core;

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
        friend auto operator<=>(Diagnostic_related_information const& lhs, Diagnostic_related_information const& rhs) = default;
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

        friend auto operator <=>(Diagnostic const& lhs, Diagnostic const& rhs)
        {
            if (auto cmp = lhs.file_path <=> rhs.file_path; cmp != 0)
                return cmp;
            if (auto cmp = lhs.range <=> rhs.range; cmp != 0)
                return cmp;
            if (auto cmp = lhs.source <=> rhs.source; cmp != 0)
                return cmp;
            if (auto cmp = lhs.severity <=> rhs.severity; cmp != 0)
                return cmp;
            if (auto cmp = lhs.code <=> rhs.code; cmp != 0)
                return cmp;
            if (auto cmp = lhs.message <=> rhs.message; cmp != 0)
                return cmp;
            return lhs.related_information <=> rhs.related_information;
        }

        friend bool operator==(Diagnostic const& lhs, Diagnostic const& rhs)
        {
            return (lhs <=> rhs) == 0;
        }
    };

    

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
