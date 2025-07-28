
#include "fl/json.h"
#include "fl/string.h"
#include "fl/vector.h"
#include "fl/deque.h"
#include "fl/function.h"
#include "fl/sketch_macros.h"
#include "fl/math.h" // For floor function
#include <string> // ok include

// Define INT16_MIN, INT16_MAX, and UINT8_MAX if not already defined
#ifndef INT16_MIN
#define INT16_MIN (-32768)
#endif

#ifndef INT16_MAX
#define INT16_MAX 32767
#endif

#ifndef UINT8_MAX
#define UINT8_MAX 255
#endif

// Helper function to check if a double can be exactly represented as a float
static bool canBeRepresentedAsFloat(double value) {
    // Check for special values
    if (isnan(value) || isinf(value)) {
        return true; // These can be represented as float
    }
    
    // Convert to float and back to double
    float f = static_cast<float>(value);
    double d = static_cast<double>(f);
    
    // Check if they're equal
    return (value == d);
}

#if FASTLED_ENABLE_JSON

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(null-dereference)
#include "third_party/arduinojson/json.h"
FL_DISABLE_WARNING_POP



namespace fl {


JsonValue& get_null_value() {
    static JsonValue null_value;
    return null_value;
}

fl::shared_ptr<JsonValue> JsonValue::parse(const fl::string& txt) {
    #if !FASTLED_ENABLE_JSON
    return fl::make_shared<JsonValue>(fl::string(txt));
    #else
    // Determine the size of the JsonDocument needed.
    FLArduinoJson::JsonDocument doc;

    FLArduinoJson::DeserializationError error = FLArduinoJson::deserializeJson(doc, txt.c_str());

    if (error) {
        FL_WARN("JSON parsing failed: " << error.c_str());
        return fl::make_shared<JsonValue>(nullptr); // Return null on error
    }

    // Helper function to convert FLArduinoJson::JsonVariantConst to fl::Json::JsonValue
    struct Converter {
        static fl::shared_ptr<JsonValue> convert(const FLArduinoJson::JsonVariantConst& src) {
            if (src.isNull()) {
                return fl::make_shared<JsonValue>(nullptr);
            } else if (src.is<bool>()) {
                return fl::make_shared<JsonValue>(src.as<bool>());
            } else if (src.is<int64_t>()) {
                // Handle 64-bit integers
                return fl::make_shared<JsonValue>(src.as<int64_t>());
            } else if (src.is<int32_t>()) {
                // Handle 32-bit integers explicitly for platform compatibility
                return fl::make_shared<JsonValue>(static_cast<int64_t>(src.as<int32_t>()));
            } else if (src.is<uint32_t>()) {
                // Handle unsigned 32-bit integers
                return fl::make_shared<JsonValue>(static_cast<int64_t>(src.as<uint32_t>()));
            } else if (src.is<double>()) {
                // Handle double precision floats
                return fl::make_shared<JsonValue>(src.as<double>());
            } else if (src.is<float>()) {
                // Handle single precision floats explicitly
                return fl::make_shared<JsonValue>(static_cast<double>(src.as<float>()));
            } else if (src.is<const char*>()) {
                return fl::make_shared<JsonValue>(fl::string(src.as<const char*>()));
            } else if (src.is<FLArduinoJson::JsonArrayConst>()) {
                FLArduinoJson::JsonArrayConst arr = src.as<FLArduinoJson::JsonArrayConst>();
                
                // Empty arrays should remain regular arrays
                if (arr.size() == 0) {
                    return fl::make_shared<JsonValue>(JsonArray{});
                }
                
                // Track optimization flags for array types
                bool isUint8 = true;  // Can all values fit in uint8_t?
                bool isInt16 = true;  // Can all values fit in int16_t?
                bool isFloat = true;  // Can all values be exactly represented as floats?
                
                // Enum to represent array optimization types
                enum ArrayType {
                    ALL_UINT8,
                    ALL_INT16,
                    ALL_FLOATS,
                    GENERIC_ARRAY
                };
                
                FASTLED_WARN("Array conversion: processing " << arr.size() << " items");
                
                for (const auto& item : arr) {
                    // Check if all items are numeric
                    if (!item.is<int32_t>() && !item.is<int64_t>() && !item.is<double>()) {
                        // Non-numeric value found, no optimization possible
                        isUint8 = false;
                        isInt16 = false;
                        isFloat = false;
                        FASTLED_WARN("Non-numeric value found, no optimization possible");
                        break;
                    }
                    
                    // Update type flags based on item type
                    if (item.is<double>()) {
                        double val = item.as<double>();
                        FASTLED_WARN("Processing double value: " << val);
                        
                        // Check if this can be exactly represented as a float
                        if (!canBeRepresentedAsFloat(val)) {
                            isFloat = false;
                            FASTLED_WARN("Value " << val << " cannot be exactly represented as float");
                        } else {
                            FASTLED_WARN("Value " << val << " CAN be exactly represented as float");
                        }
                        
                        // Check if this could fit in integer types
                        if (val < 0 || val > UINT8_MAX || val != floor(val)) {
                            isUint8 = false;
                            // FASTLED_WARN("Value " << val << " does not fit in uint8_t");
                        }
                        if (val < INT16_MIN || val > INT16_MAX || val != floor(val)) {
                            isInt16 = false;
                            // FASTLED_WARN("Value " << val << " does not fit in int16_t");
                        }
                    } else {
                        // Integer value
                        int64_t val = item.is<int32_t>() ? item.as<int32_t>() : item.as<int64_t>();
                        // FASTLED_WARN("Checking integer value: " << val);
                        
                        // Check uint8 range
                        if (val < 0 || val > UINT8_MAX) {
                            isUint8 = false;
                            // FASTLED_WARN("Value " << val << " does not fit in uint8_t");
                        }
                        
                        // Check int16 range
                        if (val < INT16_MIN || val > INT16_MAX) {
                            isInt16 = false;
                            FASTLED_WARN("Value " << val << " does not fit in int16_t");
                        }
                        
                        // Check if this integer can be exactly represented as a float
                        // All integers within the range of float precision can be exactly represented
                        if (val < -16777216 || val > 16777216) { // 2^24, beyond which floats lose precision
                            isFloat = false;
                            // FASTLED_WARN("Value " << val << " cannot be exactly represented as float");
                        }
                    }
                }
                
                // Special handling for float arrays - always prefer float arrays for ScreenMap use case
                // Check if all values are numeric (even if they don't fit in smaller types)
                bool allNumeric = true;
                for (const auto& item : arr) {
                    if (!item.is<int32_t>() && !item.is<int64_t>() && !item.is<double>()) {
                        allNumeric = false;
                        break;
                    }
                }
                
                // Determine the optimal array type based on the flags
                ArrayType arrayType = GENERIC_ARRAY;
                if (isUint8 && arr.size() > 0) {
                    arrayType = ALL_UINT8;
                } else if (isInt16 && arr.size() > 0) {
                    arrayType = ALL_INT16;
                } else if (isFloat && arr.size() > 0) {
                    // Only convert to float arrays when ALL values can be exactly represented as floats
                    // Don't convert just because values are numeric - precision matters
                    arrayType = ALL_FLOATS;
                }
                
                // Apply the most appropriate optimization based on determined type
                switch (arrayType) {
                    case ALL_UINT8: {
                        // All values fit in uint8_t - most compact representation
                        fl::vector<uint8_t> byteData;
                        for (const auto& item : arr) {
                            if (item.is<double>()) {
                                byteData.push_back(static_cast<uint8_t>(item.as<double>()));
                            } else {
                                int64_t val = item.is<int32_t>() ? item.as<int32_t>() : item.as<int64_t>();
                                byteData.push_back(static_cast<uint8_t>(val));
                            }
                        }
                        return fl::make_shared<JsonValue>(fl::move(byteData));
                    }
                    case ALL_INT16: {
                        // All values fit in int16_t - good compression
                        fl::vector<int16_t> intData;
                        for (const auto& item : arr) {
                            if (item.is<double>()) {
                                intData.push_back(static_cast<int16_t>(item.as<double>()));
                            } else {
                                int64_t val = item.is<int32_t>() ? item.as<int32_t>() : item.as<int64_t>();
                                intData.push_back(static_cast<int16_t>(val));
                            }
                        }
                        return fl::make_shared<JsonValue>(fl::move(intData));
                    }
                    case ALL_FLOATS: {
                        // All values can be exactly represented as floats - use float vector
                        fl::vector<float> floatData;
                        for (const auto& item : arr) {
                            if (item.is<double>()) {
                                floatData.push_back(static_cast<float>(item.as<double>()));
                            } else {
                                int64_t val = item.is<int32_t>() ? item.as<int32_t>() : item.as<int64_t>();
                                floatData.push_back(static_cast<float>(val));
                            }
                        }
                        return fl::make_shared<JsonValue>(fl::move(floatData));
                    }
                    case GENERIC_ARRAY:
                    default: {
                        // No optimization possible - use regular array
                        JsonArray regularArr;
                        for (const auto& item : arr) {
                            regularArr.push_back(convert(item));
                        }
                        return fl::make_shared<JsonValue>(fl::move(regularArr));
                    }
                }
            } else if (src.is<FLArduinoJson::JsonObjectConst>()) {
                JsonObject obj;
                for (const auto& kv : src.as<FLArduinoJson::JsonObjectConst>()) {
                    obj[fl::string(kv.key().c_str())] = convert(kv.value());
                }
                return fl::make_shared<JsonValue>(fl::move(obj));
            }
            return fl::make_shared<JsonValue>(nullptr); // Should not happen
        }
    };

    return Converter::convert(doc.as<FLArduinoJson::JsonVariantConst>());
    #endif
}

fl::string JsonValue::to_string() const {
#if !FASTLED_ENABLE_JSON
    // For a native implementation without external libraries, we'd need to implement
    // a custom serializer. For now, we'll delegate to Json::to_string_native() 
    // by creating a temporary Json object.
    Json temp;
    // Actually, let's implement a minimal version that handles basic types
    // since we don't want to rely on the ArduinoJson library for this method.
    
    if (is_null()) {
        return "null";
    } else if (is_bool()) {
        auto opt = as_bool();
        return opt ? (*opt ? "true" : "false") : "null";
    } else if (is_int()) {
        auto opt = as_int();
        return opt ? fl::to_string(*opt) : "null";
    } else if (is_double()) {
        auto opt = as_double();
        return opt ? fl::to_string(*opt) : "null";
    } else if (is_string()) {
        auto opt = as_string();
        return opt ? "\"" + *opt + "\"" : "null";
    } else if (is_array()) {
        fl::string result = "[";
        auto opt = as_array();
        if (opt) {
            bool first = true;
            for (const auto& item : *opt) {
                if (!first) result += ",";
                first = false;
                if (item) {
                    result += item->to_string();
                } else {
                    result += "null";
                }
            }
        }
        result += "]";
        return result;
    } else if (is_object()) {
        fl::string result = "{";
        auto opt = as_object();
        if (opt) {
            bool first = true;
            for (const auto& kv : *opt) {
                if (!first) result += ",";
                first = false;
                result += "\"" + kv.first + "\":";
                if (kv.second) {
                    result += kv.second->to_string();
                } else {
                    result += "null";
                }
            }
        }
        result += "}";
        return result;
    } else if (is_audio()) {
        return "[audio]";
    } else if (is_bytes()) {
        return "[bytes]";
    } else if (is_floats()) {
        return "[floats]";
    }
    return "null";
#else
    // Parse the JSON value to a string, then parse it back to a Json object,
    // and use the working to_string_native method
    // This is a workaround to avoid reimplementing the serialization logic
    
    // First, create a temporary Json document with this value
    Json temp;
    // Use the public method to set the value
    temp.set_value(fl::make_shared<JsonValue>(*this));
    // Use the working implementation
    return temp.to_string_native();
#endif
}

fl::string Json::to_string_native() const {
    #if !FASTLED_ENABLE_JSON
    return m_value ? m_value->to_string() : "null";
    #else
    if (!m_value) {
        return "null";
    }
    
    // 🚨 NEW APPROACH: Use fl::deque for memory-efficient JSON serialization
    // This avoids memory fragmentation and bypasses broken ArduinoJson integration
    fl::deque<char> json_chars;
    
    // Helper lambda for appending strings to deque
    auto append_string = [&json_chars](const char* str) {
        while (*str) {
            json_chars.push_back(*str);
            ++str;
        }
    };
    
    // Helper lambda for appending fl::string to deque
    auto append_fl_string = [&json_chars](const fl::string& str) {
        for (size_t i = 0; i < str.size(); ++i) {
            json_chars.push_back(str[i]);
        }
    };
    
    // Helper lambda for appending escaped strings
    auto append_escaped_string = [&](const fl::string& str) {
        json_chars.push_back('"');
        for (size_t i = 0; i < str.size(); ++i) {
            char c = str[i];
            switch (c) {
                case '"':  append_string("\\\""); break;
                case '\\': append_string("\\\\"); break;
                case '\n': append_string("\\n"); break;
                case '\r': append_string("\\r"); break;
                case '\t': append_string("\\t"); break;
                case '\b': append_string("\\b"); break;
                case '\f': append_string("\\f"); break;
                default:
                    json_chars.push_back(c);
                    break;
            }
        }
        json_chars.push_back('"');
    };
    
    // Recursive function to serialize JsonValue to deque
    fl::function<void(const JsonValue&)> serialize_value = [&](const JsonValue& value) {
        if (value.is_null()) {
            append_string("null");
        } else if (value.is_bool()) {
            auto opt = value.as_bool();
            if (opt) {
                append_string(*opt ? "true" : "false");
            } else {
                append_string("null");
            }
        } else if (value.is_int()) {
            auto opt = value.as_int();
            if (opt) {
                fl::string num_str;
                num_str.append(*opt);
                append_fl_string(num_str);
            } else {
                append_string("null");
            }
        } else if (value.is_double()) {
            auto opt = value.as_double();
            if (opt) {
                fl::string num_str;
                num_str.append(static_cast<float>(*opt));
                append_fl_string(num_str);
            } else {
                append_string("null");
            }
        } else if (value.is_string()) {
            auto opt = value.as_string();
            if (opt) {
                append_escaped_string(*opt);
            } else {
                append_string("null");
            }
        } else if (value.is_array()) {
            auto opt = value.as_array();
            if (opt) {
                json_chars.push_back('[');
                bool first = true;
                for (const auto& item : *opt) {
                    if (!first) {
                        json_chars.push_back(',');
                    }
                    first = false;
                    if (item) {
                        serialize_value(*item);
                    } else {
                        append_string("null");
                    }
                }
                json_chars.push_back(']');
            } else {
                append_string("null");
            }
        } else if (value.is_object()) {
            auto opt = value.as_object();
            if (opt) {
                json_chars.push_back('{');
                bool first = true;
                for (const auto& kv : *opt) {
                    if (!first) {
                        json_chars.push_back(',');
                    }
                    first = false;
                    append_escaped_string(kv.first);
                    json_chars.push_back(':');
                    if (kv.second) {
                        serialize_value(*kv.second);
                    } else {
                        append_string("null");
                    }
                }
                json_chars.push_back('}');
            } else {
                append_string("null");
            }
        } else {
            // Handle specialized array types
            if (value.is_audio()) {
                auto audioOpt = value.as_audio();
                if (audioOpt) {
                    json_chars.push_back('[');
                    bool first = true;
                    for (const auto& item : *audioOpt) {
                        if (!first) {
                            json_chars.push_back(',');
                        }
                        first = false;
                        fl::string num_str;
                        num_str.append(static_cast<int>(item));
                        append_fl_string(num_str);
                    }
                    json_chars.push_back(']');
                } else {
                    append_string("null");
                }
            } else if (value.is_bytes()) {
                auto bytesOpt = value.as_bytes();
                if (bytesOpt) {
                    json_chars.push_back('[');
                    bool first = true;
                    for (const auto& item : *bytesOpt) {
                        if (!first) {
                            json_chars.push_back(',');
                        }
                        first = false;
                        fl::string num_str;
                        num_str.append(static_cast<int>(item));
                        append_fl_string(num_str);
                    }
                    json_chars.push_back(']');
                } else {
                    append_string("null");
                }
            } else if (value.is_floats()) {
                auto floatsOpt = value.as_floats();
                if (floatsOpt) {
                    json_chars.push_back('[');
                    bool first = true;
                    for (const auto& item : *floatsOpt) {
                        if (!first) {
                            json_chars.push_back(',');
                        }
                        first = false;
                                                 fl::string num_str;
                         num_str.append(static_cast<float>(item));
                        append_fl_string(num_str);
                    }
                    json_chars.push_back(']');
                } else {
                    append_string("null");
                }
            } else {
                append_string("null");
            }
        }
    };
    
    // Serialize the root value
    serialize_value(*m_value);
    
    // Convert deque to fl::string efficiently
    fl::string result;
    if (!json_chars.empty()) {
        // Get pointer to the underlying data and construct string directly
        result.assign(&json_chars[0], json_chars.size());
    }
    
    return result;
    #endif
}

// Forward declaration for the serializeValue function
fl::string serializeValue(const JsonValue& value);


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

} // namespace fl


#endif  // FASTLED_ENABLE_JSON
