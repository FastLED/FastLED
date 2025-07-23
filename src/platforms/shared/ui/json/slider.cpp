#include "platforms/shared/ui/json/slider.h"
#include "fl/json.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "platforms/shared/ui/json/ui.h"

#include "fl/compiler_control.h"

FL_DISABLE_WARNING(deprecated-declarations)

#if FASTLED_ENABLE_JSON

namespace fl {

JsonSliderImpl::JsonSliderImpl(const fl::string &name, float value, float min,
                               float max, float step)
    : mMin(min), mMax(max), mValue(value), mStep(step) {
    if (ALMOST_EQUAL_FLOAT(mStep, -1.f)) {
        mStep = (mMax - mMin) / 100.0f;
    }
    auto updateFunc = JsonUiInternal::UpdateFunction(
        [this](const fl::Json &json) {
            static_cast<JsonSliderImpl *>(this)->updateInternal(json);
        });

    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<JsonSliderImpl *>(this)->toJson(json);
        });
    mInternal = fl::make_shared<JsonUiInternal>(name, fl::move(updateFunc),
                                       fl::move(toJsonFunc));
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonSliderImpl::~JsonSliderImpl() { removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal)); }

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


void JsonSliderImpl::updateInternal(const fl::Json &json) {
    // Use ideal JSON API directly with flexible numeric access - handles both int and float
    auto maybeValue = json.get_flexible<float>();
    if (maybeValue.has_value()) {
        setValue(*maybeValue);
    } else {
        // Fallback: if it's not a number, log the type and keep current value
        FL_ASSERT(false, "*** SLIDER UPDATE ERROR: "
                    << name() << " " << json.type_str()
                    << " is not a numeric value.");
    }
}

} // namespace fl

#endif // __EMSCRIPTEN__
