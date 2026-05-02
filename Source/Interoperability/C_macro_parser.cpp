module;

#include <cctype>
#include <fstream>

module iris.c_macro_parser;

import std;

import iris.core;
import iris.core.expressions;
import iris.core.types;

namespace iris::c
{
    std::string_view trim_text(std::string_view const text)
    {
        std::size_t begin = 0;
        while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
            ++begin;

        std::size_t end = text.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
            --end;

        return text.substr(begin, end - begin);
    }

    bool ends_with_backslash(std::string const& line)
    {
        std::size_t end = line.size();
        while (end > 0 && std::isspace(static_cast<unsigned char>(line[end - 1])) != 0)
            --end;

        return end > 0 && line[end - 1] == '\\';
    }

    std::string remove_trailing_backslash(std::string const& line)
    {
        std::size_t end = line.size();
        while (end > 0 && std::isspace(static_cast<unsigned char>(line[end - 1])) != 0)
            --end;

        if (end > 0 && line[end - 1] == '\\')
            --end;

        return line.substr(0, end);
    }

    bool is_identifier_start(char const character)
    {
        return std::isalpha(static_cast<unsigned char>(character)) != 0 || character == '_';
    }

    bool is_identifier_character(char const character)
    {
        return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_';
    }

    std::optional<std::pmr::string> get_macro_replacement_text(
        std::string_view const macro_name,
        iris::Source_range_location const& source_location
    )
    {
        if (!source_location.file_path.has_value())
            return std::nullopt;

        std::ifstream file{source_location.file_path.value()};
        if (!file.good())
            return std::nullopt;

        std::string line;
        std::uint32_t current_line = 0;
        std::uint32_t const target_line = source_location.range.start.line;
        while (current_line < target_line && std::getline(file, line))
            ++current_line;

        if (current_line != target_line)
            return std::nullopt;

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::string combined_line = line;
        while (ends_with_backslash(combined_line))
        {
            std::string const line_without_backslash = remove_trailing_backslash(combined_line);

            std::string next_line;
            if (!std::getline(file, next_line))
                break;

            if (!next_line.empty() && next_line.back() == '\r')
                next_line.pop_back();

            combined_line = line_without_backslash + " " + next_line;
        }

        std::string_view const text = trim_text(combined_line);
        if (!text.starts_with("#define"))
            return std::nullopt;

        std::size_t index = 7;
        while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) != 0)
            ++index;

        if (index >= text.size() || !is_identifier_start(text[index]))
            return std::nullopt;

        std::size_t const macro_name_begin = index;
        ++index;
        while (index < text.size() && is_identifier_character(text[index]))
            ++index;

        std::string_view const parsed_macro_name = text.substr(macro_name_begin, index - macro_name_begin);
        if (parsed_macro_name != macro_name)
            return std::nullopt;

        std::string_view const replacement_text = trim_text(text.substr(index));
        if (replacement_text.empty())
            return std::nullopt;

        return std::pmr::string{replacement_text};
    }

    enum class Macro_expression_token_type
    {
        Identifier,
        Integer,
        Left_parenthesis,
        Right_parenthesis,
        Left_bracket,
        Right_bracket,
        Comma,
        Address_of,
        Indirection
    };

    struct Macro_expression_token
    {
        Macro_expression_token_type type;
        std::string text;
    };

    std::optional<std::vector<Macro_expression_token>> tokenize_macro_expression(std::string_view const expression)
    {
        std::vector<Macro_expression_token> tokens;

        std::size_t index = 0;
        while (index < expression.size())
        {
            char const character = expression[index];

            if (std::isspace(static_cast<unsigned char>(character)) != 0)
            {
                ++index;
                continue;
            }

            if (is_identifier_start(character))
            {
                std::size_t begin = index;
                ++index;
                while (index < expression.size() && is_identifier_character(expression[index]))
                    ++index;

                tokens.push_back({
                    .type = Macro_expression_token_type::Identifier,
                    .text = std::string{expression.substr(begin, index - begin)}
                });
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(character)) != 0)
            {
                std::size_t begin = index;
                ++index;
                while (index < expression.size())
                {
                    char const value = expression[index];
                    bool const is_numeric_character = std::isalnum(static_cast<unsigned char>(value)) != 0 || value == '\'';
                    if (!is_numeric_character)
                        break;
                    ++index;
                }

                tokens.push_back({
                    .type = Macro_expression_token_type::Integer,
                    .text = std::string{expression.substr(begin, index - begin)}
                });
                continue;
            }

            switch (character)
            {
            case '(':
                tokens.push_back({ .type = Macro_expression_token_type::Left_parenthesis, .text = "(" });
                ++index;
                break;
            case ')':
                tokens.push_back({ .type = Macro_expression_token_type::Right_parenthesis, .text = ")" });
                ++index;
                break;
            case '[':
                tokens.push_back({ .type = Macro_expression_token_type::Left_bracket, .text = "[" });
                ++index;
                break;
            case ']':
                tokens.push_back({ .type = Macro_expression_token_type::Right_bracket, .text = "]" });
                ++index;
                break;
            case ',':
                tokens.push_back({ .type = Macro_expression_token_type::Comma, .text = "," });
                ++index;
                break;
            case '&':
                tokens.push_back({ .type = Macro_expression_token_type::Address_of, .text = "&" });
                ++index;
                break;
            case '*':
                tokens.push_back({ .type = Macro_expression_token_type::Indirection, .text = "*" });
                ++index;
                break;
            default:
                return std::nullopt;
            }
        }

        return tokens;
    }

    std::optional<std::string> normalize_integer_literal(std::string value)
    {
        value.erase(std::remove(value.begin(), value.end(), '\''), value.end());

        while (!value.empty() && std::isalpha(static_cast<unsigned char>(value.back())) != 0)
            value.pop_back();

        if (value.empty())
            return std::nullopt;

        try
        {
            unsigned long long const integer_value = std::stoull(value, nullptr, 0);
            return std::to_string(integer_value);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    enum class Macro_expression_kind
    {
        Variable,
        Integer,
        Call,
        Access_array,
        Address_of,
        Indirection
    };

    struct Macro_expression_node
    {
        Macro_expression_kind kind;
        std::string value;
        std::uint64_t left = static_cast<std::uint64_t>(-1);
        std::vector<std::uint64_t> arguments;
    };

    class Macro_expression_parser
    {
    public:
        explicit Macro_expression_parser(std::vector<Macro_expression_token> tokens) :
            m_tokens{std::move(tokens)}
        {
        }

        std::optional<std::uint64_t> parse_expression()
        {
            std::optional<std::uint64_t> result = parse_unary();
            if (!result.has_value())
                return std::nullopt;

            if (m_index != m_tokens.size())
                return std::nullopt;

            return result;
        }

        std::span<Macro_expression_node const> nodes() const
        {
            return m_nodes;
        }

    private:
        std::optional<std::uint64_t> parse_unary()
        {
            if (match(Macro_expression_token_type::Address_of))
            {
                std::optional<std::uint64_t> const expression = parse_unary();
                if (!expression.has_value())
                    return std::nullopt;

                return create_node({
                    .kind = Macro_expression_kind::Address_of,
                    .left = *expression
                });
            }

            if (match(Macro_expression_token_type::Indirection))
            {
                std::optional<std::uint64_t> const expression = parse_unary();
                if (!expression.has_value())
                    return std::nullopt;

                return create_node({
                    .kind = Macro_expression_kind::Indirection,
                    .left = *expression
                });
            }

            return parse_postfix();
        }

        std::optional<std::uint64_t> parse_postfix()
        {
            std::optional<std::uint64_t> expression = parse_primary();
            if (!expression.has_value())
                return std::nullopt;

            while (true)
            {
                if (match(Macro_expression_token_type::Left_parenthesis))
                {
                    std::vector<std::uint64_t> arguments;
                    if (!match(Macro_expression_token_type::Right_parenthesis))
                    {
                        while (true)
                        {
                            std::optional<std::uint64_t> const argument = parse_unary();
                            if (!argument.has_value())
                                return std::nullopt;

                            arguments.push_back(*argument);

                            if (match(Macro_expression_token_type::Right_parenthesis))
                                break;

                            if (!match(Macro_expression_token_type::Comma))
                                return std::nullopt;
                        }
                    }

                    expression = create_node({
                        .kind = Macro_expression_kind::Call,
                        .left = *expression,
                        .arguments = std::move(arguments)
                    });
                    continue;
                }

                if (match(Macro_expression_token_type::Left_bracket))
                {
                    std::optional<std::uint64_t> const index_expression = parse_unary();
                    if (!index_expression.has_value())
                        return std::nullopt;

                    if (!match(Macro_expression_token_type::Right_bracket))
                        return std::nullopt;

                    expression = create_node({
                        .kind = Macro_expression_kind::Access_array,
                        .left = *expression,
                        .arguments = {*index_expression}
                    });
                    continue;
                }

                break;
            }

            return expression;
        }

        std::optional<std::uint64_t> parse_primary()
        {
            if (std::optional<Macro_expression_token> token = consume(Macro_expression_token_type::Identifier))
            {
                return create_node({
                    .kind = Macro_expression_kind::Variable,
                    .value = token->text
                });
            }

            if (std::optional<Macro_expression_token> token = consume(Macro_expression_token_type::Integer))
            {
                std::string integer_value = token->text;
                if (std::optional<std::string> normalized = normalize_integer_literal(token->text))
                    integer_value = *normalized;

                return create_node({
                    .kind = Macro_expression_kind::Integer,
                    .value = std::move(integer_value)
                });
            }

            if (match(Macro_expression_token_type::Left_parenthesis))
            {
                std::optional<std::uint64_t> const expression = parse_unary();
                if (!expression.has_value())
                    return std::nullopt;

                if (!match(Macro_expression_token_type::Right_parenthesis))
                    return std::nullopt;

                return expression;
            }

            return std::nullopt;
        }

        std::uint64_t create_node(Macro_expression_node node)
        {
            std::uint64_t const index = m_nodes.size();
            m_nodes.push_back(std::move(node));
            return index;
        }

        bool match(Macro_expression_token_type const token_type)
        {
            if (m_index >= m_tokens.size())
                return false;

            if (m_tokens[m_index].type != token_type)
                return false;

            ++m_index;
            return true;
        }

        std::optional<Macro_expression_token> consume(Macro_expression_token_type const token_type)
        {
            if (m_index >= m_tokens.size())
                return std::nullopt;

            if (m_tokens[m_index].type != token_type)
                return std::nullopt;

            Macro_expression_token const result = m_tokens[m_index];
            ++m_index;
            return result;
        }

        std::vector<Macro_expression_token> m_tokens;
        std::size_t m_index = 0;
        std::vector<Macro_expression_node> m_nodes;
    };

    std::optional<std::size_t> emit_macro_expression(
        std::span<Macro_expression_node const> const nodes,
        std::uint64_t const node_index,
        iris::Statement& statement
    )
    {
        if (node_index >= nodes.size())
            return std::nullopt;

        Macro_expression_node const& node = nodes[node_index];

        std::size_t const expression_index = statement.expressions.size();
        statement.expressions.push_back(iris::Expression{.data = iris::Invalid_expression{}});

        auto const create_expression_index = [](std::size_t const value) -> iris::Expression_index
        {
            return {.expression_index = static_cast<std::uint64_t>(value)};
        };

        switch (node.kind)
        {
        case Macro_expression_kind::Variable:
        {
            statement.expressions[expression_index] = iris::create_variable_expression(std::pmr::string{node.value});
            return expression_index;
        }
        case Macro_expression_kind::Integer:
        {
            iris::Type_reference const type = iris::create_fundamental_type_type_reference(iris::Fundamental_type::C_int);
            statement.expressions[expression_index] = iris::create_constant_expression(type, node.value);
            return expression_index;
        }
        case Macro_expression_kind::Call:
        {
            std::optional<std::size_t> const callee_expression_index = emit_macro_expression(nodes, node.left, statement);
            if (!callee_expression_index.has_value())
                return std::nullopt;

            std::pmr::vector<iris::Expression_index> arguments;
            for (std::uint64_t const argument_node : node.arguments)
            {
                std::optional<std::size_t> const argument_expression_index = emit_macro_expression(nodes, argument_node, statement);
                if (!argument_expression_index.has_value())
                    return std::nullopt;

                arguments.push_back(create_expression_index(*argument_expression_index));
            }

            statement.expressions[expression_index] = iris::Expression
            {
                .data = iris::Call_expression
                {
                    .expression = create_expression_index(*callee_expression_index),
                    .arguments = std::move(arguments)
                }
            };
            return expression_index;
        }
        case Macro_expression_kind::Access_array:
        {
            if (node.arguments.size() != 1)
                return std::nullopt;

            std::optional<std::size_t> const left_expression_index = emit_macro_expression(nodes, node.left, statement);
            if (!left_expression_index.has_value())
                return std::nullopt;

            std::optional<std::size_t> const index_expression_index = emit_macro_expression(nodes, node.arguments[0], statement);
            if (!index_expression_index.has_value())
                return std::nullopt;

            statement.expressions[expression_index] = iris::Expression
            {
                .data = iris::Access_array_expression
                {
                    .expression = create_expression_index(*left_expression_index),
                    .index = create_expression_index(*index_expression_index)
                }
            };
            return expression_index;
        }
        case Macro_expression_kind::Address_of:
        case Macro_expression_kind::Indirection:
        {
            std::optional<std::size_t> const operand_expression_index = emit_macro_expression(nodes, node.left, statement);
            if (!operand_expression_index.has_value())
                return std::nullopt;

            statement.expressions[expression_index] = iris::Expression
            {
                .data = iris::Unary_expression
                {
                    .expression = create_expression_index(*operand_expression_index),
                    .operation = node.kind == Macro_expression_kind::Address_of ? iris::Unary_operation::Address_of : iris::Unary_operation::Indirection
                }
            };
            return expression_index;
        }
        }

        return std::nullopt;
    }

    std::string_view strip_outer_parentheses(std::string_view text)
    {
        while (text.size() >= 2 && text.front() == '(' && text.back() == ')')
        {
            std::size_t depth = 0;
            bool wraps_whole_expression = true;
            for (std::size_t index = 0; index < text.size(); ++index)
            {
                if (text[index] == '(')
                    ++depth;
                else if (text[index] == ')')
                    --depth;

                if (depth == 0 && index + 1 < text.size())
                {
                    wraps_whole_expression = false;
                    break;
                }
            }

            if (!wraps_whole_expression)
                break;

            text = trim_text(text.substr(1, text.size() - 2));
        }

        return text;
    }

    bool is_cast_prefix_content(std::string_view const text)
    {
        if (text.empty())
            return false;

        for (char const character : text)
        {
            bool const is_valid = std::isalnum(static_cast<unsigned char>(character)) != 0 ||
                character == '_' ||
                character == ' ' ||
                character == '\t' ||
                character == '*';

            if (!is_valid)
                return false;
        }

        return true;
    }

    std::string_view strip_leading_cast_prefixes(std::string_view text)
    {
        while (text.size() >= 3 && text.front() == '(')
        {
            std::size_t depth = 0;
            std::size_t close_parenthesis_index = static_cast<std::size_t>(-1);
            for (std::size_t index = 0; index < text.size(); ++index)
            {
                if (text[index] == '(')
                    ++depth;
                else if (text[index] == ')')
                    --depth;

                if (depth == 0)
                {
                    close_parenthesis_index = index;
                    break;
                }
            }

            if (close_parenthesis_index == static_cast<std::size_t>(-1))
                break;

            std::string_view const cast_content = trim_text(text.substr(1, close_parenthesis_index - 1));
            if (!is_cast_prefix_content(cast_content))
                break;

            std::string_view const remainder = trim_text(text.substr(close_parenthesis_index + 1));
            if (remainder.empty())
                break;

            text = remainder;
        }

        return text;
    }

    std::optional<iris::Statement> parse_macro_replacement_text_to_statement(std::string_view const replacement_text)
    {
        std::string_view expression = trim_text(replacement_text);
        if (expression.empty())
            return std::nullopt;

        expression = strip_outer_parentheses(expression);
        expression = strip_leading_cast_prefixes(expression);
        expression = strip_outer_parentheses(expression);

        std::optional<std::vector<Macro_expression_token>> tokens = tokenize_macro_expression(expression);
        if (!tokens.has_value())
            return std::nullopt;

        Macro_expression_parser parser{std::move(*tokens)};
        std::optional<std::uint64_t> root_node = parser.parse_expression();
        if (!root_node.has_value())
            return std::nullopt;

        iris::Statement statement;
        std::optional<std::size_t> const root_expression_index = emit_macro_expression(parser.nodes(), *root_node, statement);
        if (!root_expression_index.has_value() || *root_expression_index != 0)
            return std::nullopt;

        return statement;
    }
}
