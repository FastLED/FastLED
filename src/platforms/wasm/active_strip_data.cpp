

#ifdef __EMSCRIPTEN__

// ⚠️⚠️⚠️ CRITICAL WARNING: C++ ↔ JavaScript STRIP DATA BRIDGE - HANDLE WITH EXTREME CARE! ⚠️⚠️⚠️
//
// 🚨 THIS FILE CONTAINS C++ TO JAVASCRIPT STRIP DATA CCALL BINDINGS 🚨
//
// DO NOT MODIFY FUNCTION SIGNATURES WITHOUT UPDATING CORRESPONDING JAVASCRIPT CODE!
//
// This file manages the critical bridge for LED strip pixel data between C++ and JavaScript
// using the ccall mechanism only (embind mechanism has been removed).
//
// Any changes to:
// - getStripPixelData() function signature  
// - Parameter types or return values
// - EMSCRIPTEN_KEEPALIVE declarations
//
// Will BREAK JavaScript pixel data access and cause SILENT RUNTIME FAILURES!
//
// Key integration points that MUST remain synchronized:
// - getStripPixelData(int stripIndex, int* outSize) ccall function
// - JavaScript Module.ccall('getStripPixelData', ...) calls
// - Parameter and return type consistency between C++ and JS
//
// Before making ANY changes:
// 1. Understand this affects real-time pixel data in the browser
// 2. Test with real WASM builds that render LED animations
// 3. Verify pixel data arrays are properly transferred to JavaScript
// 4. Check that strip indexing remains consistent
//
// ⚠️⚠️⚠️ REMEMBER: Pixel data errors cause corrupted LED displays! ⚠️⚠️⚭️

#include <emscripten.h>
#include <emscripten/emscripten.h>


// embind headers removed - using only ccall mechanism


#include "fl/map.h"
#include "fl/singleton.h"
#include "fl/span.h"

#include "active_strip_data.h"
#include "fl/map.h"
#include "fl/namespace.h"
#include "fl/str.h"
#include "fl/json.h"
#include "js.h"
#include "platforms/wasm/engine_listener.h"

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

// NEW: JSON parsing using fl::Json API (WORKING - parsing is fully functional)
bool ActiveStripData::parseStripJsonInfo(const char* jsonStr) {
    if (!jsonStr) return false;
    
    // Use the working fl::Json parsing API
    auto json = fl::Json::parse(jsonStr);
    
    if (!json.has_value() || !json.is_array()) {
        printf("ERROR: Failed to parse strip JSON or not an array\n");
        return false;
    }
    
    printf("DEBUG: Parsing JSON with %zu strips\n", json.getSize());
    
    // Clear existing data
    mStripMap.clear();
    
    // Parse each strip in the array
    for (size_t i = 0; i < json.getSize(); ++i) {
        auto stripObj = json[static_cast<int>(i)];
        
        if (!stripObj.is_object()) {
            printf("WARNING: Strip %zu is not an object\n", i);
            continue;
        }
        
        // Extract strip_id and type using safe defaults
        int stripId = stripObj["strip_id"] | -1;
        fl::string type = stripObj["type"] | fl::string("unknown");
        
        if (stripId < 0) {
            printf("WARNING: Strip %zu has invalid strip_id\n", i);
            continue;
        }
        
        printf("DEBUG: Parsed strip %d with type %s\n", stripId, type.c_str());
        
        // For now, just log the parsed data
        // In a real implementation, you might store configuration data
        // or update some internal state based on the parsed information
    }
    
    printf("SUCCESS: Parsed %zu strips from JSON\n", json.getSize());
    return true;
}

Str ActiveStripData::infoJsonString() {
    // LEGACY API - WORKING: Create strip info JSON using ArduinoJSON
    FLArduinoJson::JsonDocument doc;
    auto array = doc.to<FLArduinoJson::JsonArray>();

    // Debug: Log how many strips we have
    printf("DEBUG: ActiveStripData has %zu strips\n", mStripMap.size());

    for (const auto &[stripIndex, stripData] : mStripMap) {
        auto obj = array.add<FLArduinoJson::JsonObject>();
        obj["strip_id"] = stripIndex;
        obj["type"] = "r8g8b8";
        printf("DEBUG: Added strip %d to JSON\n", stripIndex);
    }

    Str jsonBuffer;
    serializeJson(doc, jsonBuffer);
    
    // Debug: Log the final JSON
    printf("DEBUG: Generated JSON: %s\n", jsonBuffer.c_str());
    
    // Ensure we always return a valid JSON array, even if empty
    if (jsonBuffer.empty()) {
        printf("DEBUG: JSON buffer was empty, returning []\n");
        return Str("[]");
    }
    
    // Verify the JSON starts with [ to ensure it's an array
    if (jsonBuffer.length() == 0 || jsonBuffer[0] != '[') {
        printf("DEBUG: JSON doesn't start with [, returning []\n");
        return Str("[]");
    }
    
    return jsonBuffer;
}

Str ActiveStripData::infoJsonStringNew() {
    // NEW API - FOR FUTURE USE: Once fl::Json creation is implemented, this will work
    printf("DEBUG: Using NEW fl::Json API for strip info generation\n");
    
    // TODO: Uncomment when fl::Json creation is fully working
    /*
    auto json = fl::Json::createArray();
    for (const auto &[stripIndex, stripData] : mStripMap) {
        auto obj = fl::Json::createObject();
        obj.set("strip_id", stripIndex);
        obj.set("type", "r8g8b8");
        json.push_back(obj);
    }
    Str jsonBuffer = json.serialize();
    return jsonBuffer;
    */
    
    // TEMPORARY: Fall back to legacy implementation until creation is fixed
    printf("WARNING: fl::Json creation not ready, falling back to legacy API\n");
    return infoJsonString();
}


/// WARNING: For some reason the following code must be here, when
/// it was moved to embind.cpp frame data stopped being updated.
// gcc constructor to get the
// ActiveStripData instance created.
__attribute__((constructor)) void __init_ActiveStripData() {
    ActiveStripData::Instance();
}


// ccall implementation - single exported function
// JavaScript usage:
//   let sizePtr = Module._malloc(4);
//   let dataPtr = Module.ccall('getStripPixelData', 'number', ['number', 'number'], [stripIndex, sizePtr]);
//   if (dataPtr !== 0) {
//       let size = Module.getValue(sizePtr, 'i32');
//       let pixelData = new Uint8Array(Module.HEAPU8.buffer, dataPtr, size);
//   }
//   Module._free(sizePtr);
extern "C" EMSCRIPTEN_KEEPALIVE 
uint8_t* getStripPixelData(int stripIndex, int* outSize) {
    ActiveStripData& instance = ActiveStripData::Instance();
    SliceUint8 stripData;
    
    if (instance.getData().get(stripIndex, &stripData)) {
        if (outSize) *outSize = static_cast<int>(stripData.size());
        return const_cast<uint8_t*>(stripData.data());
    }
    
    if (outSize) *outSize = 0;
    return nullptr;
}



// embind implementation removed - now using only ccall mechanism above


} // namespace fl

#endif
