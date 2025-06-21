#include "fl/json.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "platforms/shared/ui/json/number_field.h"
#include "ui_internal.h"
#include "platforms/wasm/ui/ui_deps.h"

#include "fl/json.h"

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
    addUiComponent(mInternal);
}

JsonNumberFieldImpl::~JsonNumberFieldImpl() { removeUiComponent(mInternal); }

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
    mValue = value;
}

void JsonNumberFieldImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    if (value.is<double>()) {
        double newValue = value.as<double>();
        setValue(newValue);
    } else if (value.is<int>()) {
        int newValue = value.as<int>();
        setValue(static_cast<double>(newValue));
    }
}

} // namespace fl

#endif // __EMSCRIPTEN__
