// json.hpp — JSON value data model.
//
// Defines `Value`, a recursive `std::variant` that can hold any JSON type
// (string, bool, double, null, `Array`, or `Object`), together with the
// `Array` and `Object` type aliases used throughout jpick.
#pragma once

#include <variant>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstddef>
#include <stdexcept>

namespace jpick
{

    struct Value;
    using Array = std::vector<Value>;
    using Object = std::unordered_map<std::string, Value>;

    struct Value
    {
        std::variant<std::string, bool, double, Array, Object, std::nullptr_t> data;

        bool is_string() const { return std::holds_alternative<std::string>(data); }
        bool is_number() const { return std::holds_alternative<double>(data); }
        bool is_bool() const { return std::holds_alternative<bool>(data); }
        bool is_null() const { return std::holds_alternative<std::nullptr_t>(data); }
        bool is_array() const { return std::holds_alternative<Array>(data); }
        bool is_object() const { return std::holds_alternative<Object>(data); }

        const std::string &as_string() const
        {
            if (const std::string *p = std::get_if<std::string>(&data))
                return *p;
            throw std::runtime_error("Value is not a String");
        }
        double as_number() const
        {
            if (const double *p = std::get_if<double>(&data))
                return *p;
            throw std::runtime_error("Value is not a Number");
        }
        bool as_bool() const
        {
            if (const bool *p = std::get_if<bool>(&data))
                return *p;
            throw std::runtime_error("Value is not a Bool");
        }
        const Array &as_array() const
        {
            if (const Array *p = std::get_if<Array>(&data))
                return *p;
            throw std::runtime_error("Value is not an Array");
        }
        const Object &as_object() const
        {
            if (const Object *p = std::get_if<Object>(&data))
                return *p;
            throw std::runtime_error("Value is not an Object");
        }
        const Value &operator[](const std::string &key) const
        {
            const Object &obj = as_object();
            auto it = obj.find(key);
            if (it == obj.end()) throw std::runtime_error("Field does not exist");
            return it->second;
        }
        const Value &operator[](std::size_t index) const
        {
            const Array &arr = as_array();
            if (index >= arr.size()) throw std::runtime_error("Index out of range");
            return arr[index];
        }
    };

} // namespace jpick