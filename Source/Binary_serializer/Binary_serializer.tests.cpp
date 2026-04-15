#include <memory_resource>
#include <string>
#include <vector>

#include <catch2/catch_all.hpp>

import iris.binary_serializer;
import iris.core;
import iris.core.types;

namespace iris::binary_serializer
{
    iris::Function_declaration create_function_declaration()
    {
        std::pmr::vector<iris::Type_reference> input_parameter_types
        {
            iris::Type_reference{.data = iris::Fundamental_type::Byte},
            iris::Type_reference{.data = iris::Fundamental_type::Byte},
        };

        std::pmr::vector<iris::Type_reference> output_parameter_types
        {
            Type_reference{.data = iris::Fundamental_type::Byte},
        };

        std::pmr::vector<std::pmr::string> input_parameter_names
        {
            "lhs", "rhs"
        };

        std::pmr::vector<std::pmr::string> output_parameter_names
        {
            {"sum"}
        };

        iris::Function_type function_type
        {
            .input_parameter_types = std::move(input_parameter_types),
            .output_parameter_types = std::move(output_parameter_types),
            .is_variadic = false
        };

        std::pmr::vector<iris::Source_position> input_parameter_source_positions
        {
            {
                .line = 3,
                .column = 22
            }
        };

        std::pmr::vector<iris::Source_position> output_parameter_source_positions
        {
            {
                .line = 3,
                .column = 38
            }
        };

        return iris::Function_declaration
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

    iris::Function_definition create_function_definition()
    {
        std::pmr::vector<iris::Expression> expressions
        {
            {
                iris::Expression{
                    .data = iris::Variable_expression{
                        .name = "lhs"
                    }
                },
                iris::Expression{
                    .data = Variable_expression{
                        .name = "rhs"
                    }
                },
                iris::Expression{
                    .data = iris::Binary_expression{
                        .left_hand_side = iris::Expression_index{
                            .expression_index = 0
                        },
                        .right_hand_side = iris::Expression_index{
                            .expression_index = 1
                        },
                        .operation = iris::Binary_operation::Add
                    }
                },
                iris::Expression{
                    .data = iris::Return_expression{
                        .expression = iris::Expression_index{
                            .expression_index = 2
                        },
                    }
                }
            }
        };

        std::pmr::vector<iris::Statement> statements
        {
            {
                iris::Statement{
                    .expressions = std::move(expressions)
                }
            }
        };

        iris::Function_definition function
        {
            .name = "Add",
            .statements = std::move(statements)
        };

        return function;
    }

    iris::Struct_declaration create_struct_declaration()
    {
        iris::Type_reference const uint32_type = iris::create_integer_type_type_reference(32, false);

        iris::Struct_declaration declaration = {};
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
                    iris::Expression{
                        .data = iris::Constant_expression{
                            .type = uint32_type,
                            .data = "0"
                        }
                    }
                }
            },
            {
                .expressions = {
                    iris::Expression{
                        .data = iris::Constant_expression{
                            .type = uint32_type,
                            .data = "0"
                        }
                    }
                }
            },
            {
                .expressions = {
                    iris::Expression{
                        .data = iris::Constant_expression{
                            .type = uint32_type,
                            .data = "0"
                        }
                    }
                }
            }
        };
        return declaration;
    }

    iris::Module create_core_module()
    {
        iris::Language_version const language_version
        {
            .major = 1,
            .minor = 2,
            .patch = 3
        };

        iris::Module_dependencies dependencies
        {
            .alias_imports = {
                {
                    .module_name = "C.Standard_library",
                    .alias = "Cstl",
                    .usages = { "puts" }
                }
            }
        };

        iris::Module_declarations export_declarations
        {
            .struct_declarations = { create_struct_declaration() },
            .function_declarations = { create_function_declaration() },
        };

        iris::Module_definitions definitions
        {
            .function_definitions = { create_function_definition() }
        };

        return iris::Module
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

        iris::Module const input = create_core_module();

        std::optional<std::pmr::vector<std::byte>> const data = serialize_module(
            input,
            output_allocator,
            temporaries_allocator
        );
        REQUIRE(data.has_value());

        std::optional<iris::Module> const output = deserialize_module(
            data.value()
        );
        REQUIRE(output.has_value());

        CHECK(input == output.value());
    }

    TEST_CASE("Test binary serialization of instanced declarations")
    {
        std::pmr::polymorphic_allocator<> output_allocator;
        std::pmr::polymorphic_allocator<> temporaries_allocator;

        iris::Struct_declaration const declaration = create_struct_declaration();
        iris::Module const input
        {
            .instanced_declarations = {
                .struct_declarations = {
                    declaration,
                    declaration
                }
            }
        };

        std::optional<std::pmr::vector<std::byte>> const data = serialize_module(
            input,
            output_allocator,
            temporaries_allocator
        );
        REQUIRE(data.has_value());

        std::optional<iris::Module> const output = deserialize_module(
            data.value()
        );
        REQUIRE(output.has_value());

        CHECK(input == output.value());
    }
}
