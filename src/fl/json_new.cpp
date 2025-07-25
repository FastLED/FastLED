#include "fl/json_new.h"
#if FASTLED_ENABLE_JSON
#include "third_party/arduinojson/json.h"
#endif

namespace fl {

#if FASTLED_ENABLE_JSON
// Helper function to convert FLArduinoJson variant to our json_value
fl::shared_ptr<json_value> convertFromArduinoJson(const FLArduinoJson::JsonVariantConst& variant) {
    if (variant.isNull()) {
        return fl::make_shared<json_value>(nullptr);
    } else if (variant.is<bool>()) {
        return fl::make_shared<json_value>(variant.as<bool>());
    } else if (variant.is<int>()) {
        return fl::make_shared<json_value>(static_cast<int64_t>(variant.as<int>()));
    } else if (variant.is<long>()) {
        return fl::make_shared<json_value>(static_cast<int64_t>(variant.as<long>()));
    } else if (variant.is<float>()) {
        return fl::make_shared<json_value>(static_cast<double>(variant.as<float>()));
    } else if (variant.is<double>()) {
        return fl::make_shared<json_value>(variant.as<double>());
    } else if (variant.is<const char*>()) {
        return fl::make_shared<json_value>(fl::string(variant.as<const char*>()));
    } else if (variant.is<FLArduinoJson::JsonArrayConst>()) {
        FLArduinoJson::JsonArrayConst arr = variant.as<FLArduinoJson::JsonArrayConst>();
        json_array result;
        result.reserve(arr.size());
        for (const auto& item : arr) {
            result.push_back(convertFromArduinoJson(item));
        }
        return fl::make_shared<json_value>(fl::move(result));
    } else if (variant.is<FLArduinoJson::JsonObjectConst>()) {
        FLArduinoJson::JsonObjectConst obj = variant.as<FLArduinoJson::JsonObjectConst>();
        json_object result;
        for (auto kvp : obj) {  // Remove const reference to avoid const issues
            result[fl::string(kvp.key().c_str())] = convertFromArduinoJson(kvp.value());
        }
        return fl::make_shared<json_value>(fl::move(result));
    }
    return fl::make_shared<json_value>(nullptr);
}

// Helper function to convert our json_value to FLArduinoJson variant
void convertToArduinoJson(const json_value& value, FLArduinoJson::JsonVariant& variant) {
    if (value.is_null()) {
        variant.set(nullptr);
    } else if (value.is_bool()) {
        auto ptr = value.data.ptr<bool>();
        variant.set(*ptr);
    } else if (value.is_int()) {
        auto ptr = value.data.ptr<int64_t>();
        variant.set(*ptr);
    } else if (value.is_double()) {
        auto ptr = value.data.ptr<double>();
        variant.set(*ptr);
    } else if (value.is_string()) {
        auto ptr = value.data.ptr<fl::string>();
        variant.set(ptr->c_str());
    } else if (value.is_array()) {
        FLArduinoJson::JsonArray arr = variant.to<FLArduinoJson::JsonArray>();
        auto ptr = value.data.ptr<json_array>();
        const json_array& jsonArray = *ptr;
        for (const auto& itemPtr : jsonArray) {
            if (itemPtr) {
                FLArduinoJson::JsonVariant nestedVariant = arr.add<FLArduinoJson::JsonVariant>();
                convertToArduinoJson(*itemPtr, nestedVariant);
            }
        }
    } else if (value.is_object()) {
        FLArduinoJson::JsonObject obj = variant.to<FLArduinoJson::JsonObject>();
        auto ptr = value.data.ptr<json_object>();
        const json_object& jsonObject = *ptr;
        for (const auto& kvp : jsonObject) {
            if (kvp.second) {
                FLArduinoJson::JsonVariant nestedVariant = obj[fl::string(kvp.first).c_str()];
                convertToArduinoJson(*kvp.second, nestedVariant);
            }
        }
    }
}
#endif

// Parse JSON string to json_value
fl::shared_ptr<json_value> json_value::parse(const fl::string &txt) {
#if FASTLED_ENABLE_JSON
    FLArduinoJson::JsonDocument doc; // Use JsonDocument instead of DynamicJsonDocument
    FLArduinoJson::DeserializationError error = FLArduinoJson::deserializeJson(doc, txt.c_str());
    
    if (error) {
        // Return null value on parse error
        return fl::make_shared<json_value>(nullptr);
    }
    
    return convertFromArduinoJson(doc.as<FLArduinoJson::JsonVariantConst>());
#else
    // If JSON is disabled, return null value
    return fl::make_shared<json_value>(nullptr);
#endif
}

// Convert json_value to JSON string
fl::string json_value::to_string() const {
#if FASTLED_ENABLE_JSON
    FLArduinoJson::JsonDocument doc; // Use JsonDocument instead of DynamicJsonDocument
    FLArduinoJson::JsonVariant variant = doc.as<FLArduinoJson::JsonVariant>();
    convertToArduinoJson(*this, variant);
    
    // Estimate size and serialize
    size_t len = FLArduinoJson::measureJson(doc);
    fl::string output;
    output.reserve(len + 1); // Reserve space for null terminator
    FLArduinoJson::serializeJson(doc, const_cast<char*>(output.data()), len + 1);
    
    return output;
#else
    // If JSON is disabled, return empty string
    return fl::string();
#endif
}

} // namespace fl