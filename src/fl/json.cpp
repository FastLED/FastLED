
#include "fl/json.h"
#include "fl/string.h"
#include "fl/vector.h"
#include "fl/sketch_macros.h"
#include "fl/math.h" // For floor function

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
                
                // Enum to represent array optimization types
                enum ArrayType {
                    ALL_UINT8,
                    ALL_INT16,
                    ALL_FLOATS,
                    GENERIC_ARRAY
                };
                
                // Single pass: determine the optimal array type
                bool isUint8 = true;
                bool isInt16 = true;
                bool isFloat = true;
                
                for (const auto& item : arr) {
                    // Check if all items are numeric
                    if (!item.is<int32_t>() && !item.is<int64_t>() && !item.is<double>()) {
                        // Non-numeric value found, no optimization possible
                        isUint8 = false;
                        isInt16 = false;
                        isFloat = false;
                        // FASTLED_WARN("Non-numeric value found, no optimization possible");
                        break;
                    }
                    
                    // Update type flags based on item type
                    if (item.is<double>()) {
                        double val = item.as<double>();
                        // FASTLED_WARN("Checking float value: " << val);
                        
                        // Check if this can be exactly represented as a float
                        if (!canBeRepresentedAsFloat(val)) {
                            isFloat = false;
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
                } else if ((isFloat || allNumeric) && arr.size() > 0) {
                    // For ScreenMap use case, prefer float arrays when we have numeric values
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
        return "[array]";
    } else if (is_object()) {
        return "{object}";
    } else if (is_audio()) {
        return "[audio]";
    } else if (is_bytes()) {
        return "[bytes]";
    } else if (is_floats()) {
        return "[floats]";
    }
    return "null";
}

fl::string Json::to_string_native() const {
    #if !FASTLED_ENABLE_JSON
    return m_value ? m_value->to_string() : "null";
    #else
    if (!m_value) {
        return "null";
    }
    
    // Create a JsonDocument to hold our data
    FLArduinoJson::JsonDocument doc;
    
    // Helper function to convert fl::Json::JsonValue to FLArduinoJson::JsonVariant
    struct Converter {
        static void convert(const JsonValue& src, FLArduinoJson::JsonVariant dst) {
            FL_WARN("*** Json::to_string_native: Converting JsonValue to FLArduinoJson::JsonVariant");
            if (src.is_null()) {
                dst.set(nullptr);
            } else if (src.is_bool()) {
                auto opt = src.as_bool();
                if (opt) dst.set(*opt);
            } else if (src.is_int()) {
                auto opt = src.as_int();
                if (opt) dst.set(*opt);
            } else if (src.is_double()) {
                auto opt = src.as_double();
                if (opt) dst.set(*opt);
            } else if (src.is_string()) {
                auto opt = src.as_string();
                if (opt) dst.set(opt->c_str());
            } else if (src.is_array()) {
                auto opt = src.as_array();
                if (opt) {
                    FLArduinoJson::JsonArray arr = dst.to<FLArduinoJson::JsonArray>();
                    for (const auto& item : *opt) {
                        if (item) {
                            FLArduinoJson::JsonVariant var = arr.add<FLArduinoJson::JsonVariant>();
                            convert(*item, var);
                        } else {
                            arr.add(nullptr);
                        }
                    }
                }
            } else if (src.is_object()) {
                auto opt = src.as_object();
                if (opt) {
                    FLArduinoJson::JsonObject obj = dst.to<FLArduinoJson::JsonObject>();
                    for (const auto& kv : *opt) {
                        if (kv.second) {
                            FLArduinoJson::JsonVariant var = obj[kv.first.c_str()];
                            convert(*kv.second, var);
                        } else {
                            obj[kv.first.c_str()] = nullptr;
                        }
                    }
                }
            } else {
                FL_WARN("*** Json::to_string_native: Unknown JsonValue type" << src.to_string());
            }
        }
    };
    
    // Convert our Value to JsonDocument
    Converter::convert(*m_value, doc);
    
    // Serialize to string
    fl::string output;
    FLArduinoJson::serializeJson(doc, output);


    return output;
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
