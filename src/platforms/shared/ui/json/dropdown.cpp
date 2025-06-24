#include "platforms/shared/ui/json/dropdown.h"
#include "fl/json.h"
#include "platforms/shared/ui/json/ui.h"
#include <string.h>
#include "fl/slice.h"

#if FASTLED_ENABLE_JSON

using namespace fl;

namespace fl {

// Common initialization function
void JsonDropdownImpl::commonInit(const fl::string &name) {
    auto updateFunc = JsonUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &value) {
            static_cast<JsonDropdownImpl *>(this)->updateInternal(value);
        });
    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<JsonDropdownImpl *>(this)->toJson(json);
        });
    mInternal = JsonUiInternalPtr::New(name, fl::move(updateFunc),
                                     fl::move(toJsonFunc));
    addJsonUiComponent(mInternal);
}

// Constructor with array of options and count
JsonDropdownImpl::JsonDropdownImpl(const fl::string &name, const fl::string* options, size_t count) 
    : mSelectedIndex(0) {
    for (size_t i = 0; i < count; ++i) {
        mOptions.push_back(options[i]);
    }
    commonInit(name);
}

// Constructor with fl::Slice<fl::string>
JsonDropdownImpl::JsonDropdownImpl(const fl::string &name, fl::Slice<fl::string> options) 
    : mSelectedIndex(0) {
    for (const auto &option : options) {
        mOptions.push_back(option);
    }
    commonInit(name);
}

// Constructor with initializer_list (only available if C++11 support exists)
JsonDropdownImpl::JsonDropdownImpl(const fl::string &name, fl::initializer_list<fl::string> options) 
    : mSelectedIndex(0) {
    for (const auto &option : options) {
        mOptions.push_back(option);
    }
    commonInit(name);
}

JsonDropdownImpl::~JsonDropdownImpl() { removeJsonUiComponent(mInternal); }

JsonDropdownImpl &JsonDropdownImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonDropdownImpl::name() const { return mInternal->name(); }

void JsonDropdownImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["group"] = mInternal->groupName().c_str();
    json["type"] = "dropdown";
    json["id"] = mInternal->id();
    json["value"] = static_cast<int>(mSelectedIndex);
    
    FLArduinoJson::JsonArray optionsArray = json["options"].to<FLArduinoJson::JsonArray>();
    for (const auto &option : mOptions) {
        optionsArray.add(option.c_str());
    }
}

fl::string JsonDropdownImpl::value() const {
    if (mSelectedIndex < mOptions.size()) {
        return mOptions[mSelectedIndex];
    }
    return fl::string();
}

int JsonDropdownImpl::value_int() const {
    return static_cast<int>(mSelectedIndex);
}

void JsonDropdownImpl::setSelectedIndex(int index) {
    if (index >= 0 && static_cast<size_t>(index) < mOptions.size()) {
        mSelectedIndex = static_cast<size_t>(index);
    }
}

size_t JsonDropdownImpl::getOptionCount() const { return mOptions.size(); }

fl::string JsonDropdownImpl::getOption(size_t index) const {
    if (index < mOptions.size()) {
        return mOptions[index];
    }
    return fl::string();
}

const fl::string &JsonDropdownImpl::groupName() const { return mInternal->groupName(); }

void JsonDropdownImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

JsonDropdownImpl &JsonDropdownImpl::operator=(int index) {
    setSelectedIndex(index);
    return *this;
}

void JsonDropdownImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    if (value.is<int>()) {
        int newIndex = value.as<int>();
        setSelectedIndex(newIndex);
    }
}

} // namespace fl

#endif // __EMSCRIPTEN__
