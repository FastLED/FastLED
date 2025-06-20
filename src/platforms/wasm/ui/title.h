#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsTitleImpl {
  public:
    jsTitleImpl(const fl::Str &name);
    ~jsTitleImpl();
    jsTitleImpl &Group(const fl::Str &name) {
        mGroup = name;
        return *this;
    }

    const fl::Str &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    const fl::Str &groupName() const { return mGroup; }
    
    // Method to allow parent UIBase class to set the group
    void setGroupInternal(const fl::Str &groupName) { mGroup = groupName; }

  private:
    jsUiInternalPtr mInternal;
    fl::Str mGroup;
};

} // namespace fl
