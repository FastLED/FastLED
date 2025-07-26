#include "platforms/shared/ui/json/dropdown.h"
#include "fl/json2.h"
#include "fl/json2.h"
#include "platforms/shared/ui/json/ui.h"
#include <string.h>
#include "fl/span.h"

#if FASTLED_ENABLE_JSON



namespace fl {

// Common initialization function
void JsonDropdownImpl::commonInit(const fl::string &name) {
    auto updateFunc = JsonUiInternal::UpdateFunction(
        [this](const fl::json2::Json &value) {
            this->updateInternal(value);
        });
    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this](fl::json2::Json &json) {
            this->toJson(json);
        });
    mInternal = fl::make_shared<JsonUiInternal>(name, fl::move(updateFunc),
                                     fl::move(toJsonFunc));
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

// Constructor with array of options and count
JsonDropdownImpl::JsonDropdownImpl(const fl::string &name, const fl::string* options, size_t count) 
    : mSelectedIndex(0) {
    for (size_t i = 0; i < count; ++i) {
        mOptions.push_back(options[i]);
    }
    commonInit(name);
}

// Constructor with fl::span<fl::string>
JsonDropdownImpl::JsonDropdownImpl(const fl::string &name, fl::span<fl::string> options) 
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

JsonDropdownImpl::~JsonDropdownImpl() { removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal)); }

JsonDropdownImpl &JsonDropdownImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonDropdownImpl::name() const { return mInternal->name(); }

void JsonDropdownImpl::toJson(fl::json2::Json &json) const {
    json.set("name", name());
    json.set("group", mInternal->groupName());
    json.set("type", "dropdown");
    json.set("id", mInternal->id());
    json.set("value", static_cast<int>(mSelectedIndex));
    
    fl::json2::Json optionsArray = fl::json2::Json::array();
    for (const auto &option : mOptions) {
        optionsArray.push_back(option);
    }
    json.set("options", optionsArray);
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
    const fl::json2::Json &value) {
    setSelectedIndex(value | 0);
}

} // namespace fl

#endif // __EMSCRIPTEN__
