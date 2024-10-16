
#include "json.h"
#include "platforms/stub/wasm/js.h"
#include "ui_manager.h"

FASTLED_NAMESPACE_BEGIN

jsCheckbox::jsCheckbox(const char* name, bool value)
    : mValue(value) {
    auto updateFunc = [this](const char* jsonStr) { this->updateInternal(jsonStr); };
    auto toJsonStrFunc = [this]() { return this->toJsonStr(); };
    mInternal = std::make_shared<jsUiInternal>(name, std::move(updateFunc), std::move(toJsonStrFunc));
    jsUiManager::addComponent(mInternal);
}

jsCheckbox::~jsCheckbox() {
    jsUiManager::removeComponent(mInternal);
}

const char* jsCheckbox::name() const {
    return mInternal->name();
}

std::string jsCheckbox::toJsonStr() const {
    JsonDictEncoder encoder;
    encoder.addField("name", name());
    encoder.addField("type", "checkbox");
    encoder.addField("id", mInternal->id());
    encoder.addField("value", mValue);
    return encoder.str();
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
