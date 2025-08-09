
#include "fl/json.h"
#include "fl/string.h"
#include "fl/vector.h"
#include "fl/deque.h"
#include "fl/function.h"
#include "fl/sketch_macros.h"
#include "fl/math.h" // For floor function
#include "fl/compiler_control.h"
#include "fl/thread_local.h"

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


#if FASTLED_ENABLE_JSON

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_NULL_DEREFERENCE
#include "third_party/arduinojson/json.h"
FL_DISABLE_WARNING_POP

#endif  // FASTLED_ENABLE_JSON

namespace fl {



// Helper function to check if a double can be reasonably represented as a float
// Used for debug logging - may appear unused in release builds
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(unused-function)
static bool canBeRepresentedAsFloat(double value) {

    
    auto isnan = [](double value) -> bool {
        return value != value;
    };
    
    // Check for special values
    if (isnan(value)) {
        return true; // These can be represented as float
    }
    
    // Check if the value is within reasonable float range
    // Reject values that are clearly beyond float precision (beyond 2^24 for integers)
    // or outside the float range
    if (fl::fl_abs(value) > 16777216.0) { // 2^24 - beyond which floats lose integer precision
        return false;
    }
    
    // For values within reasonable range, allow conversion even with minor precision loss
    // This handles cases like 300000.14159 which should be convertible to float
    // even though it loses some precision
    return true;
}
FL_DISABLE_WARNING_POP    


JsonValue& get_null_value() {
    static ThreadLocal<JsonValue> null_value;
    return null_value.access();
}

JsonObject& get_empty_json_object() {
    // thread_local JsonObject empty_object;
    static ThreadLocal<JsonObject> empty_object;
    return empty_object.access();
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
                // Handle double precision floats - convert to float
                return fl::make_shared<JsonValue>(static_cast<float>(src.as<double>()));
            } else if (src.is<float>()) {
                // Handle single precision floats explicitly
                return fl::make_shared<JsonValue>(src.as<float>());
            } else if (src.is<const char*>()) {
                return fl::make_shared<JsonValue>(fl::string(src.as<const char*>()));
            } else if (src.is<FLArduinoJson::JsonArrayConst>()) {
                FLArduinoJson::JsonArrayConst arr = src.as<FLArduinoJson::JsonArrayConst>();
                
                // Empty arrays should remain regular arrays
                if (arr.size() == 0) {
                    return fl::make_shared<JsonValue>(JsonArray{});
                }
                
                // Enum to represent array optimization types
                enum ArrayType {
                    ALL_UINT8,
                    ALL_INT16,
                    ALL_FLOATS,
                    GENERIC_ARRAY
                };
                
                // Helper struct to track array type info
                struct ArrayTypeInfo {
                    bool isUint8 = true;
                    bool isInt16 = true;
                    bool isFloat = true;
                    
                    void disableAll() {
                        isUint8 = false;
                        isInt16 = false;
                        isFloat = false;
                    }
                    
                    void checkNumericValue(double val) {
                        // Check integer ranges in one pass
                        bool isInteger = val == floor(val);
                        if (!isInteger || val < 0 || val > UINT8_MAX) {
                            isUint8 = false;
                        }
                        if (!isInteger || val < INT16_MIN || val > INT16_MAX) {
                            isInt16 = false;
                        }
                        if (!canBeRepresentedAsFloat(val)) {
                            isFloat = false;
                        }
                    }
                    
                    void checkIntegerValue(int64_t val) {
                        // Check all ranges in one pass
                        if (val < 0 || val > UINT8_MAX) {
                            isUint8 = false;
                        }
                        if (val < INT16_MIN || val > INT16_MAX) {
                            isInt16 = false;
                        }
                        if (val < -16777216 || val > 16777216) {
                            isFloat = false;
                        }
                    }
                    
                    ArrayType getBestType() const {
                        if (isUint8) return ALL_UINT8;
                        if (isInt16) return ALL_INT16;
                        if (isFloat) return ALL_FLOATS;
                        return GENERIC_ARRAY;
                    }
                };
                
                ArrayTypeInfo typeInfo;
                
                #if FASTLED_DEBUG_LEVEL >= 2
                FASTLED_WARN("Array conversion: processing " << arr.size() << " items");
                #endif
                
                for (const auto& item : arr) {
                    // Check if all items are numeric
                    if (!item.is<int32_t>() && !item.is<int64_t>() && !item.is<double>()) {
                        typeInfo.disableAll();
                        #if FASTLED_DEBUG_LEVEL >= 2
                        FASTLED_WARN("Non-numeric value found, no optimization possible");
                        #endif
                        break;
                    }
                    
                    // Update type flags based on item type
                    if (item.is<double>()) {
                        double val = item.as<double>();
                        typeInfo.checkNumericValue(val);
                    } else {
                        int64_t val = item.is<int32_t>() ? item.as<int32_t>() : item.as<int64_t>();
                        typeInfo.checkIntegerValue(val);
                    }
                }
                
                // Determine the optimal array type based on the flags
                ArrayType arrayType = arr.size() > 0 ? typeInfo.getBestType() : GENERIC_ARRAY;
                
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
    // Parse the JSON value to a string, then parse it back to a Json object,
    // and use the working to_string_native method
    // This is a workaround to avoid reimplementing the serialization logic
    
    // First, create a temporary Json document with this value
    Json temp;
    // Use the public method to set the value
    temp.set_value(fl::make_shared<JsonValue>(*this));
    // Use the working implementation
    return temp.to_string_native();
}

fl::string Json::to_string_native() const {
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
        } else if (value.is_float()) {
            auto opt = value.as_float();
            if (opt) {
                fl::string num_str;
                // Use fl::string::append which already handles float formatting correctly
                //num_str.append(*opt);
                num_str.append(static_cast<float>(*opt), 3);
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
                        num_str.append(static_cast<float>(item), 6);
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
