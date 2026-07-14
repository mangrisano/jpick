// serializer.hpp — turn a Value back into JSON text.
//
// Provides `escape_string` and `serialize`, which renders a `Value` as valid
// JSON either compact or pretty-printed (controlled by the `pretty` flag).
// Dispatch over the variant is done with `std::visit` and the `overloaded`
// helper defined here.
#pragma once

#include <string>
#include <charconv>
#include <stdexcept>
#include <variant>
#include "jpick/json.hpp"

namespace jpick
{

    template <class... Ts>
    struct overloaded : Ts...
    {
        using Ts::operator()...;
    };

    inline std::string escape_string(const std::string &s)
    {
        std::string out;
        for (char c : s)
        {
            switch (c)
            {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\t':
                out += "\\t";
                break;
            case '\r':
                out += "\\r";
                break;
            default:
                out += c;
                break;
            }
        }
        return out;
    }

    inline std::string serialize(const Value &value, bool pretty = false, std::size_t depth = 0)
    {
        const auto visitor = overloaded{
            [](const std::string &s) -> std::string
            {
                return "\"" + escape_string(s) + "\"";
            },
            [](bool b) -> std::string
            {
                return b ? "true" : "false";
            },
            [](double n) -> std::string
            {
                char buf[32];
                auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), n);
                return std::string(buf, ptr);
            },
            [](std::nullptr_t) -> std::string
            {
                return "null";
            },
            [&](const Array &arr) -> std::string
            {
                if (arr.empty())
                    return "[]";
                const std::string indent(depth * 2, ' ');
                const std::string inner_indent((depth + 1) * 2, ' ');
                const std::string sep = pretty ? ",\n" : ", ";
                std::string out = pretty ? "[\n" : "[";
                for (std::size_t i = 0; i < arr.size(); ++i)
                {
                    if (i > 0)
                        out += sep;
                    if (pretty)
                        out += inner_indent;
                    out += serialize(arr[i], pretty, depth + 1);
                }
                out += pretty ? "\n" + indent + "]" : "]";
                return out;
            },
            [&](const Object &obj) -> std::string
            {
                if (obj.empty())
                    return "{}";
                const std::string indent(depth * 2, ' ');
                const std::string inner_indent((depth + 1) * 2, ' ');
                const std::string sep = pretty ? ",\n" : ", ";
                std::string out = pretty ? "{\n" : "{";
                bool first = true;
                for (const auto &[key, val] : obj)
                {
                    if (!first)
                        out += sep;
                    first = false;
                    if (pretty)
                        out += inner_indent;
                    out += "\"" + escape_string(key) + "\": " + serialize(val, pretty, depth + 1);
                }
                out += pretty ? "\n" + indent + "}" : "}";
                return out;
            },
        };
        return std::visit(visitor, value.data);
    }

} // namespace jpick
