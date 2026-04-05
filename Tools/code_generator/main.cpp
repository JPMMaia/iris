#include <fstream>
#include <string_view>

import h.tools.code_generator;

int main(int const argc, char const* const* const argv)
{
    if (argc < 3)
    {
        return 1;
    }

    std::string_view const operation = argv[1];

    if (operation == "reflection_json")
    {
        char const* const input_filename = argv[2];
        char const* const output_filename = argv[3];

        std::ifstream input_stream{ input_filename };
        std::ofstream output_stream{ output_filename };

        h::tools::code_generator::generate_json_data(
            input_stream,
            output_stream
        );
    }
    else if (operation == "typescript_interface")
    {
        char const* const input_filename = argv[2];
        char const* const output_filename = argv[3];

        std::ifstream input_stream{ input_filename };
        std::ofstream output_stream{ output_filename };

        h::tools::code_generator::generate_typescript_interface(
            input_stream,
            output_stream
        );
    }
    else if (operation == "typescript_intermediate_representation")
    {
        char const* const input_filename = argv[2];
        char const* const output_filename = argv[3];

        std::ifstream input_stream{ input_filename };
        std::ofstream output_stream{ output_filename };

        h::tools::code_generator::generate_typescript_intermediate_representation(
            input_stream,
            output_stream
        );
    }
    else if (operation == "serialize_binary_code")
    {
        char const* const input_filename = argv[2];
        char const* const output_filename = argv[3];

        std::ifstream input_stream{ input_filename };
        std::ofstream output_stream{ output_filename };

        h::tools::code_generator::generate_serialize_binary_code(
            input_stream,
            output_stream
        );
    }
    else if (operation == "serialize_json_code")
    {
        char const* const input_filename = argv[2];
        char const* const output_filename = argv[3];

        std::ifstream input_stream{ input_filename };
        std::ofstream output_stream{ output_filename };

        h::tools::code_generator::generate_serialize_json_code(
            input_stream,
            output_stream
        );
    }
    else if (operation == "operators")
    {
        char const* const input_filename = argv[2];
        char const* const output_filename = argv[3];

        std::ifstream input_stream{ input_filename };
        std::ofstream output_stream{ output_filename };

        h::tools::code_generator::generate_json_operators_code(
            input_stream,
            output_stream
        );
    }
    else if (operation == "expressions_visitor")
    {
        char const* const input_filename = argv[2];
        char const* const output_filename = argv[3];

        std::ifstream input_stream{ input_filename };
        std::ofstream output_stream{ output_filename };

        h::tools::code_generator::generate_expressions_visitor(
            input_stream,
            output_stream
        );
    }

    return 0;
}
