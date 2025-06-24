#pragma once

#include "fl/stdint.h"

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"

namespace fl {

class JsonButtonImpl {
  public:
    JsonButtonImpl(const fl::string &name);
    ~JsonButtonImpl();
    JsonButtonImpl &Group(const fl::string &name);

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    bool isPressed() const;
    bool clicked() const;
    int clickedCount() const;
    const fl::string &groupName() const;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

    void click();

  private:
    struct Updater : fl::EngineEvents::Listener {
        void init(JsonButtonImpl *owner);
        ~Updater();
        void onPlatformPreLoop2() override;
        JsonButtonImpl *mOwner = nullptr;
    };

    Updater mUpdater;

    void updateInternal(const FLArduinoJson::JsonVariantConst &value);

    JsonUiInternalPtr mInternal;
    bool mPressed = false;
    bool mPressedLast = false;
    bool mClickedHappened = false;
    int mClickedCount = 0;
};

} // namespace fl
