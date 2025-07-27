#pragma once

#include "ui_internal.h" // Ensure JsonUiInternal is included
#include "fl/string.h"
#include "fl/json.h"
#include "fl/math_macros.h"

namespace fl {

class JsonUiSliderInternal : public JsonUiInternal {
private:
    float mValue;
    float mMin;
    float mMax;
    float mStep;

public:
    // Constructor: Initializes the base JsonUiInternal with name, value, min, max, and step.
    JsonUiSliderInternal(const fl::string& name, float value, float min, float max, float step)
        : JsonUiInternal(name), mValue(value), mMin(min), mMax(max), mStep(step) {}

    // Override toJson to serialize the slider's data directly.
    void toJson(fl::Json& json) const override {
        json.set("name", name());
        json.set("group", groupName());
        json.set("type", "slider");
        json.set("id", id());
        json.set("value", mValue);
        json.set("min", mMin);
        json.set("max", mMax);
        if (mStep > 0) {
            json.set("step", mStep);
        }
    }

    // Override updateInternal to handle incoming JSON data for the slider.
    void updateInternal(const fl::Json& json) override {
        setValue(json | 0.0f);
    }

    // Accessors for slider value
    float value() const { return mValue; }

    float value_normalized() const {
        if (ALMOST_EQUAL(mMax, mMin, 0.0001f)) {
            return 0;
        }
        return (mValue - mMin) / (mMax - mMin);
    }

    float getMax() const { return mMax; }
    float getMin() const { return mMin; }

    void setValue(float value) {
        if (value < mMin) {
            value = mMin;
        } else if (value > mMax) {
            value = mMax;
        }
        mValue = value;
    }

    float getStep() const { return mStep; }
};

} // namespace fl
