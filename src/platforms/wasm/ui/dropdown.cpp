#ifdef __EMSCRIPTEN__

#include "platforms/wasm/ui/dropdown.h"
#include "fl/json.h"
#include "platforms/wasm/ui/ui_manager.h"
#include <string.h>

using namespace fl;

namespace fl {

// Common initialization function
void jsDropdownImpl::commonInit(const Str &name) {
    auto updateFunc = jsUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &json) {
            static_cast<jsDropdownImpl *>(this)->updateInternal(json);
        });
    auto toJsonFunc =
        jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<jsDropdownImpl *>(this)->toJson(json);
        });
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc),
                                     std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
}

// Constructor with array of options and count
jsDropdownImpl::jsDropdownImpl(const Str &name, const fl::Str* options, size_t count) 
    : mSelectedIndex(0) {
    for (size_t i = 0; i < count; ++i) {
        mOptions.push_back(options[i]);
    }
    if (mOptions.empty()) {
        mOptions.push_back(fl::Str("No options"));
    }
    commonInit(name);
}

// Constructor with fl::vector of options
jsDropdownImpl::jsDropdownImpl(const Str &name, const fl::vector<fl::Str>& options) 
    : mSelectedIndex(0) {
    for (size_t i = 0; i < options.size(); ++i) {
        mOptions.push_back(options[i]);
    }
    if (mOptions.empty()) {
        mOptions.push_back(fl::Str("No options"));
    }
    commonInit(name);
}

#if FASTLED_HAS_INITIALIZER_LIST
// Constructor with initializer_list (only available if C++11 support exists)
jsDropdownImpl::jsDropdownImpl(const Str &name, fl::initializer_list<fl::Str> options) 
    : mSelectedIndex(0) {
    for (const auto& option : options) {
        mOptions.push_back(option);
    }
    if (mOptions.empty()) {
        mOptions.push_back(fl::Str("No options"));
    }
    commonInit(name);
}
#endif

jsDropdownImpl::~jsDropdownImpl() { jsUiManager::removeComponent(mInternal); }

const Str &jsDropdownImpl::name() const { return mInternal->name(); }

void jsDropdownImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["group"] = mGroup.c_str();
    json["type"] = "dropdown";
    json["id"] = mInternal->id();
    json["value"] = static_cast<int>(mSelectedIndex);
    
    FLArduinoJson::JsonArray options = json.createNestedArray("options");
    for (size_t i = 0; i < mOptions.size(); ++i) {
        options.add(mOptions[i].c_str());
    }
}

fl::Str jsDropdownImpl::value() const { 
    if (mSelectedIndex < mOptions.size()) {
        return mOptions[mSelectedIndex]; 
    }
    return fl::Str("Invalid");
}

int jsDropdownImpl::value_int() const { 
    return static_cast<int>(mSelectedIndex); 
}

void jsDropdownImpl::setSelectedIndex(int index) {
    if (index >= 0 && index < static_cast<int>(mOptions.size())) {
        mSelectedIndex = static_cast<size_t>(index);
    }
}

fl::Str jsDropdownImpl::getOption(size_t index) const {
    if (index < mOptions.size()) {
        return mOptions[index];
    }
    return fl::Str("Invalid");
}

void jsDropdownImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    // We expect value to be an integer representing the selected index
    int index = value.as<int>();
    setSelectedIndex(index);
}

} // namespace fl

#endif // __EMSCRIPTEN__
