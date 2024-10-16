#include "json.h"
#include "platforms/stub/wasm/js.h"
#include "ui_manager.h"

FASTLED_NAMESPACE_BEGIN

jsButton::jsButton(const char* name)
    : mPressed(false) {
    auto updateFunc = [this](const char* jsonStr) { this->updateInternal(jsonStr); };
    auto toJsonStrFunc = [this]() { return this->toJsonStr(); };
    mInternal = std::make_shared<jsUiInternal>(name, std::move(updateFunc), std::move(toJsonStrFunc));
    jsUiManager::addComponent(mInternal);
}

jsButton::~jsButton() {
    jsUiManager::removeComponent(mInternal);
}

const char* jsButton::name() const {
    return mInternal->name();
}

std::string jsButton::toJsonStr() const {
    JsonDictEncoder encoder;
    char nameBuf[256];
    snprintf(nameBuf, sizeof(nameBuf), "Button: %s", name());
    encoder.addField("name", nameBuf);
    encoder.addField("type", "button");
    encoder.addField("id", mInternal->id());
    encoder.addField("pressed", mPressed);
    return encoder.str();
}

bool jsButton::isPressed() const {
    return mPressed;
}

void jsButton::setPressed(bool pressed) {
    mPressed = pressed;
}

void jsButton::updateInternal(const char* jsonStr) {
    mPressed = (strcmp(jsonStr, "true") == 0);
}

jsButton::operator bool() const {
    return mPressed;
}

FASTLED_NAMESPACE_END
