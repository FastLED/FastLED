#define ARDUINOJSON_USE_LONG_LONG 1
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
                
                // Check what type of optimization we can apply
                bool allUint8 = true;
                bool allInt16 = true;
                bool allBoolean = true;
                
                // First pass: determine what types are present
                for (const auto& item : arr) {
                    // Check if all items are numeric
                    if (!item.is<int32_t>() && !item.is<int64_t>() && !item.is<double>()) {
                        // Non-numeric value found, no optimization possible
                        allUint8 = false;
                        allInt16 = false;
                        allBoolean = false;
                        break;
                    }
                    
                    // Check if all items are integers
                    if (item.is<double>()) {
                        allBoolean = false;
                        
                        // Check if this float value is actually 1.0 or 0.0 (could be boolean)
                        double val = item.as<double>();
                        if (val != 0.0 && val != 1.0) {
                            allBoolean = false;
                        }
                        
                        // Check if this could fit in integer types
                        if (val < 0 || val > UINT8_MAX || val != floor(val)) {
                            allUint8 = false;
                        }
                        if (val < INT16_MIN || val > INT16_MAX || val != floor(val)) {
                            allInt16 = false;
                        }
                    } else {
                        // Integer value
                        int64_t val = item.is<int32_t>() ? item.as<int32_t>() : item.as<int64_t>();
                        
                        // Check boolean (0 or 1)
                        if (val != 0 && val != 1) {
                            allBoolean = false;
                        }
                        
                        // Check uint8 range
                        if (val < 0 || val > UINT8_MAX) {
                            allUint8 = false;
                        }
                        
                        // Check int16 range
                        if (val < INT16_MIN || val > INT16_MAX) {
                            allInt16 = false;
                        }
                    }
                }
                
                // Apply the most appropriate optimization
                if (allBoolean && arr.size() > 1) {
                    // All values are 0 or 1 - use a compact bit representation
                    // For now, we'll use uint8_t vector for boolean arrays
                    fl::vector<uint8_t> byteData;
                    for (const auto& item : arr) {
                        if (item.is<double>()) {
                            double val = item.as<double>();
                            byteData.push_back(static_cast<uint8_t>(val));
                        } else {
                            int64_t val = item.is<int32_t>() ? item.as<int32_t>() : item.as<int64_t>();
                            byteData.push_back(static_cast<uint8_t>(val));
                        }
                    }
                    return fl::make_shared<JsonValue>(fl::move(byteData));
                } else if (allUint8) {
                    // All values fit in uint8_t - most compact representation
                    fl::vector<uint8_t> byteData;
                    for (const auto& item : arr) {
                        if (item.is<double>()) {
                            double val = item.as<double>();
                            byteData.push_back(static_cast<uint8_t>(val));
                        } else {
                            int64_t val = item.is<int32_t>() ? item.as<int32_t>() : item.as<int64_t>();
                            byteData.push_back(static_cast<uint8_t>(val));
                        }
                    }
                    return fl::make_shared<JsonValue>(fl::move(byteData));
                } else if (allInt16) {
                    // All values fit in int16_t - good compression
                    fl::vector<int16_t> audioData;
                    for (const auto& item : arr) {
                        if (item.is<double>()) {
                            double val = item.as<double>();
                            audioData.push_back(static_cast<int16_t>(val));
                        } else {
                            int64_t val = item.is<int32_t>() ? item.as<int32_t>() : item.as<int64_t>();
                            audioData.push_back(static_cast<int16_t>(val));
                        }
                    }
                    return fl::make_shared<JsonValue>(fl::move(audioData));
                } else {
                    // No optimization possible - use regular array
                    JsonArray regularArr;
                    for (const auto& item : arr) {
                        regularArr.push_back(convert(item));
                    }
                    return fl::make_shared<JsonValue>(fl::move(regularArr));
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
}

fl::string Json::to_string_native() const {
    if (!m_value) {
        return "null";
    }
    
    // Create a JsonDocument to hold our data
    FLArduinoJson::JsonDocument doc;
    
    // Helper function to convert fl::Json::JsonValue to FLArduinoJson::JsonVariant
    struct Converter {
        static void convert(const JsonValue& src, FLArduinoJson::JsonVariant dst) {
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
            }
        }
    };
    
    // Convert our Value to JsonDocument
    Converter::convert(*m_value, doc);
    
    // Serialize to string
    fl::string output;
    FLArduinoJson::serializeJson(doc, output);
    return output;
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
