#pragma once

#include "fl/singleton.h"

#include "fl/map.h"
#include "fl/ptr.h"
#include "fl/set.h"
#include "fl/engine_events.h"

#include "fl/json.h"
#include "fl/function.h"
#include "platforms/shared/ui/json/ui_internal.h"

namespace fl {

class UiManager : fl::EngineEvents::Listener {
  public:
    using Callback = fl::function<void(const char *)>;
    UiManager(Callback updateJs) : mUpdateJs(updateJs) { fl::EngineEvents::addListener(this); }
    ~UiManager() { fl::EngineEvents::removeListener(this); }

    void addComponent(fl::WeakPtr<JsonUiInternal> component);
    void removeComponent(fl::WeakPtr<JsonUiInternal> component);

    // Internal representation.
    void updateUiComponents(const char *jsonStr);
    void executeUiUpdates(const FLArduinoJson::JsonDocument &doc);

    Callback mUpdateJs;

  private:
    
    typedef fl::FixedSet<fl::WeakPtr<JsonUiInternal>, 64> JsonUIRefSet;

    void onPlatformPreLoop() override {
        if (!mHasPendingUpdate) {
            return;
        }
        executeUiUpdates(mPendingJsonUpdate);
        mPendingJsonUpdate.clear();
        mHasPendingUpdate = false;
    }

    void onEndShowLeds() override;

    fl::vector<JsonUiInternalPtr> getComponents();
    void toJson(FLArduinoJson::JsonArray &json);

    JsonUIRefSet mComponents;
    fl::mutex mMutex;

    bool mItemsAdded = false;
    FLArduinoJson::JsonDocument mPendingJsonUpdate;
    bool mHasPendingUpdate = false;
};

} // namespace fl
