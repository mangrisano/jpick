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

} // namespace jpick