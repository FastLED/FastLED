#ifdef __EMSCRIPTEN__

#include <sstream>
#include <string.h>

#include "platforms/wasm/js.h"
#include "ui_manager.h"
#include "fl/json.h"
#include "fl/namespace.h"

using namespace fl;

FASTLED_NAMESPACE_BEGIN



jsSlider::jsSlider(const Str& name, float value, float min, float max, float step)
    : mMin(min), mMax(max), mValue(value), mStep(step) {
    if (mStep == -1.f) {
        mStep = (mMax - mMin) / 100.0f;
    }
    auto updateFunc = jsUiInternal::UpdateFunction(this, [](void* self, const FLArduinoJson::JsonVariantConst& json) {
        static_cast<jsSlider*>(self)->updateInternal(json);
    });
    auto toJsonFunc = jsUiInternal::ToJsonFunction(this, [](void* self, FLArduinoJson::JsonObject& json) {
        static_cast<jsSlider*>(self)->toJson(json);
    });
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc), std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
}

jsSlider::~jsSlider() {
    jsUiManager::removeComponent(mInternal);
}

const Str& jsSlider::name() const {
    return mInternal->name();
}

void jsSlider::toJson(FLArduinoJson::JsonObject& json) const {
    json["name"] = name();
    json["type"] = "slider";
    json["group"] = mGroup.c_str();
    json["id"] = mInternal->id();
    json["min"] = mMin;
    json["max"] = mMax;
    json["value"] = mValue;
    json["step"] = mStep;
}

float jsSlider::value() const { 
    return mValue; 
}

void jsSlider::updateInternal(const FLArduinoJson::JsonVariantConst& value) {
    // We expect jsonStr to actually be a value string, so simply parse it.
    float v = value.as<float>();
    setValue(v);
}

void jsSlider::setValue(float value) {
    mValue = std::max(mMin, std::min(mMax, value));
    if (mValue != value) {
        // The value was outside the range so print out a warning that we
        // clamped.
        const Str& name = mInternal->name();
        int id = mInternal->id();
        printf(
            "Warning: UISlider %s with id %d value %f was clamped to range [%f, %f] -> %f\n",
            name.c_str(), id,
            value, mMin, mMax, mValue);
    }
}


jsSlider::operator float() const { 
    return mValue; 
}

jsSlider::operator uint8_t() const {
    return static_cast<uint8_t>(mValue);
}

jsSlider::operator uint16_t() const {
    return static_cast<uint8_t>(mValue);
}

jsSlider::operator int() const {
    return static_cast<int>(mValue);
}

FASTLED_NAMESPACE_END

#endif  // __EMSCRIPTEN__
