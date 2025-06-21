#if defined(__EMSCRIPTEN__) || defined(FASTLED_TESTING)

#include "platforms/wasm/ui/dropdown.h"
#include "fl/json.h"
#include "platforms/wasm/ui/ui_deps.h"
#include <string.h>
#include "fl/slice.h"

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
    mInternal = jsUiInternalPtr::New(name, fl::move(updateFunc),
                                     fl::move(toJsonFunc));
    addUiComponent(mInternal);
}

// Constructor with array of options and count
jsDropdownImpl::jsDropdownImpl(const Str &name, const fl::string* options, size_t count) 
    : mSelectedIndex(0) {
    for (size_t i = 0; i < count; ++i) {
        mOptions.push_back(options[i]);
    }
    if (mOptions.empty()) {
        mOptions.push_back(fl::string("No options"));
    }
    commonInit(name);
}

// Constructor with fl::Slice<fl::string>
jsDropdownImpl::jsDropdownImpl(const Str &name, fl::Slice<fl::string> options) 
    : mSelectedIndex(0) {
    for (size_t i = 0; i < options.size(); ++i) {
        mOptions.push_back(options[i]);
    }
    if (mOptions.empty()) {
        mOptions.push_back(fl::string("No options"));
    }
    commonInit(name);
}

// Constructor with initializer_list (only available if C++11 support exists)
jsDropdownImpl::jsDropdownImpl(const Str &name, fl::initializer_list<fl::string> options) 
    : mSelectedIndex(0) {
    for (const auto& option : options) {
        mOptions.push_back(option);
    }
    if (mOptions.empty()) {
        mOptions.push_back(fl::string("No options"));
    }
    commonInit(name);
}

jsDropdownImpl::~jsDropdownImpl() { removeUiComponent(mInternal); }

const Str &jsDropdownImpl::name() const { return mInternal->name(); }

void jsDropdownImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["group"] = mGroup.c_str();
    json["type"] = "dropdown";
    json["id"] = mInternal->id();
    json["value"] = static_cast<int>(mSelectedIndex);
    
    FLArduinoJson::JsonArray options = json["options"].to<FLArduinoJson::JsonArray>();
    for (size_t i = 0; i < mOptions.size(); ++i) {
        options.add(mOptions[i].c_str());
    }
}

fl::string jsDropdownImpl::value() const { 
    if (mSelectedIndex < mOptions.size()) {
        return mOptions[mSelectedIndex]; 
    }
    return fl::string("Invalid");
}

int jsDropdownImpl::value_int() const { 
    return static_cast<int>(mSelectedIndex); 
}

void jsDropdownImpl::setSelectedIndex(int index) {
    if (index >= 0 && index < static_cast<int>(mOptions.size())) {
        mSelectedIndex = static_cast<size_t>(index);
    }
}

fl::string jsDropdownImpl::getOption(size_t index) const {
    if (index < mOptions.size()) {
        return mOptions[index];
    }
    return fl::string("Invalid");
}

void jsDropdownImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    // We expect value to be an integer representing the selected index
    int index = value.as<int>();
    setSelectedIndex(index);
}

} // namespace fl

#endif // __EMSCRIPTEN__
