module;

#include <nlohmann/json.hpp>

module iris.compiler.diagnostic;

import std;

import iris.core;
import iris.json_serializer;

namespace iris::compiler
{
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
