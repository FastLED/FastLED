#pragma once

#include <stdint.h>

#include "fl/engine_events.h"
#include "fl/str.h"
#include "fl/vector.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsDropdownImpl {
  public:
    jsDropdownImpl(const fl::string &name, int *value,
                   const fl::vector<fl::string> &options);
    ~jsDropdownImpl();
    jsDropdownImpl &Group(const fl::string &name) {
        mInternal->setGroup(name);
        return *this;
    }

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    int value() const;
    void setValue(int value);
    const fl::vector<fl::string> &options() const;
    void setOptions(const fl::vector<fl::string> &options);
    const fl::string &groupName() const { return mInternal->groupName(); }
    
    // Method to allow parent UIBase class to set the group
    void setGroupInternal(const fl::string &groupName) { mInternal->setGroup(groupName); }

    jsDropdownImpl &operator=(int value) {
        setValue(value);
        return *this;
    }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);

    jsUiInternalPtr mInternal;
    int *mValue;
    fl::vector<fl::string> mOptions;
};

} // namespace fl
