#include <memory_resource>
#include <string>
#include <vector>

#include <catch2/catch_all.hpp>

import h.binary_serializer;
import h.core;
import h.core.types;

namespace h::binary_serializer
{
    h::Function_declaration create_function_declaration()
    {
        std::pmr::vector<h::Type_reference> input_parameter_types
        {
            h::Type_reference{.data = h::Fundamental_type::Byte},
            h::Type_reference{.data = h::Fundamental_type::Byte},
        };

        std::pmr::vector<h::Type_reference> output_parameter_types
        {
            Type_reference{.data = h::Fundamental_type::Byte},
        };

        std::pmr::vector<std::pmr::string> input_parameter_names
        {
            "lhs", "rhs"
        };

        std::pmr::vector<std::pmr::string> output_parameter_names
        {
            {"sum"}
        };

        h::Function_type function_type
        {
            .input_parameter_types = std::move(input_parameter_types),
            .output_parameter_types = std::move(output_parameter_types),
            .is_variadic = false
        };

        std::pmr::vector<h::Source_position> input_parameter_source_positions
        {
            {
                .line = 3,
                .column = 22
            }
        };

        std::pmr::vector<h::Source_position> output_parameter_source_positions
        {
            {
                .line = 3,
                .column = 38
            }
        };

        return h::Function_declaration
        {
            .name = "Add",
            .type = std::move(function_type),
            .input_parameter_names = std::move(input_parameter_names),
            .output_parameter_names = std::move(output_parameter_names),
            .linkage = Linkage::External,
            .source_location = Source_range_location
            {
                .file_path = std::nullopt,
                .range = {
                    .start = {
                        .line = 2,
                        .column = 6,
                    },
                    .end = {
                        .line = 4,
                        .column = 2,
                    }
                },
            },
            .input_parameter_source_positions = std::move(input_parameter_source_positions),
            .output_parameter_source_positions = std::move(output_parameter_source_positions)
        };
    }

    h::Function_definition create_function_definition()
    {
        std::pmr::vector<h::Expression> expressions
        {
            {
                h::Expression{
                    .data = h::Variable_expression{
                        .name = "lhs"
                    }
                },
                h::Expression{
                    .data = Variable_expression{
                        .name = "rhs"
                    }
                },
                h::Expression{
                    .data = h::Binary_expression{
                        .left_hand_side = h::Expression_index{
                            .expression_index = 0
                        },
                        .right_hand_side = h::Expression_index{
                            .expression_index = 1
                        },
                        .operation = h::Binary_operation::Add
                    }
                },
                h::Expression{
                    .data = h::Return_expression{
                        .expression = h::Expression_index{
                            .expression_index = 2
                        },
                    }
                }
            }
        };

        std::pmr::vector<h::Statement> statements
        {
            {
                h::Statement{
                    .expressions = std::move(expressions)
                }
            }
        };

        h::Function_definition function
        {
            .name = "Add",
            .statements = std::move(statements)
        };

        return function;
    }

    h::Struct_declaration create_struct_declaration()
    {
        h::Type_reference const uint32_type = h::create_integer_type_type_reference(32, false);

        h::Struct_declaration declaration = {};
        declaration.name = "My_struct";
        declaration.member_names = {
            "first",
            "second",
            "third"
        };
        declaration.member_types = {
            uint32_type,
            uint32_type,
            uint32_type
        };
        declaration.member_bit_fields = {
            24,
            8,
            std::nullopt
        };
        declaration.member_default_values = {
            {
                .expressions = {
                    h::Expression{
                        .data = h::Constant_expression{
                            .type = uint32_type,
                            .data = "0"
                        }
                    }
                }
            },
            {
                .expressions = {
                    h::Expression{
                        .data = h::Constant_expression{
                            .type = uint32_type,
                            .data = "0"
                        }
                    }
                }
            },
            {
                .expressions = {
                    h::Expression{
                        .data = h::Constant_expression{
                            .type = uint32_type,
                            .data = "0"
                        }
                    }
                }
            }
        };
        return declaration;
    }

    h::Module create_core_module()
    {
        h::Language_version const language_version
        {
            .major = 1,
            .minor = 2,
            .patch = 3
        };

        h::Module_dependencies dependencies
        {
            .alias_imports = {
                {
                    .module_name = "C.Standard_library",
                    .alias = "Cstl",
                    .usages = { "puts" }
                }
            }
        };

        h::Module_declarations export_declarations
        {
            .struct_declarations = { create_struct_declaration() },
            .function_declarations = { create_function_declaration() },
        };

        h::Module_definitions definitions
        {
            .function_definitions = { create_function_definition() }
        };

        return h::Module
        {
            .language_version = language_version,
            .name = "module_name",
            .content_hash = 12089789297091071925ull,
            .dependencies = std::move(dependencies),
            .export_declarations = std::move(export_declarations),
            .internal_declarations = Module_declarations{},
            .definitions = std::move(definitions),
        };
    }

    TEST_CASE("Test binary serialization of Module")
    {
        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        h::Module const input = create_core_module();

        std::optional<std::pmr::vector<std::byte>> const data = serialize_module(
            input,
            output_allocator,
            temporaries_allocator
        );
        REQUIRE(data.has_value());

        std::optional<h::Module> const output = deserialize_module(
            data.value()
        );
        REQUIRE(output.has_value());

        CHECK(input == output.value());
    }
}
