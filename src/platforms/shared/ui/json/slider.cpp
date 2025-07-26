#include "platforms/shared/ui/json/slider.h"
#include "fl/json.h"
#include "fl/json2.h"
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
        [this](const fl::Json &value) {
            static_cast<JsonSliderImpl *>(this)->updateInternal(value);
        });

    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this](fl::Json &json) {
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

void JsonSliderImpl::toJson(fl::Json &json) const {
    json.set("name", name());
    json.set("group", mInternal->groupName());
    json.set("type", "slider");
    json.set("id", mInternal->id());
    json.set("value", mValue);
    json.set("min", mMin);
    json.set("max", mMax);
    if (mStep > 0) {
        json.set("step", mStep);
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
    const fl::Json &value) {
    fl::string str = value.serialize();
    fl::json2::Json json2_obj = fl::json2::Json::parse(str.c_str());
    setValue(json2_obj | 0.0f);
}

} // namespace fl

#endif // __EMSCRIPTEN__
