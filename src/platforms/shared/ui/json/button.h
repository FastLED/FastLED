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
class JsonUiButtonInternal;

class JsonButtonImpl {
  public:
    JsonButtonImpl(const fl::string &name) FL_NO_EXCEPT;
    ~JsonButtonImpl();
    JsonButtonImpl &Group(const fl::string &name) FL_NO_EXCEPT;

    const fl::string &name() const FL_NO_EXCEPT;
    void toJson(fl::json &json) const FL_NO_EXCEPT;
    bool isPressed() const FL_NO_EXCEPT;
    bool clicked() const FL_NO_EXCEPT;
    int clickedCount() const FL_NO_EXCEPT;
    fl::string groupName() const FL_NO_EXCEPT;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName) FL_NO_EXCEPT;

    void click() FL_NO_EXCEPT;

    int id() const FL_NO_EXCEPT;

  private:
    struct Updater : fl::EngineEvents::Listener {
        void init(JsonButtonImpl *owner) FL_NO_EXCEPT;
        ~Updater();
        void onPlatformPreLoop2() FL_NO_EXCEPT override;
        JsonButtonImpl *mOwner = nullptr;
    };

    Updater mUpdater;

    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiButtonInternal> mInternal;
    bool mPressedLast = false;
    bool mClickedHappened = false;
    int mClickedCount = 0;
};

} // namespace fl
