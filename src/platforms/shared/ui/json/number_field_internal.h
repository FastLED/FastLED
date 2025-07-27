#pragma once

#include "ui_internal.h" // Ensure JsonUiInternal is included
#include "fl/string.h"
#include "fl/json.h"
#include "fl/math_macros.h"

namespace fl {

class JsonUiNumberFieldInternal : public JsonUiInternal {
private:
    double mValue;
    double mMin;
    double mMax;

public:
    // Constructor: Initializes the base JsonUiInternal with name, value, min, and max.
    JsonUiNumberFieldInternal(const fl::string& name, double value, double min, double max)
        : JsonUiInternal(name), mValue(value), mMin(min), mMax(max) {}

    // Override toJson to serialize the number field's data directly.
    void toJson(fl::Json& json) const override {
        json.set("name", name());
        json.set("group", groupName());
        json.set("type", "number");
        json.set("id", id());
        json.set("value", static_cast<float>(mValue));
        json.set("min", static_cast<float>(mMin));
        json.set("max", static_cast<float>(mMax));
    }

    // Override updateInternal to handle incoming JSON data for the number field.
    void updateInternal(const fl::Json& json) override {
        setValueInternal(json | 0.0);
    }

    // Accessors for number field value
    double value() const { return mValue; }

    void setValue(double value) {
        if (value < mMin) {
            value = mMin;
        } else if (value > mMax) {
            value = mMax;
        }
        double oldValue = mValue;
        mValue = value;
        
        // If value actually changed, mark this component as changed for polling
        if (!ALMOST_EQUAL_FLOAT(mValue, oldValue)) {
            markChanged();
        }
    }

    void setValueInternal(double value) {
        // Internal method for updates from JSON UI system - no change notification
        if (value < mMin) {
            value = mMin;
        } else if (value > mMax) {
            value = mMax;
        }
        mValue = value;
    }

    double min() const { return mMin; }
    double max() const { return mMax; }
};

} // namespace fl
