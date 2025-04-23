
#ifdef __EMSCRIPTEN__

#include <string.h>
#include "fl/namespace.h"

#include "platforms/wasm/js.h"
#include "fl/json.h"
#include "ui_manager.h"

using namespace fl;

FASTLED_NAMESPACE_BEGIN

jsCheckboxImpl::jsCheckboxImpl(const Str& name, bool value)
    : mValue(value) {
    auto updateFunc = jsUiInternal::UpdateFunction([this](const FLArduinoJson::JsonVariantConst& json) {
        static_cast<jsCheckboxImpl*>(this)->updateInternal(json);
    });
    auto toJsonFunc = jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject& json) {
        static_cast<jsCheckboxImpl*>(this)->toJson(json);
    });
    //mInternal = jsUiInternalPtr::New(name, std::move(updateFunc), std::move(toJsonFunc));
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc), std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
}

jsCheckboxImpl::~jsCheckboxImpl() {
    jsUiManager::removeComponent(mInternal);
}

const Str& jsCheckboxImpl::name() const {
    return mInternal->name();
}

void jsCheckboxImpl::toJson(FLArduinoJson::JsonObject& json) const {
    json["name"] = name();
    json["group"] = mGroup.c_str();
    json["type"] = "checkbox";
    json["id"] = mInternal->id();
    json["value"] = mValue;
}

bool jsCheckboxImpl::value() const {
    return mValue;
}

void jsCheckboxImpl::setValue(bool value) {
    mValue = value;
}

void jsCheckboxImpl::updateInternal(const FLArduinoJson::JsonVariantConst& value) {
    // We expect jsonStr to be a boolean value string, so parse it accordingly
    mValue = value.as<bool>();
}


FASTLED_NAMESPACE_END

#endif  // __EMSCRIPTEN__
