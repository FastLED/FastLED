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
    JsonTitleImpl(const fl::string& name, const fl::string& text) FL_NO_EXCEPT;

    // Destructor: Handles cleanup of the internal component.
    ~JsonTitleImpl();

    JsonTitleImpl &Group(const fl::string &name) FL_NO_EXCEPT;

    const fl::string &name() const FL_NO_EXCEPT;
    fl::string groupName() const FL_NO_EXCEPT;
    
    // Add accessors for the title text, delegating to mInternal.
    const fl::string& text() const FL_NO_EXCEPT;
    void setText(const fl::string& text) FL_NO_EXCEPT;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName) FL_NO_EXCEPT;

    int id() const FL_NO_EXCEPT;

  private:
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiTitleInternal> mInternal;
};

} // namespace fl
