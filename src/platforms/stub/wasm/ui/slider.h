#pragma once

#include <stdint.h>
#include <string>
#include "ui.h"

class jsSlider : public jsUI {
public:
    jsSlider(const std::string& name, float min = 0.0f, float max = 255.0f, float value = 128.0f, float step = 1.0f);
    ~jsSlider();

    std::string type() const;
    std::string toJsonStr() const;
    float value() const;
    void setValue(float value);
    operator float() const;

protected:
    void updateInternal(const char* jsonStr) override;

private:
    float mMin;
    float mMax;
    float mValue;
    float mStep;
};

DECLARE_SMART_PTR_NO_FWD(jsSlider);


