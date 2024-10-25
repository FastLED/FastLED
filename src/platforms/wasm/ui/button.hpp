#include "json.h"
#include "platforms/wasm/js.h"
#include "ui_manager.h"

FASTLED_NAMESPACE_BEGIN

jsButton::jsButton(const char* name)
    : mPressed(false) {
    auto updateFunc = jsUiInternal::UpdateFunction(this, [](void* self, const ArduinoJson::JsonVariantConst& value) {
        static_cast<jsButton*>(self)->updateInternal(value);
    });

    auto toJsonFunc = jsUiInternal::ToJsonFunction(this, [](void* self, ArduinoJson::JsonObject& json) {
        static_cast<jsButton*>(self)->toJson(json);
    });
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc), std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
    mUpdater.init(this);
}

jsButton::~jsButton() {
    jsUiManager::removeComponent(mInternal);
}

const char* jsButton::name() const {
    return mInternal->name();
}

void jsButton::toJson(ArduinoJson::JsonObject& json) const {
    json["name"] = name();
    json["group"] = mGroup.c_str();
    json["type"] = "button";
    json["id"] = mInternal->id();
    json["pressed"] = mPressed;
}

bool jsButton::isPressed() const {
    // Due to ordering of operations, mPressedLast is always equal to
    // mPressed. So we kind of fudge a little on the isPressed() event
    // here;
    return mPressed || mClickedHappened;
}

void jsButton::updateInternal(const ArduinoJson::JsonVariantConst& value) {
    mPressed = value.as<bool>();
}

FASTLED_NAMESPACE_END
