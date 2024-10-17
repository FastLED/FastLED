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

FASTLED_NAMESPACE_BEGIN

// Needed or the wasm compiler will strip them out.
// Provide missing functions for WebAssembly build.
extern "C" {

// Replacement for 'millis' in WebAssembly context
EMSCRIPTEN_KEEPALIVE uint32_t millis();

// Replacement for 'micros' in WebAssembly context
EMSCRIPTEN_KEEPALIVE uint32_t micros();

// Replacement for 'delay' in WebAssembly context
EMSCRIPTEN_KEEPALIVE void delay(int ms);
}

// Sets the canvas size. This assumes one strip per row. This is
// method is pretty inflexible and is likely to change in the future.
void jsSetCanvasSize(int cledcontroler_id, int width, int height);

class jsSlider {
  public:
    jsSlider(const char *name, float value = 128.0f, float min = 0.0f, float max = 255.0f,
             float step = 1.0f);
    ~jsSlider();

    const char *name() const;
    void toJson(ArduinoJson::JsonObject& json) const;
    float value() const;
    void setValue(float value);
    operator float() const;
    operator uint8_t() const;
    operator uint16_t() const;
    operator int() const;

    template <typename T> T as() const { return static_cast<T>(mValue); }

    jsSlider& operator=(float value) { setValue(value); return *this; }
    jsSlider& operator=(int value) { setValue(static_cast<float>(value)); return *this; }

  private:
    void updateInternal(const char *jsonStr);

    std::shared_ptr<jsUiInternal> mInternal;
    float mMin;
    float mMax;
    float mValue;
    float mStep;
};

class jsCheckbox {
  public:
    jsCheckbox(const char *name, bool value);
    ~jsCheckbox();

    const char *name() const;
    void toJson(ArduinoJson::JsonObject& json) const;
    bool value() const;
    void setValue(bool value);
    operator bool() const;
    operator int() const;

    jsCheckbox& operator=(bool value) { setValue(value); return *this; }
    jsCheckbox& operator=(int value) { setValue(value != 0); return *this; }

  private:
    void updateInternal(const char *jsonStr);

    std::shared_ptr<jsUiInternal> mInternal;
    bool mValue;
};

class jsButton {
  public:
    jsButton(const char *name);
    ~jsButton();

    const char *name() const;
    void toJson(ArduinoJson::JsonObject& json) const;
    bool isPressed() const;
    bool clicked() const {
        bool clickedHappened = mPressed && (mPressed != mPressedLast);
        return mClickedHappened;
    }
    operator bool() const { return clicked(); }

  private:
    struct Updater : EngineEvents::Listener {
        void init(jsButton *owner) {
            mOwner = owner;
            EngineEvents::addListener(this);
        }
        ~Updater() { EngineEvents::removeListener(this); }
        void onPlatformPreLoop2() override {
            mOwner->mClickedHappened = mOwner->mPressed && (mOwner->mPressed != mOwner->mPressedLast);
            mOwner->mPressedLast = mOwner->mPressed;
        }
        jsButton *mOwner = nullptr;
    };

    Updater mUpdater;

    void updateInternal(const char *jsonStr);

    std::shared_ptr<jsUiInternal> mInternal;
    bool mPressed = false;
    bool mPressedLast = false;
    bool mClickedHappened = false;
};


inline void jsSetCanvasSize(int cledcontoller_id, const XYMap& xymap) {
    int width = xymap.getWidth();
    int height = xymap.getHeight();
    ArduinoJson::JsonDocument doc;
    doc["strip_id"] = cledcontoller_id;
    doc["event"] = "set_canvas_size";
    doc["width"] = width;
    doc["height"] = height;
    std::string jsonStr;
    serializeJson(doc, jsonStr);
    EM_ASM_({
        globalThis.FastLED_onStripUpdate = globalThis.FastLED_onStripUpdate || function(jsonStr) {
            console.log("Missing globalThis.FastLED_onStripUpdate(jsonStr) function");
        };
        var jsonStr = UTF8ToString($0);  // Convert C string to JavaScript string
        var jsonData = JSON.parse(jsonStr);
        globalThis.FastLED_onStripUpdate(jsonData);
    }, jsonStr.c_str());
}

inline void jsOnFrame() {
    EM_ASM_({
        globalThis.FastLED_onFrame = globalThis.FastLED_onFrame || function(frameData, callback) {
            console.log("Missing globalThis.FastLED_onFrame() function");
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
        globalThis.FastLED_onFrame(globalThis.FastLED_onFrameData, globalThis.onFastLedUiUpdateFunction);
    });
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

typedef jsSlider Slider;
typedef jsCheckbox Checkbox;
typedef jsButton Button;


FASTLED_NAMESPACE_END
