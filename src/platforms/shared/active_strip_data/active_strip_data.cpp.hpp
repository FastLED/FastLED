// IWYU pragma: private

#include "platforms/shared/active_strip_data/active_strip_data.h"

#include "fl/stl/map.h"
#include "fl/stl/string.h"
#include "fl/stl/json.h"
#include "fl/stl/compiler_control.h"
// CLEDController is forward declared in header - no include needed


namespace fl {

ActiveStripData &ActiveStripData::Instance() {
    return fl::Singleton<ActiveStripData>::instance();
}

void ActiveStripData::update(int id, u32 now, fl::span<const u8> pixel_data) {
    FL_UNUSED(now);
    mStripMap.update(id, pixel_data);
}

void ActiveStripData::updateScreenMap(int id, const ScreenMap &screenmap) {
    mScreenMap.update(id, screenmap);
}

void ActiveStripData::onCanvasUiSet(CLEDController *strip,
                                   const ScreenMap &screenmap) {
    // Use the IdTracker for consistent strip ID management across all platforms
    int id = mIdTracker.getOrCreateId(strip);
    updateScreenMap(id, screenmap);
}

// NEW: JSON parsing using fl::json API (WORKING - parsing is fully functional)
bool ActiveStripData::parseStripJsonInfo(const char* jsonStr) {
    if (!jsonStr) return false;
    
    // Use the working fl::json parsing API
    auto json = fl::json::parse(jsonStr);
    
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
    return infoJsonStringNew();
}

fl::string ActiveStripData::infoJsonStringNew() {
    // NEW API - Using fl::json creation API (PROPER IMPLEMENTATION)
    // 
    // This is the target implementation that the JSON creation API must support
    
#if FASTLED_ENABLE_JSON
    // Create a JSON array using the new fl::json API
    auto json = fl::json::createArray();
    
    // Add each strip as an object to the array
    for (const auto &pair : mStripMap) {
        int stripIndex = pair.first;
        auto stripObj = fl::json::createObject();
        stripObj.set("strip_id", stripIndex);
        stripObj.set("type", "r8g8b8");
        json.push_back(stripObj);
    }
    
    // Serialize the JSON structure
    return json.serialize();
#else
    return fl::string("[]");
#endif
}

} // namespace fl 
