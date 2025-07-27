#pragma once

#include "ui_internal.h" // Ensure JsonUiInternal is included
#include "fl/string.h"
#include "fl/json.h"

namespace fl {

class JsonUiTitleInternal : public JsonUiInternal {
private:
    fl::string mText;

public:
    // Constructor: Initializes the base JsonUiInternal with name, and sets the title text.
    JsonUiTitleInternal(const fl::string& name, const fl::string& text)
        : JsonUiInternal(name), mText(text) {}

    // Override toJson to serialize the title's data directly.
    // This function will be called by JsonUiManager to get the component's state.
    void toJson(fl::Json& json) const override {
        json.set("name", name());
        json.set("type", "title");
        json.set("group", groupName()); // Assuming groupName() is accessible from base
        json.set("id", id());           // Assuming id() is accessible from base
        json.set("text", mText);
    }

    // Override updateInternal. Titles typically don't have update functionality
    // from the UI, so this can be a no-op.
    void updateInternal(const fl::Json& json) override {
        // No update needed for title components
    }

    // Accessors for the title text.
    const fl::string& text() const { return mText; }
    void setText(const fl::string& text) { mText = text; }
};

} // namespace fl
