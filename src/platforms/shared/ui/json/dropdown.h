#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "fl/vector.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/json.h"

namespace fl {

class JsonDropdownImpl {
  public:

    // Constructor with fl::Slice<fl::string> for arrays and containers.
    JsonDropdownImpl(const fl::string &name, fl::Slice<fl::string> options);

    // Constructor with initializer_list (FastLED requires C++11 support)
    JsonDropdownImpl(const fl::string &name, fl::initializer_list<fl::string> options);
    
    template<typename Iterator>
    JsonDropdownImpl(const fl::string &name, Iterator begin, Iterator end)
        : mSelectedIndex(0) {
        for (Iterator it = begin; it != end; ++it) {
            mOptions.push_back(*it);
        }
        commonInit(name);
    }
    
    ~JsonDropdownImpl();
    JsonDropdownImpl &Group(const fl::string &name);

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    fl::string value() const;
    int value_int() const;
    void setSelectedIndex(int index);
    size_t getOptionCount() const;
    fl::string getOption(size_t index) const;
    const fl::string &groupName() const;

    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

    JsonDropdownImpl &operator=(int index);

  private:
    // Private constructor with array of options and count (used by template constructor)
    JsonDropdownImpl(const fl::string &name, const fl::string* options, size_t count);
    
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);
    void commonInit(const fl::string &name);

    JsonUiInternalPtr mInternal;
    fl::vector<fl::string> mOptions;
    size_t mSelectedIndex;
};

} // namespace fl
