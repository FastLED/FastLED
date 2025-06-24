

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
#include "fl/slice.h"

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

Str ActiveStripData::infoJsonString() {
    FLArduinoJson::JsonDocument doc;
    auto array = doc.to<FLArduinoJson::JsonArray>();

    for (const auto &[stripIndex, stripData] : mStripMap) {
        auto obj = array.add<FLArduinoJson::JsonObject>();
        obj["strip_id"] = stripIndex;
        obj["type"] = "r8g8b8";
    }

    Str jsonBuffer;
    serializeJson(doc, jsonBuffer);
    return jsonBuffer;
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
