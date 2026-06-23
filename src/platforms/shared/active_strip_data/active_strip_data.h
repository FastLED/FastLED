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

    static ActiveStripData &Instance() FL_NOEXCEPT;
    void update(int id, u32 now, fl::span<const u8> pixel_data) FL_NOEXCEPT;
    void updateScreenMap(int id, const ScreenMap &screenmap) FL_NOEXCEPT;

    // JSON creation methods
    fl::string infoJsonString() FL_NOEXCEPT; // Legacy implementation (working)
    fl::string infoJsonStringNew() FL_NOEXCEPT; // New fl::json API (when creation is fixed)

    // JSON parsing methods (NEW - using working fl::json parsing API)
    bool parseStripJsonInfo(const char* jsonStr) FL_NOEXCEPT; // Parse strip configuration from JSON
    
    const StripDataMap &getData() const FL_NOEXCEPT { return mStripMap; }
    const ScreenMapMap &getScreenMaps() const FL_NOEXCEPT { return mScreenMap; }

    ~ActiveStripData() { fl::EngineEvents::removeListener(this); }

    void onBeginFrame() FL_NOEXCEPT override { mStripMap.clear(); }

    void onCanvasUiSet(CLEDController *strip,
                       const ScreenMap &screenmap) FL_NOEXCEPT override;

    bool hasScreenMap(int id) const FL_NOEXCEPT { return mScreenMap.has(id); }

    /**
     * Get the ID tracker for strip ID management
     */
    IdTracker& getIdTracker() FL_NOEXCEPT { return mIdTracker; }

  private:
    friend class fl::Singleton<ActiveStripData>;
    ActiveStripData() FL_NOEXCEPT {
        fl::EngineEvents::Listener *listener = this;
        fl::EngineEvents::addListener(listener);
    }

    StripDataMap mStripMap;
    ScreenMapMap mScreenMap;
    IdTracker mIdTracker;
};

} // namespace fl 
