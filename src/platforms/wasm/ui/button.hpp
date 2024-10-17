#include "json.h"
#include "platforms/wasm/js.h"
#include "ui_manager.h"

FASTLED_NAMESPACE_BEGIN

jsButton::jsButton(const char* name)
    : mPressed(false) {
    auto updateFunc = [this](const char* jsonStr) { this->updateInternal(jsonStr); };
    auto toJsonStrFunc = [this]() { return this->toJsonStr(); };
    mInternal = std::make_shared<jsUiInternal>(name, std::move(updateFunc), std::move(toJsonStrFunc));
    jsUiManager::addComponent(mInternal);
    mUpdater.init(this);
}

jsButton::~jsButton() {
    jsUiManager::removeComponent(mInternal);
}

const char* jsButton::name() const {
    return mInternal->name();
}

std::string jsButton::toJsonStr() const {
    ArduinoJson::DynamicJsonDocument doc(256);
    ArduinoJson::JsonObject json = doc.to<ArduinoJson::JsonObject>();
    json["name"] = name();
    json["type"] = "button";
    json["id"] = mInternal->id();
    json["pressed"] = mPressed;
    std::string output;
    serializeJson(doc, output);
    return output;
}

bool jsButton::isPressed() const {
    // Due to ordering of operations, mPressedLast is always equal to
    // mPressed. So we kind of fudge a little on the isPressed() event
    // here;
    return mPressed || mClickedHappened;
}


void jsButton::updateInternal(const char* jsonStr) {
    mPressed = (strcmp(jsonStr, "true") == 0);
}

FASTLED_NAMESPACE_END
