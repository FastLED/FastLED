#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "fl/vector.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsDropdownImpl {
  public:

    // Constructor with fl::Slice<fl::string> for arrays and containers.
    jsDropdownImpl(const fl::string &name, fl::Slice<fl::string> options);

    // Constructor with initializer_list (FastLED requires C++11 support)
    jsDropdownImpl(const fl::string &name, fl::initializer_list<fl::string> options);
    
    ~jsDropdownImpl();
    jsDropdownImpl &Group(const fl::string &name) {
        mGroup = name;
        return *this;
    };

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    fl::string value() const;
    int value_int() const;
    void setSelectedIndex(int index);
    size_t getOptionCount() const { return mOptions.size(); }
    fl::string getOption(size_t index) const;
    const fl::string &groupName() const { return mGroup; }

    // Method to allow parent UIBase class to set the group
    void setGroup(const fl::string &groupName) { mGroup = groupName; }

    jsDropdownImpl &operator=(int index) {
        setSelectedIndex(index);
        return *this;
    }

  private:
    // Private constructor with array of options and count (used by template constructor)
    jsDropdownImpl(const fl::string &name, const fl::string* options, size_t count);
    
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);
    void commonInit(const fl::string &name);

    jsUiInternalPtr mInternal;
    fl::vector<fl::string> mOptions;
    size_t mSelectedIndex;
    fl::string mGroup;
};

} // namespace fl
