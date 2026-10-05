#pragma once

#include <string>
#include <vector>
#include <map>
#include <string_view>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <cstdint>

namespace util {

enum class JsonType {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

class JsonValue {
public:
    JsonType type = JsonType::Null;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::vector<JsonValue> arr_val;
    std::vector<std::pair<std::string, JsonValue>> obj_val;

    JsonValue() : type(JsonType::Null) {}
    JsonValue(std::nullptr_t) : type(JsonType::Null) {}
    JsonValue(bool b) : type(JsonType::Boolean), bool_val(b) {}
    JsonValue(int n) : type(JsonType::Number), num_val(static_cast<double>(n)) {}
    JsonValue(int64_t n) : type(JsonType::Number), num_val(static_cast<double>(n)) {}
    JsonValue(double d) : type(JsonType::Number), num_val(d) {}
    JsonValue(const char* s) : type(JsonType::String), str_val(s ? s : "") {}
    JsonValue(std::string s) : type(JsonType::String), str_val(std::move(s)) {}
    JsonValue(std::vector<JsonValue> arr) : type(JsonType::Array), arr_val(std::move(arr)) {}
    JsonValue(std::vector<std::pair<std::string, JsonValue>> obj) : type(JsonType::Object), obj_val(std::move(obj)) {}

    // Factory methods
    static JsonValue null() { return JsonValue(); }
    static JsonValue array() { JsonValue v; v.type = JsonType::Array; return v; }
    static JsonValue object() { JsonValue v; v.type = JsonType::Object; return v; }

    // Type checks
    bool is_null() const { return type == JsonType::Null; }
    bool is_bool() const { return type == JsonType::Boolean; }
    bool is_number() const { return type == JsonType::Number; }
    bool is_string() const { return type == JsonType::String; }
    bool is_array() const { return type == JsonType::Array; }
    bool is_object() const { return type == JsonType::Object; }

    // Accessors
    bool as_bool(bool default_val = false) const { return is_bool() ? bool_val : default_val; }
    double as_double(double default_val = 0.0) const { return is_number() ? num_val : default_val; }
    int as_int(int default_val = 0) const { return is_number() ? static_cast<int>(num_val) : default_val; }
    const std::string& as_string() const { return str_val; }
    std::string as_string_or(const std::string& fallback) const { return is_string() ? str_val : fallback; }

    const std::vector<JsonValue>& as_array() const { return arr_val; }
    std::vector<JsonValue>& as_array() { return arr_val; }

    const std::vector<std::pair<std::string, JsonValue>>& as_object() const { return obj_val; }
    std::vector<std::pair<std::string, JsonValue>>& as_object() { return obj_val; }

    // Object helpers
    bool has_key(const std::string& key) const;
    const JsonValue& operator[](const std::string& key) const;
    JsonValue& operator[](const std::string& key);

    // Array helpers
    size_t size() const;
    const JsonValue& operator[](size_t index) const;
    JsonValue& operator[](size_t index);
    void push_back(JsonValue val);

    // Serialization & Parsing
    std::string serialize(int indent = 0) const;
    static JsonValue parse(std::string_view input, std::string* error_out = nullptr);

private:
    void serialize_internal(std::string& out, int indent, int current_indent) const;
};

} // namespace util
