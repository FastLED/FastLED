#include "platforms/shared/ui/json/checkbox.h"
#include "fl/json.h"
#include "platforms/shared/ui/json/ui.h"
#include <string.h>

#include "fl/json.h"

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

const fl::string &JsonCheckboxImpl::name() const { return mInternal->name(); }

void JsonCheckboxImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["group"] = mInternal->groupName().c_str();
    json["type"] = "checkbox";
    json["id"] = mInternal->id();
    json["value"] = mValue;
}

bool JsonCheckboxImpl::value() const { return mValue; }

void JsonCheckboxImpl::setValue(bool value) { mValue = value; }

void JsonCheckboxImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    if (value.is<bool>()) {
        bool newValue = value.as<bool>();
        setValue(newValue);
    }
}

} // namespace fl

#endif // __EMSCRIPTEN__
