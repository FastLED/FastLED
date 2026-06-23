// IWYU pragma: private

#include "platforms/shared/ui/json/checkbox.h"
#include "fl/stl/json.h"
#include "fl/stl/json.h"
#include "platforms/shared/ui/json/ui.h"
#include "fl/stl/noexcept.h"
namespace fl {

// Definition of the internal class that was previously in checkbox_internal.h
class JsonUiCheckboxInternal : public JsonUiInternal {
private:
    bool mValue;

public:
    // Constructor: Initializes the base JsonUiInternal with name, and sets initial value.
    JsonUiCheckboxInternal(const fl::string& name, bool value = false)
 FL_NOEXCEPT : JsonUiInternal(name), mValue(value) {}

    // Override toJson to serialize the checkbox's data directly.
    void toJson(fl::json& json) const FL_NOEXCEPT override {
        json.set("name", name());
        json.set("type", "checkbox");
        json.set("group", groupName());
        json.set("id", id());
        json.set("value", mValue);
    }

    // Override updateInternal to handle updates from JSON.
    void updateInternal(const fl::json& json) FL_NOEXCEPT override {
        mValue = json | false;
    }

    // Accessors for the checkbox value.
    bool value() const FL_NOEXCEPT { return mValue; }
    void setValue(bool value) FL_NOEXCEPT { mValue = value; }
};

JsonCheckboxImpl::JsonCheckboxImpl(const fl::string &name, bool value) FL_NOEXCEPT {
    // Create an instance of the new internal class
    mInternal = fl::make_shared<JsonUiCheckboxInternal>(name, value);

    // Register the component with the JsonUiManager
    addJsonUiComponent(mInternal);
}

JsonCheckboxImpl::~JsonCheckboxImpl() { 
    // Ensure the component is removed from the global registry
    removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonCheckboxImpl &JsonCheckboxImpl::Group(const fl::string &name) FL_NOEXCEPT {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonCheckboxImpl::name() const FL_NOEXCEPT { return mInternal->name(); }

void JsonCheckboxImpl::toJson(fl::json &json) const FL_NOEXCEPT {
    mInternal->toJson(json);
}

bool JsonCheckboxImpl::value() const FL_NOEXCEPT { return mInternal->value(); }

void JsonCheckboxImpl::setValue(bool value) FL_NOEXCEPT { 
    bool oldValue = mInternal->value();
    mInternal->setValue(value); 
    
    // If value actually changed, mark this component as changed for polling
    if (mInternal->value() != oldValue) {
        mInternal->markChanged();
    }
}

fl::string JsonCheckboxImpl::groupName() const FL_NOEXCEPT { return mInternal->groupName(); }

void JsonCheckboxImpl::setGroup(const fl::string &groupName) FL_NOEXCEPT { mInternal->setGroup(groupName); }

JsonCheckboxImpl &JsonCheckboxImpl::operator=(bool value) FL_NOEXCEPT {
    setValue(value);
    return *this;
}

JsonCheckboxImpl &JsonCheckboxImpl::operator=(int value) FL_NOEXCEPT {
    setValue(value != 0);
    return *this;
}

int JsonCheckboxImpl::id() const FL_NOEXCEPT {
    return mInternal->id();
}

} // namespace fl
