#include "fl/json.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "platforms/shared/ui/json/number_field.h"
#include "ui_internal.h"
#include "platforms/shared/ui/json/ui.h"

#if FASTLED_ENABLE_JSON

using namespace fl;

namespace fl {

JsonNumberFieldImpl::JsonNumberFieldImpl(const fl::string &name, double value,
                                        double min, double max)
    : mValue(value), mMin(min), mMax(max) {
    auto updateFunc = JsonUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &value) {
            static_cast<JsonNumberFieldImpl *>(this)->updateInternal(value);
        });

    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<JsonNumberFieldImpl *>(this)->toJson(json);
        });
    mInternal = JsonUiInternalPtr::New(name, fl::move(updateFunc),
                                     fl::move(toJsonFunc));
    addJsonUiComponent(mInternal);
}

JsonNumberFieldImpl::~JsonNumberFieldImpl() { removeJsonUiComponent(mInternal); }

JsonNumberFieldImpl &JsonNumberFieldImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonNumberFieldImpl::name() const { return mInternal->name(); }

void JsonNumberFieldImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["group"] = mInternal->groupName().c_str();
    json["type"] = "number";
    json["id"] = mInternal->id();
    json["value"] = mValue;
    json["min"] = mMin;
    json["max"] = mMax;
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
    const FLArduinoJson::JsonVariantConst &value) {
    if (value.is<double>()) {
        double newValue = value.as<double>();
        setValueInternal(newValue);  // Use internal method to avoid change notification
    } else if (value.is<int>()) {
        int newValue = value.as<int>();
        setValueInternal(static_cast<double>(newValue));  // Use internal method to avoid change notification
    }
}

} // namespace fl

#endif // __EMSCRIPTEN__
