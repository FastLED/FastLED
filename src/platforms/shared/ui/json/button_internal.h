#pragma once

#include "ui_internal.h" // Ensure JsonUiInternal is included
#include "fl/string.h"
#include "fl/json.h"

namespace fl {

class JsonUiButtonInternal : public JsonUiInternal {
private:
    bool mPressed;
    // Add other button-specific data members here if needed

public:
    // Constructor: Initializes the base JsonUiInternal with name.
    JsonUiButtonInternal(const fl::string& name)
        : JsonUiInternal(name), mPressed(false) {}

    // Override toJson to serialize the button's data directly.
    void toJson(fl::Json& json) const override {
        json.set("name", name());
        json.set("type", "button");
        json.set("group", groupName());
        json.set("id", id());
        json.set("pressed", mPressed);
    }

    // Override updateInternal to handle incoming JSON data for the button.
    void updateInternal(const fl::Json& json) override {
        mPressed = json | false;
    }

    // Accessors for button state
    bool isPressed() const { return mPressed; }
    void setPressed(bool pressed) { mPressed = pressed; }
};

} // namespace fl
