
#ifdef __EMSCRIPTEN__

#include <string>

#include "fl/json.h"
#include "fl/namespace.h"

#include "platforms/wasm/ui/ui_manager.h"
#include "platforms/wasm/ui/audio.h"
#include "fl/warn.h"

using namespace fl;

namespace fl {

jsAudioImpl::jsAudioImpl(const Str& name) {
    auto updateFunc = jsUiInternal::UpdateFunction([this](const FLArduinoJson::JsonVariantConst& value) {
        static_cast<jsAudioImpl*>(this)->updateInternal(value);
    });

    auto toJsonFunc = jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject& json) {
        static_cast<jsAudioImpl*>(this)->toJson(json);
    });
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc), std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
    mUpdater.init(this);
}

jsAudioImpl::~jsAudioImpl() {
    jsUiManager::removeComponent(mInternal);
}

const Str& jsAudioImpl::name() const {
    return mInternal->name();
}

void jsAudioImpl::toJson(FLArduinoJson::JsonObject& json) const {
    json["name"] = name();
    json["group"] = mGroup.c_str();
    json["type"] = "audio";
    json["id"] = mInternal->id();
}


void jsAudioImpl::updateInternal(const FLArduinoJson::JsonVariantConst& value) {
    FASTLED_WARN("Unimplemented jsAudioImpl::updateInternal");
}


}  // namespace fl

#endif  // __EMSCRIPTEN__
