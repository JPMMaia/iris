#include <cstdio>
#include <filesystem>
#include <optional>
#include <string_view>

#include <catch2/catch_all.hpp>

import iris.common;
import iris.core;
import iris.core.formatter;
import iris.parser.convertor;
import iris.parser.parse_tree;
import iris.parser.parser;

namespace iris::parser
{
    static std::filesystem::path const g_test_source_files_path = std::filesystem::path{ TEST_SOURCE_FILES_PATH };

    static void test_convertor(
        std::string_view const input_file
    )
    {
        std::filesystem::path const input_file_path = g_test_source_files_path / input_file;
        std::optional<std::pmr::u8string> const file_contents = iris::common::get_file_utf8_contents(input_file_path);
        REQUIRE(file_contents.has_value());
        
        std::pmr::u8string const& source = file_contents.value();

        Parser parser = create_parser();
        Parse_tree tree = parse(parser, source);
        Parse_node const root = get_root_node(tree);

        std::optional<iris::Module> const converted_module = parse_node_to_module(
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

            std::pmr::string const converted_text = iris::format_module(
                converted_module.value(),
                format_options
            );

            std::pmr::string const non_utf8_source{source.begin(), source.end()};

            CHECK(converted_text == non_utf8_source);
        }

        destroy_tree(std::move(tree));
        destroy_parser(std::move(parser));
    }

    TEST_CASE("Converts array_slices.iris", "[Convertor]")
    {
        std::string_view const input_file = "array_slices.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts assert_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "assert_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts assignment_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "assignment_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts binary_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "binary_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts binary_expressions_precedence.iris", "[Convertor]")
    {
        std::string_view const input_file = "binary_expressions_precedence.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts bit_fields.iris", "[Convertor]")
    {
        std::string_view const input_file = "bit_fields.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts block_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "block_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts booleans.iris", "[Convertor]")
    {
        std::string_view const input_file = "booleans.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts break_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "break_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts char_literals.iris", "[Convertor]")
    {
        std::string_view const input_file = "char_literals.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_call_function_that_returns_bool.iris", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_call_function_that_returns_bool.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_call_function_with_struct.iris", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_call_function_with_struct.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_define_function_with_struct.iris", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_define_function_with_struct.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_return_big_struct.iris", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_return_big_struct.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_return_empty_struct.iris", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_return_empty_struct.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_return_int.iris", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_return_int.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_return_pointer.iris", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_return_pointer.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_return_small_struct.iris", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_return_small_struct.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_with_big_struct.iris", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_with_big_struct.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_with_empty_struct.iris", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_with_empty_struct.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_with_int_arguments.iris", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_with_int_arguments.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_with_pointer.iris", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_with_pointer.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts c_interoperability_function_with_small_struct.iris", "[Convertor]")
    {
        std::string_view const input_file = "c_interoperability_function_with_small_struct.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts cast_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "cast_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_alias.iris", "[Convertor]")
    {
        std::string_view const input_file = "comment_alias.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_enums.iris", "[Convertor]")
    {
        std::string_view const input_file = "comment_enums.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "comment_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_functions.iris", "[Convertor]")
    {
        std::string_view const input_file = "comment_functions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_global_variables.iris", "[Convertor]")
    {
        std::string_view const input_file = "comment_global_variables.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_imports.iris", "[Convertor]")
    {
        std::string_view const input_file = "comment_imports.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_module_declaration.iris", "[Convertor]")
    {
        std::string_view const input_file = "comment_module_declaration.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_structs.iris", "[Convertor]")
    {
        std::string_view const input_file = "comment_structs.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts comment_unions.iris", "[Convertor]")
    {
        std::string_view const input_file = "comment_unions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts compile_time_for.iris", "[Convertor]")
    {
        std::string_view const input_file = "compile_time_for.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts compile_time_if.iris", "[Convertor]")
    {
        std::string_view const input_file = "compile_time_if.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts compile_time_var.iris", "[Convertor]")
    {
        std::string_view const input_file = "compile_time_var.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts constant_array_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "constant_array_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_all.iris", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_all.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_c_headers.iris", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_c_headers.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_for_loop.iris", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_for_loop.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_function_call.iris", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_function_call.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_if.iris", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_if.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_structs.iris", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_structs.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_switch.iris", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_switch.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_unions.iris", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_unions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_variables.iris", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_variables.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts debug_information_while_loop.iris", "[Convertor]")
    {
        std::string_view const input_file = "debug_information_while_loop.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts decimal_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "decimal_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts defer_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "defer_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts defer_expressions_with_debug_information.iris", "[Convertor]")
    {
        std::string_view const input_file = "defer_expressions_with_debug_information.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts dereference_and_access_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "dereference_and_access_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts dynamic_array.iris", "[Convertor]")
    {
        std::string_view const input_file = "dynamic_array.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts dynamic_array_usage.iris", "[Convertor]")
    {
        std::string_view const input_file = "dynamic_array_usage.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts empty_return_expression.iris", "[Convertor]")
    {
        std::string_view const input_file = "empty_return_expression.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts for_loop_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "for_loop_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts function_contracts.iris", "[Convertor]")
    {
        std::string_view const input_file = "function_contracts.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts function_pointers.iris", "[Convertor]")
    {
        std::string_view const input_file = "function_pointers.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts hello_world.iris", "[Convertor]")
    {
        std::string_view const input_file = "hello_world.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts instantiate_uninitialized.iris", "[Convertor]")
    {
        std::string_view const input_file = "instantiate_uninitialized.iris";
        test_convertor(input_file);
    }
    
    TEST_CASE("Converts instantiate_zero_initialized.iris", "[Convertor]")
    {
        std::string_view const input_file = "instantiate_zero_initialized.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts if_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "if_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts if_return_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "if_return_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts module_with_dots.iris", "[Convertor]")
    {
        std::string_view const input_file = "module_with_dots.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts multiple_modules_a.iris", "[Convertor]")
    {
        std::string_view const input_file = "multiple_modules_a.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts multiple_modules_b.iris", "[Convertor]")
    {
        std::string_view const input_file = "multiple_modules_b.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts multiple_modules_c.iris", "[Convertor]")
    {
        std::string_view const input_file = "multiple_modules_c.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts null_pointers.iris", "[Convertor]")
    {
        std::string_view const input_file = "null_pointers.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts numbers.iris", "[Convertor]")
    {
        std::string_view const input_file = "numbers.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts numeric_casts.iris", "[Convertor]")
    {
        std::string_view const input_file = "numeric_casts.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts pointers.iris", "[Convertor]")
    {
        std::string_view const input_file = "pointers.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts size_of.iris", "[Convertor]")
    {
        std::string_view const input_file = "size_of.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts soa_array_type.iris", "[Convertor]")
    {
        std::string_view const input_file = "soa_array_type.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts soa_array_view_type.iris", "[Convertor]")
    {
        std::string_view const input_file = "soa_array_view_type.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts soa_array_view_from_pointer.iris", "[Convertor]")
    {
        std::string_view const input_file = "soa_array_view_from_pointer.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts switch_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "switch_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts type_of_in_constructor_type_argument.iris", "[Convertor]")
    {
        std::string_view const input_file = "type_of_in_constructor_type_argument.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts ternary_condition_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "ternary_condition_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts test_framework.iris", "[Convertor]")
    {
        std::string_view const input_file = "test_framework.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts unary_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "unary_expressions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts unique_name.iris", "[Convertor]")
    {
        std::string_view const input_file = "unique_name.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_alias.iris", "[Convertor]")
    {
        std::string_view const input_file = "using_alias.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_alias_from_modules.iris", "[Convertor]")
    {
        std::string_view const input_file = "using_alias_from_modules.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_enum_flags.iris", "[Convertor]")
    {
        std::string_view const input_file = "using_enum_flags.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_enums.iris", "[Convertor]")
    {
        std::string_view const input_file = "using_enums.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_enums_from_modules.iris", "[Convertor]")
    {
        std::string_view const input_file = "using_enums_from_modules.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_function_constructors.iris", "[Convertor]")
    {
        std::string_view const input_file = "using_function_constructors.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_global_variables.iris", "[Convertor]")
    {
        std::string_view const input_file = "using_global_variables.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_structs.iris", "[Convertor]")
    {
        std::string_view const input_file = "using_structs.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_type_constructors.iris", "[Convertor]")
    {
        std::string_view const input_file = "using_type_constructors.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts using_unions.iris", "[Convertor]")
    {
        std::string_view const input_file = "using_unions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts variables.iris", "[Convertor]")
    {
        std::string_view const input_file = "variables.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts variadic_functions.iris", "[Convertor]")
    {
        std::string_view const input_file = "variadic_functions.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts void_pointer.iris", "[Convertor]")
    {
        std::string_view const input_file = "void_pointer.iris";
        test_convertor(input_file);
    }

    TEST_CASE("Converts while_loop_expressions.iris", "[Convertor]")
    {
        std::string_view const input_file = "while_loop_expressions.iris";
        test_convertor(input_file);
    }

}
