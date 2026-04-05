#include <argparse/argparse.hpp>

#include <filesystem>
#include <iostream>
#include <string>

import h.core;
import h.json_serializer;
import h.parser.convertor;
import h.parser.parser;

int main(int const argc, char const* const* argv)
{
    argparse::ArgumentParser program("hlang_parser");

    // hlang <source_file> <output_file>
    program.add_argument("source_file")
        .help("Source file to parse")
        .required();
    program.add_argument("output_file")
        .help("Destination file path.")
        .required();

    try
    {
        program.parse_args(argc, argv);
    }
    catch (std::exception const& error)
    {
        std::cerr << error.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    std::filesystem::path const source_file_path = program.get<std::string>("source_file");
    std::filesystem::path const output_file_path = program.get<std::string>("output_file");

    h::parser::Parser const parser = h::parser::create_parser();

    std::optional<h::Module> const core_module = h::parser::parse_and_convert_to_module(
        source_file_path,
        {},
        {}
    );
    if (!core_module.has_value())
        return -1;
    
    h::json::write_module_to_file(output_file_path, core_module.value());

    return 0;
}