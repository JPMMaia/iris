#define CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS
#include <catch2/catch_all.hpp>

#include <iostream>
#include <memory_resource>
#include <string>
#include <unordered_set>

import iris.tools.code_generator;

namespace iris::tools::code_generator
{
    TEST_CASE("Print generate_read_enum_json_code")
    {
        Enum const operation
        {
            .name = "Binary_expression::Operation",
            .values = {
                "Add",
                "Subtract",
                "Multiply"
            }
        };

        std::pmr::string const code = generate_read_enum_json_code(operation, 0);
        std::cout << code << std::endl;
    }

    TEST_CASE("Print generate_write_enum_json_code")
    {
        Enum const operation
        {
            .name = "Binary_expression::Operation",
            .values = {
                "Add",
                "Subtract",
                "Multiply"
            }
        };

        std::pmr::string const code = generate_write_enum_json_code(operation, 0);
        std::cout << code << std::endl;
    }

    TEST_CASE("Print generate_read_struct_json_code for Variable_expression")
    {
        std::pmr::unordered_map<std::pmr::string, Enum> const enum_types
        {
            std::make_pair(
                "Variable_expression::Type",
                Enum
                {
                    .name = "Variable_expression::Type",
                    .values = {}
                }
            )
        };

        std::pmr::unordered_map<std::pmr::string, Struct> const struct_types;

        Struct const struct_type
        {
            .name = "Variable_expression",
            .members = {
                Member {
                    .type = Type{.name = "Variable_expression::Type"},
                    .name = "type"
                },
                Member {
                    .type = Type{.name = "std::uint64_t"},
                    .name = "id"
                }
            }
        };

        std::pmr::string const code = generate_read_struct_json_code(struct_type, enum_types, struct_types, 0);
        std::cout << code << std::endl;
    }

    TEST_CASE("Print generate_read_struct_json_code for Binary_expression")
    {
        std::pmr::unordered_map<std::pmr::string, Enum> const enum_types
        {
            std::make_pair(
                "Binary_expression::Operation",
                Enum
                {
                    .name = "Binary_expression::Operation",
                    .values = {}
                }
            )
        };

        std::pmr::unordered_map<std::pmr::string, Struct> const struct_types
        {
            std::make_pair(
                "Variable_expression",
                Struct
                {
                    .name = "Variable_expression",
                    .members = {}
                }
            )
        };

        Struct const struct_type
        {
            .name = "Binary_expression",
            .members = {
                Member {
                    .type = Type{.name = "Variable_expression"},
                    .name = "left_hand_side"
                },
                Member {
                    .type = Type{.name = "Variable_expression"},
                    .name = "right_hand_side"
                },
                Member {
                    .type = Type{.name = "Binary_expression::Operation"},
                    .name = "operation"
                }
            }
        };

        std::pmr::string const code = generate_read_struct_json_code(struct_type, enum_types, struct_types, 0);
        std::cout << code << std::endl;
    }

    TEST_CASE("Parse enum")
    {
        std::stringstream string_stream;
        string_stream << "enum Foo\n";
        string_stream << "{\n";
        string_stream << "    value_0,\n";
        string_stream << "    value_1,\n";
        string_stream << "    value_2\n";
        string_stream << "};\n";

        File_types const file_types = identify_file_types(string_stream);
        REQUIRE(file_types.enums.size() == 1);

        Enum const enum_type = file_types.enums[0];
        CHECK(enum_type.name == "Foo");
        REQUIRE(enum_type.values.size() == 3);
        CHECK(enum_type.values[0] == "value_0");
        CHECK(enum_type.values[1] == "value_1");
        CHECK(enum_type.values[2] == "value_2");
    }

    TEST_CASE("Parse enum class")
    {
        std::stringstream string_stream;
        string_stream << "enum class Foo\n";
        string_stream << "{\n";
        string_stream << "    value_0,\n";
        string_stream << "    value_1,\n";
        string_stream << "    value_2\n";
        string_stream << "};\n";

        File_types const file_types = identify_file_types(string_stream);
        REQUIRE(file_types.enums.size() == 1);

        Enum const enum_type = file_types.enums[0];
        CHECK(enum_type.name == "Foo");
        REQUIRE(enum_type.values.size() == 3);
        CHECK(enum_type.values[0] == "value_0");
        CHECK(enum_type.values[1] == "value_1");
        CHECK(enum_type.values[2] == "value_2");
    }

    TEST_CASE("Parse enum class with initializer")
    {
        std::stringstream string_stream;
        string_stream << "enum class Foo\n";
        string_stream << "{\n";
        string_stream << "    value_0 = 0,\n";
        string_stream << "    value_1 = 1,\n";
        string_stream << "    value_2 = 2\n";
        string_stream << "};\n";

        File_types const file_types = identify_file_types(string_stream);
        REQUIRE(file_types.enums.size() == 1);

        Enum const enum_type = file_types.enums[0];
        CHECK(enum_type.name == "Foo");
        REQUIRE(enum_type.values.size() == 3);
        CHECK(enum_type.values[0] == "value_0");
        CHECK(enum_type.values[1] == "value_1");
        CHECK(enum_type.values[2] == "value_2");
    }

    TEST_CASE("Parse struct")
    {
        std::stringstream string_stream;
        string_stream << "struct Foo\n";
        string_stream << "{\n";
        string_stream << "    std::string member_0; // A comment\n";
        string_stream << "    std::uint64_t member_1;\n";
        string_stream << "    std::uint64_t member_2;\n";
        string_stream << "};\n";

        File_types const file_types = identify_file_types(string_stream);
        REQUIRE(file_types.structs.size() == 1);

        Struct const struct_type = file_types.structs[0];
        CHECK(struct_type.name == "Foo");
        REQUIRE(struct_type.members.size() == 3);
        CHECK(struct_type.members[0].type.name == "std::string");
        CHECK(struct_type.members[0].name == "member_0");
        CHECK(struct_type.members[1].type.name == "std::uint64_t");
        CHECK(struct_type.members[1].name == "member_1");
        CHECK(struct_type.members[2].type.name == "std::uint64_t");
        CHECK(struct_type.members[2].name == "member_2");
    }

    TEST_CASE("Parse struct with using type")
    {
        std::stringstream string_stream;
        string_stream << "struct Foo\n";
        string_stream << "{\n";
        string_stream << "    using Data_type = std::variant<\nint,\nfloat\n>;\n\n";
        string_stream << "    Data_type member_0;\n";
        string_stream << "};\n";

        File_types const file_types = identify_file_types(string_stream);
        REQUIRE(file_types.structs.size() == 1);

        Struct const struct_type = file_types.structs[0];
        CHECK(struct_type.name == "Foo");
        REQUIRE(struct_type.members.size() == 1);
        CHECK(struct_type.members[0].type.name == "std::variant<int,float>");
        CHECK(struct_type.members[0].name == "member_0");
    }

    TEST_CASE("Parse struct with default initializer")
    {
        std::stringstream string_stream;
        string_stream << "struct Foo\n";
        string_stream << "{\n";
        string_stream << "    std::string member_0 = {};\n";
        string_stream << "    std::uint64_t member_1 = 64;\n";
        string_stream << "    std::uint64_t member_2 = 32;\n";
        string_stream << "};\n";

        File_types const file_types = identify_file_types(string_stream);
        REQUIRE(file_types.structs.size() == 1);

        Struct const struct_type = file_types.structs[0];
        CHECK(struct_type.name == "Foo");
        REQUIRE(struct_type.members.size() == 3);
        CHECK(struct_type.members[0].type.name == "std::string");
        CHECK(struct_type.members[0].name == "member_0");
        CHECK(struct_type.members[1].type.name == "std::uint64_t");
        CHECK(struct_type.members[1].name == "member_1");
        CHECK(struct_type.members[2].type.name == "std::uint64_t");
        CHECK(struct_type.members[2].name == "member_2");
    }

    TEST_CASE("Parse struct with auto operator<=>")
    {
        std::stringstream string_stream;
        string_stream << "struct Foo\n";
        string_stream << "{\n";
        string_stream << "    std::string member_0 = {};\n";
        string_stream << "    std::uint64_t member_1;\n";
        string_stream << "    std::uint64_t member_2;\n";
        string_stream << "    \n";
        string_stream << "    auto operator<=>(Foo const&, Foo const&) = default;\n";
        string_stream << "    friend auto operator<=>(Foo const&, Foo const&) = default;\n";
        string_stream << "};\n";

        File_types const file_types = identify_file_types(string_stream);
        REQUIRE(file_types.structs.size() == 1);

        Struct const struct_type = file_types.structs[0];
        CHECK(struct_type.name == "Foo");
        REQUIRE(struct_type.members.size() == 3);
        CHECK(struct_type.members[0].type.name == "std::string");
        CHECK(struct_type.members[0].name == "member_0");
        CHECK(struct_type.members[1].type.name == "std::uint64_t");
        CHECK(struct_type.members[1].name == "member_1");
        CHECK(struct_type.members[2].type.name == "std::uint64_t");
        CHECK(struct_type.members[2].name == "member_2");
    }

    TEST_CASE("Parse struct with equality operator")
    {
        std::stringstream string_stream;
        string_stream << "struct Foo\n";
        string_stream << "{\n";
        string_stream << "    std::string member_0 = {};\n";
        string_stream << "    std::uint64_t member_1;\n";
        string_stream << "    std::uint64_t member_2;\n";
        string_stream << "    \n";
        string_stream << "    friend bool operator==(Foo const&, Foo const&) = default;\n";
        string_stream << "};\n";

        File_types const file_types = identify_file_types(string_stream);
        REQUIRE(file_types.structs.size() == 1);

        Struct const struct_type = file_types.structs[0];
        CHECK(struct_type.name == "Foo");
        REQUIRE(struct_type.members.size() == 3);
        CHECK(struct_type.members[0].type.name == "std::string");
        CHECK(struct_type.members[0].name == "member_0");
        CHECK(struct_type.members[1].type.name == "std::uint64_t");
        CHECK(struct_type.members[1].name == "member_1");
        CHECK(struct_type.members[2].type.name == "std::uint64_t");
        CHECK(struct_type.members[2].name == "member_2");
    }
}
