#pragma once

#include "ui_internal.h" // Ensure JsonUiInternal is included
#include "fl/string.h"
#include "fl/json.h"

namespace fl {

class JsonUiHelpInternal : public JsonUiInternal {
private:
    fl::string mMarkdownContent;

public:
    // Constructor: Initializes the base JsonUiInternal with name and markdown content.
    JsonUiHelpInternal(const fl::string& name, const fl::string& markdownContent)
        : JsonUiInternal(name), mMarkdownContent(markdownContent) {}

    // Override toJson to serialize the help component's data directly.
    void toJson(fl::Json& json) const override {
        json.set("name", name());
        json.set("type", "help");
        json.set("group", groupName());
        json.set("id", id());
        json.set("markdownContent", mMarkdownContent);
    }

    // Override updateInternal. Help components typically don't have update functionality
    // from the UI, so this can be a no-op.
    void updateInternal(const fl::Json& json) override {
        // No update needed for help components
    }

    // Accessors for the markdown content.
    const fl::string& markdownContent() const { return mMarkdownContent; }
    void setMarkdownContent(const fl::string& markdownContent) { mMarkdownContent = markdownContent; }
};

} // namespace fl
