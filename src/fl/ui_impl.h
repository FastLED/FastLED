#pragma once

#include <stdint.h>

#include "fl/audio.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "fl/str.h"
#include "fl/template_magic.h"
#include "fl/unused.h"
#include "fl/warn.h"
#include "platforms/ui_defs.h"

#ifndef FASTLED_HAS_UI_SLIDER
#define FASTLED_HAS_UI_SLIDER 0
#endif

#ifndef FASTLED_HAS_UI_BUTTON
#define FASTLED_HAS_UI_BUTTON 0
#endif

#ifndef FASTLED_HAS_UI_CHECKBOX
#define FASTLED_HAS_UI_CHECKBOX 0
#endif

#ifndef FASTLED_HAS_UI_NUMBER_FIELD
#define FASTLED_HAS_UI_NUMBER_FIELD 0
#endif

#ifndef FASTLED_HAS_UI_TITLE
#define FASTLED_HAS_UI_TITLE 0
#endif

#ifndef FASTLED_HAS_UI_DESCRIPTION
#define FASTLED_HAS_UI_DESCRIPTION 0
#endif

#ifndef FASTLED_HAS_UI_AUDIO
#define FASTLED_HAS_UI_AUDIO 0
#endif

namespace fl {

// If the platform is missing ui components, provide stubs.

#if !FASTLED_HAS_UI_SLIDER

class UISliderImpl {
  public:
    // If step is -1, it will be calculated as (max - min) / 100
    UISliderImpl(const char *name, float value = 128.0f, float min = 1,
                 float max = 255, float step = -1.f)
        : mValue(value), mMin(MIN(min, max)), mMax(MAX(min, max)) {
        FASTLED_UNUSED(name);
        FASTLED_UNUSED(step);
        if (value < min) {
            mValue = min;
        }
        if (value > max) {
            mValue = max;
        }
    }
    ~UISliderImpl() {}
    float value() const { return mValue; }
    float getMax() const { return mMax; }
    float getMin() const { return mMin; }
    void setValue(float value) { mValue = MAX(mMin, MIN(mMax, value)); }
    operator float() const { return mValue; }
    operator uint8_t() const { return static_cast<uint8_t>(mValue); }
    operator uint16_t() const { return static_cast<uint16_t>(mValue); }
    operator int() const { return static_cast<int>(mValue); }
    template <typename T> T as() const { return static_cast<T>(mValue); }

    int as_int() const { return static_cast<int>(mValue); }

    UISliderImpl &operator=(float value) {
        setValue(value);
        return *this;
    }
    UISliderImpl &operator=(int value) {
        setValue(static_cast<float>(value));
        return *this;
    }

  private:
    float mValue;
    float mMin;
    float mMax;
};

// template operator for >= against a jsSliderImpl

#endif

#if !FASTLED_HAS_UI_BUTTON

class UIButtonImpl {
  public:
    UIButtonImpl(const char *name) { FASTLED_UNUSED(name); }
    ~UIButtonImpl() {}
    bool isPressed() const { return false; }
    bool clicked() const { return false; }
    int clickedCount() const { return 0; }
    operator bool() const { return false; }
    void click() {}
    fl::Str name() const { return mName; }

  private:
    fl::Str mName;
};

#endif

#if !FASTLED_HAS_UI_CHECKBOX

class UICheckboxImpl {
  public:
    UICheckboxImpl(const char *name, bool value = false) : mValue(value) {
        FASTLED_UNUSED(name);
    }
    ~UICheckboxImpl() {}
    operator bool() const { return mValue; }
    explicit operator int() const { return mValue ? 1 : 0; }
    UICheckboxImpl &operator=(bool value) {
        setValue(value);
        return *this;
    }
    UICheckboxImpl &operator=(int value) {
        setValue(value != 0);
        return *this;
    }
    bool value() const { return mValue; }

  private:
    void setValue(bool value) { mValue = value; }
    bool mValue;
};

#endif

#if !FASTLED_HAS_UI_NUMBER_FIELD

class UINumberFieldImpl {
  public:
    UINumberFieldImpl(const char *name, double value, double min = 0,
                      double max = 100)
        : mValue(value), mMin(MIN(min, max)), mMax(MAX(min, max)) {
        FASTLED_UNUSED(name);
    }
    ~UINumberFieldImpl() {}
    double value() const { return mValue; }
    void setValue(double value) { mValue = MAX(mMin, MIN(mMax, value)); }
    operator double() const { return mValue; }
    operator int() const { return static_cast<int>(mValue); }
    UINumberFieldImpl &operator=(double value) {
        setValue(value);
        return *this;
    }
    UINumberFieldImpl &operator=(int value) {
        setValue(static_cast<double>(value));
        return *this;
    }

  private:
    double mValue;
    double mMin;
    double mMax;
};

#endif

#if !FASTLED_HAS_UI_TITLE

class UITitleImpl {
  public:
    UITitleImpl(const char *name) { FASTLED_UNUSED(name); }
    ~UITitleImpl() {}
};

#endif

#if !FASTLED_HAS_UI_DESCRIPTION

class UIDescriptionImpl {
  public:
    UIDescriptionImpl(const char *name) { FASTLED_UNUSED(name); }
    ~UIDescriptionImpl() {}
};

#endif

#if !FASTLED_HAS_UI_AUDIO
class UIAudioImpl {
  public:
    UIAudioImpl(const char *name) { FASTLED_UNUSED(name); }
    ~UIAudioImpl() {}

    AudioSample next() {
        FASTLED_WARN("Audio sample not implemented");
        return AudioSample();
    }

    bool hasNext() {
        FASTLED_WARN("Audio sample not implemented");
        return false;
    }
};
#endif

} // end namespace fl
