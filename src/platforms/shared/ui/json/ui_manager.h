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
    JsonUiManager(Callback updateJs) FL_NOEXCEPT;
    ~JsonUiManager();

    void addComponent(fl::weak_ptr<JsonUiInternal> component) FL_NOEXCEPT;
    void removeComponent(fl::weak_ptr<JsonUiInternal> component) FL_NOEXCEPT;

    // Force immediate processing of any pending updates (for testing)
    void processPendingUpdates() FL_NOEXCEPT;

    // Internal representation.
    void updateUiComponents(const char *jsonStr) FL_NOEXCEPT;
    void executeUiUpdates(const fl::json &doc) FL_NOEXCEPT;

    void resetCallback(Callback updateJs) FL_NOEXCEPT {
      mUpdateJs = updateJs;
    }

    JsonUiInternalPtr findUiComponent(const char* id_or_name) FL_NOEXCEPT;


  private:
    
    typedef fl::VectorSet<fl::weak_ptr<JsonUiInternal>> JsonUIRefSet;

    void onEndFrame() FL_NOEXCEPT override;

    fl::vector<JsonUiInternalPtr> getComponents() FL_NOEXCEPT;
    void toJson(fl::json &json) FL_NOEXCEPT;
    JsonUiInternalPtr findUiComponent(const fl::string& idStr) FL_NOEXCEPT;

    Callback mUpdateJs;
    JsonUIRefSet mComponents;
    fl::mutex mMutex;

    bool mItemsAdded = false;
    fl::json mPendingJsonUpdate;
    bool mHasPendingUpdate = false;
};

} // namespace fl
