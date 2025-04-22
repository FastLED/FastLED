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


class jsSliderImpl {
  public:
    jsSliderImpl(const fl::Str& name, float value = 128.0f, float min = 0.0f, float max = 255.0f,
             float step = -1.f);
    ~jsSliderImpl();
    jsSliderImpl& Group(const fl::Str& name) { mGroup = name; return *this; }

    const fl::Str& name() const;
    void toJson(FLArduinoJson::JsonObject& json) const;
    float value() const;
    float value_normalized() const;
    float max_value() const { return mMax; }
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

    int as_int() const { return static_cast<int>(mValue); }

    jsSliderImpl& operator=(float value) { setValue(value); return *this; }
    jsSliderImpl& operator=(int value) { setValue(static_cast<float>(value)); return *this; }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst& value);

    jsUiInternalPtr mInternal;
    float mMin;
    float mMax;
    float mValue;
    float mStep;
    fl::Str mGroup;
};


class jsNumberFieldImpl {
  public:
    jsNumberFieldImpl(const fl::Str&, double value, double min = 0, double max = 100);
    ~jsNumberFieldImpl();
    jsNumberFieldImpl& Group(const fl::Str& name) { mGroup = name; return *this; }

    const fl::Str& name() const;
    void toJson(FLArduinoJson::JsonObject& json) const;
    double value() const;
    void setValue(double value);
    operator double() const;
    operator int() const;
    const fl::Str& groupName() const { return mGroup; }

    jsNumberFieldImpl& operator=(double value) { setValue(value); return *this; }
    jsNumberFieldImpl& operator=(int value) { setValue(static_cast<double>(value)); return *this; }
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


class jsCheckboxImpl {
  public:
    jsCheckboxImpl(const fl::Str&, bool value);
    ~jsCheckboxImpl();
    jsCheckboxImpl& Group(const fl::Str& name) { mGroup = name; return *this; };

    const fl::Str& name() const;
    void toJson(FLArduinoJson::JsonObject& json) const;
    bool value() const;
    void setValue(bool value);
    operator bool() const;
    operator int() const;
    const fl::Str& groupName() const { return mGroup; }

    jsCheckboxImpl& operator=(bool value) { setValue(value); return *this; }
    jsCheckboxImpl& operator=(int value) { setValue(value != 0); return *this; }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst& value);

    jsUiInternalPtr mInternal;
    bool mValue;
    fl::Str mGroup;
};

class jsButtonImpl {
  public:
    jsButtonImpl(const fl::Str& name);
    ~jsButtonImpl();
    jsButtonImpl& Group(const fl::Str& name) { mGroup = name; return *this; }

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
        void init(jsButtonImpl *owner) {
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
        jsButtonImpl *mOwner = nullptr;
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


class jsTitleImpl {
  public:
    jsTitleImpl(const fl::Str& text);
    ~jsTitleImpl();
    jsTitleImpl& Group(const fl::Str& name) { mGroup = name; return *this; }

    void toJson(FLArduinoJson::JsonObject& json) const;
    const fl::Str& groupName() const { return mGroup; }
    const fl::Str& text() const { return mText; }

  private:
    jsUiInternalPtr mInternal;
    fl::Str mGroup;
    fl::Str mText;
};

class jsDescriptionImpl {
  public:
    jsDescriptionImpl(const fl::Str& text);
    ~jsDescriptionImpl();
    jsDescriptionImpl& Group(const fl::Str& name) { mGroup = name; return *this; }


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

typedef jsNumberFieldImpl UINumberFieldImpl;
typedef jsSliderImpl UISliderImpl;
typedef jsCheckboxImpl UICheckboxImpl;
typedef jsButtonImpl UIButtonImpl;
typedef jsTitleImpl UITitleImpl;
typedef jsDescriptionImpl UIDescriptionImpl;


}  // namespace fl
