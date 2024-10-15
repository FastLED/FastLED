#pragma once

#include <stdint.h>
#include <string>
#include "ui_internal.h"
#include <memory>

FASTLED_NAMESPACE_BEGIN

class jsSlider {
public:
    jsSlider(const std::string& name, float min = 0.0f, float max = 255.0f, float value = 128.0f, float step = 1.0f);
    ~jsSlider();

    std::string name() const;
    std::string toJsonStr() const;
    float value() const;
    void setValue(float value);
    operator float() const;

private:
    void updateInternal(const char* jsonStr);

    std::shared_ptr<jsUiInternal> mInternal;
    float mMin;
    float mMax;
    float mValue;
    float mStep;
};

FASTLED_NAMESPACE_END



