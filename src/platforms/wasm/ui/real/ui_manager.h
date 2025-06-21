#pragma once

#include "fl/singleton.h"
#include "platforms/wasm/engine_listener.h"

#include "fl/map.h"
#include "fl/ptr.h"
#include "fl/set.h"

#include "fl/json.h"
#include "fl/function.h"
#include "platforms/wasm/ui/real/ui_internal.h"

namespace fl {

class UiManager : fl::EngineEvents::Listener {
  public:
    using Callback = fl::function<void(const char *)>;
    UiManager(Callback updateJs) : mUpdateJs(updateJs) { fl::EngineEvents::addListener(this); }
    ~UiManager() { fl::EngineEvents::removeListener(this); }

    void addComponent(fl::WeakPtr<jsUiInternal> component);
    void removeComponent(fl::WeakPtr<jsUiInternal> component);

    // Internal representation.
    void updateUiComponents(const char *jsonStr);
    void executeUiUpdates(const FLArduinoJson::JsonDocument &doc);

    Callback mUpdateJs;

  private:
    
    typedef fl::FixedSet<fl::WeakPtr<jsUiInternal>, 64> jsUIRefSet;

    void onPlatformPreLoop() override {
        if (!mHasPendingUpdate) {
            return;
        }
        executeUiUpdates(mPendingJsonUpdate);
        mPendingJsonUpdate.clear();
        mHasPendingUpdate = false;
    }

    void onEndShowLeds() override;

    fl::vector<jsUiInternalPtr> getComponents();
    void toJson(FLArduinoJson::JsonArray &json);

    jsUIRefSet mComponents;
    fl::mutex mMutex;

    bool mItemsAdded = false;
    FLArduinoJson::JsonDocument mPendingJsonUpdate;
    bool mHasPendingUpdate = false;
};

} // namespace fl
