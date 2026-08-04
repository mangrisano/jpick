// builtins.hpp — the builtin functions (length, keys, add, sort, join, ...)
// and the jq total order (value_less/type_rank) used by sort/min/max and the
// comparison operators. Included via query.hpp; is_truthy is forward declared
// here and defined in query.hpp (same translation unit).
#pragma once

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <charconv>
#include <cctype>
#include <stdexcept>
#include "jpick/json.hpp"
#include "jpick/serializer.hpp"
#include "jpick/lexer.hpp"
#include "jpick/parser.hpp"

namespace jpick
{
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

    // Forward declaration: from_entries reuses jq's truthiness for key lookup.
    inline bool is_truthy(const Value &value);

    // Find the value of `name` in an entry object, or nullptr when absent.
    inline const Value *find_entry_field(const Object &entry, const std::string &name)
    {
        for (const auto &[k, v] : entry)
            if (k == name)
                return &v;
        return nullptr;
    }

    // Convert an object into an array of {"key":k, "value":v} entries,
    // preserving insertion order (the inverse of from_entries). Non-objects
    // throw.
    inline Value builtin_to_entries(const Value &value)
    {
        const Object &obj = value.as_object();
        Array out;
        out.reserve(obj.size());
        for (const auto &[k, v] : obj)
        {
            Object entry;
            entry.emplace_back("key", Value(k));
            entry.emplace_back("value", v);
            out.push_back(Value(std::move(entry)));
        }
        return Value(std::move(out));
    }

    // Rebuild an object from an array of entries (the inverse of to_entries).
    // The key is the first truthy of key/k/name/Name/K/Key and the value the
    // first present of value/v/Value/V (missing yields null); a non-string key
    // is rendered as its JSON form and later entries win.
    inline Value builtin_from_entries(const Value &value)
    {
        const Array &arr = value.as_array();
        Object out;
        for (const Value &item : arr)
        {
            const Object &entry = item.as_object();

            Value key_value(nullptr);
            for (const char *name : {"key", "k", "name", "Name", "K", "Key"})
                if (const Value *found = find_entry_field(entry, name); found && is_truthy(*found))
                {
                    key_value = *found;
                    break;
                }
            const std::string key =
                key_value.is_string() ? key_value.as_string() : serialize(key_value);

            Value entry_value(nullptr);
            for (const char *name : {"value", "v", "Value", "V"})
                if (const Value *found = find_entry_field(entry, name))
                {
                    entry_value = *found;
                    break;
                }

            bool replaced = false;
            for (auto &pair : out)
                if (pair.first == key)
                {
                    pair.second = entry_value;
                    replaced = true;
                    break;
                }
            if (!replaced)
                out.emplace_back(key, entry_value);
        }
        return Value(std::move(out));
    }

    // Deep containment test, matching jq's `contains`: a string contains a
    // substring, an array contains another when each of its elements is
    // contained in some element of the former, an object contains another when
    // every key matches and its value is contained recursively, and any other
    // value contains only an equal value.
    inline bool value_contains(const Value &haystack, const Value &needle)
    {
        if (haystack.is_object() && needle.is_object())
        {
            for (const auto &[key, sub] : needle.as_object())
            {
                const Value *found = find_entry_field(haystack.as_object(), key);
                if (!found || !value_contains(*found, sub))
                    return false;
            }
            return true;
        }
        if (haystack.is_array() && needle.is_array())
        {
            for (const Value &want : needle.as_array())
            {
                bool found = false;
                for (const Value &have : haystack.as_array())
                    if (value_contains(have, want))
                    {
                        found = true;
                        break;
                    }
                if (!found)
                    return false;
            }
            return true;
        }
        if (haystack.is_string() && needle.is_string())
            return haystack.as_string().find(needle.as_string()) != std::string::npos;
        return haystack == needle;
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

    // Parse a value as a number: a number is returned unchanged, a string is
    // parsed (the whole string must be a valid number). Other types throw.
    inline Value builtin_tonumber(const Value &value)
    {
        if (value.is_number())
            return value;
        if (value.is_string())
        {
            const std::string &s = value.as_string();
            double number = 0;
            const char *first = s.data();
            const char *last = first + s.size();
            auto [ptr, ec] = std::from_chars(first, last, number);
            if (ec == std::errc() && ptr == last)
                return Value(number);
            throw std::runtime_error("Cannot parse as a number: " + s);
        }
        throw std::runtime_error("tonumber requires a number or a string");
    }

    // Render a value as a string: strings pass through, everything else is
    // serialized to its JSON form (numbers, bools, null, arrays, objects).
    inline Value builtin_tostring(const Value &value)
    {
        if (value.is_string())
            return value;
        return Value(serialize(value));
    }

    // Parse a string that contains JSON into the corresponding value (the
    // inverse of @json), e.g. to decode a JSON-encoded field. Non-strings and
    // malformed JSON throw.
    inline Value builtin_fromjson(const Value &value)
    {
        const std::string &s = value.as_string();
        std::vector<Token> tokens = tokenize(s);
        Parser parser(tokens);
        return parser.parse();
    }

    // Lowercase the ASCII letters A-Z of a string; other bytes are unchanged.
    inline Value builtin_ascii_downcase(const Value &value)
    {
        std::string s = value.as_string();
        for (char &c : s)
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c + 32);
        return Value(s);
    }

    // Uppercase the ASCII letters a-z of a string; other bytes are unchanged.
    inline Value builtin_ascii_upcase(const Value &value)
    {
        std::string s = value.as_string();
        for (char &c : s)
            if (c >= 'a' && c <= 'z')
                c = static_cast<char>(c - 32);
        return Value(s);
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

    // Remove `prefix` from the start of a string if present; a value without
    // the prefix (or a non-string) is returned unchanged (like jq).
    inline Value builtin_ltrimstr(const Value &value, const std::string &prefix)
    {
        if (!value.is_string())
            return value;
        const std::string &s = value.as_string();
        if (s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0)
            return Value(s.substr(prefix.size()));
        return value;
    }

    // Remove `suffix` from the end of a string if present; a value without the
    // suffix (or a non-string) is returned unchanged (like jq).
    inline Value builtin_rtrimstr(const Value &value, const std::string &suffix)
    {
        if (!value.is_string())
            return value;
        const std::string &s = value.as_string();
        if (s.size() >= suffix.size() &&
            s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0)
            return Value(s.substr(0, s.size() - suffix.size()));
        return value;
    }

    // True if the string starts with `prefix`. The input must be a string.
    inline Value builtin_startswith(const Value &value, const std::string &prefix)
    {
        const std::string &s = value.as_string();
        return Value(s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0);
    }

    // True if the string ends with `suffix`. The input must be a string.
    inline Value builtin_endswith(const Value &value, const std::string &suffix)
    {
        const std::string &s = value.as_string();
        return Value(s.size() >= suffix.size() &&
                     s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0);
    }

    // Table of unary builtins with signature Value(const Value&). A pipe
    // segment matching one of these names is applied to every value in the
    // stream. Builtins that take a string argument live in
    // string_arg_builtins(); `empty` stays special-cased in eval_simple.
    inline const std::map<std::string, Value (*)(const Value &)> &unary_builtins()
    {
        static const std::map<std::string, Value (*)(const Value &)> table = {
            {"length", builtin_length},
            {"keys", builtin_keys},
            {"to_entries", builtin_to_entries},
            {"from_entries", builtin_from_entries},
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
            {"tonumber", builtin_tonumber},
            {"tostring", builtin_tostring},
            {"fromjson", builtin_fromjson},
            {"ascii_downcase", builtin_ascii_downcase},
            {"ascii_upcase", builtin_ascii_upcase},
        };
        return table;
    }

    // Table of builtins of the form name("arg") that take one string argument,
    // signature Value(const Value&, const std::string&). Looked up by name in
    // eval_simple, mirroring unary_builtins for the no-argument ones.
    inline const std::map<std::string, Value (*)(const Value &, const std::string &)> &
    string_arg_builtins()
    {
        static const std::map<std::string, Value (*)(const Value &, const std::string &)> table = {
            {"has", builtin_has},
            {"join", builtin_join},
            {"split", builtin_split},
            {"ltrimstr", builtin_ltrimstr},
            {"rtrimstr", builtin_rtrimstr},
            {"startswith", builtin_startswith},
            {"endswith", builtin_endswith},
        };
        return table;
    }
} // namespace jpick
