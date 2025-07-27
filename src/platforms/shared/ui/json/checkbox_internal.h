#pragma once

#include "ui_internal.h" // Ensure JsonUiInternal is included
#include "fl/string.h"
#include "fl/json.h"

namespace fl {

class JsonUiCheckboxInternal : public JsonUiInternal {
private:
    bool mValue;

public:
    // Constructor: Initializes the base JsonUiInternal with name and initial value.
    JsonUiCheckboxInternal(const fl::string& name, bool value)
        : JsonUiInternal(name), mValue(value) {}

    // Override toJson to serialize the checkbox's data directly.
    void toJson(fl::Json& json) const override {
        json.set("name", name());
        json.set("type", "checkbox");
        json.set("group", groupName());
        json.set("id", id());
        json.set("value", mValue);
    }

    // Override updateInternal to handle incoming JSON data for the checkbox.
    void updateInternal(const fl::Json& json) override {
        mValue = json | false;
    }

    // Accessors for checkbox value
    bool value() const { return mValue; }
    void setValue(bool value) { mValue = value; }
};

} // namespace fl
