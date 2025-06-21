#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/json.h"

namespace fl {

class JsonDescriptionImpl {
  public:
    JsonDescriptionImpl(const fl::string &name);
    ~JsonDescriptionImpl();
    JsonDescriptionImpl &Group(const fl::string &name) {
        mInternal->setGroup(name);
        return *this;
    }

    const fl::string &text() const { return mText; }

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    const fl::string &groupName() const { return mInternal->groupName(); }
    
    // Method to allow parent UIBase class to set the group
    void setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

  private:
    JsonUiInternalPtr mInternal;
    fl::string mText;
};

} // namespace fl
