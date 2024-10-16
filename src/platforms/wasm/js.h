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
void jsSetCanvasSize(int width, int height);

class jsSlider {
  public:
    jsSlider(const char *name, float min = 0.0f, float max = 255.0f,
             float value = 128.0f, float step = 1.0f);
    ~jsSlider();

    const char *name() const;
    std::string toJsonStr() const;
    float value() const;
    void setValue(float value);
    operator float() const;
    operator uint8_t() const;
    operator uint16_t() const;
    operator int() const;

    template <typename T> T as() const { return static_cast<T>(mValue); }

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
    std::string toJsonStr() const;
    bool value() const;
    void setValue(bool value);
    operator bool() const;
    operator int() const;

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
    std::string toJsonStr() const;
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

FASTLED_NAMESPACE_END