
#ifdef __EMSCRIPTEN__

#include <string.h>
#include "fl/namespace.h"

#include "platforms/wasm/js.h"
#include "fl/json.h"
#include "ui_manager.h"

using namespace fl;

FASTLED_NAMESPACE_BEGIN

jsCheckbox::jsCheckbox(const Str& name, bool value)
    : mValue(value) {
    auto updateFunc = jsUiInternal::UpdateFunction(this, [](void* self, const FLArduinoJson::JsonVariantConst& json) {
        static_cast<jsCheckbox*>(self)->updateInternal(json);
    });
    auto toJsonFunc = jsUiInternal::ToJsonFunction(this, [](void* self, FLArduinoJson::JsonObject& json) {
        static_cast<jsCheckbox*>(self)->toJson(json);
    });
    //mInternal = jsUiInternalPtr::New(name, std::move(updateFunc), std::move(toJsonFunc));
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc), std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
}

jsCheckbox::~jsCheckbox() {
    jsUiManager::removeComponent(mInternal);
}

const Str& jsCheckbox::name() const {
    return mInternal->name();
}

void jsCheckbox::toJson(FLArduinoJson::JsonObject& json) const {
    json["name"] = name();
    json["group"] = mGroup.c_str();
    json["type"] = "checkbox";
    json["id"] = mInternal->id();
    json["value"] = mValue;
}

bool jsCheckbox::value() const {
    return mValue;
}

void jsCheckbox::setValue(bool value) {
    mValue = value;
}

void jsCheckbox::updateInternal(const FLArduinoJson::JsonVariantConst& value) {
    // We expect jsonStr to be a boolean value string, so parse it accordingly
    mValue = value.as<bool>();
}

jsCheckbox::operator bool() const {
    return mValue;
}

jsCheckbox::operator int() const {
    return mValue;
}

FASTLED_NAMESPACE_END

#endif  // __EMSCRIPTEN__
