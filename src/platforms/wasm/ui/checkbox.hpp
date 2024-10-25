
#include "json.h"
#include "platforms/wasm/js.h"
#include "ui_manager.h"
#include <string.h>

FASTLED_NAMESPACE_BEGIN

jsCheckbox::jsCheckbox(const char* name, bool value)
    : mValue(value) {
    auto updateFunc = jsUiInternal::UpdateFunction(this, [](void* self, const ArduinoJson::JsonVariantConst& json) {
        static_cast<jsCheckbox*>(self)->updateInternal(json);
    });
    auto toJsonFunc = jsUiInternal::ToJsonFunction(this, [](void* self, ArduinoJson::JsonObject& json) {
        static_cast<jsCheckbox*>(self)->toJson(json);
    });
    //mInternal = jsUiInternalPtr::New(name, std::move(updateFunc), std::move(toJsonFunc));
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc), std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
}

jsCheckbox::~jsCheckbox() {
    jsUiManager::removeComponent(mInternal);
}

const char* jsCheckbox::name() const {
    return mInternal->name();
}

void jsCheckbox::toJson(ArduinoJson::JsonObject& json) const {
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

void jsCheckbox::updateInternal(const ArduinoJson::JsonVariantConst& value) {
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
