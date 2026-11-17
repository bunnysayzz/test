// json.h - recursive descent JSON with a depth limit.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace json {

enum class Type { Null, Bool, Number, String, Array, Object };

struct Value {
    Type type = Type::Null;
    bool boolean = false;
    double number = 0;
    std::string string;
    std::vector<Value> array;
    std::vector<std::pair<std::string, Value>> object;

    const Value* Find(const char* key) const;
    const Value* At(size_t index) const;
    std::string AsString() const;
    double AsNumber(double fallback) const;
};

bool Parse(const std::string& text, Value& out, std::string* error = nullptr);
std::string Quote(const std::string& text);

}  // namespace json
