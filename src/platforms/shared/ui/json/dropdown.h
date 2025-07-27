#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "fl/vector.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/json.h"
#include "fl/ptr.h" // For fl::shared_ptr

namespace fl {

// Forward declaration of the internal class
class JsonUiDropdownInternal;

class JsonDropdownImpl {
  public:

    // Constructor with fl::span<fl::string> for arrays and containers.
    JsonDropdownImpl(const fl::string &name, fl::span<fl::string> options);

    // Constructor with initializer_list (FastLED requires C++11 support)
    JsonDropdownImpl(const fl::string &name, fl::initializer_list<fl::string> options);
    
    
    ~JsonDropdownImpl();
    JsonDropdownImpl &Group(const fl::string &name);

    const fl::string &name() const;
    void toJson(fl::Json &json) const;
    fl::string value() const;
    int value_int() const;
    void setSelectedIndex(int index);
    size_t getOptionCount() const;
    fl::string getOption(size_t index) const;
    const fl::string &groupName() const;

    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

    int id() const;

    JsonDropdownImpl &operator=(int index);

  private:
    // Private constructor with array of options and count (used by template constructor)
    JsonDropdownImpl(const fl::string &name, const fl::string* options, size_t count);
    
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiDropdownInternal> mInternal;
};

} // namespace fl
