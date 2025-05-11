
#ifdef __EMSCRIPTEN__

#include "fl/json.h"
#include "fl/namespace.h"

#include "platforms/wasm/ui/button.h"
#include "platforms/wasm/ui/ui_manager.h"

using namespace fl;

namespace fl {

jsButtonImpl::jsButtonImpl(const Str &name) : mPressed(false) {
    auto updateFunc = jsUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &value) {
            static_cast<jsButtonImpl *>(this)->updateInternal(value);
        });

    auto toJsonFunc =
        jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<jsButtonImpl *>(this)->toJson(json);
        });
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc),
                                     std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
    mUpdater.init(this);
}

jsButtonImpl::~jsButtonImpl() { jsUiManager::removeComponent(mInternal); }

bool jsButtonImpl::clicked() const { return mClickedHappened; }

const Str &jsButtonImpl::name() const { return mInternal->name(); }

void jsButtonImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["group"] = mGroup.c_str();
    json["type"] = "button";
    json["id"] = mInternal->id();
    json["pressed"] = mPressed;
}

bool jsButtonImpl::isPressed() const {
    // Due to ordering of operations, mPressedLast is always equal to
    // mPressed. So we kind of fudge a little on the isPressed() event
    // here;
    return mPressed || mClickedHappened;
}

void jsButtonImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    mPressed = value.as<bool>();
}

} // namespace fl

#endif // __EMSCRIPTEN__
