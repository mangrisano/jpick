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
    inline std::vector<std::string> split_pipe(const std::string &expr)
    {
        std::vector<std::string> segments;
        std::string current;
        for (char c : expr)
        {
            if (c == '|')
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

    // Evaluate a full pipe expression: each segment is applied to every value
    // produced by the previous one, flattening the results into one stream.
    inline std::vector<Value> query_pipe(const Value &root, const std::string &expr)
    {
        std::vector<Value> stream = {root};
        for (const std::string &segment : split_pipe(expr))
        {
            const std::vector<PathStep> steps = split_path(segment);
            std::vector<Value> next;
            for (const Value &value : stream)
            {
                std::vector<Value> results = query_path(value, steps);
                next.insert(next.end(), results.begin(), results.end());
            }
            stream = std::move(next);
        }
        return stream;
    }

} // namespace jpick