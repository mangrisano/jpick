// lexer.hpp — JSON tokenizer.
//
// Turns raw JSON text into a sequence of `Token`s. Defines `TokenType`,
// `Token`, and `tokenize()`, which handles punctuation, strings (with escape
// sequences), numbers, and the `true`/`false`/`null` keywords.
#pragma once

#include <vector>
#include <variant>
#include <string>
#include <cctype>
#include <stdexcept>
#include <charconv>

namespace jpick
{

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

    // Read the four hex digits of a \uXXXX escape. `i` points at the 'u'; on
    // return it points at the last hex digit consumed.
    inline unsigned read_hex4(const std::string &input, std::size_t &i)
    {
        if (i + 4 >= input.size())
            throw std::runtime_error("Incomplete \\u escape");
        unsigned value = 0;
        for (int k = 0; k < 4; ++k)
        {
            const char c = input[i + 1 + static_cast<std::size_t>(k)];
            value <<= 4;
            if (c >= '0' && c <= '9')
                value |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f')
                value |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                value |= static_cast<unsigned>(c - 'A' + 10);
            else
                throw std::runtime_error("Invalid \\u escape");
        }
        i += 4;
        return value;
    }

    // Append the UTF-8 encoding of a Unicode code point to `out`.
    inline void append_utf8(std::string &out, unsigned cp)
    {
        if (cp <= 0x7F)
        {
            out += static_cast<char>(cp);
        }
        else if (cp <= 0x7FF)
        {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else if (cp <= 0xFFFF)
        {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
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
                case 'u':
                {
                    unsigned cp = read_hex4(input, i); // i now at last hex digit
                    if (cp >= 0xD800 && cp <= 0xDBFF)
                    {
                        // High surrogate: expect a following \uXXXX low surrogate.
                        if (i + 2 >= input.size() || input[i + 1] != '\\' || input[i + 2] != 'u')
                            throw std::runtime_error("Unpaired surrogate in \\u escape");
                        i += 2; // move to the 'u' of the low surrogate
                        const unsigned lo = read_hex4(input, i);
                        if (lo < 0xDC00 || lo > 0xDFFF)
                            throw std::runtime_error("Invalid low surrogate in \\u escape");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    else if (cp >= 0xDC00 && cp <= 0xDFFF)
                    {
                        throw std::runtime_error("Unexpected low surrogate in \\u escape");
                    }
                    append_utf8(result, cp);
                    break;
                }
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

} // namespace jpick