#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/json.h"
#include "fl/ptr.h" // For fl::shared_ptr

namespace fl {

// Forward declaration of the internal class
class JsonUiCheckboxInternal;

class JsonCheckboxImpl {
  public:
    JsonCheckboxImpl(const fl::string &, bool value);
    ~JsonCheckboxImpl();
    JsonCheckboxImpl &Group(const fl::string &name);

    const fl::string &name() const;
    void toJson(fl::Json &json) const;
    bool value() const;
    void setValue(bool value);
    const fl::string &groupName() const;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

    int id() const;

    JsonCheckboxImpl &operator=(bool value);
    JsonCheckboxImpl &operator=(int value);

  private:
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiCheckboxInternal> mInternal;
};

} // namespace fl
