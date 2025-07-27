#include "platforms/shared/ui/json/checkbox.h"
#include "fl/json.h"
#include "fl/json.h"
#include "platforms/shared/ui/json/ui.h"
#include <string.h>

#if FASTLED_ENABLE_JSON

namespace fl {

// Definition of the internal class that was previously in checkbox_internal.h
class JsonUiCheckboxInternal : public JsonUiInternal {
private:
    bool mValue;

public:
    // Constructor: Initializes the base JsonUiInternal with name, and sets initial value.
    JsonUiCheckboxInternal(const fl::string& name, bool value = false)
        : JsonUiInternal(name), mValue(value) {}

    // Override toJson to serialize the checkbox's data directly.
    void toJson(fl::Json& json) const override {
        json.set("name", name());
        json.set("type", "checkbox");
        json.set("group", groupName());
        json.set("id", id());
        json.set("value", mValue);
    }

    // Override updateInternal to handle updates from JSON.
    void updateInternal(const fl::Json& json) override {
        mValue = json | false;
    }

    // Accessors for the checkbox value.
    bool value() const { return mValue; }
    void setValue(bool value) { mValue = value; }
};

JsonCheckboxImpl::JsonCheckboxImpl(const fl::string &name, bool value) {
    // Create an instance of the new internal class
    mInternal = fl::make_shared<JsonUiCheckboxInternal>(name, value);

    // Register the component with the JsonUiManager
    addJsonUiComponent(mInternal);
}

JsonCheckboxImpl::~JsonCheckboxImpl() { 
    // Ensure the component is removed from the global registry
    removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonCheckboxImpl &JsonCheckboxImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonCheckboxImpl::name() const { return mInternal->name(); }

void JsonCheckboxImpl::toJson(fl::Json &json) const {
    mInternal->toJson(json);
}

bool JsonCheckboxImpl::value() const { return mInternal->value(); }

void JsonCheckboxImpl::setValue(bool value) { 
    bool oldValue = mInternal->value();
    mInternal->setValue(value); 
    
    // If value actually changed, mark this component as changed for polling
    if (mInternal->value() != oldValue) {
        mInternal->markChanged();
    }
}

const fl::string &JsonCheckboxImpl::groupName() const { return mInternal->groupName(); }

void JsonCheckboxImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

JsonCheckboxImpl &JsonCheckboxImpl::operator=(bool value) {
    setValue(value);
    return *this;
}

JsonCheckboxImpl &JsonCheckboxImpl::operator=(int value) {
    setValue(value != 0);
    return *this;
}

int JsonCheckboxImpl::id() const {
    return mInternal->id();
}

} // namespace fl

#endif // FASTLED_ENABLE_JSON