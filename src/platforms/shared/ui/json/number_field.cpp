#include "fl/json.h"
#include "fl/json2.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "platforms/shared/ui/json/number_field.h"
#include "ui_internal.h"
#include "platforms/shared/ui/json/ui.h"

#if FASTLED_ENABLE_JSON


namespace fl {

JsonNumberFieldImpl::JsonNumberFieldImpl(const fl::string &name, double value,
                                        double min, double max)
    : mValue(value), mMin(min), mMax(max) {
    auto updateFunc = JsonUiInternal::UpdateFunction(
        [this](const fl::json2::Json &value) {
            this->updateInternal(value);
        });

    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this](fl::json2::Json &json) {
            this->toJson(json);
        });
    mInternal = fl::make_shared<JsonUiInternal>(name, fl::move(updateFunc),
                                     fl::move(toJsonFunc));
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonNumberFieldImpl::~JsonNumberFieldImpl() { removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal)); }

JsonNumberFieldImpl &JsonNumberFieldImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonNumberFieldImpl::name() const { return mInternal->name(); }

void JsonNumberFieldImpl::toJson(fl::json2::Json &json) const {
    json.set("name", name());
    json.set("group", mInternal->groupName());
    json.set("type", "number");
    json.set("id", mInternal->id());
    json.set("value", static_cast<float>(mValue));
    json.set("min", static_cast<float>(mMin));
    json.set("max", static_cast<float>(mMax));
}

double JsonNumberFieldImpl::value() const { return mValue; }

void JsonNumberFieldImpl::setValue(double value) {
    if (value < mMin) {
        value = mMin;
    } else if (value > mMax) {
        value = mMax;
    }
    double oldValue = mValue;
    mValue = value;
    
    // If value actually changed, mark this component as changed for polling
    if (!ALMOST_EQUAL_FLOAT(mValue, oldValue)) {
        mInternal->markChanged();
    }
}

void JsonNumberFieldImpl::setValueInternal(double value) {
    // Internal method for updates from JSON UI system - no change notification
    if (value < mMin) {
        value = mMin;
    } else if (value > mMax) {
        value = mMax;
    }
    mValue = value;
}

const fl::string &JsonNumberFieldImpl::groupName() const { return mInternal->groupName(); }

void JsonNumberFieldImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

JsonNumberFieldImpl &JsonNumberFieldImpl::operator=(double value) {
    setValue(value);
    return *this;
}

JsonNumberFieldImpl &JsonNumberFieldImpl::operator=(int value) {
    setValue(static_cast<double>(value));
    return *this;
}

bool JsonNumberFieldImpl::operator==(double v) const { return ALMOST_EQUAL_FLOAT(value(), v); }

bool JsonNumberFieldImpl::operator==(int v) const { return ALMOST_EQUAL_FLOAT(value(), static_cast<double>(v)); }

bool JsonNumberFieldImpl::operator!=(double v) const { return !ALMOST_EQUAL_FLOAT(value(), v); }

bool JsonNumberFieldImpl::operator!=(int v) const { return !ALMOST_EQUAL_FLOAT(value(), static_cast<double>(v)); }

void JsonNumberFieldImpl::updateInternal(
    const fl::json2::Json &value) {
    setValueInternal(value | 0.0);  // Use internal method to avoid change notification
}

} // namespace fl

#endif // __EMSCRIPTEN__