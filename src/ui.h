#pragma once

#include <stdint.h>

#include "platforms/ui_defs.h"
#include "namespace.h"
#include "math_macros.h"

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

FASTLED_NAMESPACE_BEGIN


// If the platform is missing ui components, provide stubs.

#if !FASTLED_HAS_UI_SLIDER

class Slider {
  public:
    Slider(const char *name, float value = 128.0f, float min = 1, float max = 255, float step = 1)
      : mValue(value), mMin(MIN(min, max)), mMax(MAX(min, max)), mStep(step) {}
    ~Slider() {}
    float value() const { return mValue; }
    void setValue(float value) { mValue = MAX(mMin, MIN(mMax, value)); }
    operator float() const { return mValue; }
    operator uint8_t() const { return static_cast<uint8_t>(mValue); }
    operator uint16_t() const { return static_cast<uint16_t>(mValue); }
    operator int() const { return static_cast<int>(mValue); }
    template <typename T> T as() const { return static_cast<T>(mValue); }
    
    Slider& operator=(float value) { setValue(value); return *this; }
    Slider& operator=(int value) { setValue(static_cast<float>(value)); return *this; }
  private:
    float mValue;
    float mMin;
    float mMax;
    float mStep;
};


// template operator for >= against a jsSlider



#endif

#if !FASTLED_HAS_UI_BUTTON

class Button {
  public:
    Button(const char *name) {}
    ~Button() {}
    bool isPressed() const { return false; }
    bool clicked() const { return false; }
    operator bool() const { return false; }
};

#endif

#if !FASTLED_HAS_UI_CHECKBOX

class Checkbox {
  public:
    Checkbox(const char *name, bool value = false) : mValue(value) {}
    ~Checkbox() {}
    operator bool() const { return mValue; }
    operator int() const { return mValue ? 1 : 0; }
    Checkbox& operator=(bool value) { setValue(value); return *this; }
    Checkbox& operator=(int value) { setValue(value != 0); return *this; }
  private:
    void setValue(bool value) { mValue = value; }
    bool mValue;
};

#endif

#if !FASTLED_HAS_UI_NUMBER_FIELD

class NumberField {
  public:
    NumberField(const char *name, double value, double min = 0, double max = 100)
      : mValue(value), mMin(MIN(min, max)), mMax(MAX(min, max)) {}
    ~NumberField() {}
    double value() const { return mValue; }
    void setValue(double value) { mValue = MAX(mMin, MIN(mMax, value)); }
    operator double() const { return mValue; }
    operator int() const { return static_cast<int>(mValue); }
    NumberField& operator=(double value) { setValue(value); return *this; }
    NumberField& operator=(int value) { setValue(static_cast<double>(value)); return *this; }
  private:
    double mValue;
    double mMin;
    double mMax;
};

#endif

#define FASTLED_UI_DEFINE_OPERATORS(UI_CLASS) \
template <typename T> bool operator>= (T v, const UI_CLASS& ui) { return ui >= v; } \
template <typename T> bool operator<= (T v, const UI_CLASS& ui) { return ui <= v; } \
template <typename T> bool operator> (T v, const UI_CLASS& ui) { return ui > v; } \
template <typename T> bool operator< (T v, const UI_CLASS& ui) { return ui < v; } \
template <typename T> bool operator== (T v, const UI_CLASS& ui) { return ui == v; } \
template <typename T> bool operator!= (T v, const UI_CLASS& ui) { return ui != v; } \
template <typename T> bool operator>=(const UI_CLASS& ui, T v) { return ui >= v; }  \
template <typename T> bool operator<=(const UI_CLASS& ui, T v) { return ui <= v; } \
template <typename T> bool operator>(const UI_CLASS& ui, T v) { return ui > v; } \
template <typename T> bool operator<(const UI_CLASS& ui, T v) { return ui < v; } \
template <typename T> bool operator==(const UI_CLASS& ui, T v) { return ui == v; } \
template <typename T> bool operator!=(const UI_CLASS& ui, T v) { return ui != v; }

FASTLED_UI_DEFINE_OPERATORS(Slider);
FASTLED_UI_DEFINE_OPERATORS(NumberField);
FASTLED_UI_DEFINE_OPERATORS(Checkbox);
FASTLED_UI_DEFINE_OPERATORS(Button);

FASTLED_NAMESPACE_END
