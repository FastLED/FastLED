#pragma once

#include <memory>

#include "fl/engine_events.h"
#include "fl/map.h"
#include "fl/namespace.h"
#include "fl/screenmap.h"
#include "fl/singleton.h"
#include "fl/span.h"
#include "strip_id_map.h"


namespace fl {

typedef fl::span<const uint8_t> SliceUint8;

// Zero copy data transfer of strip information from C++ to JavaScript.
class ActiveStripData : public fl::EngineEvents::Listener {
  public:
    typedef fl::SortedHeapMap<int, SliceUint8> StripDataMap;
    typedef fl::SortedHeapMap<int, ScreenMap> ScreenMapMap;

    static ActiveStripData &Instance();
    void update(int id, uint32_t now, const uint8_t *pixel_data, size_t size);
    void updateScreenMap(int id, const ScreenMap &screenmap);

    // JSON creation methods
    fl::string infoJsonString(); // Legacy implementation (working)
    fl::string infoJsonStringNew(); // New fl::Json API (when creation is fixed)

    // JSON parsing methods (NEW - using working fl::Json parsing API)
    bool parseStripJsonInfo(const char* jsonStr); // Parse strip configuration from JSON
    
    const StripDataMap &getData() const { return mStripMap; }
    const ScreenMapMap &getScreenMaps() const { return mScreenMap; }

    ~ActiveStripData() { fl::EngineEvents::removeListener(this); }

    void onBeginFrame() override { mStripMap.clear(); }

    void onCanvasUiSet(CLEDController *strip,
                       const ScreenMap &screenmap) override {
        int id = StripIdMap::addOrGetId(strip);
        updateScreenMap(id, screenmap);
    }

    bool hasScreenMap(int id) const { return mScreenMap.has(id); }

  private:
    friend class fl::Singleton<ActiveStripData>;
    ActiveStripData() {
        fl::EngineEvents::Listener *listener = this;
        fl::EngineEvents::addListener(listener);
    }

    StripDataMap mStripMap;
    ScreenMapMap mScreenMap;
};

} // namespace fl
