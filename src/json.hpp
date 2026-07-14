#pragma once

#include <variant>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstddef>

struct Value;
using Array = std::vector<Value>;
using Object = std::unordered_map<std::string, Value>;

struct Value {
    std::variant<std::string, bool, double, Array, Object, std::nullptr_t> data;
};