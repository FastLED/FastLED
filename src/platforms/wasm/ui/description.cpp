#ifdef __EMSCRIPTEN__

#include "fl/json.h"
#include "fl/namespace.h"

#include "platforms/wasm/ui/description.h"
#include "platforms/wasm/ui/ui_manager.h"

using namespace fl;

namespace fl {

jsDescriptionImpl::jsDescriptionImpl(const Str &name) {
    auto update_fcn = jsUiInternal::UpdateFunction([](const FLArduinoJson::JsonVariantConst &) {});
    auto to_json_fcn = jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
        static_cast<jsDescriptionImpl *>(this)->toJson(json);
    });
    mInternal = jsUiInternalPtr::New("description", update_fcn, to_json_fcn);
    jsUiManager::addComponent(mInternal);
}

jsDescriptionImpl::~jsDescriptionImpl() { jsUiManager::removeComponent(mInternal); }

const Str &jsDescriptionImpl::name() const { return mInternal->name(); }

void jsDescriptionImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["type"] = "description";
    json["id"] = mInternal->id();
}

} // namespace fl

#endif // __EMSCRIPTEN__
