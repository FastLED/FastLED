#pragma once

#include <stdint.h>

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsCheckboxImpl {
  public:
    jsCheckboxImpl(const fl::string &name, bool *value);
    ~jsCheckboxImpl();
    jsCheckboxImpl &Group(const fl::string &name) {
        mInternal->setGroup(name);
        return *this;
    }

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    const fl::string &groupName() const { return mInternal->groupName(); }
    
    // Method to allow parent UIBase class to set the group
    void setGroupInternal(const fl::string &groupName) { mInternal->setGroup(groupName); }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);

    jsUiInternalPtr mInternal;
    bool *mValue;
};

} // namespace fl
