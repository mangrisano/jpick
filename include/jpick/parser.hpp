// parser.hpp — recursive-descent JSON parser.
//
// Defines `Parser`, which consumes the token stream produced by the lexer and
// builds a `Value` tree. Objects and arrays are parsed recursively via
// `parse_value`.
#pragma once

#include <stdexcept>
#include "jpick/json.hpp"
#include "jpick/lexer.hpp"

namespace jpick
{

    struct Parser
    {
        const std::vector<Token> &tokens;
        std::size_t pos = 0;

        Parser(const std::vector<Token> &t) : tokens(t) {}
        const Token &peek() const { return tokens[pos]; }
        const Token &advance()
        {
            return tokens[pos++];
        }
        const Token &expect(TokenType type)
        {
            const Token &tok = advance();
            if (tok.type != type)
                throw std::runtime_error("Unexpected token");
            return tok;
        }
        Value parse_array()
        {
            expect(TokenType::LBracket);
            Array arr;
            if (peek().type == TokenType::RBracket)
            {
                advance();
                Value result;
                result.data.emplace<Array>(arr);
                return result;
            }
            while (true)
            {
                arr.push_back(parse_value());
                const Token &tok = advance();
                if (tok.type == TokenType::Comma)
                {
                    continue;
                }
                if (tok.type == TokenType::RBracket)
                {
                    break;
                }
                throw std::runtime_error("Expected ',' or ']' in the array");
            }
            Value result;
            result.data.emplace<Array>(arr);
            return result;
        }

        Value parse_object()
        {
            expect(TokenType::LBrace);
            Object obj;
            if (peek().type == TokenType::RBrace)
            {
                advance();
                Value result;
                result.data.emplace<Object>(obj);
                return result;
            }
            while (true)
            {
                const Token &key_tok = expect(TokenType::String);
                std::string key = std::get<std::string>(key_tok.value);
                expect(TokenType::Colon);
                obj[key] = parse_value();

                const Token &tok = advance();
                if (tok.type == TokenType::Comma)
                    continue;
                if (tok.type == TokenType::RBrace)
                    break;
                throw std::runtime_error("Expected ',' or '}' in object");
            }
            Value result;
            result.data.emplace<Object>(obj);
            return result;
        }
        Value parse_value()
        {
            switch (peek().type)
            {
            case TokenType::String:
            {
                const Token &tok = advance();
                Value result;
                result.data.emplace<std::string>(std::get<std::string>(tok.value));
                return result;
            }
            case TokenType::Number:
            {
                const Token &tok = advance();
                Value result;
                result.data.emplace<double>(std::get<double>(tok.value));
                return result;
            }
            case TokenType::True:
            {
                advance();
                Value result;
                result.data.emplace<bool>(true);
                return result;
            }
            case TokenType::False:
            {
                advance();
                Value result;
                result.data.emplace<bool>(false);
                return result;
            }
            case TokenType::Null:
            {
                advance();
                Value result;
                result.data.emplace<std::nullptr_t>(nullptr);
                return result;
            }
            case TokenType::LBracket:
                return parse_array();
            case TokenType::LBrace:
                return parse_object();
            default:
                throw std::runtime_error("Unexpected token");
            }
        }
    };

} // namespace jpick
