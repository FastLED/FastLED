#pragma once

#include <stdint.h>
#include <string>
#include "ui/ui_internal.h"
#include <memory>
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

// Sets the canvas size. This assumes one strip per row. This is
// method is pretty inflexible and is likely to change in the future.
void jsSetCanvasSize(int width, int height);

class jsSlider {
public:
    jsSlider(const char* name, float min = 0.0f, float max = 255.0f, float value = 128.0f, float step = 1.0f);
    ~jsSlider();

    const char* name() const;
    std::string toJsonStr() const;
    float value() const;
    void setValue(float value);
    operator float() const;
    operator uint8_t() const;
    operator uint16_t() const;
    operator int() const;

private:
    void updateInternal(const char* jsonStr);

    std::shared_ptr<jsUiInternal> mInternal;
    float mMin;
    float mMax;
    float mValue;
    float mStep;
};

class jsCheckbox {
public:
    jsCheckbox(const char* name, bool value);
    ~jsCheckbox();

    const char* name() const;
    std::string toJsonStr() const;
    bool value() const;
    void setValue(bool value);
    operator bool() const;

private:
    void updateInternal(const char* jsonStr);

    std::shared_ptr<jsUiInternal> mInternal;
    bool mValue;
};

FASTLED_NAMESPACE_END