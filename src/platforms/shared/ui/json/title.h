#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/json.h"
#include "fl/ptr.h" // For fl::shared_ptr



namespace fl {

  // Forward declaration of the internal class
class JsonUiTitleInternal;

class JsonTitleImpl {
  public:
    // Constructor: Now takes the text directly and creates JsonUiTitleInternal.
    JsonTitleImpl(const fl::string& name, const fl::string& text);

    // Destructor: Handles cleanup of the internal component.
    ~JsonTitleImpl();

    JsonTitleImpl &Group(const fl::string &name);

    const fl::string &name() const;
    const fl::string &groupName() const;
    
    // Add accessors for the title text, delegating to mInternal.
    const fl::string& text() const;
    void setText(const fl::string& text);
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

    int id() const;

  private:
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiTitleInternal> mInternal;
};

} // namespace fl
