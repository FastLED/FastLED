#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/real/ui_internal.h"

namespace fl {

class JsonCheckboxImpl {
  public:
    JsonCheckboxImpl(const fl::string &, bool value);
    ~JsonCheckboxImpl();
    JsonCheckboxImpl &Group(const fl::string &name) {
        mInternal->setGroup(name);
        return *this;
    };

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    bool value() const;
    void setValue(bool value);
    const fl::string &groupName() const { return mInternal->groupName(); }
    
    // Method to allow parent UIBase class to set the group
    void setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

    JsonCheckboxImpl &operator=(bool value) {
        setValue(value);
        return *this;
    }
    JsonCheckboxImpl &operator=(int value) {
        setValue(value != 0);
        return *this;
    }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);

    JsonUiInternalPtr mInternal;
    bool mValue;
};

} // namespace fl
