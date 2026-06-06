#define CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS
#include <catch2/catch_all.hpp>

#include <array>
#include <string_view>
#include <string>
#include <vector>

import iris.tools.tests_results_replacer;

namespace iris::tools::tests_results_replacer
{
    TEST_CASE("Extract expected and actual results")
    {
        char const* const input_text = R"(
<TestCase name="First">
  <OverallResult success="true" skips="0"/>
</TestCase>
<TestCase name="Second">
  <Expression success="false">
    <Original>
      llvm_ir_body == expected_llvm_ir
    </Original>
    <Expanded>
      "
; Function Attrs: convergent
define void @Booleans_foo() #0 {
entry:
  %my_true_boolean = alloca i1, align 1
  store i1 true, ptr %my_true_boolean, align 1
  %my_false_boolean = alloca i1, align 1
  store i1 false, ptr %my_false_boolean, align 1
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
"
==
"
define void @Booleans_foo() {
entry:
  %my_true_boolean = alloca i1, align 1
  store i1 true, ptr %my_true_boolean, align 1
  %my_false_boolean = alloca i1, align 1
  store i1 false, ptr %my_false_boolean, align 1
  ret void
}
"
    </Expanded>
  </Expression>
  <OverallResult success="false" skips="0"/>
</TestCase>
)";

        std::pmr::vector<Test_result> const results = extract_test_results(input_text);

        REQUIRE(results.size() == 1);

        {
            Test_result const& result = results[0];
            CHECK(result.test_name == "Second");

            constexpr std::string_view actual = R"(
; Function Attrs: convergent
define void @Booleans_foo() #0 {
entry:
  %my_true_boolean = alloca i1, align 1
  store i1 true, ptr %my_true_boolean, align 1
  %my_false_boolean = alloca i1, align 1
  store i1 false, ptr %my_false_boolean, align 1
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

            CHECK(result.actual == actual);

            constexpr std::string_view expected = R"(
define void @Booleans_foo() {
entry:
  %my_true_boolean = alloca i1, align 1
  store i1 true, ptr %my_true_boolean, align 1
  %my_false_boolean = alloca i1, align 1
  store i1 false, ptr %my_false_boolean, align 1
  ret void
}
)";

            CHECK(result.expected == expected);
        }
    }

    TEST_CASE("Replace test contents")
    {
        std::array<Test_result, 2> const test_results
        {
            Test_result
            {
                .test_name = "First test",
                .actual = "\nFirst actual result\n",
                .expected = "\nFirst expected result\n",
            },
            Test_result
            {
                .test_name = "Second test",
                .actual = "\nSecond actual result\n",
                .expected = "\nSecond expected result\n",
            },
        };

        std::string_view const input_text = R"INPUT(
TEST_CASE("First test")
{
    char const* const input_file = "first.irisb";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
First expected result
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }
}

TEST_CASE("Second test")
{
    char const* const input_file = "second.irisb";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
Second expected result
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }
}
)INPUT";

        std::pmr::string const output_text = replace_test_contents(input_text, test_results);

        std::string_view const expected_output_text = R"INPUT(
TEST_CASE("First test")
{
    char const* const input_file = "first.irisb";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
First actual result
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }
}

TEST_CASE("Second test")
{
    char const* const input_file = "second.irisb";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
Second actual result
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }
}
)INPUT";

        CHECK(output_text == expected_output_text);
    }
}
