
#ifdef __EMSCRIPTEN__

#include <memory>

#include "fl/namespace.h"
#include "fl/json.h"
#include "ui_internal.h"
#include "platforms/wasm/js.h"
#include "ui_manager.h"
#include "fl/math_macros.h"

using namespace fl;

FASTLED_NAMESPACE_BEGIN


jsNumberField::jsNumberField(const Str& name, double value, double min, double max)
    : mValue(value), mMin(min), mMax(max) {
    auto updateFunc = jsUiInternal::UpdateFunction(this, [](void* self, const FLArduinoJson::JsonVariantConst& json) {
        static_cast<jsNumberField*>(self)->updateInternal(json);
    });
    auto toJsonFunc = jsUiInternal::ToJsonFunction(this, [](void* self, FLArduinoJson::JsonObject& json) {
        static_cast<jsNumberField*>(self)->toJson(json);
    });
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc), std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
}

jsNumberField::~jsNumberField() {
    jsUiManager::removeComponent(mInternal);
}

const Str& jsNumberField::name() const {
    return mInternal->name();
}

void jsNumberField::toJson(FLArduinoJson::JsonObject& json) const {
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

void jsNumberField::updateInternal(const FLArduinoJson::JsonVariantConst& value) {
    mValue = MAX(mMin, MIN(mMax, value.as<double>()));
}

jsNumberField::operator double() const {
    return value();
}

jsNumberField::operator int() const {
    return static_cast<int>(value());
}

FASTLED_NAMESPACE_END

#endif // __EMSCRIPTEN__
