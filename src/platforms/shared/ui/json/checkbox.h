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
class JsonUiCheckboxInternal;

class JsonCheckboxImpl {
  public:
    JsonCheckboxImpl(const fl::string &, bool value) FL_NO_EXCEPT;
    ~JsonCheckboxImpl();
    JsonCheckboxImpl &Group(const fl::string &name) FL_NO_EXCEPT;

    const fl::string &name() const FL_NO_EXCEPT;
    void toJson(fl::json &json) const FL_NO_EXCEPT;
    bool value() const FL_NO_EXCEPT;
    void setValue(bool value) FL_NO_EXCEPT;
    fl::string groupName() const FL_NO_EXCEPT;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName) FL_NO_EXCEPT;

    int id() const FL_NO_EXCEPT;

    JsonCheckboxImpl &operator=(bool value) FL_NO_EXCEPT;
    JsonCheckboxImpl &operator=(int value) FL_NO_EXCEPT;

  private:
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiCheckboxInternal> mInternal;
};

} // namespace fl
