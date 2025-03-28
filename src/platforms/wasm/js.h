#pragma once


#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers

#include <memory>
#include <stdint.h>
#include <stdio.h>
#include <string>



#include "fl/str.h"
#include "ui/ui_internal.h"
#include "fl/str.h"
#include "fl/engine_events.h"
#include "fl/namespace.h"
#include "fl/screenmap.h"
#include "fl/ptr.h"
#include "active_strip_data.h"



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



//////////////////////////////////////////////////////////////////////////
// BEGIN EMSCRIPTEN EXPORTS
EMSCRIPTEN_KEEPALIVE extern "C" int extern_setup();
EMSCRIPTEN_KEEPALIVE extern "C" int extern_loop();


namespace fl {


class jsSlider {
  public:
    jsSlider(const fl::Str& name, float value = 128.0f, float min = 0.0f, float max = 255.0f,
             float step = -1.f);
    ~jsSlider();
    jsSlider& Group(const fl::Str& name) { mGroup = name; return *this; }

    const fl::Str& name() const;
    void toJson(FLArduinoJson::JsonObject& json) const;
    float value() const;
    void setValue(float value);
    operator float() const;
    operator uint8_t() const;
    operator uint16_t() const;
    operator uint32_t() const { return static_cast<uint32_t>(mValue); }
    operator int() const;
    const fl::Str& groupName() const { return mGroup; }

    template <typename T> bool operator>= (T v) const { return mValue >= static_cast<float>(v); }
    template <typename T> bool operator<= (T v) const { return mValue <= static_cast<float>(v); }
    template <typename T> bool operator> (T v) const { return mValue > static_cast<float>(v); }
    template <typename T> bool operator< (T v) const { return mValue < static_cast<float>(v); }

    template <typename T> T as() const { return static_cast<T>(mValue); }

    jsSlider& operator=(float value) { setValue(value); return *this; }
    jsSlider& operator=(int value) { setValue(static_cast<float>(value)); return *this; }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst& value);

    jsUiInternalPtr mInternal;
    float mMin;
    float mMax;
    float mValue;
    float mStep;
    fl::Str mGroup;
};


class jsNumberField {
  public:
    jsNumberField(const fl::Str&, double value, double min = 0, double max = 100);
    ~jsNumberField();
    jsNumberField& Group(const fl::Str& name) { mGroup = name; return *this; }

    const fl::Str& name() const;
    void toJson(FLArduinoJson::JsonObject& json) const;
    double value() const;
    void setValue(double value);
    operator double() const;
    operator int() const;
    const fl::Str& groupName() const { return mGroup; }

    jsNumberField& operator=(double value) { setValue(value); return *this; }
    jsNumberField& operator=(int value) { setValue(static_cast<double>(value)); return *this; }
    bool operator==(double v) const { return value() == v; }
    bool operator==(int v) const { return value() == v; }
    bool operator!=(double v) const { return value() != v; }
    bool operator!=(int v) const { return value() != v; }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst& value);

    jsUiInternalPtr mInternal;
    double mValue;
    double mMin;
    double mMax;
    fl::Str mGroup;
};


class jsCheckbox {
  public:
    jsCheckbox(const fl::Str&, bool value);
    ~jsCheckbox();
    jsCheckbox& Group(const fl::Str& name) { mGroup = name; return *this; };

    const fl::Str& name() const;
    void toJson(FLArduinoJson::JsonObject& json) const;
    bool value() const;
    void setValue(bool value);
    operator bool() const;
    operator int() const;
    const fl::Str& groupName() const { return mGroup; }

    jsCheckbox& operator=(bool value) { setValue(value); return *this; }
    jsCheckbox& operator=(int value) { setValue(value != 0); return *this; }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst& value);

    jsUiInternalPtr mInternal;
    bool mValue;
    fl::Str mGroup;
};

class jsButton {
  public:
    jsButton(const fl::Str& name);
    ~jsButton();
    jsButton& Group(const fl::Str& name) { mGroup = name; return *this; }

    const fl::Str& name() const;
    void toJson(FLArduinoJson::JsonObject& json) const;
    bool isPressed() const;
    bool clicked() const {
        bool clickedHappened = mPressed && (mPressed != mPressedLast);
        return mClickedHappened;
    }
    int clickedCount() const { return mClickedCount; }
    operator bool() const { return clicked(); }
    const fl::Str& groupName() const { return mGroup; }

  private:
    struct Updater : fl::EngineEvents::Listener {
        void init(jsButton *owner) {
            mOwner = owner;
            fl::EngineEvents::addListener(this);
        }
        ~Updater() { fl::EngineEvents::removeListener(this); }
        void onPlatformPreLoop2() override {
            mOwner->mClickedHappened = mOwner->mPressed && (mOwner->mPressed != mOwner->mPressedLast);
            mOwner->mPressedLast = mOwner->mPressed;
            if (mOwner->mClickedHappened) {
                mOwner->mClickedCount++;
            }
        }
        jsButton *mOwner = nullptr;
    };

    Updater mUpdater;

    void updateInternal(const FLArduinoJson::JsonVariantConst& value);

    jsUiInternalPtr mInternal;
    bool mPressed = false;
    bool mPressedLast = false;
    bool mClickedHappened = false;
    int mClickedCount = 0;
    fl::Str mGroup;
};


class jsTitle {
  public:
    jsTitle(const fl::Str& text);
    ~jsTitle();
    jsTitle& Group(const fl::Str& name) { mGroup = name; return *this; }

    void toJson(FLArduinoJson::JsonObject& json) const;
    const fl::Str& groupName() const { return mGroup; }
    const fl::Str& text() const { return mText; }

  private:
    jsUiInternalPtr mInternal;
    fl::Str mGroup;
    fl::Str mText;
};

class jsDescription {
  public:
    jsDescription(const fl::Str& text);
    ~jsDescription();
    jsDescription& Group(const fl::Str& name) { mGroup = name; return *this; }


    void toJson(FLArduinoJson::JsonObject& json) const;
    const fl::Str& groupName() const { return mGroup; }
    const fl::Str& text() const { return mText; }

  private:
    jsUiInternalPtr mInternal;
    fl::Str mGroup;
    fl::Str mText;
};


void jsSetCanvasSize(int cledcontoller_id, const fl::ScreenMap& screenmap);
void jsOnFrame(ActiveStripData& active_strips);
void jsOnStripAdded(uintptr_t strip, uint32_t num_leds);
void updateJs(const char* jsonStr);



#define FASTLED_HAS_UI_BUTTON 1
#define FASTLED_HAS_UI_SLIDER 1
#define FASTLED_HAS_UI_CHECKBOX 1
#define FASTLED_HAS_UI_NUMBER_FIELD 1
#define FASTLED_HAS_UI_TITLE 1
#define FASTLED_HAS_UI_DESCRIPTION 1

typedef jsNumberField UINumberField;
typedef jsSlider UISlider;
typedef jsCheckbox UICheckbox;
typedef jsButton UIButton;
typedef jsTitle UITitle;
typedef jsDescription UIDescription;


}  // namespace fl
