#include "platforms/shared/ui/json/checkbox.h"
#include "fl/json.h"
#include "platforms/shared/ui/json/ui.h"
#include <string.h>

#if FASTLED_ENABLE_JSON

namespace fl {

JsonCheckboxImpl::JsonCheckboxImpl(const fl::string &name, bool value)
    : mValue(value) {
    auto updateFunc = JsonUiInternal::UpdateFunction(
        [this](const fl::Json &json) {
            static_cast<JsonCheckboxImpl *>(this)->updateInternal(json);
        });

    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<JsonCheckboxImpl *>(this)->toJson(json);
        });
    mInternal = fl::make_shared<JsonUiInternal>(name, fl::move(updateFunc),
                                     fl::move(toJsonFunc));
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonCheckboxImpl::~JsonCheckboxImpl() { removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal)); }

JsonCheckboxImpl &JsonCheckboxImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonCheckboxImpl::name() const { return mInternal->name(); }

void JsonCheckboxImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["group"] = mInternal->groupName().c_str();
    json["type"] = "checkbox";
    json["id"] = mInternal->id();
    json["value"] = mValue;
}

bool JsonCheckboxImpl::value() const { return mValue; }

void JsonCheckboxImpl::setValue(bool value) { 
    bool oldValue = mValue;
    mValue = value; 
    
    // If value actually changed, mark this component as changed for polling
    if (mValue != oldValue) {
        mInternal->markChanged();
    }
}

void JsonCheckboxImpl::setValueInternal(bool value) {
    // Internal method for updates from JSON UI system - no change notification
    mValue = value;
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

void JsonCheckboxImpl::updateInternal(const fl::Json &json) {
    // Use ideal JSON API directly with type-safe default
    bool newValue = json | false;  // Gets bool value or false default
    setValueInternal(newValue);  // Use internal method to avoid change notification
}

} // namespace fl

#endif // __EMSCRIPTEN__
