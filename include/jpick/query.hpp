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
#include <algorithm>
#include <map>
#include <charconv>
#include <cctype>
#include "jpick/json.hpp"
#include "jpick/serializer.hpp"

namespace jpick
{

    // Marker for the "[]" step: iterate over all elements of an array.
    struct Iterate
    {
    };

    // Array slice: [start:end] extracts a subarray. Negative indices count
    // from the end (-1 = last element). Omitted start defaults to 0, omitted
    // end defaults to array length.
    struct Slice
    {
        int start = 0; // -1 means "from beginning"
        int end = -1;  // -1 means "to end"
        bool has_start = false;
        bool has_end = false;
    };

    using PathStep = std::variant<std::string, std::size_t, Iterate, Slice>;

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
        for (std::size_t i = 0; i < path.size(); ++i)
        {
            const char c = path[i];
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
                else if (current.find(':') != std::string::npos)
                {
                    // Slice syntax: [start:end], [:end], [start:], or [:]
                    const std::size_t colon = current.find(':');
                    Slice slice;
                    const std::string start_str = current.substr(0, colon);
                    const std::string end_str = current.substr(colon + 1);
                    if (!start_str.empty())
                    {
                        slice.start = std::stoi(start_str);
                        slice.has_start = true;
                    }
                    if (!end_str.empty())
                    {
                        slice.end = std::stoi(end_str);
                        slice.has_end = true;
                    }
                    steps.emplace_back(std::in_place_type<Slice>, slice);
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
    // Quote-aware and paren-aware: a '|' inside a string literal (e.g. "a|b")
    // or inside parentheses (e.g. select(.a | .b)) does not split.
    inline std::vector<std::string> split_pipe(const std::string &expr)
    {
        std::vector<std::string> segments;
        std::string current;
        bool in_string = false;
        int depth = 0;
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
            else if (!in_string && (c == '(' || c == ')'))
            {
                depth += (c == '(') ? 1 : -1;
                current += c;
            }
            else if (c == '|' && !in_string && depth == 0)
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
                else if (const Slice *slice = std::get_if<Slice>(&step))
                {
                    const Array &arr = value.as_array();
                    const int len = static_cast<int>(arr.size());
                    // Compute actual start and end indices.
                    int start = slice->has_start ? slice->start : 0;
                    int end = slice->has_end ? slice->end : len;
                    // Negative indices count from the end.
                    if (start < 0)
                        start += len;
                    if (end < 0)
                        end += len;
                    // Clamp to valid range.
                    start = std::max(0, std::min(start, len));
                    end = std::max(0, std::min(end, len));
                    // Build the slice.
                    Array sliced;
                    for (int i = start; i < end; ++i)
                        sliced.push_back(arr[static_cast<std::size_t>(i)]);
                    next.push_back(Value(std::move(sliced)));
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

    // Return the length of a value: for arrays and objects the count of
    // elements, for strings the count of characters. Other types return null.
    inline Value builtin_length(const Value &value)
    {
        if (value.is_array())
            return Value(static_cast<double>(value.as_array().size()));
        if (value.is_object())
            return Value(static_cast<double>(value.as_object().size()));
        if (value.is_string())
            return Value(static_cast<double>(value.as_string().size()));
        return Value(nullptr);
    }

    // Return the keys of an object (sorted) or the indices of an array.
    inline Value builtin_keys(const Value &value)
    {
        if (value.is_object())
        {
            const Object &obj = value.as_object();
            std::vector<std::string> keys;
            keys.reserve(obj.size());
            for (const auto &[k, v] : obj)
                keys.push_back(k);
            std::sort(keys.begin(), keys.end());
            Array result;
            result.reserve(keys.size());
            for (const std::string &k : keys)
                result.push_back(Value(k));
            return Value(result);
        }
        if (value.is_array())
        {
            const Array &arr = value.as_array();
            Array result;
            result.reserve(arr.size());
            for (std::size_t i = 0; i < arr.size(); ++i)
                result.push_back(Value(static_cast<double>(i)));
            return Value(result);
        }
        throw std::runtime_error("keys only works on objects and arrays");
    }

    // Return the type of a value as a string.
    inline Value builtin_type(const Value &value)
    {
        if (value.is_null())
            return Value("null");
        if (value.is_bool())
            return Value("boolean");
        if (value.is_number())
            return Value("number");
        if (value.is_string())
            return Value("string");
        if (value.is_array())
            return Value("array");
        if (value.is_object())
            return Value("object");
        throw std::runtime_error("Unknown type");
    }

    // Negate a boolean value. Other types throw an error.
    inline Value builtin_not(const Value &value)
    {
        if (value.is_bool())
            return Value(!value.as_bool());
        throw std::runtime_error("not only works on booleans");
    }

    // Sort the keys of an object into a new vector. Helper for value_less.
    inline std::vector<std::string> as_sorted_keys(const Value &value)
    {
        std::vector<std::string> keys;
        for (const auto &[k, v] : value.as_object())
            keys.push_back(k);
        std::sort(keys.begin(), keys.end());
        return keys;
    }

    // Order rank of each JSON type, following jq's total ordering:
    // null < booleans < numbers < strings < arrays < objects.
    inline int type_rank(const Value &value)
    {
        if (value.is_null())
            return 0;
        if (value.is_bool())
            return 1;
        if (value.is_number())
            return 2;
        if (value.is_string())
            return 3;
        if (value.is_array())
            return 4;
        return 5; // object
    }

    // Total ordering over JSON values, matching jq. Used by sort, unique,
    // min and max. Values of different types are ordered by `type_rank`;
    // within a type they compare naturally (arrays and objects recursively).
    inline bool value_less(const Value &a, const Value &b)
    {
        const int ra = type_rank(a);
        const int rb = type_rank(b);
        if (ra != rb)
            return ra < rb;
        switch (ra)
        {
        case 0: // null
            return false;
        case 1: // bool: false < true
            return !a.as_bool() && b.as_bool();
        case 2: // number
            return a.as_number() < b.as_number();
        case 3: // string
            return a.as_string() < b.as_string();
        case 4: // array: element-wise, then by length
        {
            const Array &aa = a.as_array();
            const Array &ba = b.as_array();
            const std::size_t n = std::min(aa.size(), ba.size());
            for (std::size_t i = 0; i < n; ++i)
            {
                if (value_less(aa[i], ba[i]))
                    return true;
                if (value_less(ba[i], aa[i]))
                    return false;
            }
            return aa.size() < ba.size();
        }
        default: // object: compare sorted key lists, then values in that order
        {
            std::vector<std::string> ak = as_sorted_keys(a);
            std::vector<std::string> bk = as_sorted_keys(b);
            if (ak != bk)
                return ak < bk;
            for (const std::string &k : ak)
            {
                const Value av = a[k];
                const Value bv = b[k];
                if (value_less(av, bv))
                    return true;
                if (value_less(bv, av))
                    return false;
            }
            return false;
        }
        }
    }

    // Sum an array of numbers, concatenate an array of strings or arrays, or
    // merge an array of objects (later keys win). An empty array yields null.
    inline Value builtin_add(const Value &value)
    {
        const Array &arr = value.as_array();
        if (arr.empty())
            return Value(nullptr);
        if (arr[0].is_number())
        {
            double sum = 0;
            for (const Value &v : arr)
                sum += v.as_number();
            return Value(sum);
        }
        if (arr[0].is_string())
        {
            std::string out;
            for (const Value &v : arr)
                out += v.as_string();
            return Value(out);
        }
        if (arr[0].is_array())
        {
            Array out;
            for (const Value &v : arr)
            {
                const Array &inner = v.as_array();
                out.insert(out.end(), inner.begin(), inner.end());
            }
            return Value(std::move(out));
        }
        if (arr[0].is_object())
        {
            Object out;
            for (const Value &v : arr)
                for (const auto &[k, val] : v.as_object())
                {
                    bool found = false;
                    for (auto &pair : out)
                        if (pair.first == k)
                        {
                            pair.second = val;
                            found = true;
                            break;
                        }
                    if (!found)
                        out.emplace_back(k, val);
                }
            return Value(std::move(out));
        }
        throw std::runtime_error("Cannot add these values");
    }

    // Return the array sorted in ascending order (jq's total ordering).
    inline Value builtin_sort(const Value &value)
    {
        Array arr = value.as_array();
        std::sort(arr.begin(), arr.end(), value_less);
        return Value(std::move(arr));
    }

    // Return the array sorted with duplicate values removed.
    inline Value builtin_unique(const Value &value)
    {
        Array arr = value.as_array();
        std::sort(arr.begin(), arr.end(), value_less);
        arr.erase(std::unique(arr.begin(), arr.end(),
                              [](const Value &a, const Value &b)
                              { return a == b; }),
                  arr.end());
        return Value(std::move(arr));
    }

    // Reverse an array or a string.
    inline Value builtin_reverse(const Value &value)
    {
        if (value.is_string())
        {
            std::string s = value.as_string();
            std::reverse(s.begin(), s.end());
            return Value(s);
        }
        Array arr = value.as_array();
        std::reverse(arr.begin(), arr.end());
        return Value(std::move(arr));
    }

    // Smallest element of an array (jq ordering). Empty array yields null.
    inline Value builtin_min(const Value &value)
    {
        const Array &arr = value.as_array();
        if (arr.empty())
            return Value(nullptr);
        const Value *m = &arr[0];
        for (const Value &v : arr)
            if (value_less(v, *m))
                m = &v;
        return *m;
    }

    // Largest element of an array (jq ordering). Empty array yields null.
    inline Value builtin_max(const Value &value)
    {
        const Array &arr = value.as_array();
        if (arr.empty())
            return Value(nullptr);
        const Value *m = &arr[0];
        for (const Value &v : arr)
            if (value_less(*m, v))
                m = &v;
        return *m;
    }

    // First element of an array. Empty array yields null.
    inline Value builtin_first(const Value &value)
    {
        const Array &arr = value.as_array();
        if (arr.empty())
            return Value(nullptr);
        return arr.front();
    }

    // Last element of an array. Empty array yields null.
    inline Value builtin_last(const Value &value)
    {
        const Array &arr = value.as_array();
        if (arr.empty())
            return Value(nullptr);
        return arr.back();
    }

    // Test whether an object has a given key. The argument must be a literal
    // string in the form has("key"). Arrays and non-objects return false.
    inline Value builtin_has(const Value &value, const std::string &key)
    {
        if (!value.is_object())
            return Value(false);
        const Object &obj = value.as_object();
        for (const auto &[k, v] : obj)
            if (k == key)
                return Value(true);
        return Value(false);
    }

    // Parse the quoted string argument of a call like name("arg"), e.g.
    // has("key") or join(", "). Returns the unquoted argument.
    inline std::string parse_string_arg(const std::string &segment, const std::string &name)
    {
        const std::string prefix = name + "(";
        const std::string usage = name + "() expects a string argument: " + name + "(\"...\")";
        if (segment.compare(0, prefix.size(), prefix) != 0 || segment.back() != ')')
            throw std::runtime_error(usage);
        const std::string inner = segment.substr(prefix.size(), segment.size() - prefix.size() - 1);
        if (inner.size() < 2 || inner.front() != '"' || inner.back() != '"')
            throw std::runtime_error(usage);
        return inner.substr(1, inner.size() - 2); // strip quotes
    }

    // Join an array into a string, separating elements with `sep`. Strings are
    // used as-is, null becomes empty, numbers and bools are stringified; arrays
    // and objects are rejected (like jq).
    inline Value builtin_join(const Value &value, const std::string &sep)
    {
        const Array &arr = value.as_array();
        std::string out;
        for (std::size_t i = 0; i < arr.size(); ++i)
        {
            if (i > 0)
                out += sep;
            const Value &v = arr[i];
            if (v.is_string())
                out += v.as_string();
            else if (v.is_null())
                continue; // null contributes nothing
            else if (v.is_number() || v.is_bool())
                out += serialize(v);
            else
                throw std::runtime_error("join: array elements must be scalars");
        }
        return Value(out);
    }

    // Split a string into an array of substrings on each occurrence of `sep`.
    inline Value builtin_split(const Value &value, const std::string &sep)
    {
        const std::string &s = value.as_string();
        if (sep.empty())
            throw std::runtime_error("split: separator cannot be empty");
        Array out;
        std::size_t prev = 0;
        std::size_t pos;
        while ((pos = s.find(sep, prev)) != std::string::npos)
        {
            out.push_back(Value(s.substr(prev, pos - prev)));
            prev = pos + sep.size();
        }
        out.push_back(Value(s.substr(prev)));
        return Value(std::move(out));
    }

    // Table of unary builtins with signature Value(const Value&). A pipe
    // segment matching one of these names is applied to every value in the
    // stream. Builtins that don't fit this shape (empty, has, join, split)
    // stay special-cased in query_pipe.
    inline const std::map<std::string, Value (*)(const Value &)> &unary_builtins()
    {
        static const std::map<std::string, Value (*)(const Value &)> table = {
            {"length", builtin_length},
            {"keys", builtin_keys},
            {"type", builtin_type},
            {"not", builtin_not},
            {"add", builtin_add},
            {"sort", builtin_sort},
            {"unique", builtin_unique},
            {"reverse", builtin_reverse},
            {"min", builtin_min},
            {"max", builtin_max},
            {"first", builtin_first},
            {"last", builtin_last},
        };
        return table;
    }
    // Split a segment on the top-level alternative operator '//'. Quote-aware
    // and paren-aware, so a '//' inside a string literal or inside parentheses
    // (e.g. select(.a // 0)) does not split. A segment without a top-level
    // '//' yields a single element.
    inline std::vector<std::string> split_alternative(const std::string &segment)
    {
        std::vector<std::string> parts;
        std::string current;
        bool in_string = false;
        int depth = 0;
        for (std::size_t i = 0; i < segment.size(); ++i)
        {
            const char c = segment[i];
            if (c == '"')
            {
                in_string = !in_string;
                current += c;
            }
            else if (c == '\\' && in_string && i + 1 < segment.size())
            {
                current += c;
                current += segment[i + 1];
                ++i;
            }
            else if (!in_string && (c == '(' || c == ')'))
            {
                depth += (c == '(') ? 1 : -1;
                current += c;
            }
            else if (c == '/' && !in_string && depth == 0 && i + 1 < segment.size() && segment[i + 1] == '/')
            {
                parts.push_back(trim(current));
                current.clear();
                ++i; // skip the second '/'
            }
            else
            {
                current += c;
            }
        }
        parts.push_back(trim(current));
        return parts;
    }

    // Recognize a JSON scalar literal segment (true, false, null, or a number)
    // so it can be used as a value, e.g. the fallback in `.a // 0`. Returns
    // true and sets `out` on success; leaves paths and words untouched.
    inline bool try_scalar_literal(const std::string &segment, Value &out)
    {
        if (segment == "true")
        {
            out = Value(true);
            return true;
        }
        if (segment == "false")
        {
            out = Value(false);
            return true;
        }
        if (segment == "null")
        {
            out = Value(nullptr);
            return true;
        }
        if (!segment.empty() &&
            (std::isdigit(static_cast<unsigned char>(segment.front())) || segment.front() == '-'))
        {
            double number = 0;
            const char *first = segment.data();
            const char *last = first + segment.size();
            auto [ptr, ec] = std::from_chars(first, last, number);
            if (ec == std::errc() && ptr == last)
            {
                out = Value(number);
                return true;
            }
        }
        return false;
    }

    // True for values jq treats as "present" in select and the alternative
    // operator: everything except null and false.
    inline bool is_truthy(const Value &value)
    {
        return !(value.is_null() || (value.is_bool() && !value.as_bool()));
    }

    // Extract the raw argument expression of a call like select(.active),
    // i.e. everything between the outermost parentheses.
    inline std::string parse_call_arg(const std::string &segment, const std::string &name)
    {
        const std::string prefix = name + "(";
        if (segment.compare(0, prefix.size(), prefix) != 0 || segment.back() != ')')
            throw std::runtime_error(name + "() is missing its argument: " + name + "(...)");
        return segment.substr(prefix.size(), segment.size() - prefix.size() - 1);
    }

    // Evaluate a single simple segment (no top-level '|' or '//') against one
    // value. A segment starting with '"' is a string literal evaluated by
    // interpolate; '@' is a format like @csv; true/false/null/a number is a
    // scalar literal; select(expr) keeps the value when expr is truthy; a plain
    // word may be a builtin; a name("arg") is a builtin with an argument;
    // everything else is a navigation path.
    inline std::vector<Value> eval_simple(const Value &value, const std::string &segment)
    {
        if (!segment.empty() && segment.front() == '"')
            return {Value(interpolate(segment, value))};
        if (!segment.empty() && segment.front() == '@')
            return {apply_format(segment, value)};
        if (Value literal; try_scalar_literal(segment, literal))
            return {literal};
        if (auto it = unary_builtins().find(segment); it != unary_builtins().end())
            return {it->second(value)};
        if (segment == "empty")
            return {};
        if (segment.rfind("select(", 0) == 0)
        {
            const std::vector<Value> results = query_pipe(value, parse_call_arg(segment, "select"));
            for (const Value &result : results)
                if (is_truthy(result))
                    return {value}; // at least one truthy result: keep the value
            return {};              // otherwise drop it
        }
        if (segment.rfind("has(", 0) == 0)
            return {builtin_has(value, parse_string_arg(segment, "has"))};
        if (segment.rfind("join(", 0) == 0)
            return {builtin_join(value, parse_string_arg(segment, "join"))};
        if (segment.rfind("split(", 0) == 0)
            return {builtin_split(value, parse_string_arg(segment, "split"))};
        return query_path(value, split_path(segment));
    }

    // Evaluate a full pipe expression: each '|'-separated segment is applied to
    // every value produced by the previous one, flattening the results into one
    // stream. Within a segment, the alternative operator '//' picks the first
    // alternative that yields a value other than null or false, otherwise it
    // falls back to the last alternative (errors in earlier alternatives are
    // ignored, like jq).
    inline std::vector<Value> query_pipe(const Value &root, const std::string &expr)
    {
        std::vector<Value> stream = {root};
        for (const std::string &segment : split_pipe(expr))
        {
            const std::vector<std::string> alternatives = split_alternative(segment);
            std::vector<Value> next;
            for (const Value &value : stream)
            {
                if (alternatives.size() == 1)
                {
                    std::vector<Value> results = eval_simple(value, segment);
                    next.insert(next.end(), results.begin(), results.end());
                    continue;
                }
                for (std::size_t i = 0; i < alternatives.size(); ++i)
                {
                    const bool last = (i + 1 == alternatives.size());
                    std::vector<Value> results;
                    try
                    {
                        results = eval_simple(value, alternatives[i]);
                    }
                    catch (const std::exception &)
                    {
                        if (last)
                            throw; // an error in the final fallback still surfaces
                        continue;  // skip a failing earlier alternative
                    }
                    std::vector<Value> present;
                    for (const Value &v : results)
                        if (is_truthy(v))
                            present.push_back(v);
                    if (!present.empty())
                    {
                        next.insert(next.end(), present.begin(), present.end());
                        break;
                    }
                    if (last) // nothing present anywhere: yield the fallback as-is
                        next.insert(next.end(), results.begin(), results.end());
                }
            }
            stream = std::move(next);
        }
        return stream;
    }

} // namespace jpick