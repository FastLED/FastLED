#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"

namespace fl {

class JsonCheckboxImpl {
  public:
    JsonCheckboxImpl(const fl::string &, bool value);
    ~JsonCheckboxImpl();
    JsonCheckboxImpl &Group(const fl::string &name);

    const fl::string &name() const;
    fl::Json toJson() const;
    bool isChecked() const;
    bool value() const;  // backward compatibility
    void setValue(bool value);
    const fl::string &groupName() const;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

    int id() const {
      return mInternal->id();
    }

    JsonCheckboxImpl &operator=(bool value);
    JsonCheckboxImpl &operator=(int value);

  private:
    void updateInternal(const fl::Json &json);
    void setValueInternal(bool value);

    JsonUiInternalPtr mInternal;
    bool mValue;
};

} // namespace fl
