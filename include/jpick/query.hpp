// query.hpp — path parsing and tree navigation.
//
// Parses a path expression like `.a.b[0]` into a list of `PathStep`s
// (`split_path`) and walks a `Value` tree accordingly (`query` for object
// keys, `query_index` for array indices, `query_path` for a full path).
#pragma once

#include <string>
#include <stdexcept>
#include <vector>
#include <variant>
#include "jpick/json.hpp"
#include "jpick/serializer.hpp"

namespace jpick
{

    // Marker for the "[]" step: iterate over all elements of an array.
    struct Iterate
    {
    };

    using PathStep = std::variant<std::string, std::size_t, Iterate>;

    inline Value query(const Value &root, const std::string &key)
    {
        return root[key];
    }

    inline Value query_index(const Value &root, std::size_t index)
    {
        return root[index];
    }

    inline std::vector<PathStep> split_path(const std::string &path)
    {
        std::vector<PathStep> steps;
        std::string current;
        for (char c : path)
        {
            if (c == '.' || c == '[')
            {
                if (!current.empty())
                {
                    steps.emplace_back(std::in_place_type<std::string>, current);
                    current.clear();
                }
            }
            else if (c == ']')
            {
                if (current.empty())
                {
                    steps.emplace_back(std::in_place_type<Iterate>);
                }
                else
                {
                    std::size_t index = std::stoul(current);
                    steps.emplace_back(std::in_place_type<std::size_t>, index);
                }
                current.clear();
            }
            else
            {
                current += c;
            }
        }
        if (!current.empty())
            steps.emplace_back(std::in_place_type<std::string>, current);
        return steps;
    }

    // Remove leading and trailing whitespace from a string.
    inline std::string trim(const std::string &s)
    {
        const std::size_t start = s.find_first_not_of(" \t\n\r");
        if (start == std::string::npos)
            return "";
        const std::size_t end = s.find_last_not_of(" \t\n\r");
        return s.substr(start, end - start + 1);
    }

    // Split a pipe expression like ".a | .b" into its trimmed segments.
    // Quote-aware: a '|' inside a string literal (e.g. "a|b" or an
    // interpolation "\(.a | .b)") does not split the expression.
    inline std::vector<std::string> split_pipe(const std::string &expr)
    {
        std::vector<std::string> segments;
        std::string current;
        bool in_string = false;
        for (std::size_t i = 0; i < expr.size(); ++i)
        {
            const char c = expr[i];
            if (c == '"')
            {
                in_string = !in_string;
                current += c;
            }
            else if (c == '\\' && in_string && i + 1 < expr.size())
            {
                // Keep the escape pair intact so that \" does not
                // wrongly toggle the in_string state.
                current += c;
                current += expr[i + 1];
                ++i;
            }
            else if (c == '|' && !in_string)
            {
                segments.push_back(trim(current));
                current.clear();
            }
            else
            {
                current += c;
            }
        }
        segments.push_back(trim(current));
        return segments;
    }

    inline std::vector<Value> query_path(const Value &root, const std::vector<PathStep> &steps)
    {
        std::vector<Value> current = {root};
        for (const PathStep &step : steps)
        {
            std::vector<Value> next;
            for (const Value &value : current)
            {
                if (const std::string *key = std::get_if<std::string>(&step))
                {
                    next.push_back(value[*key]);
                }
                else if (const std::size_t *index = std::get_if<std::size_t>(&step))
                {
                    next.push_back(value[*index]);
                }
                else // Iterate: expand the array into its elements
                {
                    for (const Value &element : value.as_array())
                        next.push_back(element);
                }
            }
            current = std::move(next);
        }
        return current;
    }

    // Render a value the way string interpolation expects it: a string is
    // emitted raw (without surrounding quotes), everything else is serialized.
    inline std::string raw_value(const Value &value)
    {
        if (value.is_string())
            return value.as_string();
        return serialize(value);
    }

    // RFC 4648 base64 encoding of an arbitrary byte string. Uses a bit
    // accumulator symmetric with base64_decode: every input byte adds 8 bits
    // and each full group of 6 bits emits one character; leftover bits are
    // zero-padded and the output is padded with '=' to a multiple of four.
    inline std::string base64_encode(const std::string &in)
    {
        static const char table[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((in.size() + 2) / 3) * 4);
        unsigned buffer = 0;
        int bits = 0;
        for (unsigned char c : in)
        {
            buffer = (buffer << 8) | c;
            bits += 8;
            while (bits >= 6)
            {
                bits -= 6;
                out += table[(buffer >> bits) & 63];
            }
        }
        if (bits > 0)
            out += table[(buffer << (6 - bits)) & 63];
        while (out.size() % 4 != 0)
            out += '=';
        return out;
    }

    // RFC 4648 base64 decoding. Whitespace is ignored, '=' ends the data,
    // any other character is rejected. Uses a 6-bit accumulator: every input
    // character adds 6 bits and each full group of 8 bits emits one byte.
    inline std::string base64_decode(const std::string &in)
    {
        const auto sextet = [](char c) -> int
        {
            if (c >= 'A' && c <= 'Z')
                return c - 'A';
            if (c >= 'a' && c <= 'z')
                return c - 'a' + 26;
            if (c >= '0' && c <= '9')
                return c - '0' + 52;
            if (c == '+')
                return 62;
            if (c == '/')
                return 63;
            return -1;
        };

        std::string out;
        unsigned buffer = 0;
        int bits = 0;
        for (char c : in)
        {
            if (c == '=')
                break;
            if (c == '\n' || c == '\r' || c == '\t' || c == ' ')
                continue;
            const int value = sextet(c);
            if (value < 0)
                throw std::runtime_error("Invalid base64 input");
            buffer = (buffer << 6) | static_cast<unsigned>(value);
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                out += static_cast<char>((buffer >> bits) & 0xFF);
            }
        }
        return out;
    }

    // Percent-encode a string for use in a URI (RFC 3986). The unreserved
    // characters A-Z a-z 0-9 - _ . ~ are kept; everything else becomes %XX.
    inline std::string uri_encode(const std::string &in)
    {
        static const char hex[] = "0123456789ABCDEF";
        std::string out;
        for (char c : in)
        {
            const unsigned char uc = static_cast<unsigned char>(c);
            if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') ||
                (uc >= '0' && uc <= '9') || uc == '-' || uc == '_' ||
                uc == '.' || uc == '~')
            {
                out += c;
            }
            else
            {
                out += '%';
                out += hex[uc >> 4];
                out += hex[uc & 0x0F];
            }
        }
        return out;
    }

    // Join the elements of an array into one line, rendering each with `field`
    // and separating them with `sep`. Shared by @sh, @csv and @tsv.
    inline std::string join_row(const Array &arr, const std::string &sep,
                                std::string (*field)(const Value &))
    {
        std::string out;
        for (std::size_t i = 0; i < arr.size(); ++i)
        {
            if (i > 0)
                out += sep;
            out += field(arr[i]);
        }
        return out;
    }

    // Escape a single scalar for a POSIX shell: strings are single-quoted
    // (inner ' becomes '\''), numbers and bools are emitted as-is. null,
    // arrays and objects cannot be escaped this way.
    inline std::string sh_scalar(const Value &value)
    {
        if (value.is_string())
        {
            std::string out = "'";
            for (char c : value.as_string())
            {
                if (c == '\'')
                    out += "'\\''";
                else
                    out += c;
            }
            out += "'";
            return out;
        }
        if (value.is_bool() || value.is_number())
            return serialize(value);
        throw std::runtime_error("Cannot escape this value for the shell");
    }

    // @sh over an array escapes each element and joins them with spaces;
    // over a single scalar it escapes just that value.
    inline std::string sh_format(const Value &value)
    {
        if (value.is_array())
            return join_row(value.as_array(), " ", sh_scalar);
        return sh_scalar(value);
    }

    // Render one field of an @csv / @tsv row. Scalars only: strings are
    // escaped, null becomes an empty field, arrays/objects are rejected.
    inline std::string csv_field(const Value &value)
    {
        if (value.is_string())
        {
            std::string out = "\"";
            for (char c : value.as_string())
            {
                if (c == '"')
                    out += "\"\"";
                else
                    out += c;
            }
            out += "\"";
            return out;
        }
        if (value.is_null())
            return "";
        if (value.is_bool() || value.is_number())
            return serialize(value);
        throw std::runtime_error("Cannot format an array or object as CSV");
    }

    inline std::string tsv_field(const Value &value)
    {
        if (value.is_string())
        {
            std::string out;
            for (char c : value.as_string())
            {
                switch (c)
                {
                case '\t':
                    out += "\\t";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                default:
                    out += c;
                    break;
                }
            }
            return out;
        }
        if (value.is_null())
            return "";
        if (value.is_bool() || value.is_number())
            return serialize(value);
        throw std::runtime_error("Cannot format an array or object as TSV");
    }

    // Apply a @format segment (e.g. @csv) to a single value, producing a
    // string value. @csv/@tsv expect an array of scalars.
    inline Value apply_format(const std::string &fmt, const Value &value)
    {
        if (fmt == "@text")
            return Value(raw_value(value));
        if (fmt == "@json")
            return Value(serialize(value));
        if (fmt == "@base64")
            return Value(base64_encode(raw_value(value)));
        if (fmt == "@base64d")
            return Value(base64_decode(raw_value(value)));
        if (fmt == "@uri")
            return Value(uri_encode(raw_value(value)));
        if (fmt == "@sh")
            return Value(sh_format(value));
        if (fmt == "@csv")
            return Value(join_row(value.as_array(), ",", csv_field));
        if (fmt == "@tsv")
            return Value(join_row(value.as_array(), "\t", tsv_field));
        throw std::runtime_error("Unknown format: " + fmt);
    }

    // Forward declaration: interpolate evaluates the inner \( ... )
    // expressions with the full pipe machinery.
    inline std::vector<Value> query_pipe(const Value &root, const std::string &expr);

    // Evaluate a string-literal segment such as "\(.name): \(.count)" against
    // a single value. `literal` still carries its surrounding double quotes.
    // Each \( ... ) is replaced by the raw form of the value it produces.
    inline std::string interpolate(const std::string &literal, const Value &value)
    {
        if (literal.size() < 2 || literal.front() != '"' || literal.back() != '"')
            throw std::runtime_error("Unterminated string literal");

        std::string out;
        for (std::size_t i = 1; i + 1 < literal.size(); ++i)
        {
            const char c = literal[i];
            if (c != '\\')
            {
                out += c;
                continue;
            }

            // A backslash cannot be the last character before the closing quote.
            if (i + 1 == literal.size() - 1)
                throw std::runtime_error("Invalid escape in string literal");

            const char next = literal[i + 1];
            if (next == '(')
            {
                const std::size_t close = literal.find(')', i + 2);
                if (close == std::string::npos)
                    throw std::runtime_error("String interpolation is missing ')'");
                const std::string inner = literal.substr(i + 2, close - (i + 2));
                std::vector<Value> results = query_pipe(value, inner);
                if (results.size() != 1)
                    throw std::runtime_error("String interpolation must produce exactly one value");
                out += raw_value(results[0]);
                i = close; // the loop's ++i moves past ')'
            }
            else
            {
                switch (next)
                {
                case 'n':
                    out += '\n';
                    break;
                case 't':
                    out += '\t';
                    break;
                case 'r':
                    out += '\r';
                    break;
                case '"':
                    out += '"';
                    break;
                case '\\':
                    out += '\\';
                    break;
                default:
                    throw std::runtime_error("Unknown escape in string literal");
                }
                ++i; // skip the escaped character
            }
        }
        return out;
    }

    // Evaluate a full pipe expression: each segment is applied to every value
    // produced by the previous one, flattening the results into one stream.
    // A segment starting with '"' is a string literal evaluated by interpolate;
    // any other segment is a navigation path.
    inline std::vector<Value> query_pipe(const Value &root, const std::string &expr)
    {
        std::vector<Value> stream = {root};
        for (const std::string &segment : split_pipe(expr))
        {
            std::vector<Value> next;
            if (!segment.empty() && segment.front() == '"')
            {
                for (const Value &value : stream)
                    next.push_back(Value(interpolate(segment, value)));
            }
            else if (!segment.empty() && segment.front() == '@')
            {
                for (const Value &value : stream)
                    next.push_back(apply_format(segment, value));
            }
            else
            {
                const std::vector<PathStep> steps = split_path(segment);
                for (const Value &value : stream)
                {
                    std::vector<Value> results = query_path(value, steps);
                    next.insert(next.end(), results.begin(), results.end());
                }
            }
            stream = std::move(next);
        }
        return stream;
    }

} // namespace jpick