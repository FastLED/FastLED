// IWYU pragma: private

#include "platforms/shared/ui/json/dropdown.h"
#include "fl/stl/json.h"
#include "fl/stl/json.h"
#include "platforms/shared/ui/json/ui.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"
namespace fl {



// Definition of the internal class that was previously in dropdown_internal.h
class JsonUiDropdownInternal : public JsonUiInternal {
private:
    fl::vector<fl::string> mOptions;
    size_t mSelectedIndex;

public:
    // Constructor: Initializes the base JsonUiInternal with name and sets initial values.
    JsonUiDropdownInternal(const fl::string& name, const fl::vector<fl::string>& options, size_t selectedIndex = 0)
 FL_NOEXCEPT : JsonUiInternal(name), mOptions(options), mSelectedIndex(selectedIndex) {}

    // Override toJson to serialize the dropdown's data directly.
    void toJson(fl::json& json) const FL_NOEXCEPT override {
        json.set("name", name());
        json.set("type", "dropdown");
        json.set("group", groupName());
        json.set("id", id());
        json.set("value", static_cast<int>(mSelectedIndex));
        
        fl::json optionsArray = fl::json::array();
        for (const auto& option : mOptions) {
            optionsArray.push_back(option);
        }
        json.set("options", optionsArray);
    }

    // Override updateInternal to handle updates from JSON.
    void updateInternal(const fl::json& json) FL_NOEXCEPT override {
        int index = json | 0;
        if (index >= 0 && static_cast<size_t>(index) < mOptions.size()) {
            mSelectedIndex = static_cast<size_t>(index);
        }
    }

    // Accessors for the dropdown state.
    const fl::vector<fl::string>& options() const FL_NOEXCEPT { return mOptions; }
    size_t selectedIndex() const FL_NOEXCEPT { return mSelectedIndex; }
    void setSelectedIndex(size_t index) FL_NOEXCEPT { 
        if (index < mOptions.size()) {
            mSelectedIndex = index;
        }
    }
    
    fl::string value() const FL_NOEXCEPT {
        if (mSelectedIndex < mOptions.size()) {
            return mOptions[mSelectedIndex];
        }
        return fl::string();
    }
};

// Constructor with array of options and count
JsonDropdownImpl::JsonDropdownImpl(const fl::string &name, const fl::string* options, size_t count) FL_NOEXCEPT {
    // Convert options to fl::vector
    fl::vector<fl::string> optionsVector;
    for (size_t i = 0; i < count; ++i) {
        optionsVector.push_back(options[i]);
    }
    
    // Create an instance of the new internal class
    mInternal = fl::make_shared<JsonUiDropdownInternal>(name, optionsVector, 0);

    // Register the component with the JsonUiManager
    addJsonUiComponent(mInternal);
}

// Constructor with fl::span<fl::string>
JsonDropdownImpl::JsonDropdownImpl(const fl::string &name, fl::span<fl::string> options) FL_NOEXCEPT {
    // Convert options to fl::vector
    fl::vector<fl::string> optionsVector;
    for (const auto &option : options) {
        optionsVector.push_back(option);
    }
    
    // Create an instance of the new internal class
    mInternal = fl::make_shared<JsonUiDropdownInternal>(name, optionsVector, 0);

    // Register the component with the JsonUiManager
    addJsonUiComponent(mInternal);
}

// Constructor with initializer_list (only available if C++11 support exists)
JsonDropdownImpl::JsonDropdownImpl(const fl::string &name, fl::initializer_list<fl::string> options) FL_NOEXCEPT {
    // Convert options to fl::vector
    fl::vector<fl::string> optionsVector;
    for (const auto &option : options) {
        optionsVector.push_back(option);
    }
    
    // Create an instance of the new internal class
    mInternal = fl::make_shared<JsonUiDropdownInternal>(name, optionsVector, 0);

    // Register the component with the JsonUiManager
    addJsonUiComponent(mInternal);
}

JsonDropdownImpl::~JsonDropdownImpl() { 
    // Ensure the component is removed from the global registry
    removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonDropdownImpl &JsonDropdownImpl::Group(const fl::string &name) FL_NOEXCEPT {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonDropdownImpl::name() const FL_NOEXCEPT { return mInternal->name(); }

void JsonDropdownImpl::toJson(fl::json &json) const FL_NOEXCEPT {
    mInternal->toJson(json);
}

fl::string JsonDropdownImpl::value() const FL_NOEXCEPT {
    return mInternal->value();
}

int JsonDropdownImpl::value_int() const FL_NOEXCEPT {
    return static_cast<int>(mInternal->selectedIndex());
}

void JsonDropdownImpl::setSelectedIndex(int index) FL_NOEXCEPT {
    if (index >= 0) {
        mInternal->setSelectedIndex(static_cast<size_t>(index));
    }
}

size_t JsonDropdownImpl::getOptionCount() const FL_NOEXCEPT { 
    return mInternal->options().size(); 
}

fl::string JsonDropdownImpl::getOption(size_t index) const FL_NOEXCEPT {
    if (index < mInternal->options().size()) {
        return mInternal->options()[index];
    }
    return fl::string();
}

fl::string JsonDropdownImpl::groupName() const FL_NOEXCEPT { return mInternal->groupName(); }

void JsonDropdownImpl::setGroup(const fl::string &groupName) FL_NOEXCEPT { mInternal->setGroup(groupName); }

JsonDropdownImpl &JsonDropdownImpl::operator=(int index) FL_NOEXCEPT {
    setSelectedIndex(index);
    return *this;
}

int JsonDropdownImpl::id() const FL_NOEXCEPT {
    return mInternal->id();
}

} // namespace fl
