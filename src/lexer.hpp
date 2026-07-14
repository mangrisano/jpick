#pragma once

#include <vector>
#include <variant>
#include <string>
#include <cctype>
#include <stdexcept>
#include <charconv>

enum class TokenType
{
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Colon,
    Comma,
    String,
    Number,
    True,
    False,
    Null,
    EndOfInput
};

struct Token
{
    TokenType type;
    std::variant<std::monostate, std::string, double> value;
};

inline TokenType read_keyword(const std::string &input, std::size_t &i)
{
    std::size_t start = i;
    while (i < input.size() && std::isalpha(static_cast<unsigned char>(input[i])))
    {
        ++i;
    }
    std::string word = input.substr(start, i - start);
    if (word == "true")
        return TokenType::True;
    if (word == "false")
        return TokenType::False;
    if (word == "null")
        return TokenType::Null;
    throw std::runtime_error("Unknown keyword: " + word);
}

inline double read_number(const std::string &input, std::size_t &i)
{
    double value{};
    const char *first = input.data() + i;
    const char *last = input.data() + input.size();
    auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{})
        throw std::runtime_error("Not valid number");
    i += static_cast<std::size_t>(ptr - first);
    return value;
}

inline std::string read_string(const std::string &input, std::size_t &i)
{
    ++i;
    std::string result{};

    while (i < input.size() && input[i] != '"')
    {
        if (input[i] == '\\')
        {
            ++i;
            if (i >= input.size())
                throw std::runtime_error("Incomplete escape");
            switch (input[i])
            {
            case '"':
                result += '"';
                break;
            case '\\':
                result += '\\';
                break;
            case '/':
                result += '/';
                break;
            case 'n':
                result += '\n';
                break;
            case 't':
                result += '\t';
                break;
            case 'r':
                result += '\r';
                break;
            case 'b':
                result += '\b';
                break;
            case 'f':
                result += '\f';
                break;
            default:
                throw std::runtime_error("Incomplete escape");
            }
            ++i;
        }
        else
        {
            result += input[i];
            ++i;
        }
    }
    ++i;
    return result;
}

inline std::vector<Token> tokenize(const std::string &input)
{
    std::vector<Token> tokens{};
    size_t i = 0;
    while (i < input.size())
    {
        if (std::isspace(static_cast<unsigned char>(input[i])))
        {
            ++i;
            continue;
        }
        if (input[i] == '"')
        {
            Token tok;
            tok.type = TokenType::String;
            tok.value.emplace<std::string>(read_string(input, i));
            tokens.push_back(std::move(tok));
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(input[i])) || input[i] == '-')
        {
            Token tok;
            tok.type = TokenType::Number;
            tok.value.emplace<double>(read_number(input, i));
            tokens.push_back(std::move(tok));
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(input[i])))
        {
            Token tok;
            tok.type = read_keyword(input, i);
            tokens.push_back(std::move(tok));
            continue;
        }
        switch (input[i])
        {
        case '{':
            tokens.push_back(Token{TokenType::LBrace, {}});
            ++i;
            break;
        case '}':
            tokens.push_back(Token{TokenType::RBrace, {}});
            ++i;
            break;
        case '[':
            tokens.push_back(Token{TokenType::LBracket, {}});
            ++i;
            break;
        case ']':
            tokens.push_back(Token{TokenType::RBracket, {}});
            ++i;
            break;
        case ':':
            tokens.push_back(Token{TokenType::Colon, {}});
            ++i;
            break;
        case ',':
            tokens.push_back(Token{TokenType::Comma, {}});
            ++i;
            break;
        default:
            throw std::runtime_error(std::string{"Unexpected character: "} + input[i]);
        }
    }
    tokens.push_back(Token{TokenType::EndOfInput, {}});
    return tokens;
}