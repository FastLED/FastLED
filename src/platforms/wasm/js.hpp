#pragma once

#include "ui/ui_internal.h"
#include <memory>
#include <stdint.h>
#include <stdio.h>
#include <string>

#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers

#include "engine_events.h"
#include "namespace.h"
#include "screenmap.h"
#include "active_strip_data.h"
#include "js.h"

FASTLED_NAMESPACE_BEGIN


inline void jsSetCanvasSize(const char* jsonString, size_t jsonSize) {
    EM_ASM_({
        globalThis.FastLED_onStripUpdate = globalThis.FastLED_onStripUpdate || function(jsonStr) {
            console.log("Missing globalThis.FastLED_onStripUpdate(jsonStr) function");
        };
        var jsonStr = UTF8ToString($0, $1);  // Convert C string to JavaScript string with length
        var jsonData = JSON.parse(jsonStr);
        globalThis.FastLED_onStripUpdate(jsonData);
    }, jsonString, jsonSize);
}


inline void jsSetCanvasSize(int cledcontoller_id, uint16_t width, uint16_t height) {
    ArduinoJson::JsonDocument doc;
    doc["strip_id"] = cledcontoller_id;
    doc["event"] = "set_canvas_size";
    doc["width"] = width;
    doc["height"] = height;
    char jsonBuffer[512];
    size_t jsonSize = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    jsSetCanvasSize(jsonBuffer, jsonSize);
}

inline void jsSetCanvasSize(int cledcontoller_id, const ScreenMap& screenmap) {
    ArduinoJson::JsonDocument doc;
    doc["strip_id"] = cledcontoller_id;
    doc["event"] = "set_canvas_map";
    // auto& array = doc.createNestedArray("map");
    auto array = doc["map"].to<ArduinoJson::JsonArray>();
    for (uint32_t i = 0; i < screenmap.getLength(); i++) {
        auto entry = array[i].to<ArduinoJson::JsonArray>();
        entry.add(screenmap[i].x);
        entry.add(screenmap[i].y);
    }
    std::string jsonBuffer;
    serializeJson(doc, jsonBuffer);
    jsSetCanvasSize(jsonBuffer.c_str(), jsonBuffer.size());
}

inline void jsOnFrame(ActiveStripData& active_strips) {
    const auto& info = active_strips.getData();
    std::string json_str = active_strips.infoJsonString();

    EM_ASM_({
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
            if (typeof jsonString === 'string' && jsonString !== null) {
                Module._jsUiManager_updateUiComponents(jsonString);
            } else {
                console.error("Invalid jsonData received:", jsonString, "expected string but instead got:", typeof jsonString);
            }
        };

       globalThis.FastLED_onFrameData = globalThis.FastLED_onFrameData || new Module.ActiveStripData();
       var activeStrips = globalThis.FastLED_onFrameData;

        var jsonStr = UTF8ToString($0);
        var jsonData = JSON.parse(jsonStr);
        for (var i = 0; i < jsonData.length; i++) {
            var stripData = jsonData[i];
            var pixelData = activeStrips.getPixelData_Uint8(stripData.strip_id);
            jsonData[i].pixel_data = pixelData;
        }
 
        globalThis.FastLED_onFrame(jsonData, globalThis.onFastLedUiUpdateFunction);
    }, json_str.c_str());
}

inline void jsOnStripAdded(uintptr_t strip, uint32_t num_leds) {
    EM_ASM_({
        globalThis.FastLED_onStripAdded = globalThis.FastLED_onStripAdded || function() {
            console.log("Missing globalThis.FastLED_onStripAdded(id, length) function");
            console.log("Added strip id: " + arguments[0] + " with length: " + arguments[1]);
        };
        globalThis.FastLED_onStripAdded($0, $1);
    }, strip, num_leds);
}

inline void updateJs(const char* jsonStr) {
    printf("updateJs: %s\n", jsonStr);
    EM_ASM_({
        globalThis.FastLED_onUiElementsAdded = globalThis.FastLED_onUiElementsAdded || function(jsonData, updateFunc) {
            console.log(new Date().toLocaleTimeString());
            console.log("Missing globalThis.FastLED_onUiElementsAdded(jsonData, updateFunc) function");
            console.log("Added ui elements:", jsonData);
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
}

#define FASTLED_HAS_UI_BUTTON 1
#define FASTLED_HAS_UI_SLIDER 1
#define FASTLED_HAS_UI_CHECKBOX 1
#define FASTLED_HAS_UI_NUMBER_FIELD 1

typedef jsNumberField NumberField;
typedef jsSlider Slider;
typedef jsCheckbox Checkbox;
typedef jsButton Button;


FASTLED_NAMESPACE_END
