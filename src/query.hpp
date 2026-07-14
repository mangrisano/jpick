#pragma once

#include <string>
#include <stdexcept>
#include <vector>
#include <variant>
#include "json.hpp"

using PathStep = std::variant<std::string, std::size_t>;

inline Value query(const Value &root, const std::string &key)
{
    const Object *obj = std::get_if<Object>(&root.data);
    if (!obj)
        throw std::runtime_error("Value is not an Object");
    auto it = obj->find(key);
    if (it == obj->end())
        throw std::runtime_error("Field does not exist");
    return it->second;
}

inline Value query_index(const Value &root, std::size_t index)
{
    const Array *arr = std::get_if<Array>(&root.data);
    if (!arr)
        throw std::runtime_error("Value is not an Array");
    if (index >= arr->size())
        throw std::runtime_error("Index out of range");
    return (*arr)[index];
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
            std::size_t index = std::stoul(current);
            steps.emplace_back(std::in_place_type<std::size_t>, index);
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

inline Value query_path(const Value &root, const std::vector<PathStep> &steps)
{
    Value current = root;
    for (const PathStep &step : steps)
    {
        if (const std::string *key = std::get_if<std::string>(&step))
        {
            current = query(current, *key);
        }
        else if (const std::size_t *index = std::get_if<std::size_t>(&step))
        {
            current = query_index(current, *index);
        }
    }
    return current;
}