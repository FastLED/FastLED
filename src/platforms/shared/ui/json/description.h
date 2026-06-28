#pragma once

// IWYU pragma: private

#include "fl/system/engine_events.h"
#include "fl/stl/string.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/stl/json.h"
#include "fl/stl/shared_ptr.h" // For fl::shared_ptr
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declaration of the internal class
class JsonUiDescriptionInternal;

class JsonDescriptionImpl {
  public:
    JsonDescriptionImpl(const fl::string &name) FL_NO_EXCEPT;
    ~JsonDescriptionImpl();
    JsonDescriptionImpl &Group(const fl::string &name) FL_NO_EXCEPT;

    const fl::string &text() const FL_NO_EXCEPT;

    const fl::string &name() const FL_NO_EXCEPT;
    void toJson(fl::json &json) const FL_NO_EXCEPT;
    fl::string groupName() const FL_NO_EXCEPT;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName) FL_NO_EXCEPT;

    int id() const FL_NO_EXCEPT;

  private:
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiDescriptionInternal> mInternal;
};

} // namespace fl
