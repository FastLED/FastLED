#pragma once

#include <stdint.h>
#include <string>
#include "ui.h"

class jsSlider : public jsUI {
public:
    jsSlider(const std::string& name, float min = 0.0f, float max = 255.0f, float value = 128.0f, float step = 1.0f);

    std::string name() const override;
    std::string type() const override;
    std::string toJsonStr() const override;
    float value() const;
    virtual void update(const char* jsonStr) override;
    void setValue(float value);
    operator float() const;

private:
    float mMin;
    float mMax;
    float mValue;
    float mStep;
    std::string mName;
};

DECLARE_SMART_PTR_NO_FWD(jsSlider);



