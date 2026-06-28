// IWYU pragma: private

#include "platforms/shared/ui/json/description.h"
#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/ui_internal.h"

#include "fl/stl/json.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
namespace fl {

class JsonUiDescriptionInternal : public JsonUiInternal {
  private:
    fl::string mText;

  public:
    // Constructor: Initializes the base JsonUiInternal with name, and sets the
    // description text.
    JsonUiDescriptionInternal(const fl::string &name, const fl::string &text)
 FL_NO_EXCEPT : JsonUiInternal(name), mText(text) {}

    // Override toJson to serialize the description's data directly.
    void toJson(fl::json &json) const FL_NO_EXCEPT override {
        json.set("name", name());
        json.set("type", "description");
        json.set("group", groupName());
        json.set("id", id());
        json.set("text", mText);
    }

    // Override updateInternal. Descriptions typically don't have update
    // functionality from the UI, so this can be a no-op.
    void updateInternal(const fl::json &json) FL_NO_EXCEPT override {
        FL_UNUSED(json);
        // No update needed for description components
    }

    // Accessors for the description text.
    const fl::string &text() const FL_NO_EXCEPT { return mText; }
    void setText(const fl::string &text) FL_NO_EXCEPT { mText = text; }
};

JsonDescriptionImpl::JsonDescriptionImpl(const string &text) FL_NO_EXCEPT {
    // Create an instance of the new internal class
    mInternal = fl::make_shared<JsonUiDescriptionInternal>("description", text);

    // Register the component with the JsonUiManager
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonDescriptionImpl::~JsonDescriptionImpl() {
    // Ensure the component is removed from the global registry
    removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonDescriptionImpl &JsonDescriptionImpl::Group(const fl::string &name) FL_NO_EXCEPT {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonDescriptionImpl::text() const FL_NO_EXCEPT {
    return mInternal->text();
}

void JsonDescriptionImpl::toJson(fl::json &json) const FL_NO_EXCEPT {
    mInternal->toJson(json);
}

const string &JsonDescriptionImpl::name() const FL_NO_EXCEPT { return mInternal->name(); }

fl::string JsonDescriptionImpl::groupName() const FL_NO_EXCEPT {
    return mInternal->groupName();
}

void JsonDescriptionImpl::setGroup(const fl::string &groupName) FL_NO_EXCEPT {
    mInternal->setGroup(groupName);
}

int JsonDescriptionImpl::id() const FL_NO_EXCEPT {
    return mInternal->id();
}

} // namespace fl
