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
    JsonDropdownImpl(const fl::string &name, fl::span<fl::string> options) FL_NO_EXCEPT;

    // Constructor with initializer_list (FastLED requires C++11 support)
    JsonDropdownImpl(const fl::string &name, fl::initializer_list<fl::string> options) FL_NO_EXCEPT;
    
    
    ~JsonDropdownImpl();
    JsonDropdownImpl &Group(const fl::string &name) FL_NO_EXCEPT;

    const fl::string &name() const FL_NO_EXCEPT;
    void toJson(fl::json &json) const FL_NO_EXCEPT;
    fl::string value() const FL_NO_EXCEPT;
    int value_int() const FL_NO_EXCEPT;
    void setSelectedIndex(int index) FL_NO_EXCEPT;
    size_t getOptionCount() const FL_NO_EXCEPT;
    fl::string getOption(size_t index) const FL_NO_EXCEPT;
    fl::string groupName() const FL_NO_EXCEPT;

    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName) FL_NO_EXCEPT;

    int id() const FL_NO_EXCEPT;

    JsonDropdownImpl &operator=(int index) FL_NO_EXCEPT;

  private:
    // Private constructor with array of options and count (used by template constructor)
    JsonDropdownImpl(const fl::string &name, const fl::string* options, size_t count) FL_NO_EXCEPT;
    
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiDropdownInternal> mInternal;
};

} // namespace fl
