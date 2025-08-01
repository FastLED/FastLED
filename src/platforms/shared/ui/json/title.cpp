#include "title.h"
#include "fl/json.h"
#include "platforms/shared/ui/json/ui.h"

#if FASTLED_ENABLE_JSON

namespace fl {

class JsonUiTitleInternal : public JsonUiInternal {
  private:
    fl::string mText;

  public:
    // Constructor: Initializes the base JsonUiInternal with name, and sets the
    // title text.
    JsonUiTitleInternal(const fl::string &name, const fl::string &text)
        : JsonUiInternal(name), mText(text) {}

    // Override toJson to serialize the title's data directly.
    // This function will be called by JsonUiManager to get the component's
    // state.
    void toJson(fl::Json &json) const override {
        json.set("name", name());
        json.set("type", "title");
        json.set("group",
                 groupName()); // Assuming groupName() is accessible from base
        json.set("id", id());  // Assuming id() is accessible from base
        json.set("text", mText);
    }

    // Override updateInternal. Titles typically don't have update functionality
    // from the UI, so this can be a no-op.
    void updateInternal(const fl::Json &json) override {
        // No update needed for title components
        FL_UNUSED(json);
    }

    // Accessors for the title text.
    const fl::string &text() const { return mText; }
    void setText(const fl::string &text) { mText = text; }
};

// Constructor implementation
JsonTitleImpl::JsonTitleImpl(const fl::string &name, const fl::string &text) {
    // Create an instance of the new internal class
    mInternal = fl::make_shared<JsonUiTitleInternal>(name, text);

    // Register the component with the JsonUiManager
    addJsonUiComponent(mInternal);
}

// Destructor implementation
JsonTitleImpl::~JsonTitleImpl() {
    // Ensure the component is removed from the global registry
    removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));

    // No need to clear functions anymore as there are no lambdas capturing
    // 'this' if (mInternal) {
    //     mInternal->clearFunctions();
    // }
}

JsonTitleImpl &JsonTitleImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const string &JsonTitleImpl::name() const { return mInternal->name(); }

const fl::string &JsonTitleImpl::groupName() const {
    return mInternal->groupName();
}

// Delegate text accessors to the internal object
const fl::string &JsonTitleImpl::text() const { return mInternal->text(); }

void JsonTitleImpl::setText(const fl::string &text) {
    mInternal->setText(text);
}

void JsonTitleImpl::setGroup(const fl::string &groupName) {
    mInternal->setGroup(groupName);
}

int JsonTitleImpl::id() const { return mInternal->id(); }

} // namespace fl

#endif
