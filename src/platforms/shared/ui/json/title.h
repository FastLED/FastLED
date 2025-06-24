#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/json.h"


namespace fl {

class JsonTitleImpl {
  public:
    JsonTitleImpl(const fl::string &text);
    ~JsonTitleImpl();
    JsonTitleImpl &Group(const fl::string &name);

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    const fl::string &groupName() const;
    const fl::string &text() const;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

  private:
    JsonUiInternalPtr mInternal;
    fl::string mText;
};

} // namespace fl
