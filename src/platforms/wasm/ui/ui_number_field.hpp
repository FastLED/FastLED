

#ifndef UI_NUMBER_FIELD_HPP
#define UI_NUMBER_FIELD_HPP

#include "namespace.h"
#include "json.h"
#include "ui_internal.h"
#include "platforms/wasm/js.h"
#include "ui_manager.h"
#include "math_macros.h"
#include <memory>

FASTLED_NAMESPACE_BEGIN


jsNumberField::jsNumberField(const char* name, double value, double min, double max)
    : mValue(value), mMin(min), mMax(max) {
    auto updateFunc = jsUiInternal::UpdateFunction(this, [](void* self, const ArduinoJson::JsonVariantConst& json) {
        static_cast<jsNumberField*>(self)->updateInternal(json);
    });
    auto toJsonFunc = jsUiInternal::ToJsonFunction(this, [](void* self, ArduinoJson::JsonObject& json) {
        static_cast<jsNumberField*>(self)->toJson(json);
    });
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc), std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
}

jsNumberField::~jsNumberField() {
    jsUiManager::removeComponent(mInternal);
}

const char* jsNumberField::name() const {
    return mInternal->name();
}

void jsNumberField::toJson(ArduinoJson::JsonObject& json) const {
    json["name"] = name();
    json["group"] = mGroup.c_str();
    json["type"] = "number";
    json["id"] = mInternal->id();
    json["value"] = mValue;
    json["min"] = mMin;
    json["max"] = mMax;
}

double jsNumberField::value() const {
    return mValue;
}

void jsNumberField::setValue(double value) {
    mValue = MAX(mMin, MIN(mMax, value));
}

void jsNumberField::updateInternal(const ArduinoJson::JsonVariantConst& value) {
    mValue = MAX(mMin, MIN(mMax, value.as<double>()));
}

jsNumberField::operator double() const {
    return value();
}

jsNumberField::operator int() const {
    return static_cast<int>(value());
}

FASTLED_NAMESPACE_END

#endif // UI_NUMBER_FIELD_HPP
