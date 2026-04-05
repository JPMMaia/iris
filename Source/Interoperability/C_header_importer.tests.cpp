#include <iosfwd>

import h.common;
import h.common.filesystem;
import h.core;
import h.core.expressions;
import h.core.types;
import h.c_header_converter;
import h.json_serializer.operators;

using h::json::operators::operator<<;

#include <catch2/catch_test_macros.hpp>

import std;

namespace h::c
{
    constexpr char const* g_vulkan_headers_location = VULKAN_HEADERS_LOCATION;

    std::filesystem::path find_c_header_path(
        std::string_view const filename,
        std::span<std::filesystem::path const> const header_search_directories
    )
    {
        {
            std::filesystem::path const file_path = filename;
            if (file_path.is_absolute())
                return file_path;
        }

        for (std::filesystem::path const& search_path : header_search_directories)
        {
            for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator{ search_path })
            {
                if (entry.path().filename() == filename)
                {
                    return entry.path();
                }
            }
        }

        REQUIRE(false);
        return "";
    }

    h::Alias_type_declaration const& find_alias_type_declaration(h::Module const& header_module, std::string_view const name)
    {
        std::span<h::Alias_type_declaration const> const declarations = header_module.export_declarations.alias_type_declarations;
        auto const location = std::find_if(declarations.begin(), declarations.end(), [name](h::Alias_type_declaration const& value) -> bool { return value.name == name; });
        REQUIRE(location != declarations.end());
        return *location;
    }

    h::Enum_declaration const& find_enum_declaration(h::Module const& header_module, std::string_view const name)
    {
        std::span<h::Enum_declaration const> const declarations = header_module.export_declarations.enum_declarations;
        auto const location = std::find_if(declarations.begin(), declarations.end(), [name](h::Enum_declaration const& value) -> bool { return value.name == name; });
        REQUIRE(location != declarations.end());
        return *location;
    }

    h::Function_declaration const& find_function_declaration(h::Module const& header_module, std::string_view const name)
    {
        std::span<h::Function_declaration const> const function_declarations = header_module.export_declarations.function_declarations;
        auto const location = std::find_if(function_declarations.begin(), function_declarations.end(), [name](h::Function_declaration const& function_declaration) -> bool { return function_declaration.name == name; });
        REQUIRE(location != function_declarations.end());
        return *location;
    }

    h::Global_variable_declaration const& find_global_variable_declaration(h::Module const& header_module, std::string_view const name)
    {
        std::span<h::Global_variable_declaration const> const global_variable_declarations = header_module.export_declarations.global_variable_declarations;
        auto const location = std::find_if(global_variable_declarations.begin(), global_variable_declarations.end(), [name](h::Global_variable_declaration const& global_variable_declaration) -> bool { return global_variable_declaration.name == name; });
        REQUIRE(location != global_variable_declarations.end());
        return *location;
    }

    h::Struct_declaration const& find_struct_declaration(h::Module const& header_module, std::string_view const name)
    {
        std::span<h::Struct_declaration const> const struct_declarations = header_module.export_declarations.struct_declarations;
        auto const location = std::find_if(struct_declarations.begin(), struct_declarations.end(), [name](h::Struct_declaration const& struct_declaration) -> bool { return struct_declaration.name == name; });
        REQUIRE(location != struct_declarations.end());
        return *location;
    }

    h::Union_declaration const& find_union_declaration(h::Module const& header_module, std::string_view const name)
    {
        std::span<h::Union_declaration const> const union_declarations = header_module.export_declarations.union_declarations;
        auto const location = std::find_if(union_declarations.begin(), union_declarations.end(), [name](h::Union_declaration const& union_declaration) -> bool { return union_declaration.name == name; });
        REQUIRE(location != union_declarations.end());
        return *location;
    }

    void check_enum_constant_value(h::Enum_declaration const& actual, std::size_t const value_index, std::string_view const value_expected_data)
    {
        auto const& expression_data = actual.values[value_index].value.value().expressions[0].data;

        REQUIRE(std::holds_alternative<h::Constant_expression>(expression_data));
        Constant_expression const& expression = std::get<h::Constant_expression>(expression_data);

        h::Integer_type const int32_type
        {
            .number_of_bits = 32,
            .is_signed = true
        };

        CHECK(expression == h::Constant_expression{ .type = int32_type, .data = std::pmr::string{value_expected_data} });
    }

    static h::Statement create_cast_constant_statement(
        std::string_view const data,
        h::Type_reference source_type,
        h::Type_reference destination_type
    )
    {
        return
        {
            .expressions = {
                h::Expression
                {
                    .data = h::Cast_expression
                    {
                        .source = { .expression_index = 1 },
                        .destination_type = destination_type,
                        .cast_type = Cast_type::Numeric
                    }
                },
                h::Expression
                {
                    .data = h::Constant_expression
                    {
                        .type = source_type,
                        .data = std::pmr::string{data}
                    }
                }
            }
        };
    }

    h::Module const& import_vulkan_header_module()
    {
        static std::optional<h::Module> header_module_optional = std::nullopt;
        if (header_module_optional.has_value())
            return header_module_optional.value();

        std::filesystem::path const vulkan_headers_path = g_vulkan_headers_location;
        std::filesystem::path const vulkan_header_path = vulkan_headers_path / "vulkan" / "vulkan.h";

        std::pmr::vector<std::filesystem::path> include_directories;
        include_directories.push_back(vulkan_headers_path);

        h::c::Options const options = {
            .include_directories = include_directories,
            .allow_errors = false,
        };

        header_module_optional = h::c::import_header("vulkan", vulkan_header_path, options);
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == vulkan_header_path);
    
        return header_module;
    }
    

    TEST_CASE("Import stdio.h C header creates 'puts' function declaration")
    {
        std::pmr::vector<std::filesystem::path> const header_search_directories = 
            h::common::get_default_header_search_directories();

        std::filesystem::path const stdio_header_path = find_c_header_path("stdio.h", header_search_directories);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.stdio", stdio_header_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == stdio_header_path);

        h::Function_declaration const& actual = h::c::find_function_declaration(header_module, "puts");

        CHECK(actual.name == "puts");

        REQUIRE(actual.unique_name.has_value());
        CHECK(actual.unique_name.value() == "puts");

        {
            h::Type_reference const c_char_type_reference{ .data = h::Fundamental_type::C_char };
            h::Type_reference const c_char_const_pointer_type_reference{ .data = h::Pointer_type{.element_type = {c_char_type_reference}, .is_mutable = false } };
            CHECK(actual.type.input_parameter_types == std::pmr::vector<h::Type_reference>{c_char_const_pointer_type_reference});
        }

        {
            h::Type_reference const expected_return_type{ .data = h::Fundamental_type::C_int };

            REQUIRE(actual.type.output_parameter_types.size() == 1);
            CHECK(actual.type.output_parameter_types[0] == expected_return_type);
        }

        REQUIRE(actual.input_parameter_names.size() == 1);
        CHECK(!actual.input_parameter_names[0].empty());
        REQUIRE(actual.output_parameter_names.size() == 1);
        CHECK(!actual.output_parameter_names[0].empty());
        CHECK(actual.linkage == Linkage::External);
    }

    TEST_CASE("Import time.h C header creates 'time_t' typedef")
    {
        std::pmr::vector<std::filesystem::path> const header_search_directories = 
            h::common::get_default_header_search_directories();

        std::filesystem::path const time_header_path = find_c_header_path("time.h", header_search_directories);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.time", time_header_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == time_header_path);

        h::Alias_type_declaration const& actual = h::c::find_alias_type_declaration(header_module, "time_t");

        CHECK(actual.name == "time_t");

        REQUIRE(actual.unique_name.has_value());
        CHECK(actual.unique_name.value() == "time_t");

        CHECK(!actual.type.empty());
    }

    TEST_CASE("Import vulkan.h C header creates 'VkPhysicalDeviceType' enum")
    {
        h::Module const& header_module = import_vulkan_header_module();

        h::Enum_declaration const& actual = h::c::find_enum_declaration(header_module, "VkPhysicalDeviceType");

        CHECK(actual.name == "VkPhysicalDeviceType");
        REQUIRE(actual.unique_name.has_value());
        CHECK(actual.unique_name.value() == "VkPhysicalDeviceType");

        REQUIRE(actual.values.size() >= 5);

        CHECK(actual.values[0].name == "Other");
        check_enum_constant_value(actual, 0, "0");

        CHECK(actual.values[1].name == "Integrated_gpu");
        check_enum_constant_value(actual, 1, "1");

        CHECK(actual.values[2].name == "Discrete_gpu");
        check_enum_constant_value(actual, 2, "2");

        CHECK(actual.values[3].name == "Virtual_gpu");
        check_enum_constant_value(actual, 3, "3");

        CHECK(actual.values[4].name == "Cpu");
        check_enum_constant_value(actual, 4, "4");
    }

    TEST_CASE("Import vulkan.h C header creates 'VkCommandPoolCreateInfo' struct")
    {
        h::Module const& header_module = import_vulkan_header_module();

        h::Struct_declaration const& actual = h::c::find_struct_declaration(header_module, "VkCommandPoolCreateInfo");

        CHECK(actual.name == "VkCommandPoolCreateInfo");

        REQUIRE(actual.unique_name.has_value());
        CHECK(actual.unique_name.value() == "VkCommandPoolCreateInfo");

        CHECK(actual.is_packed == false);
        CHECK(actual.is_literal == false);

        REQUIRE(actual.member_types.size() == 4);
        REQUIRE(actual.member_names.size() == 4);
        REQUIRE(actual.member_bit_fields.size() == 4);

        {
            CHECK(actual.member_names[0] == "sType");
            CHECK(actual.member_types[0] == h::create_custom_type_reference("vulkan", "VkStructureType"));
            CHECK(actual.member_bit_fields[0].has_value() == false);
        }

        {
            CHECK(actual.member_names[1] == "pNext");
            CHECK(actual.member_types[1] == h::create_pointer_type_type_reference({}, false));
            CHECK(actual.member_bit_fields[1].has_value() == false);
        }

        {
            CHECK(actual.member_names[2] == "flags");
            CHECK(actual.member_types[2] == h::create_custom_type_reference("vulkan", "VkCommandPoolCreateFlags"));
            CHECK(actual.member_bit_fields[2].has_value() == false);
        }

        {
            CHECK(actual.member_names[3] == "queueFamilyIndex");
            CHECK(actual.member_types[3] == h::create_integer_type_type_reference(32, false));
            CHECK(actual.member_bit_fields[3].has_value() == false);
        }
    }

    TEST_CASE("Import vulkan.h C header creates 'VkExtent2D' struct")
    {
        h::Module const& header_module = import_vulkan_header_module();

        h::Struct_declaration const& actual = h::c::find_struct_declaration(header_module, "VkExtent2D");

        CHECK(actual.name == "VkExtent2D");

        REQUIRE(actual.unique_name.has_value());
        CHECK(actual.unique_name.value() == "VkExtent2D");

        REQUIRE(actual.member_types.size() == 2);
        REQUIRE(actual.member_names.size() == 2);
        REQUIRE(actual.member_bit_fields.size() == 2);
        REQUIRE(actual.member_default_values.size() == 2);

        h::Type_reference const uint32_type
        {
            .data = h::Integer_type
            {
                .number_of_bits = 32,
                .is_signed = false
            }
        };

        h::Statement const expected_default_value =
        {
            .expressions = {
                h::Expression
                {
                    .data = h::Constant_expression
                    {
                        .type = uint32_type,
                        .data = "0"
                    }
                }
            }
        };

        {
            CHECK(actual.member_names[0] == "width");
            CHECK(actual.member_types[0] == uint32_type);
            CHECK(actual.member_bit_fields[0].has_value() == false);
            CHECK(actual.member_default_values[0] == expected_default_value);
        }

        {
            CHECK(actual.member_names[1] == "height");
            CHECK(actual.member_types[1] == uint32_type);
            CHECK(actual.member_bit_fields[0].has_value() == false);
            CHECK(actual.member_default_values[1] == expected_default_value);
        }
    }

    TEST_CASE("Import vulkan.h C header creates 'VkRect2D' struct")
    {
        h::Module const& header_module = import_vulkan_header_module();

        h::Struct_declaration const& actual = h::c::find_struct_declaration(header_module, "VkRect2D");

        CHECK(actual.name == "VkRect2D");

        REQUIRE(actual.unique_name.has_value());
        CHECK(actual.unique_name.value() == "VkRect2D");

        REQUIRE(actual.member_types.size() == 2);
        REQUIRE(actual.member_names.size() == 2);
        REQUIRE(actual.member_bit_fields.size() == 2);
        REQUIRE(actual.member_default_values.size() == 2);

        {
            CHECK(actual.member_names[0] == "offset");

            h::Custom_type_reference const expected_type =
            {
                .module_reference = {
                    .name = "vulkan"
                },
                .name = "VkOffset2D"
            };

            CHECK(actual.member_types[0] == h::Type_reference{ .data = expected_type });
            CHECK(actual.member_bit_fields[0].has_value() == false);

            h::Statement const expected_default_value =
            {
                .expressions = {
                    h::Expression
                    {
                        .data = h::Instantiate_expression
                        {
                            .type = Instantiate_expression_type::Default,
                            .members = {}
                        }
                    }
                }
            };

            CHECK(actual.member_default_values[0] == expected_default_value);
        }

        {
            CHECK(actual.member_names[1] == "extent");

            h::Custom_type_reference const expected_type =
            {
                .module_reference = {
                    .name = "vulkan"
                },
                .name = "VkExtent2D"
            };

            CHECK(actual.member_types[1] == h::Type_reference{ .data = expected_type });
            CHECK(actual.member_bit_fields[1].has_value() == false);

            h::Statement const expected_default_value =
            {
                .expressions = {
                    h::Expression
                    {
                        .data = h::Instantiate_expression
                        {
                            .type = Instantiate_expression_type::Default,
                            .members = {}
                        }
                    }
                }
            };

            CHECK(actual.member_default_values[1] == expected_default_value);
        }
    }

    TEST_CASE("Import vulkan.h C header creates 'VkClearAttachment' struct")
    {
        h::Module const& header_module = import_vulkan_header_module();

        h::Struct_declaration const& actual = h::c::find_struct_declaration(header_module, "VkClearAttachment");

        CHECK(actual.name == "VkClearAttachment");

        REQUIRE(actual.unique_name.has_value());
        CHECK(actual.unique_name.value() == "VkClearAttachment");

        REQUIRE(actual.member_types.size() == 3);
        REQUIRE(actual.member_names.size() == 3);
        REQUIRE(actual.member_bit_fields.size() == 3);
        REQUIRE(actual.member_default_values.size() == 3);

        h::Type_reference const uint32_type
        {
            .data = h::Integer_type
            {
                .number_of_bits = 32,
                .is_signed = false
            }
        };

        {
            CHECK(actual.member_names[0] == "aspectMask");

            h::Custom_type_reference const expected_type =
            {
                .module_reference = {
                    .name = "vulkan"
                },
                .name = "VkImageAspectFlags"
            };

            CHECK(actual.member_types[0] == h::Type_reference{ .data = expected_type });
            CHECK(actual.member_bit_fields[0].has_value() == false);

            h::Statement const expected_default_value =
            {
                .expressions = {
                    h::Expression
                    {
                        .data = h::Cast_expression
                        {
                            .source = { .expression_index = 1 },
                            .destination_type = actual.member_types[0],
                            .cast_type = Cast_type::Numeric
                        }
                    },
                    h::Expression
                    {
                        .data = h::Constant_expression
                        {
                            .type = uint32_type,
                            .data = "0"
                        }
                    }
                }
            };

            CHECK(actual.member_default_values[0] == create_cast_constant_statement("0", uint32_type, actual.member_types[0]));
        }

        {
            CHECK(actual.member_names[1] == "colorAttachment");
            CHECK(actual.member_types[1] == uint32_type);
            CHECK(actual.member_bit_fields[1].has_value() == false);

            h::Statement const expected_default_value =
            {
                .expressions = {
                    h::Expression
                    {
                        .data = h::Constant_expression
                        {
                            .type = uint32_type,
                            .data = "0"
                        }
                    }
                }
            };

            CHECK(actual.member_default_values[1] == expected_default_value);
        }

        {
            CHECK(actual.member_names[2] == "clearValue");

            h::Custom_type_reference const expected_type =
            {
                .module_reference = {
                    .name = "vulkan"
                },
                .name = "VkClearValue"
            };

            CHECK(actual.member_types[2] == h::Type_reference{ .data = expected_type });
            CHECK(actual.member_bit_fields[2].has_value() == false);

            Type_reference const float32_type = h::create_fundamental_type_type_reference(h::Fundamental_type::Float32);

            h::Expression const color_array_value = h::Expression
            {
                .data = h::Constant_array_expression
                {
                    .array_data =
                    {
                        h::create_statement({h::create_constant_expression(float32_type, "0.0")}),
                        h::create_statement({h::create_constant_expression(float32_type, "0.0")}),
                        h::create_statement({h::create_constant_expression(float32_type, "0.0")}),
                        h::create_statement({h::create_constant_expression(float32_type, "0.0")}),
                    }
                }
            };

            h::Statement const expected_default_value =
            {
                .expressions = {
                    h::Expression
                    {
                        .data = h::Instantiate_expression
                        {
                            .type = Instantiate_expression_type::Default,
                            .members = {
                                Instantiate_member_value_pair
                                {
                                    .member_name = "color",
                                    .value = {.expression_index = 1}
                                }
                            }
                        }
                    },
                    {
                        .data = h::Instantiate_expression
                        {
                            .type = Instantiate_expression_type::Default,
                            .members = {
                                Instantiate_member_value_pair
                                {
                                    .member_name = "float32",
                                    .value = {.expression_index = 2}
                                }
                            }
                        }
                    },
                    {
                        .data = h::Constant_array_expression
                        {
                            .array_data =
                            {
                                h::create_statement({h::create_constant_expression(float32_type, "0.0")}),
                                h::create_statement({h::create_constant_expression(float32_type, "0.0")}),
                                h::create_statement({h::create_constant_expression(float32_type, "0.0")}),
                                h::create_statement({h::create_constant_expression(float32_type, "0.0")}),
                            }
                        }
                    }
                }
            };

            CHECK(actual.member_default_values[2] == expected_default_value);
        }
    }

    TEST_CASE("Import vulkan.h C header creates 'VkBufferCreateInfo' struct")
    {
        h::Module const& header_module = import_vulkan_header_module();

        h::Struct_declaration const& actual = h::c::find_struct_declaration(header_module, "VkBufferCreateInfo");

        CHECK(actual.name == "VkBufferCreateInfo");

        REQUIRE(actual.unique_name.has_value());
        CHECK(actual.unique_name.value() == "VkBufferCreateInfo");

        REQUIRE(actual.member_types.size() == 8);
        REQUIRE(actual.member_names.size() == 8);
        REQUIRE(actual.member_bit_fields.size() == 8);
        REQUIRE(actual.member_default_values.size() == 8);

        h::Type_reference const uint32_type
        {
            .data = h::Integer_type
            {
                .number_of_bits = 32,
                .is_signed = false
            }
        };

        h::Type_reference const uint64_type
        {
            .data = h::Integer_type
            {
                .number_of_bits = 64,
                .is_signed = false
            }
        };

        CHECK(actual.member_names[0] == "sType");
        CHECK(actual.member_types[0] == create_custom_type_reference("vulkan", "VkStructureType"));
        CHECK(actual.member_bit_fields[0].has_value() == false);
        CHECK(actual.member_default_values[0] == h::create_statement(h::create_enum_value_expressions("VkStructureType", "Application_info")));

        CHECK(actual.member_names[1] == "pNext");
        CHECK(actual.member_types[1] == h::create_pointer_type_type_reference({}, false));
        CHECK(actual.member_bit_fields[1].has_value() == false);
        CHECK(actual.member_default_values[1] == h::create_statement({ h::create_null_pointer_expression() }));

        CHECK(actual.member_names[2] == "flags");
        CHECK(actual.member_types[2] == create_custom_type_reference("vulkan", "VkBufferCreateFlags"));
        CHECK(actual.member_bit_fields[2].has_value() == false);
        CHECK(actual.member_default_values[2] == create_cast_constant_statement("0", uint32_type, actual.member_types[2]));

        CHECK(actual.member_names[3] == "size");
        CHECK(actual.member_types[3] == create_custom_type_reference("vulkan", "VkDeviceSize"));
        CHECK(actual.member_bit_fields[3].has_value() == false);
        CHECK(actual.member_default_values[3] == create_cast_constant_statement("0", uint64_type, actual.member_types[3]));

        CHECK(actual.member_names[4] == "usage");
        CHECK(actual.member_types[4] == create_custom_type_reference("vulkan", "VkBufferUsageFlags"));
        CHECK(actual.member_bit_fields[4].has_value() == false);
        CHECK(actual.member_default_values[4] == create_cast_constant_statement("0", uint32_type, actual.member_types[4]));

        CHECK(actual.member_names[5] == "sharingMode");
        CHECK(actual.member_types[5] == create_custom_type_reference("vulkan", "VkSharingMode"));
        CHECK(actual.member_bit_fields[5].has_value() == false);
        CHECK(actual.member_default_values[5] == h::create_statement(h::create_enum_value_expressions("VkSharingMode", "Exclusive")));

        CHECK(actual.member_names[6] == "queueFamilyIndexCount");
        CHECK(actual.member_types[6] == uint32_type);
        CHECK(actual.member_bit_fields[6].has_value() == false);
        CHECK(actual.member_default_values[6] == h::create_statement({ h::create_constant_expression(uint32_type, "0") }));

        CHECK(actual.member_names[7] == "pQueueFamilyIndices");
        CHECK(actual.member_types[7] == h::create_pointer_type_type_reference({ h::create_integer_type_type_reference(32, false) }, false));
        CHECK(actual.member_bit_fields[7].has_value() == false);
        CHECK(actual.member_default_values[7] == h::create_statement({ h::create_null_pointer_expression() }));
    }

    TEST_CASE("Import vulkan.h C header has correct macro types")
    {
        h::Module const& header_module = import_vulkan_header_module();
        h::Global_variable_declaration const& actual = h::c::find_global_variable_declaration(header_module, "VK_API_VERSION_1_0");

        CHECK(actual.name == "VK_API_VERSION_1_0");

        REQUIRE(actual.unique_name.has_value());
        CHECK(actual.unique_name.value() == "VK_API_VERSION_1_0");

        h::Type_reference const uint32_type
        {
            .data = h::Integer_type
            {
                .number_of_bits = 32,
                .is_signed = false
            }
        };

        REQUIRE(actual.type.has_value());
        CHECK(actual.type.value() == uint32_type);
        
        CHECK(actual.global_type == h::Global_variable_type::Macro);
    }

    TEST_CASE("Import vulkan.h C header creates 'VkClearColorValue' union")
    {
        h::Module const& header_module = import_vulkan_header_module();

        h::Union_declaration const& actual = h::c::find_union_declaration(header_module, "VkClearColorValue");

        CHECK(actual.name == "VkClearColorValue");

        REQUIRE(actual.unique_name.has_value());
        CHECK(actual.unique_name.value() == "VkClearColorValue");

        REQUIRE(actual.member_types.size() == 3);
        REQUIRE(actual.member_names.size() == 3);

        {
            CHECK(actual.member_names[0] == "float32");

            h::Constant_array_type const expected_type =
            {
                .value_type = {
                     h::Type_reference
                    {
                        .data = h::Fundamental_type::Float32
                    }
                },
                .size = 4
            };

            CHECK(actual.member_types[0] == h::Type_reference{ .data = expected_type });
        }

        {
            CHECK(actual.member_names[1] == "int32");

            h::Constant_array_type const expected_type =
            {
                .value_type = {
                    h::Type_reference
                    {
                        .data = h::Integer_type
                        {
                            .number_of_bits = 32,
                            .is_signed = true
                        }
                    }
                },
                .size = 4
            };

            CHECK(actual.member_types[1] == h::Type_reference{ .data = expected_type });
        }

        {
            CHECK(actual.member_names[2] == "uint32");

            h::Constant_array_type const expected_type =
            {
                .value_type = {
                    h::Type_reference
                    {
                        .data = h::Integer_type
                        {
                            .number_of_bits = 32,
                            .is_signed = false
                        }
                    }
                },
                .size = 4
            };

            CHECK(actual.member_types[2] == h::Type_reference{ .data = expected_type });
        }
    }

    TEST_CASE("Handles anonymous declarations inside structs")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "anonymous_declarations_inside_structs";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
struct My_data
{
    int type;
    union
    {
        int x;
        double y;
        float z;
    };
    struct
    {
        int v1;
        int v2;
    };
    union
    {
        int a;
        double b;
        float c;
    } member_1;
    struct
    {
        int v1;
        int v2;
    } member_2;
};
)";

        std::filesystem::path const header_file_path = root_directory_path / "My_data.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.My_data", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == header_file_path);

        {
            h::Struct_declaration const& declaration = header_module.export_declarations.struct_declarations[0];
            CHECK(declaration.name == "My_data");

            CHECK(declaration.member_names[0] == "type");
            CHECK(declaration.member_types[0] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));
            CHECK(declaration.member_bit_fields[0].has_value() == false);

            CHECK(declaration.member_names[1] == "anonymous_0");
            CHECK(declaration.member_types[1] == h::create_custom_type_reference("c.My_data", "_c_My_data_Anonymous_1"));
            CHECK(declaration.member_bit_fields[1].has_value() == false);

            CHECK(declaration.member_names[2] == "anonymous_1");
            CHECK(declaration.member_types[2] == h::create_custom_type_reference("c.My_data", "_c_My_data_Anonymous_2"));
            CHECK(declaration.member_bit_fields[2].has_value() == false);

            CHECK(declaration.member_names[3] == "member_1");
            CHECK(declaration.member_types[3] == h::create_custom_type_reference("c.My_data", "_c_My_data_Anonymous_3"));
            CHECK(declaration.member_bit_fields[3].has_value() == false);

            CHECK(declaration.member_names[4] == "member_2");
            CHECK(declaration.member_types[4] == h::create_custom_type_reference("c.My_data", "_c_My_data_Anonymous_4"));
            CHECK(declaration.member_bit_fields[4].has_value() == false);
        }

        {
            h::Union_declaration const& declaration = header_module.internal_declarations.union_declarations[0];
            CHECK(declaration.name == "_c_My_data_Anonymous_1");

            CHECK(declaration.member_names[0] == "x");
            CHECK(declaration.member_types[0] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));

            CHECK(declaration.member_names[1] == "y");
            CHECK(declaration.member_types[1] == h::create_fundamental_type_type_reference(h::Fundamental_type::Float64));

            CHECK(declaration.member_names[2] == "z");
            CHECK(declaration.member_types[2] == h::create_fundamental_type_type_reference(h::Fundamental_type::Float32));
        }

        {
            h::Struct_declaration const& declaration = header_module.internal_declarations.struct_declarations[0];
            CHECK(declaration.name == "_c_My_data_Anonymous_2");

            CHECK(declaration.member_names[0] == "v1");
            CHECK(declaration.member_types[0] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));
            CHECK(declaration.member_bit_fields[0].has_value() == false);

            CHECK(declaration.member_names[1] == "v2");
            CHECK(declaration.member_types[1] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));
            CHECK(declaration.member_bit_fields[1].has_value() == false);
        }

        {
            h::Union_declaration const& declaration = header_module.internal_declarations.union_declarations[1];
            CHECK(declaration.name == "_c_My_data_Anonymous_3");

            CHECK(declaration.member_names[0] == "a");
            CHECK(declaration.member_types[0] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));

            CHECK(declaration.member_names[1] == "b");
            CHECK(declaration.member_types[1] == h::create_fundamental_type_type_reference(h::Fundamental_type::Float64));

            CHECK(declaration.member_names[2] == "c");
            CHECK(declaration.member_types[2] == h::create_fundamental_type_type_reference(h::Fundamental_type::Float32));
        }

        {
            h::Struct_declaration const& declaration = header_module.internal_declarations.struct_declarations[1];
            CHECK(declaration.name == "_c_My_data_Anonymous_4");

            CHECK(declaration.member_names[0] == "v1");
            CHECK(declaration.member_types[0] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));
            CHECK(declaration.member_bit_fields[0].has_value() == false);

            CHECK(declaration.member_names[1] == "v2");
            CHECK(declaration.member_types[1] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));
            CHECK(declaration.member_bit_fields[1].has_value() == false);
        }
    }

    TEST_CASE("Handles anonymous declarations inside unions")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "anonymous_declarations_inside_unions";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
union My_data
{
    int type;
    union
    {
        int x;
        double y;
        float z;
    };
    struct
    {
        int v1;
        int v2;
    };
    union
    {
        int a;
        double b;
        float c;
    } member_1;
    struct
    {
        int v1;
        int v2;
    } member_2;
};
)";

        std::filesystem::path const header_file_path = root_directory_path / "My_data.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.My_data", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == header_file_path);

        {
            h::Union_declaration const& declaration = header_module.export_declarations.union_declarations[0];
            CHECK(declaration.name == "My_data");

            CHECK(declaration.member_names[0] == "type");
            CHECK(declaration.member_types[0] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));

            CHECK(declaration.member_names[1] == "anonymous_0");
            CHECK(declaration.member_types[1] == h::create_custom_type_reference("c.My_data", "_c_My_data_Anonymous_1"));

            CHECK(declaration.member_names[2] == "anonymous_1");
            CHECK(declaration.member_types[2] == h::create_custom_type_reference("c.My_data", "_c_My_data_Anonymous_2"));

            CHECK(declaration.member_names[3] == "member_1");
            CHECK(declaration.member_types[3] == h::create_custom_type_reference("c.My_data", "_c_My_data_Anonymous_3"));

            CHECK(declaration.member_names[4] == "member_2");
            CHECK(declaration.member_types[4] == h::create_custom_type_reference("c.My_data", "_c_My_data_Anonymous_4"));
        }

        {
            h::Union_declaration const& declaration = header_module.internal_declarations.union_declarations[0];
            CHECK(declaration.name == "_c_My_data_Anonymous_1");

            CHECK(declaration.member_names[0] == "x");
            CHECK(declaration.member_types[0] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));

            CHECK(declaration.member_names[1] == "y");
            CHECK(declaration.member_types[1] == h::create_fundamental_type_type_reference(h::Fundamental_type::Float64));

            CHECK(declaration.member_names[2] == "z");
            CHECK(declaration.member_types[2] == h::create_fundamental_type_type_reference(h::Fundamental_type::Float32));
        }

        {
            h::Struct_declaration const& declaration = header_module.internal_declarations.struct_declarations[0];
            CHECK(declaration.name == "_c_My_data_Anonymous_2");

            CHECK(declaration.member_names[0] == "v1");
            CHECK(declaration.member_types[0] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));
            CHECK(declaration.member_bit_fields[0].has_value() == false);

            CHECK(declaration.member_names[1] == "v2");
            CHECK(declaration.member_types[1] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));
            CHECK(declaration.member_bit_fields[1].has_value() == false);
        }

        {
            h::Union_declaration const& declaration = header_module.internal_declarations.union_declarations[1];
            CHECK(declaration.name == "_c_My_data_Anonymous_3");

            CHECK(declaration.member_names[0] == "a");
            CHECK(declaration.member_types[0] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));

            CHECK(declaration.member_names[1] == "b");
            CHECK(declaration.member_types[1] == h::create_fundamental_type_type_reference(h::Fundamental_type::Float64));

            CHECK(declaration.member_names[2] == "c");
            CHECK(declaration.member_types[2] == h::create_fundamental_type_type_reference(h::Fundamental_type::Float32));
        }

        {
            h::Struct_declaration const& declaration = header_module.internal_declarations.struct_declarations[1];
            CHECK(declaration.name == "_c_My_data_Anonymous_4");

            CHECK(declaration.member_names[0] == "v1");
            CHECK(declaration.member_types[0] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));
            CHECK(declaration.member_bit_fields[0].has_value() == false);

            CHECK(declaration.member_names[1] == "v2");
            CHECK(declaration.member_types[1] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));
            CHECK(declaration.member_bit_fields[1].has_value() == false);
        }
    }

    TEST_CASE("Handles nested anonymous declarations")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "nested_anonymous_declarations";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
struct My_data
{
    union
    {
        struct
        {
            int x;
            double y;
        };
        float z;
    };
};
)";

        std::filesystem::path const header_file_path = root_directory_path / "My_data.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.My_data", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        REQUIRE(header_module.internal_declarations.struct_declarations.size() == 1);
        REQUIRE(header_module.internal_declarations.union_declarations.size() == 1);

        {
            h::Struct_declaration const& declaration = header_module.internal_declarations.struct_declarations[0];
            CHECK(declaration.name == "_c_My_data_Anonymous_2");
        }

        {
            h::Union_declaration const& declaration = header_module.internal_declarations.union_declarations[0];
            CHECK(declaration.name == "_c_My_data_Anonymous_1");
        }
    }

    
    TEST_CASE("Global variables are imported correctly")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "global_constants";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
#define FLOAT(c) c ## f
#define MY_FLOAT FLOAT(3.5);

const auto my_global_0 = MY_FLOAT;
float my_global_1 = 0.0f;

typedef int Sint32;
Sint32 my_global_2 = 0;
)";

        std::filesystem::path const header_file_path = root_directory_path / "My_data.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.My_data", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == header_file_path);

        {
            h::Global_variable_declaration const& declaration = header_module.export_declarations.global_variable_declarations[0];
            CHECK(declaration.name == "my_global_0");
            CHECK(declaration.name == declaration.unique_name.value());
            CHECK(declaration.global_type == h::Global_variable_type::Constant);
            
            CHECK(declaration.type == h::create_fundamental_type_type_reference(h::Fundamental_type::Float32));
            REQUIRE(declaration.type.has_value());
            CHECK(declaration.initial_value == h::create_statement({ h::create_constant_expression(*declaration.type, "3.500000") }));

            CHECK(*declaration.source_location == h::create_source_range_location(header_file_path, 5, 12, 5, 13));
        }

        {
            h::Global_variable_declaration const& declaration = header_module.export_declarations.global_variable_declarations[1];
            CHECK(declaration.name == "my_global_1");
            CHECK(declaration.name == declaration.unique_name.value());
            CHECK(declaration.global_type == h::Global_variable_type::Mutable);
            
            CHECK(declaration.type == h::create_fundamental_type_type_reference(h::Fundamental_type::Float32));
            REQUIRE(declaration.type.has_value());
            CHECK(declaration.initial_value == h::create_statement({ h::create_constant_expression(*declaration.type, "0.000000") }));

            CHECK(*declaration.source_location == h::create_source_range_location(header_file_path, 6, 7, 6, 8));
        }

        {
            h::Global_variable_declaration const& declaration = header_module.export_declarations.global_variable_declarations[2];
            CHECK(declaration.name == "my_global_2");
            CHECK(declaration.name == declaration.unique_name.value());
            CHECK(declaration.global_type == h::Global_variable_type::Mutable);
            
            CHECK(declaration.type == create_custom_type_reference("c.My_data", "Sint32"));
            REQUIRE(declaration.type.has_value());
            CHECK(declaration.initial_value == h::create_statement({ h::create_constant_expression(h::create_custom_type_reference("c.My_data", "Sint32"), "0") }));

            CHECK(*declaration.source_location == h::create_source_range_location(header_file_path, 9, 8, 9, 9));
        }
    }

    TEST_CASE("Macros constants are imported as global variable macros")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "macro_constants";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
#define MY_INT 10
#define NOT_CONSTANT struct
#define MY_FLOAT 3.5f
#define MY_DOUBLE 3.5
#define MY_STRING "a string"

#define A_UINT64(c) c ## ULL
#define MY_UINT64 A_UINT64(20)

)";

        std::filesystem::path const header_file_path = root_directory_path / "My_data.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.My_data", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == header_file_path);

        {
            h::Global_variable_declaration const& declaration = header_module.export_declarations.global_variable_declarations[0];
            CHECK(declaration.name == "MY_INT");
            CHECK(declaration.name == declaration.unique_name.value());
            CHECK(declaration.global_type == h::Global_variable_type::Macro);
            
            REQUIRE(declaration.type.has_value());
            CHECK(*declaration.type == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));
            CHECK(declaration.initial_value == h::create_statement({ h::create_constant_expression(*declaration.type, "10") }));

            CHECK(*declaration.source_location == h::create_source_range_location(header_file_path, 2, 9, 2, 10));
        }

        {
            h::Global_variable_declaration const& declaration = header_module.export_declarations.global_variable_declarations[1];
            CHECK(declaration.name == "MY_FLOAT");
            CHECK(declaration.name == declaration.unique_name.value());
            CHECK(declaration.global_type == h::Global_variable_type::Macro);
            
            REQUIRE(declaration.type.has_value());
            CHECK(*declaration.type == h::create_fundamental_type_type_reference(h::Fundamental_type::Float32));
            CHECK(declaration.initial_value == h::create_statement({ h::create_constant_expression(*declaration.type, "3.500000") }));

            CHECK(*declaration.source_location == h::create_source_range_location(header_file_path, 4, 9, 4, 10));
        }

        {
            h::Global_variable_declaration const& declaration = header_module.export_declarations.global_variable_declarations[2];
            CHECK(declaration.name == "MY_DOUBLE");
            CHECK(declaration.name == declaration.unique_name.value());
            CHECK(declaration.global_type == h::Global_variable_type::Macro);
            
            REQUIRE(declaration.type.has_value());
            CHECK(*declaration.type == h::create_fundamental_type_type_reference(h::Fundamental_type::Float64));
            CHECK(declaration.initial_value == h::create_statement({ h::create_constant_expression(*declaration.type, "3.500000") }));

            CHECK(*declaration.source_location == h::create_source_range_location(header_file_path, 5, 9, 5, 10));
        }

        {
            h::Global_variable_declaration const& declaration = header_module.export_declarations.global_variable_declarations[3];
            CHECK(declaration.name == "MY_STRING");
            CHECK(declaration.name == declaration.unique_name.value());
            CHECK(declaration.global_type == h::Global_variable_type::Macro);
            
            REQUIRE(declaration.type.has_value());
            CHECK(*declaration.type == h::create_c_string_type_reference(true));
            CHECK(declaration.initial_value == h::create_statement({ h::create_constant_expression(*declaration.type, "a string") }));

            CHECK(*declaration.source_location == h::create_source_range_location(header_file_path, 6, 9, 6, 10));
        }

        {
            h::Global_variable_declaration const& declaration = header_module.export_declarations.global_variable_declarations[4];
            CHECK(declaration.name == "MY_UINT64");
            CHECK(declaration.name == declaration.unique_name.value());
            CHECK(declaration.global_type == h::Global_variable_type::Macro);
            
            REQUIRE(declaration.type.has_value());
            CHECK(*declaration.type == h::create_fundamental_type_type_reference(h::Fundamental_type::C_ulonglong));
            CHECK(declaration.initial_value == h::create_statement({ h::create_constant_expression(*declaration.type, "20") }));

            CHECK(*declaration.source_location == h::create_source_range_location(header_file_path, 9, 9, 9, 10));
        }
    }

    TEST_CASE("Static global constants are imported as global variable macros")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "static_constants";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
static const int MY_VAR = 1;
const int my_global = 2;
)";

        std::filesystem::path const header_file_path = root_directory_path / "My_data.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.My_data", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == header_file_path);

        {
            h::Global_variable_declaration const& declaration = header_module.export_declarations.global_variable_declarations[0];
            CHECK(declaration.name == "MY_VAR");
            CHECK(declaration.name == declaration.unique_name.value());
            CHECK(declaration.global_type == h::Global_variable_type::Macro);
            
            REQUIRE(declaration.type.has_value());
            CHECK(*declaration.type == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));
            CHECK(declaration.initial_value == h::create_statement({ h::create_constant_expression(*declaration.type, "1") }));

            CHECK(*declaration.source_location == h::create_source_range_location(header_file_path, 2, 18, 2, 19));
        }

        {
            h::Global_variable_declaration const& declaration = header_module.export_declarations.global_variable_declarations[1];
            CHECK(declaration.name == "my_global");
            CHECK(declaration.name == declaration.unique_name.value());
            CHECK(declaration.global_type == h::Global_variable_type::Constant);
            
            REQUIRE(declaration.type.has_value());
            CHECK(*declaration.type == h::create_fundamental_type_type_reference(h::Fundamental_type::C_int));
            CHECK(declaration.initial_value == h::create_statement({ h::create_constant_expression(*declaration.type, "2") }));

            CHECK(*declaration.source_location == h::create_source_range_location(header_file_path, 3, 11, 3, 12));
        }
    }

    TEST_CASE("Function comments are imported")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "function comments";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
/**
 * Adds the two given values and returns
 * the result.
 *
 * \param first the first parameter.
 * \param second the second parameter.
 * \return the result of adding the first
 * and second parameters.
 *
 * Another line.
 *
 * Another comment.
 */
int add(int first, int second)
{
    return first + second;
}
)";

        std::filesystem::path const header_file_path = root_directory_path / "My_data.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.comments", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == header_file_path);

        {
            h::Function_declaration const& declaration = header_module.export_declarations.function_declarations[0];
            CHECK(declaration.name == "add");
            CHECK(declaration.name == declaration.unique_name.value());
            
            CHECK(declaration.comment.has_value());
            if (declaration.comment.has_value())
            {
                std::pmr::string const expected_comment = R"(Adds the two given values and returns the result.

Another line.

Another comment.

@input_parameter first: the first parameter.
@input_parameter second: the second parameter.
@output_parameter result: the result of adding the first and second parameters.
)";

                CHECK(*declaration.comment == expected_comment);
            }
        }
    }

    TEST_CASE("Declarations that match public prefix will be made public, otherwise they are made private")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "public_prefix";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
struct PRE_Vector2i
{
    int x;
    int y;
};

struct MyVector2i
{
    int x;
    int y;
};
)";

        std::filesystem::path const header_file_path = root_directory_path / "vector2i.h";
        h::common::write_to_file(header_file_path, header_content);

        std::array<std::pmr::string, 1> const public_prefixes
        {
            "PRE_",
        };
        h::c::Options const options
        {
            .public_prefixes = public_prefixes,
        };
        std::optional<h::Module> const header_module_optional = h::c::import_header("c.vector2i", header_file_path, options);
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        REQUIRE(header_module.export_declarations.struct_declarations.size() == 1);
        {
            h::Struct_declaration const& declaration = header_module.export_declarations.struct_declarations[0];
            CHECK(declaration.name == "PRE_Vector2i");
        }

        REQUIRE(header_module.internal_declarations.struct_declarations.size() == 1);
        {
            h::Struct_declaration const& declaration = header_module.internal_declarations.struct_declarations[0];
            CHECK(declaration.name == "MyVector2i");
        }
    }

    TEST_CASE("If a declaration name matches a remove prefix, then the prefix is removed")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "remove_prefix";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
struct PRE_Vector2i
{
    int x;
    int y;
};

struct MyNestedVector
{
    PRE_Vector2i a;
    PRE_Vector2i b;
};
)";

        std::filesystem::path const header_file_path = root_directory_path / "vector2i.h";
        h::common::write_to_file(header_file_path, header_content);

        std::array<std::pmr::string, 1> const remove_prefixes
        {
            "PRE_",
        };
        h::c::Options const options
        {
            .remove_prefixes = remove_prefixes,
        };
        std::optional<h::Module> const header_module_optional = h::c::import_header("c.vector2i", header_file_path, options);
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        REQUIRE(header_module.export_declarations.struct_declarations.size() == 2);
        
        {
            h::Struct_declaration const& declaration = header_module.export_declarations.struct_declarations[0];
            CHECK(declaration.name == "Vector2i");
            CHECK(*declaration.unique_name == "PRE_Vector2i");
        }

        {
            h::Struct_declaration const& declaration = header_module.export_declarations.struct_declarations[1];
            CHECK(declaration.name == "MyNestedVector");
            CHECK(*declaration.unique_name == "MyNestedVector");

            CHECK(declaration.member_types[0] == create_custom_type_reference("c.vector2i", "Vector2i"));
            CHECK(declaration.member_bit_fields[0].has_value() == false);
            CHECK(declaration.member_types[1] == create_custom_type_reference("c.vector2i", "Vector2i"));
            CHECK(declaration.member_bit_fields[1].has_value() == false);
        }
    }

    TEST_CASE("Imports function pointer types")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "function_pointer_type";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
typedef int(*function_pointer_type)(int a, int b);
)";

        std::filesystem::path const header_file_path = root_directory_path / "function_pointer_type.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.function_pointer_type", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        {
            h::Alias_type_declaration const& declaration = header_module.export_declarations.alias_type_declarations[0];
            CHECK(declaration.name == "function_pointer_type");

            REQUIRE(declaration.type.size() == 1);

            h::Type_reference const c_int_type = h::create_fundamental_type_type_reference(h::Fundamental_type::C_int);

            h::Function_type const expected_function_type
            {
                .input_parameter_types = {c_int_type, c_int_type},
                .output_parameter_types = {c_int_type},
                .is_variadic = false,
            };
            
            h::Type_reference const expected_function_pointer_type = h::create_function_type_type_reference(
                expected_function_type,
                {"a", "b"},
                {"result"}
            );

            CHECK(declaration.type[0] == expected_function_pointer_type);
        }
    }

    TEST_CASE("Handles bit fields")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "bit_fields";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
struct My_data
{
    unsigned int a : 24;
    unsigned int b : 8;
    unsigned int c : 1;
    unsigned int d : 20;
    unsigned int e : 11;
};
)";

        std::filesystem::path const header_file_path = root_directory_path / "My_data.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.My_data", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == header_file_path);

        {
            h::Struct_declaration const& declaration = header_module.export_declarations.struct_declarations[0];
            CHECK(declaration.name == "My_data");

            REQUIRE(declaration.member_names.size() == 5);
            REQUIRE(declaration.member_types.size() == 5);
            REQUIRE(declaration.member_bit_fields.size() == 5);

            CHECK(declaration.member_names[0] == "a");
            CHECK(declaration.member_types[0] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_uint));
            CHECK(declaration.member_bit_fields[0] == 24);

            CHECK(declaration.member_names[1] == "b");
            CHECK(declaration.member_types[1] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_uint));
            CHECK(declaration.member_bit_fields[1] == 8);

            CHECK(declaration.member_names[2] == "c");
            CHECK(declaration.member_types[2] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_uint));
            CHECK(declaration.member_bit_fields[2] == 1);

            CHECK(declaration.member_names[3] == "d");
            CHECK(declaration.member_types[3] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_uint));
            CHECK(declaration.member_bit_fields[3] == 20);

            CHECK(declaration.member_names[4] == "e");
            CHECK(declaration.member_types[4] == h::create_fundamental_type_type_reference(h::Fundamental_type::C_uint));
            CHECK(declaration.member_bit_fields[4] == 11);
        }
    }


    TEST_CASE("Handles opaque handles")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "opaque_handles";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
typedef struct My_opaque_type_t* My_opaque_type;

typedef struct My_non_opaque_type {
} My_non_opaque_type;

typedef const My_non_opaque_type* My_non_opaque_type_alias;

#define DEFINE_HANDLE(object) typedef struct object##_t* object;
DEFINE_HANDLE(My_macro_type)
)";

        std::filesystem::path const header_file_path = root_directory_path / "My_data.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.My_data", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == header_file_path);

        CHECK(header_module.export_declarations.struct_declarations.size() == 1);
        CHECK(header_module.internal_declarations.struct_declarations.empty());

        REQUIRE(header_module.export_declarations.alias_type_declarations.size() == 3);

        REQUIRE(header_module.export_declarations.forward_declarations.size() == 2);

        {
            h::Alias_type_declaration const& declaration = header_module.export_declarations.alias_type_declarations[0];
            CHECK(declaration.name == "My_opaque_type");

            REQUIRE(declaration.type.size() == 1);
            CHECK(declaration.type[0] == h::create_pointer_type_type_reference({h::create_custom_type_reference("c.My_data", "My_opaque_type_t")}, true));

            CHECK(declaration.source_location == h::create_source_range_location(header_file_path, 2, 34, 2, 35));
        }

        {
            h::Alias_type_declaration const& declaration = header_module.export_declarations.alias_type_declarations[1];
            CHECK(declaration.name == "My_non_opaque_type_alias");

            REQUIRE(declaration.type.size() == 1);
            CHECK(declaration.type[0] == h::create_pointer_type_type_reference({h::create_custom_type_reference("c.My_data", "My_non_opaque_type")}, false));

            CHECK(declaration.source_location == h::create_source_range_location(header_file_path, 7, 35, 7, 36));
        }

        {
            h::Alias_type_declaration const& declaration = header_module.export_declarations.alias_type_declarations[2];
            CHECK(declaration.name == "My_macro_type");

            REQUIRE(declaration.type.size() == 1);
            CHECK(declaration.type[0] == h::create_pointer_type_type_reference({h::create_custom_type_reference("c.My_data", "My_macro_type_t")}, true));

            CHECK(declaration.source_location == h::create_source_range_location(header_file_path, 10, 15, 10, 16));
        }

        {
            h::Forward_declaration const& declaration = header_module.export_declarations.forward_declarations[0];
            CHECK(declaration.name == "My_opaque_type_t");
            CHECK(declaration.unique_name == "My_opaque_type_t");

            CHECK(declaration.source_location == h::create_source_range_location(header_file_path, 2, 16, 2, 17));
        }

        {
            h::Forward_declaration const& declaration = header_module.export_declarations.forward_declarations[1];
            CHECK(declaration.name == "My_macro_type_t");
            CHECK(declaration.unique_name == "My_macro_type_t");

            CHECK(declaration.source_location == h::create_source_range_location(header_file_path, 10, 1, 10, 2));
        }
    }

    TEST_CASE("Do not add redundant forward declarations")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "redundant_forward_declarations";
        std::filesystem::create_directories(root_directory_path);
                
        
        std::string const dependency_content = R"(
typedef struct My_struct My_type;

struct My_struct {
    int a;
};
)";

        std::filesystem::path const dependency_file_path = root_directory_path / "Dependency.h";
        h::common::write_to_file(dependency_file_path, dependency_content);

        std::string const header_content = R"(
#include "Dependency.h"

struct My_struct create_my_struct();
)";

        std::filesystem::path const header_file_path = root_directory_path / "My_data.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.My_data", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == header_file_path);

        REQUIRE(header_module.export_declarations.alias_type_declarations.size() == 1);
        REQUIRE(header_module.export_declarations.forward_declarations.size() == 0);
        REQUIRE(header_module.export_declarations.struct_declarations.size() == 1);
    }


    TEST_CASE("Include debug information of function declarations")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "debug_information_functions";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
struct Vector2i
{
    int x;
    int y;
};

Vector2i add(Vector2i lhs, Vector2i rhs);
)";

        std::filesystem::path const header_file_path = root_directory_path / "vector2i.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.vector2i", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == header_file_path);

        {
            h::Function_declaration const& declaration = header_module.export_declarations.function_declarations[0];
            CHECK(declaration.name == "add");

            CHECK(declaration.source_location == h::create_source_range_location(header_file_path, 8, 10, 8, 11));

            std::pmr::vector<h::Source_position> expected_input_parameter_source_positions
            {
                {.line = 8, .column = 23},
                {.line = 8, .column = 37},
            };

            CHECK(declaration.input_parameter_source_positions == expected_input_parameter_source_positions);

            std::pmr::vector<h::Source_position> expected_output_parameter_source_positions
            {
                {.line = 8, .column = 1},
            };

            CHECK(declaration.output_parameter_source_positions == expected_output_parameter_source_positions);
        }
    }

    TEST_CASE("Include debug information of structs")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "debug_information_structs";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
struct Vector2i
{
    int x;
    int y;
};

Vector2i add(Vector2i lhs, Vector2i rhs);
)";

        std::filesystem::path const header_file_path = root_directory_path / "vector2i.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.vector2i", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == header_file_path);

        {
            h::Struct_declaration const& declaration = header_module.export_declarations.struct_declarations[0];
            CHECK(declaration.name == "Vector2i");

            CHECK(declaration.source_location == h::create_source_range_location(header_file_path, 2, 8, 2, 9));

            std::pmr::vector<h::Source_position> expected_member_source_positions
            {
                {.line = 4, .column = 9},
                {.line = 5, .column = 9},
            };

            CHECK(declaration.member_source_positions == expected_member_source_positions);
        }
    }

    TEST_CASE("Include debug information of unions")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "debug_information_unions";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
union Value
{
    int a;
    float b; 
};
)";

        std::filesystem::path const header_file_path = root_directory_path / "value.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.value", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == header_file_path);

        {
            h::Union_declaration const& declaration = header_module.export_declarations.union_declarations[0];
            CHECK(declaration.name == "Value");

            CHECK(declaration.source_location == h::create_source_range_location(header_file_path, 2, 7, 2, 8));

            std::pmr::vector<h::Source_position> expected_member_source_positions
            {
                {.line = 4, .column = 9},
                {.line = 5, .column = 11},
            };

            CHECK(declaration.member_source_positions == expected_member_source_positions);
        }
    }

    TEST_CASE("Include debug information of enums")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "debug_information_enums";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
enum My_enum
{
    A = 0,
    B,
};
)";

        std::filesystem::path const header_file_path = root_directory_path / "my_enum.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.my_enum", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == header_file_path);

        {
            h::Enum_declaration const& declaration = header_module.export_declarations.enum_declarations[0];
            CHECK(declaration.name == "My_enum");

            CHECK(declaration.source_location == h::create_source_range_location(header_file_path, 2, 6, 2, 7));
        }
    }

    TEST_CASE("Include debug information of alias")
    {
        std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_header_importer" / "debug_information_alias";
        std::filesystem::create_directories(root_directory_path);

        std::string const header_content = R"(
typedef int My_int;
typedef My_int My_alias;
)";

        std::filesystem::path const header_file_path = root_directory_path / "alias.h";
        h::common::write_to_file(header_file_path, header_content);

        std::optional<h::Module> const header_module_optional = h::c::import_header("c.alias", header_file_path, {});
        REQUIRE(header_module_optional.has_value());
        h::Module const& header_module = header_module_optional.value();

        CHECK(header_module.source_file_path == header_file_path);

        {
            h::Alias_type_declaration const& declaration = header_module.export_declarations.alias_type_declarations[0];
            CHECK(declaration.name == "My_int");

            CHECK(declaration.source_location == h::create_source_range_location(header_file_path, 2, 13, 2, 14));
        }

        {
            h::Alias_type_declaration const& declaration = header_module.export_declarations.alias_type_declarations[1];
            CHECK(declaration.name == "My_alias");

            CHECK(declaration.source_location == h::create_source_range_location(header_file_path, 3, 16, 3, 17));
        }
    }
}
