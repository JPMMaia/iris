export module h.tools.code_generator;

import std;

namespace h::tools::code_generator
{
    export struct Type
    {
        std::pmr::string name;
    };

    export struct Enum
    {
        std::pmr::string name;
        std::pmr::vector<std::pmr::string> values;
    };

    export std::pmr::string generate_read_enum_json_code(
        Enum enum_type,
        int indentation
    );

    export std::pmr::string generate_write_enum_json_code(
        Enum enum_type,
        int indentation
    );

    export struct Member
    {
        Type type;
        std::pmr::string name;
    };

    export struct Struct
    {
        std::pmr::string name;
        std::pmr::vector<Member> members;
    };

    export std::pmr::string generate_read_struct_json_code(
        Struct const& struct_type,
        std::pmr::unordered_map<std::pmr::string, Enum> const& enum_types,
        std::pmr::unordered_map<std::pmr::string, Struct> const& struct_types,
        int indentation
    );

    export std::pmr::string generate_write_struct_json_code(
        Struct const& struct_type,
        std::pmr::unordered_map<std::pmr::string, Enum> const& enum_types,
        std::pmr::unordered_map<std::pmr::string, Struct> const& struct_types,
        int indentation
    );

    export struct File_types
    {
        std::pmr::vector<Enum> enums;
        std::pmr::vector<Struct> structs;
    };

    export File_types identify_file_types(
        std::istream& input_stream
    );

    export void generate_read_json_code(
        std::istream& input_stream,
        std::ostream& output_stream,
        std::string_view const export_module_name,
        std::string_view const module_name_to_import,
        std::string_view const namespace_name
    );

    export void generate_write_json_code(
        std::istream& input_stream,
        std::ostream& output_stream,
        std::string_view const export_module_name,
        std::string_view const module_name_to_import,
        std::string_view const namespace_name
    );

    export void generate_json_operators_code(
        std::istream& input_stream,
        std::ostream& output_stream
    );

    export void generate_json_data(
        std::istream& input_stream,
        std::ostream& output_stream
    );

    export void generate_typescript_interface(
        std::istream& input_stream,
        std::ostream& output_stream
    );

    export void generate_typescript_intermediate_representation(
        std::istream& input_stream,
        std::ostream& output_stream
    );

    export void generate_serialize_binary_code(
        std::istream& input_stream,
        std::ostream& output_stream
    );

    export void generate_serialize_json_code(
        std::istream& input_stream,
        std::ostream& output_stream
    );

    export void generate_expressions_visitor(
        std::istream& input_stream,
        std::ostream& output_stream
    );
}
