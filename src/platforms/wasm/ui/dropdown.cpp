#ifdef __EMSCRIPTEN__

#include "fl/json.h"
#include "fl/namespace.h"

#include "platforms/wasm/ui/dropdown.h"
#include "platforms/wasm/ui/ui_manager.h"

using namespace fl;

namespace fl {

jsDropdownImpl::jsDropdownImpl(const Str &name, int *value,
                               const fl::vector<fl::string> &options)
    : mValue(value), mOptions(options) {
    auto updateFunc = jsUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &value) {
            static_cast<jsDropdownImpl *>(this)->updateInternal(value);
        });

    auto toJsonFunc =
        jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<jsDropdownImpl *>(this)->toJson(json);
        });
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc),
                                     std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
}

jsDropdownImpl::~jsDropdownImpl() {
    jsUiManager::removeComponent(mInternal);
}

int jsDropdownImpl::value() const { return *mValue; }

void jsDropdownImpl::setValue(int value) { *mValue = value; }

const fl::vector<fl::string> &jsDropdownImpl::options() const {
    return mOptions;
}

void jsDropdownImpl::setOptions(const fl::vector<fl::string> &options) {
    mOptions = options;
}

const Str &jsDropdownImpl::name() const { return mInternal->name(); }

void jsDropdownImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["type"] = "dropdown";
    json["id"] = mInternal->id();
    json["value"] = *mValue;

    FLArduinoJson::JsonArray optionsArray = json.createNestedArray("options");
    for (const auto &option : mOptions) {
        optionsArray.add(option.c_str());
    }
}

void jsDropdownImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    *mValue = value.as<int>();
}

} // namespace fl

#endif // __EMSCRIPTEN__
