#include "fl/json2.h"
#include "fl/string.h"
#include "fl/vector.h"

namespace fl {
namespace json2 {

Value& get_null_value() {
    static Value null_value;
    return null_value;
}

// Forward declarations for parser helper functions
namespace {
    // Skip whitespace characters
    void skip_whitespace(const fl::string& str, size_t& pos) {
        while (pos < str.length() && (str[pos] == ' ' || str[pos] == '\t' || str[pos] == '\n' || str[pos] == '\r')) {
            ++pos;
        }
    }
    
    // Parse a JSON value from string
    fl::shared_ptr<Value> parse_value(const fl::string& str, size_t& pos);
    
    // Parse a JSON string
    fl::string parse_string(const fl::string& str, size_t& pos) {
        if (pos >= str.length() || str[pos] != '"') {
            return "";
        }
        
        ++pos; // Skip opening quote
        fl::string result;
        while (pos < str.length() && str[pos] != '"') {
            if (str[pos] == '\\') {
                ++pos;
                if (pos < str.length()) {
                    switch (str[pos]) {
                        case '"': result += '"'; break;
                        case '\\': result += '\\'; break;
                        case '/': result += '/'; break;
                        case 'b': result += '\b'; break;
                        case 'f': result += '\f'; break;
                        case 'n': result += '\n'; break;
                        case 'r': result += '\r'; break;
                        case 't': result += '\t'; break;
                        // TODO: Handle unicode escapes \uXXXX
                        default: result += str[pos]; break;
                    }
                }
            } else {
                result += str[pos];
            }
            ++pos;
        }
        
        if (pos < str.length() && str[pos] == '"') {
            ++pos; // Skip closing quote
        }
        
        return result;
    }
    
    // Parse a JSON number
    fl::shared_ptr<Value> parse_number(const fl::string& str, size_t& pos) {
        size_t start = pos;
        if (pos < str.length() && (str[pos] == '-' || str[pos] == '+')) {
            ++pos;
        }
        
        // Parse integer part
        while (pos < str.length() && str[pos] >= '0' && str[pos] <= '9') {
            ++pos;
        }
        
        // Parse fractional part
        if (pos < str.length() && str[pos] == '.') {
            ++pos;
            while (pos < str.length() && str[pos] >= '0' && str[pos] <= '9') {
                ++pos;
            }
        }
        
        // Parse exponent
        if (pos < str.length() && (str[pos] == 'e' || str[pos] == 'E')) {
            ++pos;
            if (pos < str.length() && (str[pos] == '-' || str[pos] == '+')) {
                ++pos;
            }
            while (pos < str.length() && str[pos] >= '0' && str[pos] <= '9') {
                ++pos;
            }
        }
        
        fl::string num_str = str.substr(start, pos - start);
        
        // Try to parse as integer first
        if (num_str.find('.') == fl::string::npos && 
            num_str.find('e') == fl::string::npos && num_str.find('E') == fl::string::npos) {
            // Parse as integer using our own function
            int int_val = fl::StringFormatter::parseInt(num_str.c_str(), num_str.length());
            return fl::make_shared<Value>(static_cast<long long>(int_val));
        }
        
        // Parse as double using our own function
        float float_val = fl::StringFormatter::parseFloat(num_str.c_str(), num_str.length());
        return fl::make_shared<Value>(static_cast<double>(float_val));
    }
    
    // Parse a JSON boolean
    fl::shared_ptr<Value> parse_boolean(const fl::string& str, size_t& pos) {
        if (pos + 4 <= str.length() && str.substr(pos, 4) == "true") {
            pos += 4;
            return fl::make_shared<Value>(true);
        } else if (pos + 5 <= str.length() && str.substr(pos, 5) == "false") {
            pos += 5;
            return fl::make_shared<Value>(false);
        }
        return fl::make_shared<Value>(false);
    }
    
    // Parse a JSON null
    fl::shared_ptr<Value> parse_null(const fl::string& str, size_t& pos) {
        if (pos + 4 <= str.length() && str.substr(pos, 4) == "null") {
            pos += 4;
            return fl::make_shared<Value>(nullptr);
        }
        return fl::make_shared<Value>(nullptr);
    }
    
    // Parse a JSON array
    fl::shared_ptr<Value> parse_array(const fl::string& str, size_t& pos) {
        if (pos >= str.length() || str[pos] != '[') {
            return fl::make_shared<Value>(Array{});
        }
        
        ++pos; // Skip opening bracket
        skip_whitespace(str, pos);
        
        Array array;
        if (pos < str.length() && str[pos] == ']') {
            ++pos; // Skip closing bracket for empty array
            return fl::make_shared<Value>(fl::move(array));
        }
        
        while (pos < str.length() && str[pos] != ']') {
            auto value = parse_value(str, pos);
            if (value) {
                array.push_back(value);
            }
            
            skip_whitespace(str, pos);
            if (pos < str.length() && str[pos] == ',') {
                ++pos;
                skip_whitespace(str, pos);
            }
        }
        
        if (pos < str.length() && str[pos] == ']') {
            ++pos; // Skip closing bracket
        }
        
        return fl::make_shared<Value>(fl::move(array));
    }
    
    // Parse a JSON object
    fl::shared_ptr<Value> parse_object(const fl::string& str, size_t& pos) {
        if (pos >= str.length() || str[pos] != '{') {
            return fl::make_shared<Value>(Object{});
        }
        
        ++pos; // Skip opening brace
        skip_whitespace(str, pos);
        
        Object object;
        if (pos < str.length() && str[pos] == '}') {
            ++pos; // Skip closing brace for empty object
            return fl::make_shared<Value>(fl::move(object));
        }
        
        while (pos < str.length() && str[pos] != '}') {
            skip_whitespace(str, pos);
            
            // Parse key
            fl::string key = parse_string(str, pos);
            
            skip_whitespace(str, pos);
            
            // Expect colon
            if (pos < str.length() && str[pos] == ':') {
                ++pos;
                skip_whitespace(str, pos);
                
                // Parse value
                auto value = parse_value(str, pos);
                if (value) {
                    object[key] = value;
                }
            }
            
            skip_whitespace(str, pos);
            if (pos < str.length() && str[pos] == ',') {
                ++pos;
                skip_whitespace(str, pos);
            }
        }
        
        if (pos < str.length() && str[pos] == '}') {
            ++pos; // Skip closing brace
        }
        
        return fl::make_shared<Value>(fl::move(object));
    }
    
    // Parse a JSON value
    fl::shared_ptr<Value> parse_value(const fl::string& str, size_t& pos) {
        skip_whitespace(str, pos);
        
        if (pos >= str.length()) {
            return fl::make_shared<Value>(nullptr);
        }
        
        char c = str[pos];
        switch (c) {
            case '{':
                return parse_object(str, pos);
            case '[':
                return parse_array(str, pos);
            case '"':
                return fl::make_shared<Value>(parse_string(str, pos));
            case 't':
            case 'f':
                return parse_boolean(str, pos);
            case 'n':
                return parse_null(str, pos);
            case '-':
            case '+':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                return parse_number(str, pos);
            default:
                return fl::make_shared<Value>(nullptr);
        }
    }
}

fl::shared_ptr<Value> Value::parse(const fl::string& txt) {
    size_t pos = 0;
    return parse_value(txt, pos);
}

fl::string Json::to_string_native() const {
    return serializeValue(*m_value);
}

fl::string serializeValue(const Value& value);

} // namespace json2
} // namespace fl

namespace fl {
namespace json2 {

fl::string Json::normalizeJsonString(const char* jsonStr) {
    fl::string result;
    if (!jsonStr) {
        return result;
    }
    size_t len = strlen(jsonStr);
    for (size_t i = 0; i < len; ++i) {
        char c = jsonStr[i];
        if (c != ' ' && c != '	' && c != '\n' && c != '\r') {
            result += c;
        }
    }
    return result;
}

} // namespace json2
} // namespace fl

namespace fl {
namespace json2 {

fl::string serializeValue(const Value& value) {
    if (value.is_null()) {
        return "null";
    } else if (value.is_bool()) {
        auto opt = value.as_bool();
        return opt && *opt ? "true" : "false";
    } else if (value.is_int()) {
        auto opt = value.as_int();
        if (opt) {
            return fl::to_string(*opt);
        }
    } else if (value.is_double()) {
        auto opt = value.as_double();
        if (opt) {
            return fl::to_string(*opt);
        }
    } else if (value.is_string()) {
        auto opt = value.as_string();
        if (opt) {
            // Escape special characters in the string
            fl::string escaped = "\"";
            for (char c : *opt) {
                switch (c) {
                    case '"': escaped += "\\\""; break;
                    case '\\': escaped += "\\\\"; break;
                    case '\b': escaped += "\\b"; break;
                    case '\f': escaped += "\\f"; break;
                    case '\n': escaped += "\\n"; break;
                    case '\r': escaped += "\\r"; break;
                    case '\t': escaped += "\\t"; break;
                    default: escaped += c; break;
                }
            }
            escaped += "\"";
            return escaped;
        }
    } else if (value.is_array()) {
        auto opt = value.as_array();
        if (opt) {
            fl::string result = "[";
            bool first = true;
            for (const auto& item : *opt) {
                if (!first) {
                    result += ",";
                }
                // Check for null pointer
                if (item) {
                    result += serializeValue(*item);
                } else {
                    result += "null";
                }
                first = false;
            }
            result += "]";
            return result;
        }
    } else if (value.is_object()) {
        auto opt = value.as_object();
        if (opt) {
            fl::string result = "{";
            bool first = true;
            for (const auto& kv : *opt) {
                if (!first) {
                    result += ",";
                }
                result += "\"" + kv.first + "\":"; 
                // Check for null pointer
                if (kv.second) {
                    result += serializeValue(*kv.second);
                } else {
                    result += "null";
                }
                first = false;
            }
            result += "}";
            return result;
        }
    }
    
    return "null";
}

} // namespace json2
} // namespace fl
