#define ARDUINOJSON_USE_LONG_LONG 1
#include "fl/json2.h"
#include "fl/string.h"
#include "fl/vector.h"
#include "fl/json.h"  // For FLArduinoJson::JsonDocument

namespace fl {
namespace json2 {

Value& get_null_value() {
    static Value null_value;
    return null_value;
}

fl::shared_ptr<Value> Value::parse(const fl::string& txt) {
    // Determine the size of the JsonDocument needed.
    FLArduinoJson::JsonDocument doc;

    FLArduinoJson::DeserializationError error = FLArduinoJson::deserializeJson(doc, txt.c_str());

    if (error) {
        FL_WARN("JSON parsing failed: " << error.c_str());
        return fl::make_shared<Value>(nullptr); // Return null on error
    }

    // Helper function to convert FLArduinoJson::JsonVariantConst to fl::json2::Value
    struct Converter {
        static fl::shared_ptr<Value> convert(const FLArduinoJson::JsonVariantConst& src) {
            if (src.isNull()) {
                return fl::make_shared<Value>(nullptr);
            } else if (src.is<bool>()) {
                return fl::make_shared<Value>(src.as<bool>());
            } else if (src.is<int64_t>()) {
                // Handle 64-bit integers
                return fl::make_shared<Value>(src.as<int64_t>());
            } else if (src.is<int32_t>()) {
                // Handle 32-bit integers explicitly for platform compatibility
                return fl::make_shared<Value>(static_cast<int64_t>(src.as<int32_t>()));
            } else if (src.is<uint32_t>()) {
                // Handle unsigned 32-bit integers
                return fl::make_shared<Value>(static_cast<int64_t>(src.as<uint32_t>()));
            } else if (src.is<double>()) {
                // Handle double precision floats
                return fl::make_shared<Value>(src.as<double>());
            } else if (src.is<float>()) {
                // Handle single precision floats explicitly
                return fl::make_shared<Value>(static_cast<double>(src.as<float>()));
            } else if (src.is<const char*>()) {
                return fl::make_shared<Value>(fl::string(src.as<const char*>()));
            } else if (src.is<FLArduinoJson::JsonArrayConst>()) {
                Array arr;
                for (const auto& item : src.as<FLArduinoJson::JsonArrayConst>()) {
                    arr.push_back(convert(item));
                }
                return fl::make_shared<Value>(fl::move(arr));
            } else if (src.is<FLArduinoJson::JsonObjectConst>()) {
                Object obj;
                for (const auto& kv : src.as<FLArduinoJson::JsonObjectConst>()) {
                    obj[fl::string(kv.key().c_str())] = convert(kv.value());
                }
                return fl::make_shared<Value>(fl::move(obj));
            }
            return fl::make_shared<Value>(nullptr); // Should not happen
        }
    };

    return Converter::convert(doc.as<FLArduinoJson::JsonVariantConst>());
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
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
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
        if (opt) {
            return *opt ? "true" : "false";
        }
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