#pragma once

// IWYU pragma: private

#include "fl/stl/singleton.h"

#include "fl/stl/map.h"
#include "fl/stl/memory.h"
#include "fl/stl/set.h"
#include "fl/engine_events.h"

#include "fl/stl/json.h"
#include "fl/stl/json.h"
#include "fl/stl/function.h"
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
    void executeUiUpdates(const fl::json &doc);

    void resetCallback(Callback updateJs) {
      mUpdateJs = updateJs;
    }

    JsonUiInternalPtr findUiComponent(const char* id_or_name);


  private:
    
    typedef fl::VectorSet<fl::weak_ptr<JsonUiInternal>> JsonUIRefSet;

    void onEndFrame() override;

    fl::vector<JsonUiInternalPtr> getComponents();
    void toJson(fl::json &json);
    JsonUiInternalPtr findUiComponent(const fl::string& idStr);

    Callback mUpdateJs;
    JsonUIRefSet mComponents;
    fl::mutex mMutex;

    bool mItemsAdded = false;
    fl::json mPendingJsonUpdate;
    bool mHasPendingUpdate = false;
};

} // namespace fl
