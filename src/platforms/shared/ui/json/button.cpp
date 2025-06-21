#include "fl/json.h"
#include "fl/namespace.h"

#include "platforms/shared/ui/json/button.h"
#include "platforms/shared/ui/json/ui.h"

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

using namespace fl;

namespace fl {

JsonButtonImpl::JsonButtonImpl(const string &name) : mPressed(false) {
    auto updateFunc = JsonUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &value) {
            static_cast<JsonButtonImpl *>(this)->updateInternal(value);
        });

    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<JsonButtonImpl *>(this)->toJson(json);
        });
    mInternal = JsonUiInternalPtr::New(name, fl::move(updateFunc),
                                     fl::move(toJsonFunc));
    addJsonUiComponent(mInternal);
    mUpdater.init(this);
}

JsonButtonImpl::~JsonButtonImpl() { removeJsonUiComponent(mInternal); }

bool JsonButtonImpl::clicked() const { return mClickedHappened; }

const string &JsonButtonImpl::name() const { return mInternal->name(); }

void JsonButtonImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["group"] = mInternal->groupName().c_str();
    json["type"] = "button";
    json["id"] = mInternal->id();
    json["pressed"] = mPressed;
}

bool JsonButtonImpl::isPressed() const {
    return mPressed;
}

void JsonButtonImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    if (value.is<bool>()) {
        bool newPressed = value.as<bool>();
        mPressed = newPressed;
    }
}

} // namespace fl

#endif // __EMSCRIPTEN__
