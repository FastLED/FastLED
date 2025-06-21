
#if defined(__EMSCRIPTEN__) || defined(FASTLED_TESTING)

#include "platforms/wasm/ui/checkbox.h"
#include "fl/json.h"
#include "platforms/wasm/ui/ui_manager.h"
#include <string.h>

using namespace fl;

namespace fl {

jsCheckboxImpl::jsCheckboxImpl(const Str &name, bool value) : mValue(value) {
    auto updateFunc = jsUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &json) {
            static_cast<jsCheckboxImpl *>(this)->updateInternal(json);
        });
    auto toJsonFunc =
        jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<jsCheckboxImpl *>(this)->toJson(json);
        });
    // mInternal = jsUiInternalPtr::New(name, std::move(updateFunc),
    // std::move(toJsonFunc));
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc),
                                     std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
}

jsCheckboxImpl::~jsCheckboxImpl() { jsUiManager::removeComponent(mInternal); }

const Str &jsCheckboxImpl::name() const { return mInternal->name(); }

void jsCheckboxImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["group"] = mInternal->group().c_str();
    json["type"] = "checkbox";
    json["id"] = mInternal->id();
    json["value"] = mValue;
}

bool jsCheckboxImpl::value() const { return mValue; }

void jsCheckboxImpl::setValue(bool value) { mValue = value; }

void jsCheckboxImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    // We expect jsonStr to be a boolean value string, so parse it accordingly
    mValue = value.as<bool>();
}

void jsCheckboxImpl::setGroupIternal(const fl::string& name) {
    mInternal->setGroup(name);
}


} // namespace fl

#endif // __EMSCRIPTEN__
