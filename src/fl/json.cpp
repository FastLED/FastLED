#define ARDUINOJSON_USE_LONG_LONG 1
#include "fl/json.h"
#include "fl/string.h"
#include "fl/vector.h"
#include "fl/sketch_macros.h"



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
                JsonArray arr;
                for (const auto& item : src.as<FLArduinoJson::JsonArrayConst>()) {
                    arr.push_back(convert(item));
                }
                return fl::make_shared<JsonValue>(fl::move(arr));
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
