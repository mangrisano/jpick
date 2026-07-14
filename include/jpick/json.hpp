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

namespace jpick
{

    struct Value;
    using Array = std::vector<Value>;
    using Object = std::unordered_map<std::string, Value>;

    struct Value
    {
        std::variant<std::string, bool, double, Array, Object, std::nullptr_t> data;
    };

} // namespace jpick