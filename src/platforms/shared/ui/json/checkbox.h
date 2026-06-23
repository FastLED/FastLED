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
    JsonCheckboxImpl(const fl::string &, bool value) FL_NOEXCEPT;
    ~JsonCheckboxImpl();
    JsonCheckboxImpl &Group(const fl::string &name) FL_NOEXCEPT;

    const fl::string &name() const FL_NOEXCEPT;
    void toJson(fl::json &json) const FL_NOEXCEPT;
    bool value() const FL_NOEXCEPT;
    void setValue(bool value) FL_NOEXCEPT;
    fl::string groupName() const FL_NOEXCEPT;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName) FL_NOEXCEPT;

    int id() const FL_NOEXCEPT;

    JsonCheckboxImpl &operator=(bool value) FL_NOEXCEPT;
    JsonCheckboxImpl &operator=(int value) FL_NOEXCEPT;

  private:
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiCheckboxInternal> mInternal;
};

} // namespace fl
