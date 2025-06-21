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

jsSliderImpl::jsSliderImpl(const Str &name, double *value, double min,
                           double max)
    : mValue(value), mMin(min), mMax(max) {
    auto updateFunc = jsUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &value) {
            static_cast<jsSliderImpl *>(this)->updateInternal(value);
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

double jsSliderImpl::value() const { return *mValue; }

void jsSliderImpl::setValue(double value) { *mValue = value; }

double jsSliderImpl::min() const { return mMin; }

double jsSliderImpl::max() const { return mMax; }

const Str &jsSliderImpl::name() const { return mInternal->name(); }

void jsSliderImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["type"] = "slider";
    json["id"] = mInternal->id();
    json["value"] = *mValue;
    json["min"] = mMin;
    json["max"] = mMax;
}

void jsSliderImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    *mValue = value.as<double>();
}

} // namespace fl

#endif // __EMSCRIPTEN__
