#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsCheckboxImpl {
  public:
    jsCheckboxImpl(const fl::string &, bool value);
    ~jsCheckboxImpl();
    jsCheckboxImpl &Group(const fl::string &name) {
        mGroup = name;
        return *this;
    };

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    bool value() const;
    void setValue(bool value);
    const fl::string &groupName() const { return mGroup; }
    
    // Method to allow parent UIBase class to set the group
    void setGroup(const fl::string &groupName) { mGroup = groupName; }

    jsCheckboxImpl &operator=(bool value) {
        setValue(value);
        return *this;
    }
    jsCheckboxImpl &operator=(int value) {
        setValue(value != 0);
        return *this;
    }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);

    jsUiInternalPtr mInternal;
    bool mValue;
    fl::string mGroup;
};

} // namespace fl
