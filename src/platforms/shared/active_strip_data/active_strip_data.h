#pragma once

// IWYU pragma: private

#include "fl/system/engine_events.h"
#include "fl/stl/flat_map.h"
#include "fl/math/screenmap.h"
#include "fl/stl/singleton.h"
#include "fl/stl/span.h"
#include "fl/channels/id_tracker.h"
#include "fl/stl/noexcept.h"
namespace fl {
class CLEDController;
}  // namespace fl
namespace fl {



typedef fl::span<const u8> SliceUint8;

// Zero copy data transfer of strip information - platform-agnostic core logic
class ActiveStripData : public fl::EngineEvents::Listener {
  public:
    typedef fl::flat_map<int, SliceUint8> StripDataMap;
    typedef fl::flat_map<int, ScreenMap> ScreenMapMap;

    static ActiveStripData &Instance() FL_NO_EXCEPT;
    void update(int id, u32 now, fl::span<const u8> pixel_data) FL_NO_EXCEPT;
    void updateScreenMap(int id, const ScreenMap &screenmap) FL_NO_EXCEPT;

    // JSON creation methods
    fl::string infoJsonString() FL_NO_EXCEPT; // Legacy implementation (working)
    fl::string infoJsonStringNew() FL_NO_EXCEPT; // New fl::json API (when creation is fixed)

    // JSON parsing methods (NEW - using working fl::json parsing API)
    bool parseStripJsonInfo(const char* jsonStr) FL_NO_EXCEPT; // Parse strip configuration from JSON
    
    const StripDataMap &getData() const FL_NO_EXCEPT { return mStripMap; }
    const ScreenMapMap &getScreenMaps() const FL_NO_EXCEPT { return mScreenMap; }

    ~ActiveStripData() { fl::EngineEvents::removeListener(this); }

    void onBeginFrame() FL_NO_EXCEPT override { mStripMap.clear(); }

    void onCanvasUiSet(CLEDController *strip,
                       const ScreenMap &screenmap) FL_NO_EXCEPT override;

    bool hasScreenMap(int id) const FL_NO_EXCEPT { return mScreenMap.has(id); }

    /**
     * Get the ID tracker for strip ID management
     */
    IdTracker& getIdTracker() FL_NO_EXCEPT { return mIdTracker; }

  private:
    friend class fl::Singleton<ActiveStripData>;
    ActiveStripData() FL_NO_EXCEPT {
        fl::EngineEvents::Listener *listener = this;
        fl::EngineEvents::addListener(listener);
    }

    StripDataMap mStripMap;
    ScreenMapMap mScreenMap;
    IdTracker mIdTracker;
};

} // namespace fl 
