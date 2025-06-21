#if defined(__EMSCRIPTEN__) || defined(FASTLED_TESTING)

#include <memory>

#include "fl/json.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "platforms/wasm/js.h"
#include "platforms/wasm/ui/number_field.h"
#include "ui_internal.h"
#include "platforms/wasm/ui/ui_deps.h"

using namespace fl;

namespace fl {

jsNumberFieldImpl::jsNumberFieldImpl(const Str &name, double value, double min,
                                     double max)
    : mValue(value), mMin(min), mMax(max) {
    auto updateFunc = jsUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &json) {
            static_cast<jsNumberFieldImpl *>(this)->updateInternal(json);
        });
    auto toJsonFunc =
        jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<jsNumberFieldImpl *>(this)->toJson(json);
        });
    mInternal = jsUiInternalPtr::New(name, fl::move(updateFunc),
                                     fl::move(toJsonFunc));
    addUiComponent(mInternal);
}

jsNumberFieldImpl::~jsNumberFieldImpl() {
    removeUiComponent(mInternal);
}

const Str &jsNumberFieldImpl::name() const { return mInternal->name(); }

void jsNumberFieldImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["group"] = mInternal->groupName().c_str();
    json["type"] = "number";
    json["id"] = mInternal->id();
    json["value"] = mValue;
    json["min"] = mMin;
    json["max"] = mMax;
}

double jsNumberFieldImpl::value() const { return mValue; }

void jsNumberFieldImpl::setValue(double value) {
    mValue = MAX(mMin, MIN(mMax, value));
}

void jsNumberFieldImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    mValue = MAX(mMin, MIN(mMax, value.as<double>()));
}

} // namespace fl

#endif // __EMSCRIPTEN__
