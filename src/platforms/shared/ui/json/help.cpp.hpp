// IWYU pragma: private

#include "platforms/shared/ui/json/help.h"
#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/ui_internal.h"

#include "fl/stl/json.h"
#include "fl/stl/noexcept.h"
namespace fl {

class JsonUiHelpInternal : public JsonUiInternal {
  private:
    fl::string mMarkdownContent;

  public:
    // Constructor: Initializes the base JsonUiInternal with name, and sets the
    // help content.
    JsonUiHelpInternal(const fl::string &name,
                       const fl::string &markdownContent)
 FL_NO_EXCEPT : JsonUiInternal(name), mMarkdownContent(markdownContent) {}

    // Override toJson to serialize the help's data directly.
    void toJson(fl::json &json) const FL_NO_EXCEPT override {
        json.set("name", name());
        json.set("type", "help");
        json.set("group", groupName());
        json.set("id", id());
        json.set("markdownContent", mMarkdownContent);
    }

    // Override updateInternal. Help components typically don't have update
    // functionality from the UI, so this can be a no-op.
    void updateInternal(const fl::json &json) FL_NO_EXCEPT override {
        // No update needed for help components
        FL_UNUSED(json);
    }

    // Accessors for the help content.
    const fl::string &markdownContent() const FL_NO_EXCEPT { return mMarkdownContent; }
    void setMarkdownContent(const fl::string &markdownContent) FL_NO_EXCEPT {
        mMarkdownContent = markdownContent;
    }
};

JsonHelpImpl::JsonHelpImpl(const string &markdownContent) FL_NO_EXCEPT {
    // Create an instance of the new internal class
    mInternal = fl::make_shared<JsonUiHelpInternal>("help", markdownContent);

    // Register the component with the JsonUiManager
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonHelpImpl::~JsonHelpImpl() {
    // Ensure the component is removed from the global registry
    removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonHelpImpl &JsonHelpImpl::Group(const fl::string &name) FL_NO_EXCEPT {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonHelpImpl::markdownContent() const FL_NO_EXCEPT {
    return mInternal->markdownContent();
}

void JsonHelpImpl::toJson(fl::json &json) const FL_NO_EXCEPT { mInternal->toJson(json); }

const string &JsonHelpImpl::name() const FL_NO_EXCEPT { return mInternal->name(); }

fl::string JsonHelpImpl::groupName() const FL_NO_EXCEPT {
    return mInternal->groupName();
}

void JsonHelpImpl::setGroup(const fl::string &groupName) FL_NO_EXCEPT {
    mInternal->setGroup(groupName);
}

int JsonHelpImpl::id() const FL_NO_EXCEPT { return mInternal->id(); }

} // namespace fl
