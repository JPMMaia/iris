#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

import std;
import iris.common.filesystem_common;
import iris.core;
import iris.core.hash;
import iris.parser.parser;
import iris.parser.convertor;

namespace
{
    // Helper: Create a Lambda_type with given input and output types
    static iris::Lambda_type create_lambda_type(
        std::pmr::vector<iris::Type_reference> input_types,
        std::pmr::vector<iris::Type_reference> output_types
    )
    {
        return iris::Lambda_type{
            .input_parameter_types = std::move(input_types),
            .output_parameter_types = std::move(output_types),
        };
    }

    // Helper: Create a Lambda_declaration with given parameters
    static iris::Lambda_declaration create_lambda_declaration(
        std::string_view name,
        std::pmr::vector<iris::Type_reference> input_types,
        std::pmr::vector<iris::Type_reference> output_types,
        std::pmr::vector<std::pmr::string> input_names = {},
        std::pmr::vector<std::pmr::string> output_names = {}
    )
    {
        if (input_names.empty())
        {
            for (std::size_t i = 0; i < input_types.size(); ++i)
                input_names.push_back(std::pmr::string{ "param" + std::to_string(i), std::pmr::get_allocator<std::pmr::string>() });
        }
        if (output_names.empty())
        {
            for (std::size_t i = 0; i < output_types.size(); ++i)
                output_names.push_back(std::pmr::string{ "result" + std::to_string(i), std::pmr::get_allocator<std::pmr::string>() });
        }

        return iris::Lambda_declaration{
            .name = std::pmr::string{ name, std::pmr::get_allocator<std::pmr::string>() },
            .unique_name = std::nullopt,
            .input_parameter_types = std::move(input_types),
            .output_parameter_types = std::move(output_types),
            .input_parameter_names = std::move(input_names),
            .output_parameter_names = std::move(output_names),
        };
    }

    // Helper: Create a fundamental type reference
    static iris::Type_reference create_fundamental_type(iris::Fundamental_type type)
    {
        return iris::Type_reference{
            .data = type,
        };
    }

    // Helper: Create a custom type reference
    static iris::Type_reference create_custom_type(
        std::string_view module_name,
        std::string_view type_name
    )
    {
        return iris::Type_reference{
            .data = iris::Custom_type_reference{
                .module_reference = iris::Module_reference{
                    .name = std::pmr::string{ module_name, std::pmr::get_allocator<std::pmr::string>() },
                },
                .name = std::pmr::string{ type_name, std::pmr::get_allocator<std::pmr::string>() },
            },
        };
    }

    // Helper: Create an xxhash state
    static XXH64_state_t* create_hash_state()
    {
        XXH64_state_t* const state = XXH64_createState();
        REQUIRE(state != nullptr);
        CHECK(XXH64_reset(state, 0) == XXH_SUCCESS);
        return state;
    }
}

TEST_CASE("Hashes Lambda_type with single input and output type", "[Core][Hash]")
{
    XXH64_state_t* const state = create_hash_state();
    ON_EXIT(XXH64_freeState(state));

    auto const input_type = create_fundamental_type(iris::Fundamental_type::Int32);
    auto const output_type = create_fundamental_type(iris::Fundamental_type::Bool);

    auto const lambda_type = create_lambda_type(
        std::pmr::vector<iris::Type_reference>{ std::move(input_type), std::pmr::get_allocator<iris::Type_reference>() },
        std::pmr::vector<iris::Type_reference>{ std::move(output_type), std::pmr::get_allocator<iris::Type_reference>() }
    );

    XXH64_hash_t const hash = iris::hash_lambda_type(state, lambda_type);
    CHECK(hash != 0);
}

TEST_CASE("Hashes Lambda_type with multiple input and output types", "[Core][Hash]")
{
    XXH64_state_t* const state = create_hash_state();
    ON_EXIT(XXH64_freeState(state));

    std::pmr::vector<iris::Type_reference> input_types{
        create_fundamental_type(iris::Fundamental_type::Int32),
        create_fundamental_type(iris::Fundamental_type::Float64),
        create_fundamental_type(iris::Fundamental_type::Bool),
    };

    std::pmr::vector<iris::Type_reference> output_types{
        create_fundamental_type(iris::Fundamental_type::Int32),
        create_fundamental_type(iris::Fundamental_type::Float64),
    };

    auto const lambda_type = create_lambda_type(std::move(input_types), std::move(output_types));

    XXH64_hash_t const hash = iris::hash_lambda_type(state, lambda_type);
    CHECK(hash != 0);
}

TEST_CASE("Hashes Lambda_type with no parameters", "[Core][Hash]")
{
    XXH64_state_t* const state = create_hash_state();
    ON_EXIT(XXH64_freeState(state));

    auto const lambda_type = create_lambda_type({}, {});

    XXH64_hash_t const hash = iris::hash_lambda_type(state, lambda_type);
    CHECK(hash != 0);
}

TEST_CASE("Hashes Lambda_type with custom type references", "[Core][Hash]")
{
    XXH64_state_t* const state = create_hash_state();
    ON_EXIT(XXH64_freeState(state));

    std::pmr::vector<iris::Type_reference> input_types{
        create_custom_type("my.module", "MyType"),
        create_fundamental_type(iris::Fundamental_type::Int32),
    };

    std::pmr::vector<iris::Type_reference> output_types{
        create_custom_type("other.module", "OtherType"),
    };

    auto const lambda_type = create_lambda_type(std::move(input_types), std::move(output_types));

    XXH64_hash_t const hash = iris::hash_lambda_type(state, lambda_type);
    CHECK(hash != 0);
}

TEST_CASE("Different Lambda_type produce different hashes", "[Core][Hash]")
{
    XXH64_state_t* const state = create_hash_state();
    ON_EXIT(XXH64_freeState(state));

    auto const lambda_type_a = create_lambda_type(
        std::pmr::vector<iris::Type_reference>{ create_fundamental_type(iris::Fundamental_type::Int32), std::pmr::get_allocator<iris::Type_reference>() },
        std::pmr::vector<iris::Type_reference>{ create_fundamental_type(iris::Fundamental_type::Bool), std::pmr::get_allocator<iris::Type_reference>() }
    );

    auto const lambda_type_b = create_lambda_type(
        std::pmr::vector<iris::Type_reference>{ create_fundamental_type(iris::Fundamental_type::Float64), std::pmr::get_allocator<iris::Type_reference>() },
        std::pmr::vector<iris::Type_reference>{ create_fundamental_type(iris::Fundamental_type::Bool), std::pmr::get_allocator<iris::Type_reference>() }
    );

    XXH64_hash_t const hash_a = iris::hash_lambda_type(state, lambda_type_a);
    XXH64_hash_t const hash_b = iris::hash_lambda_type(state, lambda_type_b);

    CHECK(hash_a != hash_b);
}

TEST_CASE("Same Lambda_type produces same hash (deterministic)", "[Core][Hash]")
{
    XXH64_state_t* const state = create_hash_state();
    ON_EXIT(XXH64_freeState(state));

    auto const input_type = create_fundamental_type(iris::Fundamental_type::Int32);
    auto const output_type = create_fundamental_type(iris::Fundamental_type::Bool);

    auto const lambda_type = create_lambda_type(
        std::pmr::vector<iris::Type_reference>{ std::move(input_type), std::pmr::get_allocator<iris::Type_reference>() },
        std::pmr::vector<iris::Type_reference>{ std::move(output_type), std::pmr::get_allocator<iris::Type_reference>() }
    );

    XXH64_hash_t const hash1 = iris::hash_lambda_type(state, lambda_type);

    // Reset state and hash again
    CHECK(XXH64_reset(state, 0) == XXH_SUCCESS);

    auto const input_type2 = create_fundamental_type(iris::Fundamental_type::Int32);
    auto const output_type2 = create_fundamental_type(iris::Fundamental_type::Bool);

    auto const lambda_type2 = create_lambda_type(
        std::pmr::vector<iris::Type_reference>{ std::move(input_type2), std::pmr::get_allocator<iris::Type_reference>() },
        std::pmr::vector<iris::Type_reference>{ std::move(output_type2), std::pmr::get_allocator<iris::Type_reference>() }
    );

    XXH64_hash_t const hash2 = iris::hash_lambda_type(state, lambda_type2);

    CHECK(hash1 == hash2);
}

TEST_CASE("Hashes Lambda_declaration with name and types", "[Core][Hash]")
{
    XXH64_state_t* const state = create_hash_state();
    ON_EXIT(XXH64_freeState(state));

    std::pmr::vector<iris::Type_reference> input_types{
        create_fundamental_type(iris::Fundamental_type::Int32),
        create_fundamental_type(iris::Fundamental_type::Int32),
    };

    std::pmr::vector<iris::Type_reference> output_types{
        create_fundamental_type(iris::Fundamental_type::Int32),
    };

    std::pmr::vector<std::pmr::string> input_names{
        std::pmr::string{ "a", std::pmr::get_allocator<std::pmr::string>() },
        std::pmr::string{ "b", std::pmr::get_allocator<std::pmr::string>() },
    };

    std::pmr::vector<std::pmr::string> output_names{
        std::pmr::string{ "result", std::pmr::get_allocator<std::pmr::string>() },
    };

    auto const lambda_decl = create_lambda_declaration(
        "Comparator",
        std::move(input_types),
        std::move(output_types),
        std::move(input_names),
        std::move(output_names)
    );

    XXH64_hash_t const hash = iris::hash_lambda_declaration(state, lambda_decl);
    CHECK(hash != 0);
}

TEST_CASE("Hashes Lambda_declaration with no parameters", "[Core][Hash]")
{
    XXH64_state_t* const state = create_hash_state();
    ON_EXIT(XXH64_freeState(state));

    auto const lambda_decl = create_lambda_declaration("Action", {}, {});

    XXH64_hash_t const hash = iris::hash_lambda_declaration(state, lambda_decl);
    CHECK(hash != 0);
}

TEST_CASE("Hashes Lambda_declaration with custom type references", "[Core][Hash]")
{
    XXH64_state_t* const state = create_hash_state();
    ON_EXIT(XXH64_freeState(state));

    std::pmr::vector<iris::Type_reference> input_types{
        create_custom_type("my.module", "MyType"),
    };

    std::pmr::vector<iris::Type_reference> output_types{
        create_custom_type("other.module", "OtherType"),
        create_custom_type("other.module", "AnotherType"),
    };

    auto const lambda_decl = create_lambda_declaration(
        "Transformer",
        std::move(input_types),
        std::move(output_types)
    );

    XXH64_hash_t const hash = iris::hash_lambda_declaration(state, lambda_decl);
    CHECK(hash != 0);
}

TEST_CASE("Different Lambda_declarations produce different hashes", "[Core][Hash]")
{
    XXH64_state_t* const state = create_hash_state();
    ON_EXIT(XXH64_freeState(state));

    auto const lambda_decl_a = create_lambda_declaration("Comparator",
        std::pmr::vector<iris::Type_reference>{ create_fundamental_type(iris::Fundamental_type::Int32), std::pmr::get_allocator<iris::Type_reference>() },
        std::pmr::vector<iris::Type_reference>{ create_fundamental_type(iris::Fundamental_type::Int32), std::pmr::get_allocator<iris::Type_reference>() }
    );

    auto const lambda_decl_b = create_lambda_declaration("Mapper",
        std::pmr::vector<iris::Type_reference>{ create_fundamental_type(iris::Fundamental_type::Int32), std::pmr::get_allocator<iris::Type_reference>() },
        std::pmr::vector<iris::Type_reference>{ create_fundamental_type(iris::Fundamental_type::Int32), std::pmr::get_allocator<iris::Type_reference>() }
    );

    XXH64_hash_t const hash_a = iris::hash_lambda_declaration(state, lambda_decl_a);
    XXH64_hash_t const hash_b = iris::hash_lambda_declaration(state, lambda_decl_b);

    CHECK(hash_a != hash_b);
}

TEST_CASE("Same Lambda_declaration produces same hash (deterministic)", "[Core][Hash]")
{
    XXH64_state_t* const state = create_hash_state();
    ON_EXIT(XXH64_freeState(state));

    auto const lambda_decl = create_lambda_declaration(
        "Comparator",
        std::pmr::vector<iris::Type_reference>{ create_fundamental_type(iris::Fundamental_type::Int32), std::pmr::get_allocator<iris::Type_reference>() },
        std::pmr::vector<iris::Type_reference>{ create_fundamental_type(iris::Fundamental_type::Int32), std::pmr::get_allocator<iris::Type_reference>() }
    );

    XXH64_hash_t const hash1 = iris::hash_lambda_declaration(state, lambda_decl);

    // Reset state and hash again
    CHECK(XXH64_reset(state, 0) == XXH_SUCCESS);

    auto const lambda_decl2 = create_lambda_declaration(
        "Comparator",
        std::pmr::vector<iris::Type_reference>{ create_fundamental_type(iris::Fundamental_type::Int32), std::pmr::get_allocator<iris::Type_reference>() },
        std::pmr::vector<iris::Type_reference>{ create_fundamental_type(iris::Fundamental_type::Int32), std::pmr::get_allocator<iris::Type_reference>() }
    );

    XXH64_hash_t const hash2 = iris::hash_lambda_declaration(state, lambda_decl2);

    CHECK(hash1 == hash2);
}

TEST_CASE("Lambda_declaration with unique_name produces different hash", "[Core][Hash]")
{
    XXH64_state_t* const state = create_hash_state();
    ON_EXIT(XXH64_freeState(state));

    auto const input_type = create_fundamental_type(iris::Fundamental_type::Int32);
    auto const output_type = create_fundamental_type(iris::Fundamental_type::Int32);

    auto const lambda_decl_a = iris::Lambda_declaration{
        .name = std::pmr::string{ "Comparator", std::pmr::get_allocator<std::pmr::string>() },
        .unique_name = std::nullopt,
        .input_parameter_types = std::pmr::vector<iris::Type_reference>{ std::move(input_type), std::pmr::get_allocator<iris::Type_reference>() },
        .output_parameter_types = std::pmr::vector<iris::Type_reference>{ std::move(output_type), std::pmr::get_allocator<iris::Type_reference>() },
    };

    auto const input_type2 = create_fundamental_type(iris::Fundamental_type::Int32);
    auto const output_type2 = create_fundamental_type(iris::Fundamental_type::Int32);

    auto const lambda_decl_b = iris::Lambda_declaration{
        .name = std::pmr::string{ "Comparator", std::pmr::get_allocator<std::pmr::string>() },
        .unique_name = std::pmr::string{ "Comparator_001", std::pmr::get_allocator<std::pmr::string>() },
        .input_parameter_types = std::pmr::vector<iris::Type_reference>{ std::move(input_type2), std::pmr::get_allocator<iris::Type_reference>() },
        .output_parameter_types = std::pmr::vector<iris::Type_reference>{ std::move(output_type2), std::pmr::get_allocator<iris::Type_reference>() },
    };

    XXH64_hash_t const hash_a = iris::hash_lambda_declaration(state, lambda_decl_a);
    XXH64_hash_t const hash_b = iris::hash_lambda_declaration(state, lambda_decl_b);

    CHECK(hash_a != hash_b);
}

TEST_CASE("Hashes Lambda_declaration with multiple output parameters", "[Core][Hash]")
{
    XXH64_state_t* const state = create_hash_state();
    ON_EXIT(XXH64_freeState(state));

    std::pmr::vector<iris::Type_reference> input_types{
        create_fundamental_type(iris::Fundamental_type::Float64),
    };

    std::pmr::vector<iris::Type_reference> output_types{
        create_fundamental_type(iris::Fundamental_type::Float64),
        create_fundamental_type(iris::Fundamental_type::Float64),
        create_fundamental_type(iris::Fundamental_type::Float64),
    };

    std::pmr::vector<std::pmr::string> output_names{
        std::pmr::string{ "new_x", std::pmr::get_allocator<std::pmr::string>() },
        std::pmr::string{ "new_y", std::pmr::get_allocator<std::pmr::string>() },
        std::pmr::string{ "new_z", std::pmr::get_allocator<std::pmr::string>() },
    };

    auto const lambda_decl = create_lambda_declaration(
        "Transform3D",
        std::move(input_types),
        std::move(output_types),
        {},
        std::move(output_names)
    );

    XXH64_hash_t const hash = iris::hash_lambda_declaration(state, lambda_decl);
    CHECK(hash != 0);
}
