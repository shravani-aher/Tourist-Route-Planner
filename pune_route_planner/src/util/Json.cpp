#include "Json.h"
#include <cctype>
#include <iomanip>
#include <sstream>

namespace util {

static const JsonValue kNullValue{};

bool JsonValue::has_key(const std::string& key) const {
    if (!is_object()) return false;
    for (const auto& kv : obj_val) {
        if (kv.first == key) return true;
    }
    return false;
}

const JsonValue& JsonValue::operator[](const std::string& key) const {
    if (!is_object()) return kNullValue;
    for (const auto& kv : obj_val) {
        if (kv.first == key) return kv.second;
    }
    return kNullValue;
}

JsonValue& JsonValue::operator[](const std::string& key) {
    if (!is_object()) {
        type = JsonType::Object;
        obj_val.clear();
    }
    for (auto& kv : obj_val) {
        if (kv.first == key) return kv.second;
    }
    obj_val.emplace_back(key, JsonValue());
    return obj_val.back().second;
}

size_t JsonValue::size() const {
    if (is_array()) return arr_val.size();
    if (is_object()) return obj_val.size();
    return 0;
}

const JsonValue& JsonValue::operator[](size_t index) const {
    if (is_array() && index < arr_val.size()) {
        return arr_val[index];
    }
    return kNullValue;
}

JsonValue& JsonValue::operator[](size_t index) {
    if (!is_array()) {
        type = JsonType::Array;
        arr_val.clear();
    }
    if (index >= arr_val.size()) {
        arr_val.resize(index + 1);
    }
    return arr_val[index];
}

void JsonValue::push_back(JsonValue val) {
    if (!is_array()) {
        type = JsonType::Array;
        arr_val.clear();
    }
    arr_val.push_back(std::move(val));
}

static void escape_string(const std::string& s, std::string& out) {
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    out += '"';
}

void JsonValue::serialize_internal(std::string& out, int indent, int current_indent) const {
    switch (type) {
        case JsonType::Null:
            out += "null";
            break;
        case JsonType::Boolean:
            out += (bool_val ? "true" : "false");
            break;
        case JsonType::Number: {
            // Check if whole integer
            if (num_val == static_cast<int64_t>(num_val)) {
                out += std::to_string(static_cast<int64_t>(num_val));
            } else {
                std::ostringstream ss;
                ss << std::setprecision(10) << num_val;
                out += ss.str();
            }
            break;
        }
        case JsonType::String:
            escape_string(str_val, out);
            break;
        case JsonType::Array: {
            if (arr_val.empty()) {
                out += "[]";
                break;
            }
            out += "[";
            bool first = true;
            for (const auto& item : arr_val) {
                if (!first) out += ",";
                first = false;
                if (indent > 0) {
                    out += "\n";
                    out.append(current_indent + indent, ' ');
                }
                item.serialize_internal(out, indent, current_indent + indent);
            }
            if (indent > 0) {
                out += "\n";
                out.append(current_indent, ' ');
            }
            out += "]";
            break;
        }
        case JsonType::Object: {
            if (obj_val.empty()) {
                out += "{}";
                break;
            }
            out += "{";
            bool first = true;
            for (const auto& kv : obj_val) {
                if (!first) out += ",";
                first = false;
                if (indent > 0) {
                    out += "\n";
                    out.append(current_indent + indent, ' ');
                }
                escape_string(kv.first, out);
                out += ":";
                if (indent > 0) out += " ";
                kv.second.serialize_internal(out, indent, current_indent + indent);
            }
            if (indent > 0) {
                out += "\n";
                out.append(current_indent, ' ');
            }
            out += "}";
            break;
        }
    }
}

std::string JsonValue::serialize(int indent) const {
    std::string result;
    serialize_internal(result, indent, 0);
    return result;
}

// Minimal hand-written recursive descent parser
class JsonParser {
    std::string_view src;
    size_t pos = 0;
    std::string error;

    void skip_whitespace() {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r' || src[pos] == '\n')) {
            pos++;
        }
    }

    char peek() {
        skip_whitespace();
        return pos < src.size() ? src[pos] : '\0';
    }

    char get() {
        skip_whitespace();
        return pos < src.size() ? src[pos++] : '\0';
    }

    bool match(char expected) {
        skip_whitespace();
        if (pos < src.size() && src[pos] == expected) {
            pos++;
            return true;
        }
        return false;
    }

    std::string parse_string() {
        if (get() != '"') {
            error = "Expected '\"'";
            return "";
        }
        std::string s;
        while (pos < src.size()) {
            char c = src[pos++];
            if (c == '"') {
                return s;
            }
            if (c == '\\') {
                if (pos >= src.size()) break;
                char esc = src[pos++];
                switch (esc) {
                    case '"': s += '"'; break;
                    case '\\': s += '\\'; break;
                    case '/': s += '/'; break;
                    case 'b': s += '\b'; break;
                    case 'f': s += '\f'; break;
                    case 'n': s += '\n'; break;
                    case 'r': s += '\r'; break;
                    case 't': s += '\t'; break;
                    case 'u': {
                        // Minimal 4-hex unicode skip/fallback
                        if (pos + 4 <= src.size()) {
                            pos += 4;
                            s += '?';
                        }
                        break;
                    }
                    default: s += esc; break;
                }
            } else {
                s += c;
            }
        }
        error = "Unterminated string";
        return s;
    }

    JsonValue parse_number() {
        skip_whitespace();
        size_t start = pos;
        if (pos < src.size() && src[pos] == '-') pos++;
        while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) pos++;
        if (pos < src.size() && src[pos] == '.') {
            pos++;
            while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) pos++;
        }
        if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
            pos++;
            if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) pos++;
            while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) pos++;
        }
        std::string num_str(src.substr(start, pos - start));
        try {
            double val = std::stod(num_str);
            return JsonValue(val);
        } catch (...) {
            error = "Invalid number format: " + num_str;
            return JsonValue();
        }
    }

    JsonValue parse_array() {
        if (!match('[')) {
            error = "Expected '['";
            return JsonValue();
        }
        JsonValue arr = JsonValue::array();
        if (match(']')) return arr;
        while (true) {
            JsonValue elem = parse_value();
            if (!error.empty()) return JsonValue();
            arr.push_back(std::move(elem));
            if (match(']')) break;
            if (!match(',')) {
                error = "Expected ',' or ']' in array";
                return JsonValue();
            }
        }
        return arr;
    }

    JsonValue parse_object() {
        if (!match('{')) {
            error = "Expected '{'";
            return JsonValue();
        }
        JsonValue obj = JsonValue::object();
        if (match('}')) return obj;
        while (true) {
            skip_whitespace();
            if (peek() != '"') {
                error = "Expected string key in object";
                return JsonValue();
            }
            std::string key = parse_string();
            if (!error.empty()) return JsonValue();
            if (!match(':')) {
                error = "Expected ':' after key";
                return JsonValue();
            }
            JsonValue val = parse_value();
            if (!error.empty()) return JsonValue();
            obj[key] = std::move(val);
            if (match('}')) break;
            if (!match(',')) {
                error = "Expected ',' or '}' in object";
                return JsonValue();
            }
        }
        return obj;
    }

public:
    JsonParser(std::string_view s) : src(s) {}

    JsonValue parse_value() {
        skip_whitespace();
        if (pos >= src.size()) {
            error = "Unexpected end of input";
            return JsonValue();
        }
        char c = src[pos];
        if (c == 'n') {
            if (src.substr(pos, 4) == "null") {
                pos += 4;
                return JsonValue();
            }
        } else if (c == 't') {
            if (src.substr(pos, 4) == "true") {
                pos += 4;
                return JsonValue(true);
            }
        } else if (c == 'f') {
            if (src.substr(pos, 5) == "false") {
                pos += 5;
                return JsonValue(false);
            }
        } else if (c == '"') {
            return JsonValue(parse_string());
        } else if (c == '[') {
            return parse_array();
        } else if (c == '{') {
            return parse_object();
        } else if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            return parse_number();
        }
        error = std::string("Unexpected character: '") + c + "'";
        return JsonValue();
    }

    const std::string& get_error() const { return error; }
};

JsonValue JsonValue::parse(std::string_view input, std::string* error_out) {
    JsonParser parser(input);
    JsonValue val = parser.parse_value();
    if (error_out && !parser.get_error().empty()) {
        *error_out = parser.get_error();
    }
    return val;
}

} // namespace util
