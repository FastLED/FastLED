#ifdef __EMSCRIPTEN__

// ⚠️⚠️⚠️ CRITICAL WARNING: C++ ↔ JavaScript PURE ARCHITECTURE BRIDGE - HANDLE WITH EXTREME CARE! ⚠️⚠️⚠️
//
// 🚨 THIS FILE CONTAINS C++ TO JAVASCRIPT PURE DATA EXPORT FUNCTIONS 🚨
//
// DO NOT MODIFY FUNCTION SIGNATURES WITHOUT UPDATING CORRESPONDING JAVASCRIPT CODE!
// 
// This file provides a PURE DATA EXPORT LAYER between C++ and JavaScript.
// No embedded JavaScript - only simple C++ functions that export data.
// All async logic is handled in pure JavaScript modules.
//
// Key data export functions:
// - getFrameData() - exports frame data as JSON 
// - freeFrameData() - frees allocated frame data
// - getStripUpdateData() - exports strip update data
// - notifyStripAdded() - simple strip addition notification
// - processUiInput() - processes UI input from JavaScript
//
// All JavaScript integration is handled by:
// - fastled_async_controller.js - Pure JavaScript async controller
// - fastled_callbacks.js - User callback interface  
// - fastled_events.js - Event-driven architecture
//
// ⚠️⚠️⚠️ REMEMBER: This is a PURE DATA LAYER - no JavaScript embedded! ⚠️⚠️⚠️

#include <emscripten.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <cfloat> // ok include
#include <string>

#include "js_bindings.h"

#include "platforms/shared/active_strip_data/active_strip_data.h"
#include "platforms/wasm/js.h"
#include "fl/dbg.h"
#include "fl/stl/math.h"
#include "fl/stl/malloc.h"
#include "fl/stl/cstring.h"
#include "fl/screenmap.h"
#include "fl/json.h"
#include "fl/stl/stdio.h"

// EM_JS function to push screenmap updates to JavaScript
// This allows C++ to notify JavaScript when screenmap data changes
// JavaScript can then cache the data and avoid per-frame polling
EM_JS(void, js_notify_screenmap_update, (const char* jsonData), {
    if (typeof self !== 'undefined' && self.postMessage) {
        const data = UTF8ToString(jsonData);
        const parsedData = JSON.parse(data);
        self.postMessage({
            type: 'screenmap_update',
            payload: {
                screenMapData: parsedData
            }
        });
    } else if (typeof window !== 'undefined') {
        console.log('[C++] Screenmap update:', UTF8ToString(jsonData));
    }
});

// Forward declarations for functions used in this file
namespace fl {
void jsUpdateUiComponents(const std::string &jsonStr);  // okay std namespace
}

namespace fl {

// Forward declarations for functions defined later in this file
EMSCRIPTEN_KEEPALIVE void jsFillInMissingScreenMaps(ActiveStripData &active_strips);

} // namespace fl

// Exported C functions for JavaScript access
extern "C" {

/**
 * Frame Data Export Function
 * Exports frame data as JSON string with size information
 * Returns malloc'd buffer that must be freed with freeFrameData()
 */
EMSCRIPTEN_KEEPALIVE void* getFrameData(int* dataSize) {
    // Fill active strips data
    fl::ActiveStripData& active_strips = fl::ActiveStripData::Instance();
    fl::jsFillInMissingScreenMaps(active_strips);
    
    // Serialize to JSON
    fl::Str json_str = active_strips.infoJsonString();
    
    // Allocate and return data pointer
    char* buffer = (char*)fl::malloc(json_str.length() + 1);
    fl::strcpy(buffer, json_str.c_str());
    *dataSize = json_str.length();
    
    return buffer;
}

/**
 * ScreenMap Export Function
 * Exports screenMap data as JSON string with size information
 * Returns malloc'd buffer that must be freed with freeFrameData()
 */
EMSCRIPTEN_KEEPALIVE void* getScreenMapData(int* dataSize) {
    fl::ActiveStripData& active_strips = fl::ActiveStripData::Instance();
    const auto& screenMaps = active_strips.getScreenMaps();

    // Create dictionary of screenmaps (stripId → screenmap object)
    // Same format as _jsSetCanvasSize() to match JavaScript expectations
    fl::Json root = fl::Json::object();

    // Build a separate screenmap object for each strip
    for (const auto &[stripIndex, screenMap] : screenMaps) {
        // Create this strip's screenmap object
        fl::Json screenMapObj = fl::Json::object();

        // Create strips object containing only this strip
        fl::Json stripsObj = fl::Json::object();
        fl::Json stripMapObj = fl::Json::object();

        fl::Json mapObj = fl::Json::object();
        fl::Json xArray = fl::Json::array();
        fl::Json yArray = fl::Json::array();

        for (uint32_t i = 0; i < screenMap.getLength(); i++) {
            float x = screenMap[i].x;
            float y = screenMap[i].y;

            xArray.push_back(fl::Json(x));
            yArray.push_back(fl::Json(y));
        }

        mapObj.set("x", xArray);
        mapObj.set("y", yArray);
        stripMapObj.set("map", mapObj);

        // Add diameter
        stripMapObj.set("diameter", screenMap.getDiameter());

        // Add this strip to the strips object
        stripsObj.set(fl::to_string(stripIndex), stripMapObj);

        // Set strips object
        screenMapObj.set("strips", stripsObj);

        // Add this screenmap to the root dictionary
        root.set(fl::to_string(stripIndex), screenMapObj);
    }

    // Serialize to JSON
    fl::Str json_str = root.to_string();

    // Allocate and return data pointer
    char* buffer = (char*)fl::malloc(json_str.length() + 1);
    fl::strcpy(buffer, json_str.c_str());
    *dataSize = json_str.length();

    return buffer;
}

/**
 * Pure C++ Memory Management Function
 * Frees frame data allocated by getFrameData()
 */
EMSCRIPTEN_KEEPALIVE void freeFrameData(void* data) {
    if (data) {
        fl::free(data);
    }
}

/**
 * Frame Version Function
 * Gets current frame version number for JavaScript polling
 */
EMSCRIPTEN_KEEPALIVE uint32_t getFrameVersion() {
    // Simple frame counter using millis()
    // WASM is single-threaded so this is safe
    static uint32_t frameCounter = 0;
    frameCounter++;
    return frameCounter;
}

/**
 * New Frame Data Check Function
 * Checks if new frame data is available since last known version
 */
EMSCRIPTEN_KEEPALIVE bool hasNewFrameData(uint32_t lastKnownVersion) {
    // Simple implementation - in WASM single-threaded environment
    // we can assume there's always new data if the versions differ
    return getFrameVersion() > lastKnownVersion;
}

/**
 * Pure C++ UI Input Processing Function
 * Processes UI input JSON from JavaScript
 */
EMSCRIPTEN_KEEPALIVE void processUiInput(const char* jsonInput) {
    if (!jsonInput) {
        printf("Error: Received null UI input\n");
        return;
    }
    
    // Process UI input from JavaScript
    // Forward to existing UI system
    fl::jsUpdateUiComponents(std::string(jsonInput));  // okay std namespace
}

} // extern "C"

namespace fl {

/**
 * Pure C++ Strip Update Data Export Function
 * Exports strip update data as JSON for specific strip
 */
EMSCRIPTEN_KEEPALIVE void* getStripUpdateData(int stripId, int* dataSize) {
    // Generate basic strip update JSON
    fl::Json doc = fl::Json::object();
    doc.set("strip_id", stripId);
    doc.set("event", "strip_update");
    doc.set("timestamp", static_cast<int>(millis()));

    fl::Str jsonBuffer = doc.to_string();

    // Allocate and return data pointer
    char* buffer = (char*)fl::malloc(jsonBuffer.length() + 1);
    fl::strcpy(buffer, jsonBuffer.c_str());
    *dataSize = jsonBuffer.length();

    return buffer;
}

/**
 * Pure C++ Strip Addition Notification
 * Simple notification - no JavaScript embedded
 */
EMSCRIPTEN_KEEPALIVE void notifyStripAdded(int stripId, int numLeds) {
    // Simple notification - JavaScript will handle the async logic
    printf("Strip added: ID %d, LEDs %d\n", stripId, numLeds);
}

/**
 * Pure C++ UI Data Export Function
 * Exports UI changes as JSON for JavaScript processing
 */
EMSCRIPTEN_KEEPALIVE void* getUiUpdateData(int* dataSize) {
    // Export basic UI update structure
    fl::Json doc = fl::Json::object();
    doc.set("event", "ui_update");
    doc.set("timestamp", static_cast<int>(millis()));

    fl::Str jsonBuffer = doc.to_string();

    // Allocate and return data pointer
    char* buffer = (char*)fl::malloc(jsonBuffer.length() + 1);
    fl::strcpy(buffer, jsonBuffer.c_str());
    *dataSize = jsonBuffer.length();

    return buffer;
}

/**
 * Canvas Size Setting Function - Push-based notification to JavaScript
 * When a screenmap is created or updated, this function pushes the complete
 * screenmap data to JavaScript, eliminating the need for per-frame polling
 *
 * Internal function called by EngineListener when screenmaps are updated.
 * Not exposed in public API - platform-specific listener handles JS communication.
 */
void _jsSetCanvasSize(int cledcontoller_id, const fl::ScreenMap &screenmap) {
    // Get the complete screenmap data for all strips
    fl::ActiveStripData& active_strips = fl::ActiveStripData::Instance();
    const auto& screenMaps = active_strips.getScreenMaps();

    // Create dictionary of screenmaps (stripId → screenmap object)
    fl::Json root = fl::Json::object();

    // Build a separate screenmap object for each strip
    for (const auto &[stripIndex, screenMap] : screenMaps) {
        // Create this strip's screenmap object
        fl::Json screenMapObj = fl::Json::object();

        // Create strips object containing only this strip
        fl::Json stripsObj = fl::Json::object();
        fl::Json stripMapObj = fl::Json::object();

        fl::Json mapObj = fl::Json::object();
        fl::Json xArray = fl::Json::array();
        fl::Json yArray = fl::Json::array();

        for (uint32_t i = 0; i < screenMap.getLength(); i++) {
            float x = screenMap[i].x;
            float y = screenMap[i].y;

            xArray.push_back(fl::Json(x));
            yArray.push_back(fl::Json(y));
        }

        mapObj.set("x", xArray);
        mapObj.set("y", yArray);
        stripMapObj.set("map", mapObj);

        // Add diameter
        stripMapObj.set("diameter", screenMap.getDiameter());

        // Add this strip to the strips object
        stripsObj.set(fl::to_string(stripIndex), stripMapObj);

        // Set strips object
        screenMapObj.set("strips", stripsObj);

        // Add this screenmap to the root dictionary
        root.set(fl::to_string(stripIndex), screenMapObj);
    }

    // Serialize to JSON
    fl::Str jsonBuffer = root.to_string();

    // Push the complete screenmap data to JavaScript
    // JavaScript worker will cache this and avoid per-frame polling
    // NOTE: EM_JS has linking issues with partial linking - use polling approach instead
    // Worker will call getScreenMapData() during initialization to fetch this data
    FL_DBG("Screenmap update available (worker will poll via getScreenMapData): " << jsonBuffer.c_str());
}

EMSCRIPTEN_KEEPALIVE void jsFillInMissingScreenMaps(ActiveStripData &active_strips) {
    struct Function {
        static bool isSquare(int num) {
            int root = sqrt(num);
            return root * root == num;
        }
    };
    const auto &info = active_strips.getData();
    // check to see if we have any missing screenmaps.
    for (const auto &pair : info) {
        int stripIndex = pair.first;
        const auto &stripData = pair.second;
        const bool has_screen_map = active_strips.hasScreenMap(stripIndex);
        if (!has_screen_map) {
            printf("Missing screenmap for strip %d\n", stripIndex);
            // okay now generate a screenmap for this strip, let's assume
            // a linear strip with only one row.
            const uint32_t pixel_count = stripData.size() / 3;
            ScreenMap screenmap(pixel_count);
            if (pixel_count > 255 && Function::isSquare(pixel_count)) {
                printf("Creating square screenmap for %d\n", pixel_count);
                uint32_t side = sqrt(pixel_count);
                // This is a square matrix, let's assume it's a square matrix
                // and generate a screenmap for it.
                for (uint16_t i = 0; i < side; i++) {
                    for (uint16_t j = 0; j < side; j++) {
                        uint16_t index = i * side + j;
                        vec2f p = {
                            static_cast<float>(i),
                            static_cast<float>(j)
                        };
                        screenmap.set(index, p);
                    }
                }
                active_strips.updateScreenMap(stripIndex, screenmap);
                // Fire off the event to the JavaScript side that we now have
                // a screenmap for this strip.
                _jsSetCanvasSize(stripIndex, screenmap);
            } else {
                printf("Creating linear screenmap for %d\n", pixel_count);
                ScreenMap screenmap(pixel_count);
                for (uint32_t i = 0; i < pixel_count; i++) {
                    screenmap.set(i, {static_cast<float>(i), 0});
                }
                active_strips.updateScreenMap(stripIndex, screenmap);
                // Fire off the event to the JavaScript side that we now have
                // a screenmap for this strip.
                _jsSetCanvasSize(stripIndex, screenmap);
            }
        }
    }
}

/**
 * Pure C++ Frame Processing Function - Exports data instead of calling JavaScript
 */
EMSCRIPTEN_KEEPALIVE void jsOnFrame(ActiveStripData& active_strips) {
    jsFillInMissingScreenMaps(active_strips);
    // JavaScript will call getFrameData() to retrieve the frame data
    // No embedded JavaScript - pure data export approach
}

/**
 * Pure C++ Strip Addition Notification - Simple logging
 */
EMSCRIPTEN_KEEPALIVE void jsOnStripAdded(uintptr_t strip, uint32_t num_leds) {
    // Use the pure C++ notification function
    notifyStripAdded(strip, num_leds);
}

/**
 * Pure C++ UI Update Function - Simple data processing
 */
EMSCRIPTEN_KEEPALIVE void updateJs(const char* jsonStr) {
    printf("updateJs: ENTRY - PURE C++ VERSION - jsonStr=%s\n", jsonStr ? jsonStr : "nullptr");
    
    // Process UI input using pure C++ function
    ::processUiInput(jsonStr);
    
    printf("updateJs: EXIT - PURE C++ VERSION\n");
}

/**
 * Strip Pixel Data Access - Critical JavaScript Bridge
 * 
 * ⚠️⚠️⚠️ CRITICAL WARNING: C++ ↔ JavaScript STRIP DATA BRIDGE ⚠️⚠️⚠️
 * 
 * This function provides direct access to LED strip pixel data for JavaScript.
 * Any changes to the function signature will BREAK JavaScript pixel data access!
 * 
 * JavaScript usage:
 *   let sizePtr = Module._malloc(4);
 *   let dataPtr = Module.ccall('getStripPixelData', 'number', ['number', 'number'], [stripIndex, sizePtr]);
 *   if (dataPtr !== 0) {
 *       let size = Module.getValue(sizePtr, 'i32');
 *       let pixelData = new Uint8Array(Module.HEAPU8.buffer, dataPtr, size);
 *   }
 *   Module._free(sizePtr);
 */
extern "C" EMSCRIPTEN_KEEPALIVE 
uint8_t* getStripPixelData(int stripIndex, int* outSize) {
    ActiveStripData& instance = ActiveStripData::Instance();
    ActiveStripData::StripDataMap::mapped_type stripData;
    
    if (instance.getData().get(stripIndex, &stripData)) {
        if (outSize) *outSize = static_cast<int>(stripData.size());
        return const_cast<uint8_t*>(stripData.data());
    }
    
    if (outSize) *outSize = 0;
    return nullptr;
}

} // namespace fl

#endif // __EMSCRIPTEN__
