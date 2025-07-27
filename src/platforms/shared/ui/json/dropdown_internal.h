#pragma once

#include "ui_internal.h" // Ensure JsonUiInternal is included
#include "fl/string.h"
#include "fl/json.h"
#include "fl/vector.h"

namespace fl {

class JsonUiDropdownInternal : public JsonUiInternal {
private:
    fl::vector<fl::string> mOptions;
    size_t mSelectedIndex;

public:
    // Constructor: Initializes the base JsonUiInternal with name.
    JsonUiDropdownInternal(const fl::string &name, const fl::string* options, size_t count);
    JsonUiDropdownInternal(const fl::string &name, fl::span<fl::string> options)
        : JsonUiInternal(name), mOptions(options.begin(), options.end()), mSelectedIndex(0) {}

    // Override toJson to serialize the dropdown's data directly.
    void toJson(fl::Json& json) const override {
        json.set("name", name());
        json.set("group", groupName());
        json.set("type", "dropdown");
        json.set("id", id());
        json.set("value", static_cast<int>(mSelectedIndex));
        
        fl::Json optionsArray = fl::Json::array();
        for (const auto &option : mOptions) {
            optionsArray.push_back(option);
        }
        json.set("options", optionsArray);
    }

    // Override updateInternal to handle incoming JSON data for the dropdown.
    void updateInternal(const fl::Json& json) override {
        setSelectedIndex(json | 0);
    }

    // Accessors for dropdown state
    fl::string value() const {
        if (mSelectedIndex < mOptions.size()) {
            return mOptions[mSelectedIndex];
        }
        return fl::string();
    }

    int value_int() const {
        return static_cast<int>(mSelectedIndex);
    }

    void setSelectedIndex(int index) {
        if (index >= 0 && static_cast<size_t>(index) < mOptions.size()) {
            mSelectedIndex = static_cast<size_t>(index);
        }
    }

    size_t getOptionCount() const { return mOptions.size(); }

    fl::string getOption(size_t index) const {
        if (index < mOptions.size()) {
            return mOptions[index];
        }
        return fl::string();
    }
};

} // namespace fl
