#ifdef __EMSCRIPTEN__

#include <sstream>
#include <string.h>

#include "fl/json.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "platforms/wasm/js.h"
#include "ui_manager.h"

using namespace fl;

namespace fl {

jsSliderImpl::jsSliderImpl(const Str &name, float value, float min, float max,
                           float step)
    : mMin(min), mMax(max), mValue(value), mStep(step) {
    if (mStep == -1.f) {
        mStep = (mMax - mMin) / 100.0f;
    }
    auto updateFunc = jsUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &json) {
            static_cast<jsSliderImpl *>(this)->updateInternal(json);
        });
    auto toJsonFunc =
        jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<jsSliderImpl *>(this)->toJson(json);
        });
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc),
                                     std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
}

jsSliderImpl::~jsSliderImpl() { jsUiManager::removeComponent(mInternal); }

const Str &jsSliderImpl::name() const { return mInternal->name(); }

void jsSliderImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["type"] = "slider";
    json["group"] = mGroup.c_str();
    json["id"] = mInternal->id();
    json["min"] = mMin;
    json["max"] = mMax;
    json["value"] = mValue;
    json["step"] = mStep;
}

float jsSliderImpl::value() const { return mValue; }

float jsSliderImpl::value_normalized() const {
    if (ALMOST_EQUAL(mMax, mMin, 0.0001f)) {
        return 0;
    }
    return (mValue - mMin) / (mMax - mMin);
}

void jsSliderImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    // We expect jsonStr to actually be a value string, so simply parse it.
    float v = value.as<float>();
    setValue(v);
}

void jsSliderImpl::setValue(float value) {
    mValue = std::max(mMin, std::min(mMax, value));
    if (mValue != value) {
        // The value was outside the range so print out a warning that we
        // clamped.
        const Str &name = mInternal->name();
        int id = mInternal->id();
        printf("Warning: UISlider %s with id %d value %f was clamped to range "
               "[%f, %f] -> %f\n",
               name.c_str(), id, value, mMin, mMax, mValue);
    }
}

} // namespace fl

#endif // __EMSCRIPTEN__
