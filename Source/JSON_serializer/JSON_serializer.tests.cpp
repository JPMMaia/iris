#include <cstddef>
#include <memory_resource>
#include <string>
#include <variant>

#include <nlohmann/json.hpp>

import h.core;
import h.core.types;
import h.json_serializer;
import h.json_serializer.operators;

using h::json::operators::operator<<;

#include <catch2/catch_all.hpp>

namespace h
{
    template <typename Variant, typename T>
    struct index_of;

    template <typename T, typename ...Ts>
    struct index_of<std::variant<Ts...>, T> {
        static constexpr std::size_t value = [] {
            std::size_t i = 0;
            for (bool match : {std::is_same_v<T, Ts>...})
            {
                if (match) return i;
                ++i;
            }
            return std::size_t(-1);
        }();
    };

    template <typename Variant, typename T>
    inline constexpr std::size_t index_of_v = index_of<Variant, T>::value;

    TEST_CASE("Read Language_version")
    {
        std::pmr::string const json_data = R"JSON(
            {
                "major": 1,
                "minor": 2,
                "patch": 3
            }
        )JSON";

        Language_version const expected
        {
            .major = 1,
            .minor = 2,
            .patch = 3,
        };

        std::optional<Language_version> const output = h::json::read<Language_version>(json_data);

        REQUIRE(output.has_value());

        Language_version const& actual = output.value();
        CHECK(actual == expected);
    }

    TEST_CASE("Write Language_version")
    {
        Language_version const input
        {
            .major = 1,
            .minor = 2,
            .patch = 3,
        };

        std::pmr::string const expected = "{\"major\":1,\"minor\":2,\"patch\":3}";

        std::pmr::string const actual = h::json::write(input);

        CHECK(actual == expected);
    }

    TEST_CASE("Read Type_reference with Fundamental_type")
    {
        std::string const json_data = std::format(R"JSON(
            {{
                "data": {{
                    "index": {},
                    "value": "Byte"
                }}
            }}
)JSON",
            index_of_v<Type_reference::Data_type, Fundamental_type>
        );

        Type_reference const expected
        {
            .data = Fundamental_type::Byte
        };

        std::optional<Type_reference> const output = h::json::read<Type_reference>(json_data);

        REQUIRE(output.has_value());

        Type_reference const& actual = output.value();
        CHECK(actual == expected);
    }

    TEST_CASE("Write Type_reference with Fundamental_type")
    {
        Type_reference const input
        {
            .data = Fundamental_type::Byte
        };

        std::pmr::string const expected = std::pmr::string{std::format(
            "{{\"data\":{{\"index\":{},\"value\":\"Byte\"}}}}",
            index_of_v<Type_reference::Data_type, Fundamental_type>
        )};

        std::pmr::string const actual = h::json::write(input);

        CHECK(actual == expected);
    }

    TEST_CASE("Read Type_reference with Custom_type_reference")
    {
        std::string const json_data = std::format(R"JSON(
            {{
                "data": {{
                    "index": {},
                    "value": {{
                        "module_reference": {{
                            "name": "module_foo"
                        }},
                        "name": "custom_name"
                    }}
                }}
            }}
)JSON",
            index_of_v<Type_reference::Data_type, Custom_type_reference>
        );

        Type_reference const expected
        {
            .data = Custom_type_reference
            {
                .module_reference = Module_reference{
                    .name = "module_foo"
                },
                .name = "custom_name"
            }
        };

        std::optional<Type_reference> const output = h::json::read<Type_reference>(json_data);

        REQUIRE(output.has_value());

        Type_reference const& actual = output.value();
        CHECK(actual == expected);
    }

    TEST_CASE("Write Type_reference with Custom_type_reference")
    {
        Type_reference const input
        {
            .data = Custom_type_reference
            {
                .module_reference = Module_reference{
                    .name = "module_foo"
                },
                .name = "custom_name"
            }
        };

        std::pmr::string const expected = std::pmr::string{std::format(
            "{{\"data\":{{\"index\":{},\"value\":{{\"module_reference\":{{\"name\":\"module_foo\"}},\"name\":\"custom_name\"}}}}}}",
            index_of_v<Type_reference::Data_type, Custom_type_reference>
        )};

        std::pmr::string const actual = h::json::write(input);

        CHECK(actual == expected);
    }

    TEST_CASE("Read Assignment_expression with optional value")
    {
        std::pmr::string const json_data = R"JSON(
            {
                "left_hand_side": {
                    "expression_index": 1
                },
                "right_hand_side": {
                    "expression_index": 2
                },
                "additional_operation": "Add"
            }
        )JSON";

        Assignment_expression const expected
        {
            .left_hand_side = {.expression_index = 1},
            .right_hand_side = {.expression_index = 2},
            .additional_operation = Binary_operation::Add
        };

        std::optional<Assignment_expression> const output = h::json::read<Assignment_expression>(json_data);

        REQUIRE(output.has_value());

        Assignment_expression const& actual = output.value();
        CHECK(actual == expected);
    }

    TEST_CASE("Write Assignment_expression with optional value")
    {
        Assignment_expression const input
        {
            .left_hand_side = {.expression_index = 1},
            .right_hand_side = {.expression_index = 2},
            .additional_operation = Binary_operation::Add
        };

        std::pmr::string const expected = "{\"additional_operation\":\"Add\",\"left_hand_side\":{\"expression_index\":1},\"right_hand_side\":{\"expression_index\":2}}";

        std::pmr::string const actual = h::json::write(input);

        CHECK(actual == expected);
    }

    TEST_CASE("Read Assignment_expression without optional value")
    {
        std::pmr::string const json_data = R"JSON(
            {
                "left_hand_side": {
                    "expression_index": 1
                },
                "right_hand_side": {
                    "expression_index": 2
                }
            }
        )JSON";

        Assignment_expression const expected
        {
            .left_hand_side = {.expression_index = 1},
            .right_hand_side = {.expression_index = 2},
            .additional_operation = std::nullopt
        };

        std::optional<Assignment_expression> const output = h::json::read<Assignment_expression>(json_data);

        REQUIRE(output.has_value());

        Assignment_expression const& actual = output.value();
        CHECK(actual == expected);
    }

    TEST_CASE("Write Assignment_expression without optional value")
    {
        Assignment_expression const input
        {
            .left_hand_side = {.expression_index = 1},
            .right_hand_side = {.expression_index = 2},
            .additional_operation = std::nullopt
        };

        std::pmr::string const expected = "{\"left_hand_side\":{\"expression_index\":1},\"right_hand_side\":{\"expression_index\":2}}";

        std::pmr::string const actual = h::json::write(input);

        CHECK(actual == expected);
    }

    TEST_CASE("Read If_expression")
    {
        std::string const json_data = std::format(R"JSON(
            {{
                "series": {{
                    "size": 2,
                    "elements": [
                        {{
                            "condition": {{
                                "expressions": {{
                                    "size": 1,
                                    "elements": [
                                        {{
                                            "data": {{
                                                "index": {},
                                                "value": {{
                                                    "name": "some_boolean"
                                                }}
                                            }}
                                        }}
                                    ]
                                }}
                            }},
                            "then_statements": {{
                                "size": 1,
                                "elements": [
                                    {{
                                        "expressions": {{
                                            "size": 2,
                                            "elements": [
                                                {{
                                                    "data": {{
                                                        "index": {},
                                                        "value": {{
                                                            "expression": {{
                                                                "expression_index": 1
                                                            }}
                                                        }}
                                                    }}
                                                }},
                                                {{
                                                    "data": {{
                                                        "index": {},
                                                        "value": {{
                                                            "name": "value"
                                                        }}
                                                    }}
                                                }}
                                            ]
                                        }}
                                    }}
                                ]
                            }}
                        }},
                        {{
                            "then_statements": {{
                                "size": 1,
                                "elements": [
                                    {{
                                        "expressions": {{
                                            "size": 2,
                                            "elements": [
                                                {{
                                                    "data": {{
                                                        "index": {},
                                                        "value": {{
                                                            "expression": {{
                                                                "expression_index": 3
                                                            }}
                                                        }}
                                                    }}
                                                }},
                                                {{
                                                    "data": {{
                                                        "index": {},
                                                        "value": {{
                                                            "name": "value"
                                                        }}
                                                    }}
                                                }}
                                            ]
                                        }}
                                    }}
                                ]
                            }}
                        }}
                    ]
                }}
            }}
)JSON",
        index_of_v<Expression::Data_type, Variable_expression>,
        index_of_v<Expression::Data_type, Return_expression>,
        index_of_v<Expression::Data_type, Variable_expression>,
        index_of_v<Expression::Data_type, Return_expression>,
        index_of_v<Expression::Data_type, Variable_expression>
    );

        std::pmr::vector<Condition_statement_pair> expected_series
        {
            Condition_statement_pair
            {
                .condition = Statement
                {
                    .expressions = std::pmr::vector<Expression>
                    {
                        {
                            .data = Variable_expression
                            {
                                .name = "some_boolean"
                            }
                        }
                    }
                },
                .then_statements = {
                    Statement
                    {
                        .expressions = std::pmr::vector<Expression>
                        {
                            {
                                .data = Return_expression
                                {
                                    .expression = Expression_index
                                    {
                                        .expression_index = 1
                                    }
                                }
                            },
                            {
                                .data = Variable_expression
                                {
                                    .name = "value"
                                }
                            }
                        }
                    }
                }
            },
            Condition_statement_pair
            {
                .condition = std::nullopt,
                .then_statements = {
                    Statement
                    {
                        .expressions = std::pmr::vector<Expression>
                        {
                            {
                                .data = Return_expression
                                {
                                    .expression = Expression_index
                                    {
                                        .expression_index = 3
                                    }
                                }
                            },
                            {
                                .data = Variable_expression
                                {
                                    .name = "value"
                                }
                            }
                        }
                    }
                }
            }
        };

        If_expression const expected
        {
            .series = std::move(expected_series)
        };

        std::optional<If_expression> const output = h::json::read<If_expression>(json_data);

        REQUIRE(output.has_value());

        If_expression const& actual = output.value();
        CHECK(actual == expected);
    }

    TEST_CASE("Write If_expression")
    {
        std::pmr::vector<Condition_statement_pair> input_series
        {
            Condition_statement_pair
            {
                .condition = Statement
                {
                    .expressions = std::pmr::vector<Expression>
                    {
                        {
                            .data = Variable_expression
                            {
                                .name = "some_boolean",
                            }
                        }
                    }
                },
                .then_statements = {
                    Statement
                    {
                        .expressions = std::pmr::vector<Expression>
                        {
                            {
                                .data = Return_expression
                                {
                                    .expression = Expression_index
                                    {
                                        .expression_index = 1
                                    }
                                }
                            },
                            {
                                .data = Variable_expression
                                {
                                    .name = "value",
                                }
                            }
                        }
                    }
                }
            },
            Condition_statement_pair
            {
                .condition = std::nullopt,
                .then_statements = {
                    Statement
                    {
                        .expressions = std::pmr::vector<Expression>
                        {
                            {
                                .data = Return_expression
                                {
                                    .expression = Expression_index
                                    {
                                        .expression_index = 3
                                    }
                                }
                            },
                            {
                                .data = Variable_expression
                                {
                                    .name = "value",
                                }
                            }
                        }
                    }
                }
            }
        };

        If_expression const input
        {
            .series = std::move(input_series)
        };

        std::pmr::string const expected = std::pmr::string{std::format(
            "{{\"series\":{{\"elements\":[{{\"condition\":{{\"expressions\":{{\"elements\":[{{\"data\":{{\"index\":{},\"value\":{{\"name\":\"some_boolean\"}}}}}}],\"size\":1}}}},\"then_statements\":{{\"elements\":[{{\"expressions\":{{\"elements\":[{{\"data\":{{\"index\":{},\"value\":{{\"expression\":{{\"expression_index\":1}}}}}}}},{{\"data\":{{\"index\":{},\"value\":{{\"name\":\"value\"}}}}}}],\"size\":2}}}}],\"size\":1}}}},{{\"then_statements\":{{\"elements\":[{{\"expressions\":{{\"elements\":[{{\"data\":{{\"index\":{},\"value\":{{\"expression\":{{\"expression_index\":3}}}}}}}},{{\"data\":{{\"index\":{},\"value\":{{\"name\":\"value\"}}}}}}],\"size\":2}}}}],\"size\":1}}}}],\"size\":2}}}}",
            index_of_v<Expression::Data_type, Variable_expression>,
            index_of_v<Expression::Data_type, Return_expression>,
            index_of_v<Expression::Data_type, Variable_expression>,
            index_of_v<Expression::Data_type, Return_expression>,
            index_of_v<Expression::Data_type, Variable_expression>
        )};

        std::pmr::string const actual = h::json::write(input);

        CHECK(actual == expected);
    }

    TEST_CASE("Read Variable_expression")
    {
        std::pmr::string const json_data = R"JSON(
            {
                "name": "variable_name"
            }
            
        )JSON";

        Variable_expression const expected
        {
            .name = "variable_name"
        };

        std::optional<Variable_expression> const output = h::json::read<Variable_expression>(json_data);

        REQUIRE(output.has_value());

        Variable_expression const& actual = output.value();
        CHECK(actual == expected);
    }

    TEST_CASE("Read Binary_operation")
    {
        CHECK(h::json::read_enum<Binary_operation>("Add") == Binary_operation::Add);
        CHECK(h::json::read_enum<Binary_operation>("Subtract") == Binary_operation::Subtract);
        CHECK(h::json::read_enum<Binary_operation>("Multiply") == Binary_operation::Multiply);
        CHECK(h::json::read_enum<Binary_operation>("Divide") == Binary_operation::Divide);
        CHECK(h::json::read_enum<Binary_operation>("Less_than") == Binary_operation::Less_than);
    }

    TEST_CASE("Read Binary_expression")
    {
        std::pmr::string const json_data = R"JSON(
            {
                "left_hand_side": {
                    "expression_index": 2
                },
                "right_hand_side": {
                    "expression_index": 3
                },
                "operation": "Subtract"
            }
        )JSON";

        Binary_expression const expected
        {
            .left_hand_side = Expression_index
            {
                .expression_index = 2
            },
            .right_hand_side = Expression_index
            {
                .expression_index = 3
            },
            .operation = Binary_operation::Subtract
        };

        std::optional<Binary_expression> const output = h::json::read<Binary_expression>(json_data);

        REQUIRE(output.has_value());

        Binary_expression const& actual = output.value();
        CHECK(actual == expected);
    }

    TEST_CASE("Read Call_expression")
    {
        std::pmr::string const json_data = R"JSON(
            {
                "expression": {
                    "expression_index": 4
                },
                "arguments": {
                    "size": 2,
                    "elements": [
                        {
                            "expression_index": 3
                        },
                        {
                            "expression_index": 1
                        }
                    ]
                }
            }
        )JSON";

        std::pmr::vector<Expression_index> arguments
        {
            Expression_index
            {
                .expression_index = 3
            },
            Expression_index
            {
                .expression_index = 1
            },
        };

        Call_expression const expected
        {
            .expression = {
                .expression_index = 4,
            },
            .arguments = std::move(arguments)
        };

        std::optional<Call_expression> const output = h::json::read<Call_expression>(json_data);

        REQUIRE(output.has_value());

        Call_expression const& actual = output.value();
        CHECK(actual == expected);
    }

    h::Function_declaration create_expected_function_declaration()
    {
        std::pmr::vector<Type_reference> input_parameter_types
        {
            Type_reference{.data = Fundamental_type::Byte},
            Type_reference{.data = Fundamental_type::Byte},
        };

        std::pmr::vector<Type_reference> output_parameter_types
        {
            Type_reference{.data = Fundamental_type::Byte},
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

    TEST_CASE("Read Function_declaration")
    {
        std::string const json_data = std::format(R"JSON(
            {{
                "name": "Add",
                "type": {{
                    "input_parameter_types": {{
                        "size": 2,
                        "elements": [
                            {{
                                "data": {{
                                    "index": {},
                                    "value": "Byte"
                                }}
                            }},
                            {{
                                "data": {{
                                    "index": {},
                                    "value": "Byte"
                                }}
                            }}
                        ]
                    }},
                    "output_parameter_types": {{
                        "size": 1,
                        "elements": [
                            {{
                                "data": {{
                                    "index": {},
                                    "value": "Byte"
                                }}
                            }}
                        ]            
                    }},
                    "is_variadic": false
                }},
                "input_parameter_names": {{
                    "size": 2,
                    "elements": [
                        "lhs", "rhs"
                    ]
                }},
                "output_parameter_names": {{
                    "size": 1,
                    "elements": [
                        "sum"
                    ]
                }},
                "linkage": "External",
                "source_location": {{
                    "range": {{
                        "start": {{
                            "line": 2,
                            "column": 6
                        }},
                        "end": {{
                            "line": 4,
                            "column": 2
                        }}
                    }}
                }},
                "input_parameter_source_positions": {{
                    "size": 1,
                    "elements": [
                        {{
                            "line": 3,
                            "column": 22
                        }}
                    ]
                }},
                "output_parameter_source_positions": {{
                    "size": 1,
                    "elements": [
                        {{
                            "line": 3,
                            "column": 38
                        }}
                    ]
                }},
                "is_test": false,
                "preconditions": {{"size": 0, "elements": []}},
                "postconditions": {{"size": 0, "elements": []}}
            }}
)JSON",
            index_of_v<Type_reference::Data_type, Fundamental_type>,
            index_of_v<Type_reference::Data_type, Fundamental_type>,
            index_of_v<Type_reference::Data_type, Fundamental_type>
        );

        h::Function_declaration const expected = create_expected_function_declaration();

        std::optional<h::Function_declaration> const output = h::json::read<h::Function_declaration>(json_data);

        REQUIRE(output.has_value());

        h::Function_declaration const& actual = output.value();
        CHECK(actual == expected);
    }

    TEST_CASE("Write empty Function_declaration")
    {
        h::Function_declaration const input = {};

        std::pmr::string const expected = "{\"input_parameter_names\":{\"elements\":[],\"size\":0},\"is_test\":false,\"linkage\":\"External\",\"name\":\"\",\"output_parameter_names\":{\"elements\":[],\"size\":0},\"postconditions\":{\"elements\":[],\"size\":0},\"preconditions\":{\"elements\":[],\"size\":0},\"type\":{\"input_parameter_types\":{\"elements\":[],\"size\":0},\"is_variadic\":false,\"output_parameter_types\":{\"elements\":[],\"size\":0}}}";

        std::pmr::string const actual = h::json::write(input);

        CHECK(actual == expected);
    }

    h::Function_definition create_expected_function_definition()
    {
        std::pmr::vector<Expression> expressions
        {
            {
                Expression{
                    .data = Variable_expression{
                        .name = "lhs"
                    }
                },
                Expression{
                    .data = Variable_expression{
                        .name = "rhs"
                    }
                },
                Expression{
                    .data = Binary_expression{
                        .left_hand_side = Expression_index{
                            .expression_index = 0
                        },
                        .right_hand_side = Expression_index{
                            .expression_index = 1
                        },
                        .operation = Binary_operation::Add
                    }
                },
                Expression{
                    .data = Return_expression{
                        .expression = Expression_index{
                            .expression_index = 2
                        },
                    }
                }
            }
        };

        std::pmr::vector<Statement> statements
        {
            {
                Statement{
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

    TEST_CASE("Read Function_definition")
    {
        std::string const json_data = std::format(R"JSON(
        {{
            "name": "Add",
            "statements": {{
                "size": 1,
                "elements": [
                    {{
                        "expressions": {{
                            "size": 4,
                            "elements": 
                            [
                                {{
                                    "data": {{
                                        "index": {},
                                        "value": {{
                                            "name": "lhs"
                                        }}
                                    }}
                                }},
                                {{
                                    "data": {{
                                        "index": {},
                                        "value": {{
                                            "name": "rhs"
                                        }}
                                    }}
                                }},
                                {{
                                    "data": {{
                                        "index": {},
                                        "value": {{
                                            "left_hand_side": {{
                                                "expression_index": 0
                                            }},
                                            "right_hand_side": {{
                                                "expression_index": 1
                                            }},
                                            "operation": "Add"
                                        }}
                                    }}
                                }},
                                {{
                                    "data": {{
                                        "index": {},
                                        "value": {{
                                            "expression": {{
                                                "expression_index": 2
                                            }}
                                        }}
                                    }}
                                }}
                            ]
                        }}
                    }}
                ]
            }}
        }}
)JSON",
            index_of_v<Expression::Data_type, Variable_expression>,
            index_of_v<Expression::Data_type, Variable_expression>,
            index_of_v<Expression::Data_type, Binary_expression>,
            index_of_v<Expression::Data_type, Return_expression>
        );

        h::Function_definition const expected = create_expected_function_definition();

        std::optional<h::Function_definition> const output = h::json::read<h::Function_definition>(json_data);

        REQUIRE(output.has_value());

        h::Function_definition const& actual = output.value();
        CHECK(actual == expected);
    }

    h::Module create_expected_module()
    {
        Language_version const language_version
        {
            .major = 1,
            .minor = 2,
            .patch = 3
        };

        Module_dependencies dependencies
        {
            .alias_imports = {
                {
                    .module_name = "C.Standard_library",
                    .alias = "Cstl",
                    .usages = { "puts" }
                }
            }
        };

        Module_declarations export_declarations
        {
            .function_declarations = { create_expected_function_declaration() }
        };

        Module_definitions definitions
        {
            .function_definitions = { create_expected_function_definition() }
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

    TEST_CASE("Read Module")
    {
        std::string const json_data = std::format(R"JSON(
        {{
            "language_version": {{
                "major": 1,
                "minor": 2,
                "patch": 3
            }},
            "name": "module_name",
            "content_hash": 12089789297091071925,
            "dependencies": {{
                "alias_imports": {{
                    "size": 1,
                    "elements": [
                        {{
                            "module_name": "C.Standard_library",
                            "alias": "Cstl",
                            "usages": {{
                                "size": 1,
                                "elements": [
                                    "puts"
                                ]
                            }}
                        }}
                    ]
                }}
            }},
            "export_declarations": {{
                "function_declarations": {{
                    "size": 1,
                    "elements": [
                        {{
                            "name": "Add",
                            "type": {{
                                "input_parameter_types": {{
                                    "size": 2,
                                    "elements": [
                                        {{
                                            "data": {{
                                                "index": {},
                                                "value": "Byte"
                                            }}
                                        }},
                                        {{
                                            "data": {{
                                                "index": {},
                                                "value": "Byte"
                                            }}
                                        }}
                                    ]
                                }},
                                "output_parameter_types": {{
                                    "size": 1,
                                    "elements": [
                                        {{
                                            "data": {{
                                                "index": {},
                                                "value": "Byte"
                                            }}
                                        }}
                                    ]
                                }},
                                "is_variadic": false
                            }},
                            "input_parameter_names": {{
                                "size": 2,
                                "elements": [
                                    "lhs", "rhs"
                                ]
                            }},
                            "output_parameter_names": {{
                                "size": 1,
                                "elements": [
                                    "sum"
                                ]
                            }},
                            "linkage": "External",
                            "source_location": {{
                                "range": {{
                                    "start": {{
                                        "line": 2,
                                        "column": 6
                                    }},
                                    "end": {{
                                        "line": 4,
                                        "column": 2
                                    }}
                                }}
                            }},
                            "input_parameter_source_positions": {{
                                "size": 1,
                                "elements": [
                                    {{
                                        "line": 3,
                                        "column": 22
                                    }}
                                ]
                            }},
                            "output_parameter_source_positions": {{
                                "size": 1,
                                "elements": [
                                    {{
                                        "line": 3,
                                        "column": 38
                                    }}
                                ]
                            }},
                            "is_test": false
                        }}
                    ]
                }}
            }},
            "internal_declarations": {{
                "function_declarations": {{
                    "size": 0,
                    "elements": []
                }}
            }},
            "instanced_declarations": {{}},
            "definitions": {{
                "function_definitions": {{
                    "size": 1,
                    "elements": [
                        {{
                            "name": "Add",
                            "statements": {{
                                "size": 1,
                                "elements": [
                                    {{
                                        "expressions": {{
                                            "size": 4,
                                            "elements": 
                                            [
                                                {{
                                                    "data": {{
                                                        "index": {},
                                                        "value": {{
                                                            "name": "lhs"
                                                        }}
                                                    }}
                                                }},
                                                {{
                                                    "data": {{
                                                        "index": {},
                                                        "value": {{
                                                            "name": "rhs"
                                                        }}
                                                    }}
                                                }},
                                                {{
                                                    "data": {{
                                                        "index": {},
                                                        "value": {{
                                                            "left_hand_side": {{
                                                                "expression_index": 0
                                                            }},
                                                            "right_hand_side": {{
                                                                "expression_index": 1
                                                            }},
                                                            "operation": "Add"
                                                        }}
                                                    }}
                                                }},
                                                {{
                                                    "data": {{
                                                        "index": {},
                                                        "value": {{
                                                            "expression": {{
                                                                "expression_index": 2
                                                            }}
                                                        }}
                                                    }}
                                                }}
                                            ]
                                        }}
                                    }}
                                ]
                            }}
                        }}
                    ]
                }}
            }}
        }}
)JSON",
            index_of_v<Type_reference::Data_type, Fundamental_type>,
            index_of_v<Type_reference::Data_type, Fundamental_type>,
            index_of_v<Type_reference::Data_type, Fundamental_type>,
            index_of_v<Expression::Data_type, Variable_expression>,
            index_of_v<Expression::Data_type, Variable_expression>,
            index_of_v<Expression::Data_type, Binary_expression>,
            index_of_v<Expression::Data_type, Return_expression>
        );

        h::Module const expected = create_expected_module();

        std::optional<h::Module> const output = h::json::read<h::Module>(json_data);

        REQUIRE(output.has_value());

        h::Module const actual = output.value();
        CHECK(actual == expected);
    }

    h::Struct_declaration create_expected_struct_declaration()
    {
        h::Type_reference const uint32_type = create_integer_type_type_reference(32, false);

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
                    Expression{
                        .data = Constant_expression{
                            .type = uint32_type,
                            .data = "0"
                        }
                    }
                }
            },
            {
                .expressions = {
                    Expression{
                        .data = Constant_expression{
                            .type = uint32_type,
                            .data = "0"
                        }
                    }
                }
            },
            {
                .expressions = {
                    Expression{
                        .data = Constant_expression{
                            .type = uint32_type,
                            .data = "0"
                        }
                    }
                }
            }
        };
        return declaration;
    }

    TEST_CASE("Read Struct_declaration")
    {
        std::string const json_data = std::format(R"JSON(
            {{
                "name": "My_struct",
                "member_names": {{
                    "size": 3,
                    "elements": [
                        "first",
                        "second",
                        "third"
                    ]
                }},
                "member_types": {{
                    "size": 3,
                    "elements": [
                        {{
                            "data": {{
                                "index": {},
                                "value": {{
                                    "number_of_bits": 32,
                                    "is_signed": false
                                }}
                            }}
                        }},
                        {{
                            "data": {{
                                "index": {},
                                "value": {{
                                    "number_of_bits": 32,
                                    "is_signed": false
                                }}
                            }}
                        }},
                        {{
                            "data": {{
                                "index": {},
                                "value": {{
                                    "number_of_bits": 32,
                                    "is_signed": false
                                }}
                            }}
                        }}
                    ]
                }},
                "member_bit_fields": {{
                    "size": 3,
                    "elements": [
                        24,
                        8,
                        null
                    ]
                }},
                "member_default_values": {{
                    "size": 3,
                    "elements": [
                        {{
                            "expressions": {{
                                "size": 1,
                                "elements": [
                                    {{
                                        "data": {{
                                            "index": {},
                                            "value": {{
                                                "type": {{
                                                    "data": {{
                                                        "index": {},
                                                        "value": {{
                                                            "number_of_bits": 32,
                                                            "is_signed": false
                                                        }}
                                                    }}
                                                }},
                                                "data": "0"
                                            }}
                                        }}
                                    }}
                                ]
                            }}
                        }},
                        {{
                            "expressions": {{
                                "size": 1,
                                "elements": [
                                    {{
                                        "data": {{
                                            "index": {},
                                            "value": {{
                                                "type": {{
                                                    "data": {{
                                                        "index": {},
                                                        "value": {{
                                                            "number_of_bits": 32,
                                                            "is_signed": false
                                                        }}
                                                    }}
                                                }},
                                                "data": "0"
                                            }}
                                        }}
                                    }}
                                ]
                            }}
                        }},
                        {{
                            "expressions": {{
                                "size": 1,
                                "elements": [
                                    {{
                                        "data": {{
                                            "index": {},
                                            "value": {{
                                                "type": {{
                                                    "data": {{
                                                        "index": {},
                                                        "value": {{
                                                            "number_of_bits": 32,
                                                            "is_signed": false
                                                        }}
                                                    }}
                                                }},
                                                "data": "0"
                                            }}
                                        }}
                                    }}
                                ]
                            }}
                        }}
                    ]
                }},
                "is_packed": false,
                "is_literal": false
            }}
)JSON",
            index_of_v<Type_reference::Data_type, Integer_type>,
            index_of_v<Type_reference::Data_type, Integer_type>,
            index_of_v<Type_reference::Data_type, Integer_type>,
            index_of_v<Expression::Data_type, Constant_expression>,
            index_of_v<Type_reference::Data_type, Integer_type>,
            index_of_v<Expression::Data_type, Constant_expression>,
            index_of_v<Type_reference::Data_type, Integer_type>,
            index_of_v<Expression::Data_type, Constant_expression>,
            index_of_v<Type_reference::Data_type, Integer_type>
        );

        h::Struct_declaration const expected = create_expected_struct_declaration();

        std::optional<h::Struct_declaration> const output = h::json::read<h::Struct_declaration>(json_data);

        REQUIRE(output.has_value());

        h::Struct_declaration const& actual = output.value();
        CHECK(actual == expected);
    }

    TEST_CASE("Write Struct_declaration")
    {
        h::Struct_declaration const input = create_expected_struct_declaration();

        std::pmr::string const expected = std::pmr::string{std::format(
            R"({{"is_literal":false,"is_packed":false,"member_bit_fields":{{"elements":[24,8,null],"size":3}},"member_comments":{{"elements":[],"size":0}},"member_default_values":{{"elements":[{{"expressions":{{"elements":[{{"data":{{"index":{},"value":{{"data":"0","type":{{"data":{{"index":{},"value":{{"is_signed":false,"number_of_bits":32}}}}}}}}}}}}],"size":1}}}},{{"expressions":{{"elements":[{{"data":{{"index":{},"value":{{"data":"0","type":{{"data":{{"index":{},"value":{{"is_signed":false,"number_of_bits":32}}}}}}}}}}}}],"size":1}}}},{{"expressions":{{"elements":[{{"data":{{"index":{},"value":{{"data":"0","type":{{"data":{{"index":{},"value":{{"is_signed":false,"number_of_bits":32}}}}}}}}}}}}],"size":1}}}}],"size":3}},"member_names":{{"elements":["first","second","third"],"size":3}},"member_types":{{"elements":[{{"data":{{"index":{},"value":{{"is_signed":false,"number_of_bits":32}}}}}},{{"data":{{"index":{},"value":{{"is_signed":false,"number_of_bits":32}}}}}},{{"data":{{"index":{},"value":{{"is_signed":false,"number_of_bits":32}}}}}}],"size":3}},"name":"My_struct"}})",
            index_of_v<Expression::Data_type, Constant_expression>,
            index_of_v<Type_reference::Data_type, Integer_type>,
            index_of_v<Expression::Data_type, Constant_expression>,
            index_of_v<Type_reference::Data_type, Integer_type>,
            index_of_v<Expression::Data_type, Constant_expression>,
            index_of_v<Type_reference::Data_type, Integer_type>,
            index_of_v<Type_reference::Data_type, Integer_type>,
            index_of_v<Type_reference::Data_type, Integer_type>,
            index_of_v<Type_reference::Data_type, Integer_type>
        )};

        std::pmr::string const actual = h::json::write(input);

        CHECK(actual == expected);
    }
}
