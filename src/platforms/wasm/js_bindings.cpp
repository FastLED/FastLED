#ifdef __EMSCRIPTEN__

// ⚠️⚠️⚠️ CRITICAL WARNING: C++ ↔ JavaScript BRIDGE - HANDLE WITH EXTREME CARE! ⚠️⚠️⚠️
//
// 🚨 THIS FILE CONTAINS C++ TO JAVASCRIPT BINDINGS 🚨
//
// DO NOT MODIFY FUNCTION SIGNATURES WITHOUT UPDATING CORRESPONDING JAVASCRIPT CODE!
// 
// This file is a BRIDGE between C++ and JavaScript. Any changes to:
// - Function names
// - Parameter types  
// - Return types
// - Function signatures
// - EM_ASM macro contents
//
// Will BREAK the JavaScript side and cause SILENT RUNTIME FAILURES!
//
// Before making ANY changes:
// 1. Understand that this affects BOTH C++ AND JavaScript code
// 2. Plan to update JavaScript Module.cwrap() calls simultaneously  
// 3. Test extensively with real WASM builds (not just compilation)
// 4. Verify globalThis.FastLED_* callback functions still work
//
// Key JavaScript integration points in this file:
// - globalThis.FastLED_onStripUpdate()
// - globalThis.FastLED_onFrame() 
// - globalThis.FastLED_onStripAdded()
// - globalThis.FastLED_onUiElementsAdded()
// - Module.cwrap('jsUpdateUiComponents', ...)
//
// ⚠️⚠️⚠️ REMEMBER: Changes here affect the JavaScript runtime! ⚠️⚠️⚠️

// DO NOT clang-format this file!! It will destroy the EM_ASM_ macros.
// clang-format off


#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>

#include "js_bindings.h"

#include "active_strip_data.h"
#include "fl/dbg.h"
#include "fl/math.h"
#include "fl/screenmap.h"
#include "fl/json.h"

namespace fl {

static void jsSetCanvasSizeJson(const char* jsonString, size_t jsonSize) {
    // FASTLED_DBG("jsSetCanvasSize1");
    EM_ASM_({
        globalThis.FastLED_onStripUpdate = globalThis.FastLED_onStripUpdate || function(jsonStr) {
            console.log("Missing globalThis.FastLED_onStripUpdate(jsonStr) function");
        };
        var jsonStr = UTF8ToString($0, $1);  // Convert C string to JavaScript string with length
        var jsonData = JSON.parse(jsonStr);
        globalThis.FastLED_onStripUpdate(jsonData);
    }, jsonString, jsonSize);
}

static void _jsSetCanvasSize(int cledcontoller_id, const fl::ScreenMap &screenmap) {
    // FASTLED_DBG("Begin jsSetCanvasSize json serialization");
    FLArduinoJson::JsonDocument doc;
    doc["strip_id"] = cledcontoller_id;
    doc["event"] = "set_canvas_map";
    auto map = doc["map"].to<FLArduinoJson::JsonObject>();
    doc["length"] = screenmap.getLength();
    auto x = map["x"].to<FLArduinoJson::JsonArray>();
    auto y = map["y"].to<FLArduinoJson::JsonArray>();
    for (uint32_t i = 0; i < screenmap.getLength(); i++) {
        x.add(screenmap[i].x);
        y.add(screenmap[i].y);
    }
    // add diameter.
    float diameter = screenmap.getDiameter();
    if (diameter > 0.0f) {
        doc["diameter"] = diameter;
    }
    // FASTLED_DBG("Finished json dict building.");
    Str jsonBuffer;
    serializeJson(doc, jsonBuffer);
    // FASTLED_DBG("End jsSetCanvasSize json serialization");
    jsSetCanvasSizeJson(jsonBuffer.c_str(), jsonBuffer.size());
}


void jsSetCanvasSize(int cledcontoller_id, const fl::ScreenMap &screenmap) {
    _jsSetCanvasSize(cledcontoller_id, screenmap);
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
    for (const auto &[stripIndex, stripData] : info) {
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



EMSCRIPTEN_KEEPALIVE void jsOnFrame(ActiveStripData& active_strips) {
    jsFillInMissingScreenMaps(active_strips);
    Str json_str = active_strips.infoJsonString();
    EM_ASM_({

        globalThis.FastLED_sendMessage = globalThis.FastLED_sendMessage || function(msg_tag, json_data_str) {
            console.log("Missing globalThis.FastLED_sendMessage() function");
            console.log("Message was mean for tag: " + msg_tag);
            const json_data = JSON.parse(json_data_str);
            console.log("Received JSON data:", json_data);
        };

        globalThis.FastLED_onFrame = globalThis.FastLED_onFrame || function(frameInfo, callback) {
                console.log("Missing globalThis.FastLED_onFrame() function");
            //console.log("Received frame data:", frameData);
            if (typeof callback === 'function') {
                    callback();
                } else {
                console.error("Callback function is not a function but is of type " + typeof callback);
                }
            };
        globalThis.onFastLedUiUpdateFunction = globalThis.onFastLedUiUpdateFunction || function(jsonString) {
            // console.log("*** JS→C++: onFastLedUiUpdateFunction called with:", jsonString);
            if (typeof jsonString === 'string' && jsonString !== null) {
                    // console.log("*** JS→C++: Calling C++ jsUpdateUiComponents");
                    Module.cwrap('jsUpdateUiComponents', null, ['string'])(jsonString);
                    // console.log("*** JS→C++: C++ jsUpdateUiComponents call completed");
                } else {
                    console.error("*** JS→C++: Invalid jsonData received:", jsonString, "expected string but instead got:", typeof jsonString);
                }
            };

       // ActiveStripData is now accessed via ccall mechanism only
            var jsonStr = UTF8ToString($0);
            var jsonData = JSON.parse(jsonStr);
            for (var i = 0; i < jsonData.length; i++) {
                var stripData = jsonData[i];
                // Use ccall mechanism to get pixel data
                var sizePtr = Module._malloc(4);
                var dataPtr = Module.ccall('getStripPixelData', 'number', ['number', 'number'], [stripData.strip_id, sizePtr]);
                if (dataPtr !== 0) {
                    var size = Module.getValue(sizePtr, 'i32');
                    var pixelData = new Uint8Array(Module.HEAPU8.buffer, dataPtr, size);
                    jsonData[i].pixel_data = pixelData;
                } else {
                    jsonData[i].pixel_data = null;
                }
                Module._free(sizePtr);
            }

        globalThis.FastLED_onFrame(jsonData, globalThis.onFastLedUiUpdateFunction);
    }, json_str.c_str());
}

EMSCRIPTEN_KEEPALIVE void jsOnStripAdded(uintptr_t strip, uint32_t num_leds) {
    EM_ASM_({
        globalThis.FastLED_onStripAdded = globalThis.FastLED_onStripAdded || function() {
            console.log("Missing globalThis.FastLED_onStripAdded(id, length) function");
            console.log("Added strip id: " + arguments[0] + " with length: " + arguments[1]);
            };
            globalThis.FastLED_onStripAdded($0, $1);
    }, strip, num_leds);
}

EMSCRIPTEN_KEEPALIVE void updateJs(const char* jsonStr) {
    // printf("updateJs: ENTRY - jsonStr=%s\n", jsonStr ? jsonStr : "NULL");
    // FASTLED_DBG("updateJs: ENTRY - jsonStr=" << (jsonStr ? jsonStr : "NULL"));
    
    EM_ASM_({
            globalThis.FastLED_onUiElementsAdded = globalThis.FastLED_onUiElementsAdded || function(jsonData, updateFunc) {
                //console.log(new Date().toLocaleTimeString());
                console.log("Missing globalThis.FastLED_onUiElementsAdded(jsonData, updateFunc) function");
                //console.log("Added ui elements:", jsonData);
            };
            var jsonStr = UTF8ToString($0);
            var data = null;
            try {
                data = JSON.parse(jsonStr);
            } catch (error) {
                console.error("Error parsing JSON:", error);
                console.error("Problematic JSON string:", jsonStr);
                return;
            }
            if (data) {
                globalThis.FastLED_onUiElementsAdded(data);
            } else {
                console.error("Internal error, data is null");
            }

    }, jsonStr);
    
    //printf("updateJs: EXIT\n");
    //FASTLED_DBG("updateJs: EXIT");
}



} // namespace fl

#endif // __EMSCRIPTEN__
