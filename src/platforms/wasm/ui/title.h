#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"
#include "fl/json.h"


namespace fl {

class jsTitleImpl {
  public:
    jsTitleImpl(const fl::string &text);
    ~jsTitleImpl();
    jsTitleImpl &Group(const fl::string &name) {
        mInternal->setGroup(name);
        return *this;
    }

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    const fl::string &groupName() const { return mInternal->groupName(); }
    const fl::string &text() const { return mText; }
    
    // Method to allow parent UIBase class to set the group
    void setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

  private:
    jsUiInternalPtr mInternal;
    fl::string mText;
};

} // namespace fl
