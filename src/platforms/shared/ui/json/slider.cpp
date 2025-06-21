#include "fl/json.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "platforms/wasm/js.h"
#include "platforms/shared/ui/json/ui_deps.h"
#include "fl/math_macros.h"

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

using namespace fl;

namespace fl {

JsonSliderImpl::JsonSliderImpl(const fl::string &name, float value, float min,
                             float max, float step)
    : mMin(min), mMax(max), mValue(value), mStep(step) {
    if (ALMOST_EQUAL_FLOAT(mStep, -1.f)) {
        mStep = (mMax - mMin) / 100.0f;
    }
    auto updateFunc = JsonUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &value) {
            static_cast<JsonSliderImpl *>(this)->updateInternal(value);
        });

    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<JsonSliderImpl *>(this)->toJson(json);
        });
    mInternal = JsonUiInternalPtr::New(name, fl::move(updateFunc),
                                     fl::move(toJsonFunc));
    addUiComponent(mInternal);
}

JsonSliderImpl::~JsonSliderImpl() { removeUiComponent(mInternal); }

const fl::string &JsonSliderImpl::name() const { return mInternal->name(); }

void JsonSliderImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["group"] = mInternal->groupName().c_str();
    json["type"] = "slider";
    json["id"] = mInternal->id();
    json["value"] = mValue;
    json["min"] = mMin;
    json["max"] = mMax;
    if (mStep > 0) {
        json["step"] = mStep;
    }
}

float JsonSliderImpl::value() const { return mValue; }

float JsonSliderImpl::value_normalized() const {
    if (ALMOST_EQUAL(mMax, mMin, 0.0001f)) {
        return 0;
    }
    return (mValue - mMin) / (mMax - mMin);
}

void JsonSliderImpl::setValue(float value) {
    if (value < mMin) {
        value = mMin;
    } else if (value > mMax) {
        value = mMax;
    }
    mValue = value;
}

void JsonSliderImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    if (value.is<float>()) {
        float newValue = value.as<float>();
        setValue(newValue);
    } else if (value.is<int>()) {
        int newValue = value.as<int>();
        setValue(static_cast<float>(newValue));
    }
}

} // namespace fl

#endif // __EMSCRIPTEN__
