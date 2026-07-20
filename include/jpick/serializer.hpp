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
#include <algorithm>
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

    // Repeat `unit` `times` times, e.g. the indentation for a nesting level.
    inline std::string repeat(const std::string &unit, std::size_t times)
    {
        std::string out;
        out.reserve(unit.size() * times);
        for (std::size_t i = 0; i < times; ++i)
            out += unit;
        return out;
    }

    // Options controlling how a Value is rendered as JSON text.
    struct SerializeOptions
    {
        bool pretty = false;            // indent nested structures over lines
        std::string indent_unit = "  "; // one nesting level of indentation
        bool sort_keys = false;         // emit object keys in ascending order
    };

    // Serialize `value` as JSON according to `opts` (see SerializeOptions).
    inline std::string serialize(const Value &value, const SerializeOptions &opts = {},
                                 std::size_t depth = 0)
    {
        const bool pretty = opts.pretty;
        const std::string &indent_unit = opts.indent_unit;
        const bool sort_keys = opts.sort_keys;
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
                const std::string indent = repeat(indent_unit, depth);
                const std::string inner_indent = repeat(indent_unit, depth + 1);
                const std::string sep = pretty ? ",\n" : ", ";
                std::string out = pretty ? "[\n" : "[";
                for (std::size_t i = 0; i < arr.size(); ++i)
                {
                    if (i > 0)
                        out += sep;
                    if (pretty)
                        out += inner_indent;
                    out += serialize(arr[i], opts, depth + 1);
                }
                out += pretty ? "\n" + indent + "]" : "]";
                return out;
            },
            [&](const Object &obj) -> std::string
            {
                if (obj.empty())
                    return "{}";
                const std::string indent = repeat(indent_unit, depth);
                const std::string inner_indent = repeat(indent_unit, depth + 1);
                const std::string sep = pretty ? ",\n" : ", ";
                std::string out = pretty ? "{\n" : "{";

                // Emit keys sorted or in insertion order.
                const Object *entries = &obj;
                Object sorted;
                if (sort_keys)
                {
                    sorted = obj;
                    std::sort(sorted.begin(), sorted.end(),
                              [](const auto &a, const auto &b)
                              { return a.first < b.first; });
                    entries = &sorted;
                }

                bool first = true;
                for (const auto &[key, val] : *entries)
                {
                    if (!first)
                        out += sep;
                    first = false;
                    if (pretty)
                        out += inner_indent;
                    out += "\"" + escape_string(key) + "\": " + serialize(val, opts, depth + 1);
                }
                out += pretty ? "\n" + indent + "}" : "}";
                return out;
            },
        };
        return std::visit(visitor, value.data);
    }

} // namespace jpick
