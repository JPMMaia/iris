#include <argparse/argparse.hpp>

#include <filesystem>
#include <iostream>
#include <string>

import iris.core;
import iris.json_serializer;
import iris.parser.convertor;
import iris.parser.parser;

int main(int const argc, char const* const* argv)
{
    argparse::ArgumentParser program("iris_parser");

    // iris <source_file> <output_file>
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

    iris::parser::Parser const parser = iris::parser::create_parser();

    std::optional<iris::Module> const core_module = iris::parser::parse_and_convert_to_module(
        source_file_path,
        {},
        {}
    );
    if (!core_module.has_value())
        return -1;
    
    iris::json::write_module_to_file(output_file_path, core_module.value());

    return 0;
}