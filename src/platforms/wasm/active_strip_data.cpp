

#ifdef __EMSCRIPTEN__

// ⚠️⚠️⚠️ CRITICAL WARNING: C++ ↔ JavaScript STRIP DATA BRIDGE - HANDLE WITH EXTREME CARE! ⚠️⚠️⚠️
//
// 🚨 THIS FILE CONTAINS C++ TO JAVASCRIPT STRIP DATA BINDINGS 🚨
//
// DO NOT MODIFY FUNCTION SIGNATURES WITHOUT UPDATING CORRESPONDING JAVASCRIPT CODE!
//
// This file manages the critical bridge for LED strip pixel data between C++ and JavaScript.
// Any changes to:
// - EMSCRIPTEN_BINDINGS macro contents
// - ActiveStripData class methods exposed to JS
// - getPixelData_Uint8() return types or parameters
// - Constructor signatures in bindings
//
// Will BREAK JavaScript pixel data access and cause SILENT RUNTIME FAILURES!
//
// Key integration points that MUST remain synchronized:
// - EMSCRIPTEN_BINDINGS(engine_events_constructors)
// - ActiveStripData.getPixelData_Uint8() method
// - JavaScript new Module.ActiveStripData() constructor
// - JavaScript activeStrips.getPixelData_Uint8(stripIndex) calls
//
// Before making ANY changes:
// 1. Understand this affects real-time pixel data in the browser
// 2. Test with real WASM builds that render LED animations
// 3. Verify pixel data arrays are properly transferred to JavaScript
// 4. Check that strip indexing remains consistent
//
// ⚠️⚠️⚠️ REMEMBER: Pixel data errors cause corrupted LED displays! ⚠️⚠️⚠️

#include <emscripten/val.h>
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/val.h>

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

emscripten::val ActiveStripData::getPixelData_Uint8(int stripIndex) {
    // Efficient, zero copy conversion from internal data to JavaScript.
    SliceUint8 stripData;
    if (mStripMap.get(stripIndex, &stripData)) {
        const uint8_t *data = stripData.data();
        uint8_t *data_mutable = const_cast<uint8_t *>(data);
        size_t size = stripData.size();
        return emscripten::val(
            emscripten::typed_memory_view(size, data_mutable));
    }
    return emscripten::val::undefined();
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

static ActiveStripData *getActiveStripDataRef() {
    ActiveStripData *instance = &fl::Singleton<ActiveStripData>::instance();
    return instance;
}


EMSCRIPTEN_BINDINGS(engine_events_constructors) {
    emscripten::class_<ActiveStripData>("ActiveStripData")
        .constructor(&getActiveStripDataRef, emscripten::allow_raw_pointers())
        .function("getPixelData_Uint8", &ActiveStripData::getPixelData_Uint8);
}

} // namespace fl

#endif
