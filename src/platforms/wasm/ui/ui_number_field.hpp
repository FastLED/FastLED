

#pragma once

#include "namespace.h"
#include "third_party/arduinojson/json.h"
#include "ui_internal.h"
#include "platforms/wasm/js.h"

#include <memory>
#include <algorithm>


FASTLED_NAMESPACE_BEGIN



inline jsNumberField::jsNumberField(const char* name, double value, double min, double max, double step)
    : mValue(std::clamp(value, min, max)), mMin(min), mMax(max), mStep(step) {
    auto updateFunc = jsUiInternal::UpdateFunction(this, [](void* self, const ArduinoJson::JsonVariantConst& json) {
        static_cast<jsNumberField*>(self)->updateInternal(json);
    });
    auto toJsonFunc = jsUiInternal::ToJsonFunction(this, [](void* self, ArduinoJson::JsonObject& json) {
        static_cast<jsNumberField*>(self)->toJson(json);
    });
    mInternal = std::make_shared<jsUiInternal>(name, std::move(updateFunc), std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
}

inline jsNumberField::~jsNumberField() {
    jsUiManager::removeComponent(mInternal);
}

inline const char* jsNumberField::name() const {
    return mInternal->name();
}

inline void jsNumberField::toJson(ArduinoJson::JsonObject& json) const {
    json["name"] = name();
    json["type"] = "number";
    json["id"] = mInternal->id();
    json["value"] = mValue;
    json["min"] = mMin;
    json["max"] = mMax;
    json["step"] = mStep;
}

inline double jsNumberField::value() const {
    return mValue;
}

inline void jsNumberField::setValue(double value) {
    mValue = std::clamp(value, mMin, mMax);
}

inline void jsNumberField::updateInternal(const ArduinoJson::JsonVariantConst& value) {
    mValue = std::clamp(value.as<double>(), mMin, mMax);
}

inline jsNumberField::operator double() const {
    return mValue;
}

inline jsNumberField::operator int() const {
    return static_cast<int>(mValue);
}

FASTLED_NAMESPACE_END
