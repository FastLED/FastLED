#ifdef __EMSCRIPTEN__

#include <memory>

#include "fl/json.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "platforms/wasm/js.h"
#include "platforms/wasm/ui/number_field.h"
#include "ui_internal.h"
#include "ui_manager.h"

using namespace fl;

namespace fl {

jsNumberFieldImpl::jsNumberFieldImpl(const Str &name, double *value)
    : mValue(value) {
    auto updateFunc = jsUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &value) {
            static_cast<jsNumberFieldImpl *>(this)->updateInternal(value);
        });

    auto toJsonFunc =
        jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<jsNumberFieldImpl *>(this)->toJson(json);
        });
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc),
                                     std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
}

jsNumberFieldImpl::~jsNumberFieldImpl() {
    jsUiManager::removeComponent(mInternal);
}

double jsNumberFieldImpl::value() const { return *mValue; }

void jsNumberFieldImpl::setValue(double value) { *mValue = value; }

const Str &jsNumberFieldImpl::name() const { return mInternal->name(); }

void jsNumberFieldImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["type"] = "number";
    json["id"] = mInternal->id();
    json["value"] = *mValue;
}

void jsNumberFieldImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    *mValue = value.as<double>();
}

} // namespace fl

#endif // __EMSCRIPTEN__
