#include "description.h"
#include "platforms/shared/ui/json/ui.h"
#include "ui_internal.h"

#include "fl/json.h"
#include "fl/unused.h"

#if FASTLED_ENABLE_JSON

namespace fl {

class JsonUiDescriptionInternal : public JsonUiInternal {
  private:
    fl::string mText;

  public:
    // Constructor: Initializes the base JsonUiInternal with name, and sets the
    // description text.
    JsonUiDescriptionInternal(const fl::string &name, const fl::string &text)
        : JsonUiInternal(name), mText(text) {}

    // Override toJson to serialize the description's data directly.
    void toJson(fl::Json &json) const override {
        json.set("name", name());
        json.set("type", "description");
        json.set("group", groupName());
        json.set("id", id());
        json.set("text", mText);
    }

    // Override updateInternal. Descriptions typically don't have update
    // functionality from the UI, so this can be a no-op.
    void updateInternal(const fl::Json &json) override {
        FL_UNUSED(json);
        // No update needed for description components
    }

    // Accessors for the description text.
    const fl::string &text() const { return mText; }
    void setText(const fl::string &text) { mText = text; }
};

JsonDescriptionImpl::JsonDescriptionImpl(const string &text) {
    // Create an instance of the new internal class
    mInternal = fl::make_shared<JsonUiDescriptionInternal>("description", text);

    // Register the component with the JsonUiManager
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonDescriptionImpl::~JsonDescriptionImpl() {
    // Ensure the component is removed from the global registry
    removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonDescriptionImpl &JsonDescriptionImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonDescriptionImpl::text() const {
    return mInternal->text();
}

void JsonDescriptionImpl::toJson(fl::Json &json) const {
    mInternal->toJson(json);
}

const string &JsonDescriptionImpl::name() const { return mInternal->name(); }

const fl::string &JsonDescriptionImpl::groupName() const {
    return mInternal->groupName();
}

void JsonDescriptionImpl::setGroup(const fl::string &groupName) {
    mInternal->setGroup(groupName);
}

int JsonDescriptionImpl::id() const {
    return mInternal->id();
}

} // namespace fl

#endif
