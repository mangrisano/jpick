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
#include "jpick/lexer.hpp"
#include "jpick/parser.hpp"
#include "jpick/query/format.hpp"
#include "jpick/query/builtins.hpp"

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

    // Split `expr` into trimmed segments at every top-level position where
    // `is_delim(expr, i)` returns a non-zero delimiter length. Positions inside
    // a string literal or inside () / [] nesting are skipped, so a delimiter
    // there (e.g. the '|' in "a|b" or in select(.a | .b)) does not split.
    template <class IsDelim>
    inline std::vector<std::string> split_on(const std::string &expr, IsDelim is_delim)
    {
        std::vector<std::string> segments;
        std::string current;
        bool in_string = false;
        int depth = 0;
        for (std::size_t i = 0; i < expr.size(); ++i)
        {
            const char c = expr[i];
            if (in_string)
            {
                current += c;
                if (c == '\\' && i + 1 < expr.size())
                {
                    // Keep the escape pair intact so \" does not end the string.
                    current += expr[i + 1];
                    ++i;
                }
                else if (c == '"')
                {
                    in_string = false;
                }
                continue;
            }
            if (c == '"')
            {
                in_string = true;
                current += c;
            }
            else if (c == '(' || c == '[')
            {
                ++depth;
                current += c;
            }
            else if (c == ')' || c == ']')
            {
                --depth;
                current += c;
            }
            else if (const std::size_t len = depth == 0 ? is_delim(expr, i) : 0; len > 0)
            {
                segments.push_back(trim(current));
                current.clear();
                i += len - 1; // skip the rest of a multi-character delimiter
            }
            else
            {
                current += c;
            }
        }
        segments.push_back(trim(current));
        return segments;
    }

    // Split a pipe expression like ".a | .b" into its trimmed segments; a '|'
    // inside a string literal or parentheses does not split.
    inline std::vector<std::string> split_pipe(const std::string &expr)
    {
        return split_on(expr, [](const std::string &e, std::size_t i) -> std::size_t
                        { return e[i] == '|' ? 1 : 0; });
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
                // Find the matching ')' by tracking nesting depth so inner
                // calls like map(...) don't close the interpolation early.
                // Parentheses inside string literals are ignored.
                std::size_t depth = 1;
                bool in_string = false;
                std::size_t close = i + 2;
                for (; close + 1 < literal.size(); ++close)
                {
                    const char e = literal[close];
                    if (in_string)
                    {
                        if (e == '\\')
                            ++close; // skip the escaped character
                        else if (e == '"')
                            in_string = false;
                        continue;
                    }
                    if (e == '"')
                        in_string = true;
                    else if (e == '(')
                        ++depth;
                    else if (e == ')' && --depth == 0)
                        break;
                }
                if (depth != 0)
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
    // Split a segment on the top-level alternative operator '//'. Quote-aware
    // and paren-aware, so a '//' inside a string literal or inside parentheses
    // (e.g. select(.a // 0)) does not split. A segment without a top-level
    // '//' yields a single element.
    inline std::vector<std::string> split_alternative(const std::string &segment)
    {
        return split_on(segment, [](const std::string &e, std::size_t i) -> std::size_t
                        { return e[i] == '/' && i + 1 < e.size() && e[i + 1] == '/' ? 2 : 0; });
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

    // The comparison operators jpick understands, longest first so that "==" is
    // matched before a stray "=" and "<="/">=" before "<"/">".
    inline const std::vector<std::string> &comparison_operators()
    {
        static const std::vector<std::string> ops = {"==", "!=", "<=", ">=", "<", ">"};
        return ops;
    }

    // Find the first top-level comparison operator in a segment, ignoring any
    // inside string literals, parentheses or brackets. Returns its position and
    // the operator text, or {npos, ""} when the segment has no comparison.
    inline std::pair<std::size_t, std::string> find_comparison(const std::string &segment)
    {
        bool in_string = false;
        int depth = 0;
        for (std::size_t i = 0; i < segment.size(); ++i)
        {
            const char c = segment[i];
            if (c == '"')
                in_string = !in_string;
            else if (c == '\\' && in_string && i + 1 < segment.size())
                ++i; // keep an escaped char from toggling the string state
            else if (!in_string && (c == '(' || c == '['))
                ++depth;
            else if (!in_string && (c == ')' || c == ']'))
                --depth;
            else if (!in_string && depth == 0)
            {
                for (const std::string &op : comparison_operators())
                    if (segment.compare(i, op.size(), op) == 0)
                        return {i, op};
            }
        }
        return {std::string::npos, ""};
    }

    // Apply a comparison operator to two evaluated operands. Equality uses
    // Value::operator== (order-independent for objects); ordering uses
    // value_less (jq's total order).
    inline Value compare_values(const Value &lhs, const std::string &op, const Value &rhs)
    {
        if (op == "==")
            return Value(lhs == rhs);
        if (op == "!=")
            return Value(!(lhs == rhs));
        if (op == "<")
            return Value(value_less(lhs, rhs));
        if (op == ">")
            return Value(value_less(rhs, lhs));
        if (op == "<=")
            return Value(!value_less(rhs, lhs));
        return Value(!value_less(lhs, rhs)); // ">="
    }

    // Return true if `segment` is a single [ ... ] array constructor, i.e. the
    // opening '[' at the front is closed only by the final ']' (so `[.a]` is a
    // constructor but `[.a] == [.b]` and `.a[0]` are not).
    inline bool is_array_construction(const std::string &segment)
    {
        if (segment.size() < 2 || segment.front() != '[')
            return false;
        bool in_string = false;
        int depth = 0;
        for (std::size_t i = 0; i < segment.size(); ++i)
        {
            const char c = segment[i];
            if (c == '"')
                in_string = !in_string;
            else if (c == '\\' && in_string && i + 1 < segment.size())
                ++i;
            else if (!in_string && (c == '[' || c == '('))
                ++depth;
            else if (!in_string && (c == ']' || c == ')'))
            {
                --depth;
                if (depth == 0)
                    return i == segment.size() - 1;
            }
        }
        return false;
    }

    // Split an expression on top-level commas, honoring string literals and
    // both () and [] nesting, so `[.a, .b]` yields two parts but `[1, 2]`
    // inside a nested constructor stays intact.
    inline std::vector<std::string> split_comma(const std::string &expr)
    {
        return split_on(expr, [](const std::string &e, std::size_t i) -> std::size_t
                        { return e[i] == ',' ? 1 : 0; });
    }

    // Evaluate a single simple segment (no top-level '|' or '//') against one
    // value. `[ ... ]` collects the inner stream into one array; a top-level
    // comparison (==, !=, <, <=, >, >=) yields a boolean; a
    // segment starting with '"' is a string literal evaluated by interpolate;
    // '@' is a format like @csv; true/false/null/a number is a scalar literal;
    // select(expr) keeps the value when expr is truthy; a plain word may be a
    // builtin; a name("arg") is a builtin with an argument; everything else is
    // a navigation path.
    inline std::vector<Value> eval_simple(const Value &value, const std::string &segment)
    {
        if (is_array_construction(segment))
        {
            const std::string inner = trim(segment.substr(1, segment.size() - 2));
            Array out;
            if (!inner.empty())
                for (const std::string &part : split_comma(inner))
                {
                    std::vector<Value> results = query_pipe(value, part);
                    out.insert(out.end(), results.begin(), results.end());
                }
            return {Value(std::move(out))};
        }
        if (const auto [pos, op] = find_comparison(segment); pos != std::string::npos)
        {
            const std::string left = trim(segment.substr(0, pos));
            const std::string right = trim(segment.substr(pos + op.size()));
            std::vector<Value> out;
            for (const Value &lhs : query_pipe(value, left))
                for (const Value &rhs : query_pipe(value, right))
                    out.push_back(compare_values(lhs, op, rhs));
            return out;
        }
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
        if (segment.rfind("map(", 0) == 0)
        {
            const std::string inner = parse_call_arg(segment, "map");
            Array out;
            for (const Value &element : value.as_array())
            {
                std::vector<Value> results = query_pipe(element, inner);
                out.insert(out.end(), results.begin(), results.end());
            }
            return {Value(std::move(out))};
        }
        if (segment.rfind("has(", 0) == 0)
            return {builtin_has(value, parse_string_arg(segment, "has"))};
        if (segment.rfind("contains(", 0) == 0)
        {
            std::vector<Token> tokens = tokenize(parse_call_arg(segment, "contains"));
            Parser parser(tokens);
            return {Value(value_contains(value, parser.parse()))};
        }
        if (segment.rfind("join(", 0) == 0)
            return {builtin_join(value, parse_string_arg(segment, "join"))};
        if (segment.rfind("split(", 0) == 0)
            return {builtin_split(value, parse_string_arg(segment, "split"))};
        if (segment.rfind("ltrimstr(", 0) == 0)
            return {builtin_ltrimstr(value, parse_string_arg(segment, "ltrimstr"))};
        if (segment.rfind("rtrimstr(", 0) == 0)
            return {builtin_rtrimstr(value, parse_string_arg(segment, "rtrimstr"))};
        if (segment.rfind("startswith(", 0) == 0)
            return {builtin_startswith(value, parse_string_arg(segment, "startswith"))};
        if (segment.rfind("endswith(", 0) == 0)
            return {builtin_endswith(value, parse_string_arg(segment, "endswith"))};
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