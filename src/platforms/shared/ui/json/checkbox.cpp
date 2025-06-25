#include "platforms/shared/ui/json/checkbox.h"
#include "fl/json.h"
#include "platforms/shared/ui/json/ui.h"
#include <string.h>

#if FASTLED_ENABLE_JSON

using namespace fl;

namespace fl {

JsonCheckboxImpl::JsonCheckboxImpl(const fl::string &name, bool value)
    : mValue(value) {
    auto updateFunc = JsonUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &value) {
            static_cast<JsonCheckboxImpl *>(this)->updateInternal(value);
        });

    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<JsonCheckboxImpl *>(this)->toJson(json);
        });
    mInternal = JsonUiInternalPtr::New(name, fl::move(updateFunc),
                                     fl::move(toJsonFunc));
    addJsonUiComponent(mInternal);
}

JsonCheckboxImpl::~JsonCheckboxImpl() { removeJsonUiComponent(mInternal); }

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

void JsonCheckboxImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    if (value.is<bool>()) {
        bool newValue = value.as<bool>();
        setValueInternal(newValue);  // Use internal method to avoid change notification
    }
}

} // namespace fl

#endif // __EMSCRIPTEN__
