#include <cstdio>
#include <filesystem>
#include <optional>
#include <string_view>

#include <catch2/catch_all.hpp>

import h.common;
import h.core;
import h.core.formatter;
import h.parser.convertor;
import h.parser.parse_tree;
import h.parser.parser;

namespace h::parser
{
    static std::filesystem::path const g_test_source_files_path = std::filesystem::path{ TEST_SOURCE_FILES_PATH };

    static void test_convertor(
        std::string_view const input_file
    )
    {
        std::filesystem::path const input_file_path = g_test_source_files_path / input_file;
        std::optional<std::pmr::u8string> const file_contents = h::common::get_file_utf8_contents(input_file_path);
        REQUIRE(file_contents.has_value());
        
        std::pmr::u8string const& source = file_contents.value();

        Parser parser = create_parser();
        Parse_tree tree = parse(parser, source);
        Parse_node const root = get_root_node(tree);

        std::optional<h::Module> const converted_module = parse_node_to_module(
            tree,
            root,
            input_file_path,
            {},
            {}
        );

        CHECK(converted_module.has_value());
        if (converted_module.has_value())
        {
            std::pmr::polymorphic_allocator<> output_allocator;
            std::pmr::polymorphic_allocator<> temporaries_allocator;

            Format_options const format_options =
            {
                .output_allocator = output_allocator,
                .temporaries_allocator = temporaries_allocator,
            };

            std::pmr::string const converted_text = h::format_module(
                converted_module.value(),
                format_options
            );

            std::pmr::string const non_utf8_source{source.begin(), source.end()};

            CHECK(converted_text == non_utf8_source);
        }

        destroy_tree(std::move(tree));
        destroy_parser(std::move(parser));
    }

    TEST_CASE("Converts array_slices.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "array_slices.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts assert_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "assert_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts assignment_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "assignment_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts binary_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "binary_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts binary_expressions_precedence.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "binary_expressions_precedence.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts bit_fields.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "bit_fields.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts block_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "block_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts booleans.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "booleans.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts break_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "break_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_call_function_that_returns_bool.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_call_function_that_returns_bool.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_call_function_with_struct.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_call_function_with_struct.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_define_function_with_struct.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_define_function_with_struct.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_return_big_struct.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_return_big_struct.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_return_empty_struct.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_return_empty_struct.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_return_int.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_return_int.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_return_pointer.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_return_pointer.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_return_small_struct.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_return_small_struct.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_with_big_struct.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_with_big_struct.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_with_empty_struct.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_with_empty_struct.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_with_int_arguments.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_with_int_arguments.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_with_pointer.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_with_pointer.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_with_small_struct.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_with_small_struct.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts cast_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "cast_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_alias.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "comment_alias.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_enums.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "comment_enums.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "comment_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_functions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "comment_functions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_global_variables.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "comment_global_variables.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_module_declaration.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "comment_module_declaration.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_structs.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "comment_structs.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_unions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "comment_unions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts compile_time_for.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "compile_time_for.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts compile_time_if.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "compile_time_if.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts constant_array_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "constant_array_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_all.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_all.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_c_headers.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_c_headers.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_for_loop.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_for_loop.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_function_call.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_function_call.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_if.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_if.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_structs.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_structs.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_switch.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_switch.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_unions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_unions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_variables.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_variables.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_while_loop.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_while_loop.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts defer_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "defer_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts defer_expressions_with_debug_information.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "defer_expressions_with_debug_information.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts dereference_and_access_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "dereference_and_access_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts dynamic_array.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "dynamic_array.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts dynamic_array_usage.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "dynamic_array_usage.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts empty_return_expression.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "empty_return_expression.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts for_loop_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "for_loop_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts function_contracts.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "function_contracts.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts function_pointers.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "function_pointers.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts hello_world.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "hello_world.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts instantiate_uninitialized.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "instantiate_uninitialized.hltxt";
        test_convertor(input_file);
    }
    
    TEST_CASE("Converts instantiate_zero_initialized.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "instantiate_zero_initialized.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts if_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "if_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts if_return_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "if_return_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts module_with_dots.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "module_with_dots.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts multiple_modules_a.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "multiple_modules_a.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts multiple_modules_b.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "multiple_modules_b.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts multiple_modules_c.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "multiple_modules_c.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts null_pointers.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "null_pointers.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts numbers.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "numbers.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts numeric_casts.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "numeric_casts.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts pointers.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "pointers.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts size_of.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "size_of.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts switch_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "switch_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts ternary_condition_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "ternary_condition_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts test_framework.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "test_framework.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts unary_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "unary_expressions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts unique_name.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "unique_name.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_alias.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "using_alias.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_alias_from_modules.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "using_alias_from_modules.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_enum_flags.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "using_enum_flags.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_enums.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "using_enums.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_enums_from_modules.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "using_enums_from_modules.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_function_constructors.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "using_function_constructors.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_global_variables.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "using_global_variables.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_structs.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "using_structs.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_type_constructors.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "using_type_constructors.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_unions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "using_unions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts variables.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "variables.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts variadic_functions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "variadic_functions.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts void_pointer.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "void_pointer.hltxt";
        test_convertor(input_file);
    }

    TEST_CASE("Converts while_loop_expressions.hltxt", "[Convertor]")
    {
        std::string_view const input_file = "while_loop_expressions.hltxt";
        test_convertor(input_file);
    }

}
