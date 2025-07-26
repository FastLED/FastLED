#pragma once

#include "fl/singleton.h"

#include "fl/map.h"
#include "fl/memory.h"
#include "fl/set.h"
#include "fl/engine_events.h"

#include "fl/json.h"
#include "fl/json.h"
#include "fl/function.h"
#include "platforms/shared/ui/json/ui_internal.h"

namespace fl {

class JsonUiManager : fl::EngineEvents::Listener {
  public:
    using Callback = fl::function<void(const char *)>;
    JsonUiManager(Callback updateJs);
    ~JsonUiManager();

    void addComponent(fl::weak_ptr<JsonUiInternal> component);
    void removeComponent(fl::weak_ptr<JsonUiInternal> component);

    // Force immediate processing of any pending updates (for testing)
    void processPendingUpdates();

    // Internal representation.
    void updateUiComponents(const char *jsonStr);
    void executeUiUpdates(const fl::Json &doc);

    void resetCallback(Callback updateJs) {
      mUpdateJs = updateJs;
    }

    JsonUiInternalPtr findUiComponent(const char* id_or_name);


  private:
    
    typedef fl::VectorSet<fl::weak_ptr<JsonUiInternal>> JsonUIRefSet;

    void onEndFrame() override;

    fl::vector<JsonUiInternalPtr> getComponents();
    void toJson(fl::Json &json);
    JsonUiInternalPtr findUiComponent(const fl::string& idStr);

    Callback mUpdateJs;
    JsonUIRefSet mComponents;
    fl::mutex mMutex;

    bool mItemsAdded = false;
    fl::Json mPendingJsonUpdate;
    bool mHasPendingUpdate = false;
};

} // namespace fl
