#pragma once

#include <stdint.h>
#include "platforms/ui_defs.h"
#include "namespace.h"

#ifndef FASTLED_HAS_UI_SLIDER
#define FASTLED_HAS_UI_SLIDER 0
#endif

#ifndef FASTLED_HAS_UI_BUTTON
#define FASTLED_HAS_UI_BUTTON 0
#endif

#ifndef FASTLED_HAS_UI_CHECKBOX
#define FASTLED_HAS_UI_CHECKBOX 0
#endif

FASTLED_NAMESPACE_BEGIN


// If the platform is missing ui components, provide stubs.

#if !FASTLED_HAS_UI_SLIDER

class Slider {
  public:
    Slider(const char *name, float value = 128.0f, float min = 1, float max = 255, float step = 1) : mValue(value) {}
    ~Slider() {}
    float value() const { return mValue; }
    operator float() const { return mValue; }
    operator uint8_t() const { return static_cast<uint8_t>(mValue); }
    operator uint16_t() const { return static_cast<uint16_t>(mValue); }
    operator int() const { return static_cast<int>(mValue); }
    template <typename T> T as() const { return static_cast<T>(mValue); }
  private:
    float mValue;
};

#endif

#if !FASTLED_HAS_UI_BUTTON

class Button {
  public:
    Button(const char *name) {}
    ~Button() {}
    bool isPressed() const { return false; }
    bool clicked() { false; }
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
  private:
    bool mValue;
};

#endif

FASTLED_NAMESPACE_END
