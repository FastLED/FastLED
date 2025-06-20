#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "fl/vector.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsDropdownImpl {
  public:
    // Constructor with array of options and count
    jsDropdownImpl(const fl::Str &name, const fl::Str* options, size_t count);
    
    // Constructor with fl::vector of options
    jsDropdownImpl(const fl::Str &name, const fl::vector<fl::Str>& options);

#if FASTLED_HAS_INITIALIZER_LIST
    // Constructor with initializer_list (only available if C++11 support exists)
    jsDropdownImpl(const fl::Str &name, std::initializer_list<fl::Str> options);
#endif
    
    ~jsDropdownImpl();
    jsDropdownImpl &Group(const fl::Str &name) {
        mGroup = name;
        return *this;
    };

    const fl::Str &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    fl::Str value() const;
    int value_int() const;
    void setSelectedIndex(int index);
    size_t getOptionCount() const { return mOptions.size(); }
    fl::Str getOption(size_t index) const;
    const fl::Str &groupName() const { return mGroup; }

    jsDropdownImpl &operator=(int index) {
        setSelectedIndex(index);
        return *this;
    }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);
    void commonInit(const fl::Str &name);

    jsUiInternalPtr mInternal;
    fl::vector<fl::Str> mOptions;
    size_t mSelectedIndex;
    fl::Str mGroup;
};

} // namespace fl