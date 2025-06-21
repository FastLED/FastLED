#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsTitleImpl {
  public:
    jsTitleImpl(const fl::string &name);
    ~jsTitleImpl();
    jsTitleImpl &Group(const fl::string &name) {
        mInternal->setGroup(name);
        return *this;
    }

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    const fl::string &groupName() const { return mInternal->groupName(); }
    
    // Method to allow parent UIBase class to set the group
    void setGroupInternal(const fl::string &groupName) { mInternal->setGroup(groupName); }

  private:
    jsUiInternalPtr mInternal;
};

} // namespace fl
