#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/json.h"

namespace fl {

class JsonHelpImpl {
  public:
    JsonHelpImpl(const fl::string &markdownContent);
    ~JsonHelpImpl();
    JsonHelpImpl &Group(const fl::string &name);

    const fl::string &markdownContent() const;

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    const fl::string &groupName() const;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

    int id() const {
      return mInternal->id();
    }

  private:
    JsonUiInternalPtr mInternal;
    fl::string mMarkdownContent;
};

} // namespace fl 
