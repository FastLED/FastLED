#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/json.h"
#include "fl/ptr.h" // For fl::shared_ptr

namespace fl {

// Forward declaration of the internal class
class JsonUiDescriptionInternal;

class JsonDescriptionImpl {
  public:
    JsonDescriptionImpl(const fl::string &name);
    ~JsonDescriptionImpl();
    JsonDescriptionImpl &Group(const fl::string &name);

    const fl::string &text() const;

    const fl::string &name() const;
    void toJson(fl::Json &json) const;
    const fl::string &groupName() const;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

    int id() const;

  private:
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiDescriptionInternal> mInternal;
};

} // namespace fl
