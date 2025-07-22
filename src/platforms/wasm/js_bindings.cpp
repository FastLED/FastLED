#ifdef __EMSCRIPTEN__

// ⚠️⚠️⚠️ CRITICAL WARNING: C++ ↔ JavaScript ASYNC BRIDGE - HANDLE WITH EXTREME CARE! ⚠️⚠️⚠️
//
// 🚨 THIS FILE CONTAINS C++ TO JAVASCRIPT ASYNC BINDINGS 🚨
//
// DO NOT MODIFY FUNCTION SIGNATURES WITHOUT UPDATING CORRESPONDING JAVASCRIPT CODE!
// 
// This file is an ASYNC BRIDGE between C++ and JavaScript using Emscripten's Asyncify.
// Any changes to:
// - Function names and async signatures
// - Parameter types and return types  
// - Function signatures with Promise returns
// - Async JavaScript callback patterns
//
// Will BREAK the JavaScript side and cause SILENT RUNTIME FAILURES!
//
// Key async integration points in this file:
// - Async globalThis.FastLED_onStripUpdate()
// - Async globalThis.FastLED_onFrame() 
// - Async globalThis.FastLED_onStripAdded()
// - Async globalThis.FastLED_onUiElementsAdded()
// - Async Module.cwrap('jsUpdateUiComponents', ...)
//
// All JavaScript functions called from C++ can now return Promises and will be properly awaited.
//
// ⚠️⚠️⚠️ REMEMBER: These are now ASYNC functions - test with Asyncify enabled! ⚠️⚠️⚠️

// DO NOT clang-format this file!! It will destroy the async JavaScript blocks.
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

// Forward declarations for async functions
// Note: emscripten_sleep is declared in emscripten.h as void emscripten_sleep(unsigned int ms)

/**
 * Async-aware canvas size setter using JavaScript callbacks
 * Now supports Promise returns from JavaScript functions
 */
static void jsSetCanvasSizeJsonAsync(const char* jsonString, size_t jsonSize) {
    // FASTLED_DBG("jsSetCanvasSize1 - Async version");
    EM_ASM_({
        // Define async-aware callback functions
        globalThis.FastLED_onStripUpdate = globalThis.FastLED_onStripUpdate || async function(jsonData) {
            console.log("Missing async globalThis.FastLED_onStripUpdate(jsonData) function");
            // Return a resolved promise for compatibility
            return Promise.resolve();
        };
        
        // Async execution block
        (async function() {
            try {
                var jsonStr = UTF8ToString($0, $1);  // Convert C string to JavaScript string with length
                var jsonData = JSON.parse(jsonStr);
                
                // Call the async function and await the result
                const result = globalThis.FastLED_onStripUpdate(jsonData);
                
                // Handle both sync and async returns
                if (result instanceof Promise) {
                    await result;
                } else {
                    // Sync return, no need to await
                }
                
            } catch (error) {
                console.error("Error in async jsSetCanvasSizeJson:", error);
            }
        })();
        
    }, jsonString, jsonSize);
}

static void _jsSetCanvasSize(int cledcontoller_id, const fl::ScreenMap &screenmap) {
    // FASTLED_DBG("Begin jsSetCanvasSize json serialization - Async version");
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
    // FASTLED_DBG("End jsSetCanvasSize json serialization - Async version");
    jsSetCanvasSizeJsonAsync(jsonBuffer.c_str(), jsonBuffer.size());
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

/**
 * Async-aware frame processing function
 * Now supports async JavaScript callbacks that can return Promises
 */
EMSCRIPTEN_KEEPALIVE void jsOnFrame(ActiveStripData& active_strips) {
    jsFillInMissingScreenMaps(active_strips);
    Str json_str = active_strips.infoJsonString();
    EM_ASM_({

        // Define async-aware callback functions with proper error handling
        globalThis.FastLED_sendMessage = globalThis.FastLED_sendMessage || async function(msg_tag, json_data_str) {
            console.log("Missing async globalThis.FastLED_sendMessage() function");
            console.log("Message was mean for tag: " + msg_tag);
            const json_data = JSON.parse(json_data_str);
            console.log("Received JSON data:", json_data);
            return Promise.resolve(); // Always return a Promise
        };

        globalThis.FastLED_onFrame = globalThis.FastLED_onFrame || async function(frameInfo, callback) {
            console.log("Missing async globalThis.FastLED_onFrame() function");
            //console.log("Received frame data:", frameData);
            if (typeof callback === 'function') {
                // Callback might be async too
                const result = callback();
                if (result instanceof Promise) {
                    await result;
                }
            } else {
                console.error("Callback function is not a function but is of type " + typeof callback);
            }
            return Promise.resolve(); // Always return a Promise
        };
        
        globalThis.onFastLedUiUpdateFunction = globalThis.onFastLedUiUpdateFunction || async function(jsonString) {
            // console.log("*** JS→C++: async onFastLedUiUpdateFunction called with:", jsonString);
            if (typeof jsonString === 'string' && jsonString !== null) {
                try {
                    // console.log("*** JS→C++: Calling async C++ jsUpdateUiComponents");
                    
                    // Call the C++ function - it might be async now
                    const result = Module.cwrap('jsUpdateUiComponents', null, ['string'])(jsonString);
                    
                    // Handle potential async return
                    if (result instanceof Promise) {
                        await result;
                    }
                    
                    // console.log("*** JS→C++: Async C++ jsUpdateUiComponents call completed");
                } catch (error) {
                    console.error("*** JS→C++: Error in async jsUpdateUiComponents:", error);
                }
            } else {
                console.error("*** JS→C++: Invalid jsonData received:", jsonString, "expected string but instead got:", typeof jsonString);
            }
            return Promise.resolve(); // Always return a Promise
        };

        // Async execution block for frame processing
        (async function() {
            try {
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

                // Call the async frame handler and await the result
                const frameResult = globalThis.FastLED_onFrame(jsonData, globalThis.onFastLedUiUpdateFunction);
                
                // Handle both sync and async returns
                if (frameResult instanceof Promise) {
                    await frameResult;
                }
                
            } catch (error) {
                console.error("Error in async jsOnFrame:", error);
            }
        })();
        
    }, json_str.c_str());
}

/**
 * Async-aware strip addition notification
 * Now supports async JavaScript callbacks
 */
EMSCRIPTEN_KEEPALIVE void jsOnStripAdded(uintptr_t strip, uint32_t num_leds) {
    EM_ASM_({
        // Define async-aware callback function
        globalThis.FastLED_onStripAdded = globalThis.FastLED_onStripAdded || async function(stripId, numLeds) {
            console.log("Missing async globalThis.FastLED_onStripAdded(id, length) function");
            console.log("Added strip id: " + stripId + " with length: " + numLeds);
            return Promise.resolve(); // Always return a Promise
        };
        
        // Async execution block
        (async function() {
            try {
                // Call the async function and await the result
                const result = globalThis.FastLED_onStripAdded($0, $1);
                
                // Handle both sync and async returns
                if (result instanceof Promise) {
                    await result;
                }
                
            } catch (error) {
                console.error("Error in async jsOnStripAdded:", error);
            }
        })();
        
    }, strip, num_leds);
}

/**
 * Async-aware UI update function
 * Now supports async JavaScript callbacks that can return Promises
 */
EMSCRIPTEN_KEEPALIVE void updateJs(const char* jsonStr) {
    // printf("updateJs: ENTRY - ASYNC VERSION - jsonStr=%s\n", jsonStr ? jsonStr : "NULL");
    // FASTLED_DBG("updateJs: ENTRY - ASYNC VERSION - jsonStr=" << (jsonStr ? jsonStr : "NULL"));
    
    EM_ASM_({
        // Define async-aware callback function
        globalThis.FastLED_onUiElementsAdded = globalThis.FastLED_onUiElementsAdded || async function(jsonData, updateFunc) {
            //console.log(new Date().toLocaleTimeString());
            console.log("Missing async globalThis.FastLED_onUiElementsAdded(jsonData, updateFunc) function");
            //console.log("Added ui elements:", jsonData);
            return Promise.resolve(); // Always return a Promise
        };
        
        // Async execution block
        (async function() {
            try {
                var jsonStr = UTF8ToString($0);
                var data = null;
                try {
                    data = JSON.parse(jsonStr);
                } catch (parseError) {
                    console.error("Error parsing JSON:", parseError);
                    console.error("Problematic JSON string:", jsonStr);
                    return;
                }
                
                if (data) {
                    // Call the async function and await the result
                    const result = globalThis.FastLED_onUiElementsAdded(data);
                    
                    // Handle both sync and async returns
                    if (result instanceof Promise) {
                        await result;
                    }
                } else {
                    console.error("Internal error, data is null");
                }
                
            } catch (error) {
                console.error("Error in async updateJs:", error);
            }
        })();
        
    }, jsonStr);
    
    //printf("updateJs: EXIT - ASYNC VERSION\n");
    //FASTLED_DBG("updateJs: EXIT - ASYNC VERSION");
}

} // namespace fl

#endif // __EMSCRIPTEN__
