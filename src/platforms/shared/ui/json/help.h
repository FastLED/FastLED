#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/json.h"
#include "fl/ptr.h" // For fl::shared_ptr



namespace fl {

// Forward declaration of the internal class
class JsonUiHelpInternal;

class JsonHelpImpl {
  public:
    JsonHelpImpl(const fl::string &markdownContent);
    ~JsonHelpImpl();
    JsonHelpImpl &Group(const fl::string &name);

    const fl::string &markdownContent() const;

    const fl::string &name() const;
    void toJson(fl::Json &json) const;
    const fl::string &groupName() const;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

    int id() const;

    bool operator==(const fl::string& other) const { return groupName() == other; }
    bool operator!=(const fl::string& other) const { return groupName() != other; }


  private:
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiHelpInternal> mInternal;
};

} // namespace fl
