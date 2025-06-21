#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsTitleImpl {
  public:
    jsTitleImpl(const fl::string &text);
    ~jsTitleImpl();
    jsTitleImpl &Group(const fl::string &name) {
        mGroup = name;
        return *this;
    }

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    const fl::string &groupName() const { return mGroup; }
    const fl::string &text() const { return mText; }
    
    // Method to allow parent UIBase class to set the group
    void setGroupInternal(const fl::string &groupName) { mGroup = groupName; }

  private:
    jsUiInternalPtr mInternal;
    fl::string mGroup;
    fl::string mText;
};

} // namespace fl
