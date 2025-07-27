#pragma once

#include "ui_internal.h" // Ensure JsonUiInternal is included
#include "fl/string.h"
#include "fl/json.h"

namespace fl {

class JsonUiDescriptionInternal : public JsonUiInternal {
private:
    fl::string mText;

public:
    // Constructor: Initializes the base JsonUiInternal with name and text.
    JsonUiDescriptionInternal(const fl::string& name, const fl::string& text)
        : JsonUiInternal(name), mText(text) {}

    // Override toJson to serialize the description's data directly.
    void toJson(fl::Json& json) const override {
        json.set("name", name());
        json.set("type", "description");
        json.set("group", groupName());
        json.set("id", id());
        json.set("text", mText);
    }

    // Override updateInternal. Descriptions typically don't have update functionality
    // from the UI, so this can be a no-op.
    void updateInternal(const fl::Json& json) override {
        // No update needed for description components
    }

    // Accessors for the description text.
    const fl::string& text() const { return mText; }
    void setText(const fl::string& text) { mText = text; }
};

} // namespace fl
