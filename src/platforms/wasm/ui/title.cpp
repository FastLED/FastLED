#ifdef __EMSCRIPTEN__

#include "fl/json.h"
#include "fl/namespace.h"

#include "platforms/wasm/ui/title.h"
#include "platforms/wasm/ui/ui_manager.h"

using namespace fl;

namespace fl {

jsTitleImpl::jsTitleImpl(const Str &name) {
    auto update_fcn = jsUiInternal::UpdateFunction([](const FLArduinoJson::JsonVariantConst &) {});
    auto to_json_fcn = jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
        static_cast<jsTitleImpl *>(this)->toJson(json);
    });
    mInternal = jsUiInternalPtr::New("title", update_fcn, to_json_fcn);
    jsUiManager::addComponent(mInternal);
}

jsTitleImpl::~jsTitleImpl() { jsUiManager::removeComponent(mInternal); }

const Str &jsTitleImpl::name() const { return mInternal->name(); }

void jsTitleImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["type"] = "title";
    json["id"] = mInternal->id();
}

} // namespace fl

#endif // __EMSCRIPTEN__
