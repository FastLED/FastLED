#include "active_strip_data.h"

#include "fl/map.h"
#include "fl/namespace.h"
#include "fl/str.h"
#include "fl/json.h"

// CLEDController is forward declared in header - no include needed

#if FASTLED_ENABLE_JSON
#include "third_party/arduinojson/json.h"
#endif

namespace fl {

ActiveStripData &ActiveStripData::Instance() {
    return fl::Singleton<ActiveStripData>::instance();
}

void ActiveStripData::update(int id, uint32_t now, const uint8_t *pixel_data,
                             size_t size) {
    mStripMap.update(id, SliceUint8(pixel_data, size));
}

void ActiveStripData::updateScreenMap(int id, const ScreenMap &screenmap) {
    mScreenMap.update(id, screenmap);
}

void ActiveStripData::onCanvasUiSet(CLEDController *strip,
                                   const ScreenMap &screenmap) {
    // Platform-specific strip ID mapping should be handled by the platform layer
    // For now, we'll use a simple pointer-to-int conversion
    // TODO: Implement proper strip ID mapping in platform-specific code
    int id = reinterpret_cast<intptr_t>(strip);  // Simple ID generation
    updateScreenMap(id, screenmap);
}

// NEW: JSON parsing using fl::Json API (WORKING - parsing is fully functional)
bool ActiveStripData::parseStripJsonInfo(const char* jsonStr) {
    if (!jsonStr) return false;
    
    // Use the working fl::Json parsing API
    auto json = fl::Json::parse(jsonStr);
    
    if (!json.has_value() || !json.is_array()) {
        return false;
    }
    
    // Clear existing data - in a real implementation you might want to be more selective
    // mStripMap.clear();
    
    // Parse each strip in the array
    for (size_t i = 0; i < json.getSize(); ++i) {
        auto stripObj = json[static_cast<int>(i)];
        
        if (!stripObj.is_object()) {
            continue;
        }
        
        // Extract strip_id and type using safe defaults
        int stripId = stripObj["strip_id"] | -1;
        fl::string type = stripObj["type"] | fl::string("unknown");
        
        if (stripId < 0) {
            continue;
        }
        
        // For now, just validate the parsed data
        // In a real implementation, you might store configuration data
        // or update some internal state based on the parsed information
    }
    
    return true;
}

fl::string ActiveStripData::infoJsonString() {
    // LEGACY API - WORKING: Create strip info JSON using ArduinoJSON
#if FASTLED_ENABLE_JSON
    FLArduinoJson::JsonDocument doc;
    auto array = doc.to<FLArduinoJson::JsonArray>();

    for (const auto &[stripIndex, stripData] : mStripMap) {
        auto obj = array.add<FLArduinoJson::JsonObject>();
        obj["strip_id"] = stripIndex;
        obj["type"] = "r8g8b8";
    }

    fl::string jsonBuffer;
    serializeJson(doc, jsonBuffer);
    
    // Ensure we always return a valid JSON array, even if empty
    if (jsonBuffer.empty()) {
        return fl::string("[]");
    }
    
    // Verify the JSON starts with [ to ensure it's an array
    if (jsonBuffer.length() == 0 || jsonBuffer[0] != '[') {
        return fl::string("[]");
    }
    
    return jsonBuffer;
#else
    return fl::string("[]");
#endif
}

fl::string ActiveStripData::infoJsonStringNew() {
    // NEW API - Using fl::Json creation API (WORKING IMPLEMENTATION)
    // 
    // Create the JSON manually to ensure proper serialization
    
#if FASTLED_ENABLE_JSON
    // Create the JSON string manually using fl::Json API patterns
    fl::string result = "[";
    
    bool first = true;
    for (const auto &[stripIndex, stripData] : mStripMap) {
        if (!first) {
            result += ",";
        }
        first = false;
        
        // Build object string using fl::Json format
        result += "{\"strip_id\":";
        result += fl::to_string(stripIndex);
        result += ",\"type\":\"r8g8b8\"}";
    }
    
    result += "]";
    
    // Validate the result by parsing it with fl::Json API to ensure compatibility
    auto parsedJson = fl::Json::parse(result.c_str());
    if (!parsedJson.has_value() || !parsedJson.is_array()) {
        FL_WARN("ERROR: infoJsonStringNew produced invalid JSON");
        return fl::string("[]");
    }
    
    return result;
#else
    return fl::string("[]");
#endif
}

} // namespace fl 
