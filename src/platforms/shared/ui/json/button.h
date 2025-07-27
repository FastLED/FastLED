#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/json.h"
#include "fl/ptr.h" // For fl::shared_ptr

namespace fl {

// Forward declaration of the internal class
class JsonUiButtonInternal;

class JsonButtonImpl {
  public:
    JsonButtonImpl(const fl::string &name);
    ~JsonButtonImpl();
    JsonButtonImpl &Group(const fl::string &name);

    const fl::string &name() const;
    void toJson(fl::Json &json) const;
    bool isPressed() const;
    bool clicked() const;
    int clickedCount() const;
    const fl::string &groupName() const;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

    void click();

    int id() const;

  private:
    struct Updater : fl::EngineEvents::Listener {
        void init(JsonButtonImpl *owner);
        ~Updater();
        void onPlatformPreLoop2() override;
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
