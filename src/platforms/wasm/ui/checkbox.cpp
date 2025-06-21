#ifdef __EMSCRIPTEN__

#include "fl/json.h"
#include "fl/namespace.h"

#include "platforms/wasm/ui/checkbox.h"
#include "platforms/wasm/ui/ui_manager.h"
#include <string.h>

using namespace fl;

namespace fl {

jsCheckboxImpl::jsCheckboxImpl(const Str &name, bool *value) : mValue(value) {
    auto updateFunc = jsUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &value) {
            static_cast<jsCheckboxImpl *>(this)->updateInternal(value);
        });

    // mInternal = jsUiInternalPtr::New(name, std::move(updateFunc),
    //                                  std::move(toJsonFunc));
    auto toJsonFunc =
        jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<jsCheckboxImpl *>(this)->toJson(json);
        });
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc),
                                       std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
}

jsCheckboxImpl::~jsCheckboxImpl() {
    jsUiManager::removeComponent(mInternal);
}

const Str &jsCheckboxImpl::name() const { return mInternal->name(); }

void jsCheckboxImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["type"] = "checkbox";
    json["id"] = mInternal->id();
    json["value"] = *mValue;
}

void jsCheckboxImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    *mValue = value.as<bool>();
}

} // namespace fl

#endif // __EMSCRIPTEN__
