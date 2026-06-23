#pragma once

// IWYU pragma: private

#include "fl/system/engine_events.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/stl/json.h"
#include "fl/stl/shared_ptr.h" // For fl::shared_ptr
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declaration of the internal class
class JsonUiDropdownInternal;

class JsonDropdownImpl {
  public:

    // Constructor with fl::span<fl::string> for arrays and containers.
    JsonDropdownImpl(const fl::string &name, fl::span<fl::string> options) FL_NOEXCEPT;

    // Constructor with initializer_list (FastLED requires C++11 support)
    JsonDropdownImpl(const fl::string &name, fl::initializer_list<fl::string> options) FL_NOEXCEPT;
    
    
    ~JsonDropdownImpl();
    JsonDropdownImpl &Group(const fl::string &name) FL_NOEXCEPT;

    const fl::string &name() const FL_NOEXCEPT;
    void toJson(fl::json &json) const FL_NOEXCEPT;
    fl::string value() const FL_NOEXCEPT;
    int value_int() const FL_NOEXCEPT;
    void setSelectedIndex(int index) FL_NOEXCEPT;
    size_t getOptionCount() const FL_NOEXCEPT;
    fl::string getOption(size_t index) const FL_NOEXCEPT;
    fl::string groupName() const FL_NOEXCEPT;

    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName) FL_NOEXCEPT;

    int id() const FL_NOEXCEPT;

    JsonDropdownImpl &operator=(int index) FL_NOEXCEPT;

  private:
    // Private constructor with array of options and count (used by template constructor)
    JsonDropdownImpl(const fl::string &name, const fl::string* options, size_t count) FL_NOEXCEPT;
    
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiDropdownInternal> mInternal;
};

} // namespace fl
