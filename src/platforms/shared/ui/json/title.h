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
class JsonUiTitleInternal;

class JsonTitleImpl {
  public:
    // Constructor: Now takes the text directly and creates JsonUiTitleInternal.
    JsonTitleImpl(const fl::string& name, const fl::string& text) FL_NOEXCEPT;

    // Destructor: Handles cleanup of the internal component.
    ~JsonTitleImpl();

    JsonTitleImpl &Group(const fl::string &name) FL_NOEXCEPT;

    const fl::string &name() const FL_NOEXCEPT;
    fl::string groupName() const FL_NOEXCEPT;
    
    // Add accessors for the title text, delegating to mInternal.
    const fl::string& text() const FL_NOEXCEPT;
    void setText(const fl::string& text) FL_NOEXCEPT;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName) FL_NOEXCEPT;

    int id() const FL_NOEXCEPT;

  private:
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiTitleInternal> mInternal;
};

} // namespace fl
