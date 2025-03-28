#pragma once

#include <stdint.h>

#include "platforms/ui_defs.h"
#include "fl/namespace.h"
#include "fl/math_macros.h"
#include "fl/template_magic.h"
#include "fl/unused.h"

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

namespace fl {


// If the platform is missing ui components, provide stubs.

#if !FASTLED_HAS_UI_SLIDER

class UISlider {
  public:
    // If step is -1, it will be calculated as (max - min) / 100
    UISlider(const char *name, float value = 128.0f, float min = 1, float max = 255, float step = -1.f)
      : mValue(value), mMin(MIN(min, max)), mMax(MAX(min, max)) {
        FASTLED_UNUSED(name);
        FASTLED_UNUSED(step);
        if (value < min) { mValue = min; }
        if (value > max) { mValue = max; }
    }
    ~UISlider() {}
    float value() const { return mValue; }
    void setValue(float value) { mValue = MAX(mMin, MIN(mMax, value)); }
    operator float() const { return mValue; }
    operator uint8_t() const { return static_cast<uint8_t>(mValue); }
    operator uint16_t() const { return static_cast<uint16_t>(mValue); }
    operator int() const { return static_cast<int>(mValue); }
    template <typename T> T as() const { return static_cast<T>(mValue); }
    
    UISlider& operator=(float value) { setValue(value); return *this; }
    UISlider& operator=(int value) { setValue(static_cast<float>(value)); return *this; }
  private:
    float mValue;
    float mMin;
    float mMax;
};


// template operator for >= against a jsSlider



#endif

#if !FASTLED_HAS_UI_BUTTON

class UIButton {
  public:
    UIButton(const char *name) {
      FASTLED_UNUSED(name);
    }
    ~UIButton() {}
    bool isPressed() const { return false; }
    bool clicked() const { return false; }
    int clickedCount() const { return 0; }
    operator bool() const { return false; }
};

#endif

#if !FASTLED_HAS_UI_CHECKBOX

class UICheckbox {
  public:
    UICheckbox(const char *name, bool value = false) : mValue(value) {
      FASTLED_UNUSED(name);
    }
    ~UICheckbox() {}
    operator bool() const { return mValue; }
    operator int() const { return mValue ? 1 : 0; }
    UICheckbox& operator=(bool value) { setValue(value); return *this; }
    UICheckbox& operator=(int value) { setValue(value != 0); return *this; }
  private:
    void setValue(bool value) { mValue = value; }
    bool mValue;
};

#endif

#if !FASTLED_HAS_UI_NUMBER_FIELD

class UINumberField {
  public:
    UINumberField(const char *name, double value, double min = 0, double max = 100)
      : mValue(value), mMin(MIN(min, max)), mMax(MAX(min, max)) {
        FASTLED_UNUSED(name);
    }
    ~UINumberField() {}
    double value() const { return mValue; }
    void setValue(double value) { mValue = MAX(mMin, MIN(mMax, value)); }
    operator double() const { return mValue; }
    operator int() const { return static_cast<int>(mValue); }
    UINumberField& operator=(double value) { setValue(value); return *this; }
    UINumberField& operator=(int value) { setValue(static_cast<double>(value)); return *this; }
  private:
    double mValue;
    double mMin;
    double mMax;
};

#endif

#if !FASTLED_HAS_UI_TITLE


class UITitle {
  public:
    UITitle(const char *name) {
      FASTLED_UNUSED(name);
    }
    ~UITitle() {}
};

#endif




#if !FASTLED_HAS_UI_DESCRIPTION

class UIDescription {
  public:
    UIDescription(const char *name) {
      FASTLED_UNUSED(name);
    }
    ~UIDescription() {}
};

#endif


#define FASTLED_UI_DEFINE_OPERATORS(UI_CLASS) \
FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, >=) \
FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, <=) \
FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, >)  \
FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, <)  \
FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, ==) \
FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, !=)

FASTLED_UI_DEFINE_OPERATORS(UISlider);
FASTLED_UI_DEFINE_OPERATORS(UINumberField);
FASTLED_UI_DEFINE_OPERATORS(UICheckbox);
FASTLED_UI_DEFINE_OPERATORS(UIButton);

}  // end namespace fl


