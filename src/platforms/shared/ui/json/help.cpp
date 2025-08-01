#include "help.h"
#include "platforms/shared/ui/json/ui.h"
#include "ui_internal.h"

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

namespace fl {

class JsonUiHelpInternal : public JsonUiInternal {
  private:
    fl::string mMarkdownContent;

  public:
    // Constructor: Initializes the base JsonUiInternal with name, and sets the
    // help content.
    JsonUiHelpInternal(const fl::string &name,
                       const fl::string &markdownContent)
        : JsonUiInternal(name), mMarkdownContent(markdownContent) {}

    // Override toJson to serialize the help's data directly.
    void toJson(fl::Json &json) const override {
        json.set("name", name());
        json.set("type", "help");
        json.set("group", groupName());
        json.set("id", id());
        json.set("markdownContent", mMarkdownContent);
    }

    // Override updateInternal. Help components typically don't have update
    // functionality from the UI, so this can be a no-op.
    void updateInternal(const fl::Json &json) override {
        // No update needed for help components
        FL_UNUSED(json);
    }

    // Accessors for the help content.
    const fl::string &markdownContent() const { return mMarkdownContent; }
    void setMarkdownContent(const fl::string &markdownContent) {
        mMarkdownContent = markdownContent;
    }
};

JsonHelpImpl::JsonHelpImpl(const string &markdownContent) {
    // Create an instance of the new internal class
    mInternal = fl::make_shared<JsonUiHelpInternal>("help", markdownContent);

    // Register the component with the JsonUiManager
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonHelpImpl::~JsonHelpImpl() {
    // Ensure the component is removed from the global registry
    removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonHelpImpl &JsonHelpImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonHelpImpl::markdownContent() const {
    return mInternal->markdownContent();
}

void JsonHelpImpl::toJson(fl::Json &json) const { mInternal->toJson(json); }

const string &JsonHelpImpl::name() const { return mInternal->name(); }

const fl::string &JsonHelpImpl::groupName() const {
    return mInternal->groupName();
}

void JsonHelpImpl::setGroup(const fl::string &groupName) {
    mInternal->setGroup(groupName);
}

int JsonHelpImpl::id() const { return mInternal->id(); }

} // namespace fl

#endif
