
#include "third_party/arduinojson/json.h"
#include "platforms/wasm/js.h"
#include "ui_manager.h"
#include <string.h>




FASTLED_NAMESPACE_BEGIN

jsCheckbox::jsCheckbox(const char* name, bool value)
    : mValue(value) {
    auto updateFunc = [this](const char* jsonStr) { this->updateInternal(jsonStr); };
    auto toJsonFunc = [this](ArduinoJson::JsonObject& json) { this->toJson(json); };
    mInternal = std::make_shared<jsUiInternal>(name, std::move(updateFunc), std::move(toJsonFunc));
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

void jsCheckbox::updateInternal(const char* jsonStr) {
    // We expect jsonStr to be a boolean value string, so parse it accordingly
    mValue = (strcmp(jsonStr, "true") == 0);
}

jsCheckbox::operator bool() const {
    return mValue;
}

jsCheckbox::operator int() const {
    return mValue;
}

FASTLED_NAMESPACE_END
