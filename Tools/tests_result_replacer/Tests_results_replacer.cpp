module;

#include <filesystem>
#include <format>
#include <optional>
#include <regex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

module iris.tools.tests_results_replacer;

namespace iris::tools::tests_results_replacer
{
    std::optional<std::pmr::string> get_file_contents(std::filesystem::path const& path)
    {
        std::string const path_string = path.generic_string();

        std::FILE* file = std::fopen(path_string.c_str(), "rb");
        if (file == nullptr)
            return {};

        std::pmr::string contents;
        std::fseek(file, 0, SEEK_END);
        contents.resize(std::ftell(file));
        std::rewind(file);
        std::fread(&contents[0], 1, contents.size(), file);
        std::fclose(file);

        contents.erase(std::remove(contents.begin(), contents.end(), '\r'), contents.end());

        return contents;
    }

    void write_to_file(std::filesystem::path const& path, std::string_view const content)
    {
        std::string const path_string = path.generic_string();

        std::FILE* const file = std::fopen(path_string.c_str(), "w");
        if (file == nullptr)
        {
            std::string const message = std::format("Cannot write to '{}'", path_string);
            std::perror(message.c_str());
            throw std::runtime_error{ message };
        }

        std::fwrite(content.data(), sizeof(std::string_view::value_type), content.size(), file);

        std::fclose(file);
    }

    std::optional<std::pmr::string> run_tests_and_get_output(std::filesystem::path const& test_application_file_path)
    {
        std::filesystem::path const output_file_path = std::filesystem::temp_directory_path() / "h_compiler_test_result.xml";

        std::string const command = std::format("{} --reporter xml -o {} [LLVM_IR]", test_application_file_path.generic_string(), output_file_path.generic_string());

        std::system(command.c_str());

        std::optional<std::pmr::string> const output = get_file_contents(output_file_path);
        return output;
    }

    inline std::string_view trim(std::string_view const input)
    {
        auto const is_not_space = [](char const character) -> bool
        {
            return !std::isspace(character);
        };

        auto const begin = std::find_if(input.begin(), input.end(), is_not_space);
        auto const end = std::find_if(input.rbegin(), input.rend(), is_not_space).base();

        std::size_t const begin_offset = std::distance(input.begin(), begin);
        std::size_t const end_offset = std::distance(input.begin(), end);

        return input.substr(begin_offset, end_offset - begin_offset);
    }

    std::pmr::vector<Test_result> extract_test_results(std::string_view const input_text)
    {
        constexpr std::string_view begin_test_case_string = "<TestCase name=\"";
        constexpr std::string_view end_test_case_string = "</TestCase>";

        constexpr std::string_view begin_expanded_string = "<Expanded";
        constexpr std::string_view end_expanded_string = "</Expanded>";

        std::pmr::vector<Test_result> output;

        std::size_t begin_offset = 0;

        while (begin_offset < input_text.size())
        {
            std::size_t const begin_test_case_offset = input_text.find(begin_test_case_string, begin_offset) + begin_test_case_string.size();
            if (begin_test_case_offset == input_text.npos || begin_test_case_offset < begin_offset)
                break;
            
            std::size_t const end_test_name_offset = input_text.find("\"", begin_test_case_offset);
            if (end_test_name_offset == input_text.npos || end_test_name_offset < begin_test_case_offset)
                break;

            std::string_view const test_name = input_text.substr(begin_test_case_offset, end_test_name_offset - begin_test_case_offset);

            std::size_t const end_test_case_offset = input_text.find(end_test_case_string, begin_test_case_offset);
            if (end_test_case_offset == input_text.npos || end_test_case_offset < begin_offset)
                break;

            begin_offset = end_test_case_offset + end_test_case_string.size();

            std::size_t const begin_expanded_offset = input_text.find(begin_expanded_string, begin_test_case_offset);
            if (begin_expanded_offset == input_text.npos || begin_expanded_offset > end_test_case_offset)
                continue;

            std::size_t const first_quote_offset = input_text.find("\"", begin_expanded_offset);
            if (first_quote_offset == input_text.npos || first_quote_offset > end_test_case_offset)
                continue;

            std::size_t const second_quote_offset = input_text.find("\n\"\n==\n\"", first_quote_offset);
            if (second_quote_offset == input_text.npos || second_quote_offset > end_test_case_offset)
                continue;

            std::size_t const third_quote_offset = second_quote_offset + 6;
            if (third_quote_offset == input_text.npos || third_quote_offset > end_test_case_offset)
                continue;

            std::size_t const fourth_quote_offset = input_text.find("\n\"\n", third_quote_offset + 1);
            if (fourth_quote_offset == input_text.npos || fourth_quote_offset > end_test_case_offset)
                continue;

            std::size_t const begin_actual_offset = first_quote_offset + 1;
            std::size_t const end_actual_offset = second_quote_offset + 1;
            std::string_view const actual = input_text.substr(begin_actual_offset, end_actual_offset - begin_actual_offset);

            std::size_t const begin_expected_offset = third_quote_offset + 1;
            std::size_t const end_expected_offset = fourth_quote_offset + 1;
            std::string_view const expected = input_text.substr(begin_expected_offset, end_expected_offset - begin_expected_offset);

            output.push_back(
                Test_result
                {
                    .test_name = std::pmr::string{test_name},
                    .actual = std::pmr::string{actual},
                    .expected = std::pmr::string{expected},
                }
            );
        }

        return output;
    }

    std::pmr::string split_into_concatenated_literals(std::string_view const content)
    {
        constexpr std::size_t max_chunk_size = 12000;
        constexpr std::string_view separator = ")\" R\"(";

        if (content.size() <= max_chunk_size)
            return std::pmr::string{content};

        std::pmr::string output;
        output.reserve(content.size() + content.size() / max_chunk_size * separator.size());

        std::size_t chunk_begin = 0;
        while (chunk_begin < content.size())
        {
            std::size_t chunk_end = std::min(chunk_begin + max_chunk_size, content.size());

            // Prefer splitting on a newline boundary for readability.
            if (chunk_end < content.size())
            {
                std::size_t const newline_offset = content.rfind('\n', chunk_end);
                if (newline_offset != content.npos && newline_offset > chunk_begin)
                    chunk_end = newline_offset + 1;
            }

            output.append(content.substr(chunk_begin, chunk_end - chunk_begin));

            if (chunk_end < content.size())
                output.append(separator);

            chunk_begin = chunk_end;
        }

        return output;
    }

    void replace_test_contents_alternative(
        std::pmr::string& output_text,
        Test_result const& test_result,
        std::size_t const begin_test_offset,
        std::size_t const next_test_offset
    )
    {
        constexpr std::string_view begin_expected_text = "std::string const expected_llvm_ir = std::format(R\"(";
        constexpr std::string_view end_expected_text = ")\", ";
        constexpr std::string_view directory_text = "directory: \"";

        if (begin_test_offset == output_text.npos)
            return;

        std::size_t const begin_expected_offset = output_text.find(begin_expected_text, begin_test_offset);
        if (begin_expected_offset == output_text.npos || begin_expected_offset > next_test_offset)
            return;

        std::size_t const end_expected_offset = output_text.find(end_expected_text, begin_expected_offset);
        if (end_expected_offset == output_text.npos || end_expected_offset > next_test_offset)
            return;

        std::size_t const begin_offset = begin_expected_offset + begin_expected_text.size();

        std::pmr::string actual = test_result.actual;
        actual = std::regex_replace(actual, std::regex("\\{"), "{{");
        actual = std::regex_replace(actual, std::regex("\\}"), "}}");

        std::size_t current_offset = 0;

        while (true)
        {
            std::size_t const begin_directory_offset = actual.find(directory_text, current_offset);
            if (begin_directory_offset == actual.npos || begin_directory_offset < current_offset)
                break;

            std::size_t const begin_path_offset = begin_directory_offset + directory_text.size();
            
            std::size_t const end_path_offset = actual.find("\"", begin_path_offset);
            if (end_path_offset == actual.npos || end_path_offset < current_offset)
                break;

            actual.replace(begin_path_offset, end_path_offset - begin_path_offset, "{}");

            std::printf("Replaced test '%s'.\n", test_result.test_name.c_str());

            current_offset = end_path_offset;
        }

        std::pmr::string const chunked_actual = split_into_concatenated_literals(actual);

        output_text.replace(begin_offset, end_expected_offset - begin_offset, chunked_actual);
    }

    std::pmr::string replace_test_contents(std::string_view const input_text, std::span<Test_result const> const test_results)
    {
        std::pmr::string output_text{input_text};

        constexpr std::string_view begin_expected_text = "char const* const expected_llvm_ir = R\"(";
        constexpr std::string_view end_expected_text = ")\";";

        for (Test_result const& test_result : test_results)
        {
            std::string const test_case_string = std::format("TEST_CASE(\"{}\"", test_result.test_name);
            
            std::size_t const begin_test_offset = output_text.find(test_case_string);
            if (begin_test_offset == output_text.npos)
                continue;

            std::size_t const next_test_offset = output_text.find("TEST_CASE", begin_test_offset + test_case_string.size());

            std::size_t const begin_expected_test_offset = output_text.find(begin_expected_text, begin_test_offset);
            if (begin_expected_test_offset == output_text.npos || begin_expected_test_offset > next_test_offset)
            {
                replace_test_contents_alternative(output_text, test_result, begin_test_offset, next_test_offset);
                continue;
            }

            std::size_t const end_expected_test_offset = output_text.find(end_expected_text, begin_expected_test_offset);
            if (end_expected_test_offset == output_text.npos || end_expected_test_offset > next_test_offset)
                continue;

            std::size_t const begin_offset = begin_expected_test_offset + begin_expected_text.size();

            output_text.replace(begin_offset, end_expected_test_offset - begin_offset, test_result.actual);

            std::printf("Replaced test '%s'.\n", test_result.test_name.c_str());
        }

        return output_text;
    }
}
