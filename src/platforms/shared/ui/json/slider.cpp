#include "platforms/shared/ui/json/slider.h"
#include "fl/json.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "platforms/shared/ui/json/ui.h"

#include "fl/compiler_control.h"

FL_DISABLE_WARNING(deprecated-declarations)

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
    addJsonUiComponent(mInternal);
}

JsonSliderImpl::~JsonSliderImpl() { removeJsonUiComponent(mInternal); }

JsonSliderImpl &JsonSliderImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

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

float JsonSliderImpl::getMax() const { return mMax; }

float JsonSliderImpl::getMin() const { return mMin; }

void JsonSliderImpl::setValue(float value) {
    if (value < mMin) {
        value = mMin;
    } else if (value > mMax) {
        value = mMax;
    }
    mValue = value;
}

const fl::string &JsonSliderImpl::groupName() const {
    return mInternal->groupName();
}

void JsonSliderImpl::setGroup(const fl::string &groupName) {
    mInternal->setGroup(groupName);
}

int JsonSliderImpl::as_int() const { return static_cast<int>(mValue); }

JsonSliderImpl &JsonSliderImpl::operator=(float value) {
    setValue(value);
    return *this;
}

JsonSliderImpl &JsonSliderImpl::operator=(int value) {
    setValue(static_cast<float>(value));
    return *this;
}


void JsonSliderImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    if (value.is<float>()) {
        float newValue = value.as<float>();
        setValue(newValue);
    } else if (value.is<int>()) {
        int newValue = value.as<int>();
        setValue(static_cast<float>(newValue));
    } else {
        FL_ASSERT(false, "*** SLIDER UPDATE ERROR: "
                    << name() << " " << fl::getJsonTypeStr(value)
                    << " is not a float or int.");
    }
}

} // namespace fl

#endif // __EMSCRIPTEN__
