#pragma once

// IWYU pragma: private

#include "fl/stl/singleton.h"

#include "fl/stl/map.h"
#include "fl/stl/memory.h"
#include "fl/stl/set.h"
#include "fl/system/engine_events.h"

#include "fl/stl/json.h"
#include "fl/stl/json.h"
#include "fl/stl/function.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/stl/noexcept.h"

namespace fl {

class JsonUiManager : fl::EngineEvents::Listener {
  public:
    using Callback = fl::function<void(const char *)>;
    JsonUiManager(Callback updateJs) FL_NO_EXCEPT;
    ~JsonUiManager();

    void addComponent(fl::weak_ptr<JsonUiInternal> component) FL_NO_EXCEPT;
    void removeComponent(fl::weak_ptr<JsonUiInternal> component) FL_NO_EXCEPT;

    // Force immediate processing of any pending updates (for testing)
    void processPendingUpdates() FL_NO_EXCEPT;

    // Internal representation.
    void updateUiComponents(const char *jsonStr) FL_NO_EXCEPT;
    void executeUiUpdates(const fl::json &doc) FL_NO_EXCEPT;

    void resetCallback(Callback updateJs) FL_NO_EXCEPT {
      mUpdateJs = updateJs;
    }

    JsonUiInternalPtr findUiComponent(const char* id_or_name) FL_NO_EXCEPT;


  private:
    
    typedef fl::VectorSet<fl::weak_ptr<JsonUiInternal>> JsonUIRefSet;

    void onEndFrame() FL_NO_EXCEPT override;

    fl::vector<JsonUiInternalPtr> getComponents() FL_NO_EXCEPT;
    void toJson(fl::json &json) FL_NO_EXCEPT;
    JsonUiInternalPtr findUiComponent(const fl::string& idStr) FL_NO_EXCEPT;

    Callback mUpdateJs;
    JsonUIRefSet mComponents;
    fl::mutex mMutex;

    bool mItemsAdded = false;
    fl::json mPendingJsonUpdate;
    bool mHasPendingUpdate = false;
};

} // namespace fl
