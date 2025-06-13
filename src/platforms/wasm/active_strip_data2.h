#pragma once

// Trying to get rid of embind. So this is the new way to do it.
// However it is not implemented yet.

#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers

#include <memory>

#include "fl/engine_events.h"
#include "fl/map.h"
#include "fl/namespace.h"
#include "fl/screenmap.h"
#include "fl/singleton.h"
#include "fl/slice.h"
#include "strip_id_map.h"

namespace fl {

typedef fl::Slice<const uint8_t> SliceUint8;

// Zero copy data transfer of strip information from C++ to JavaScript.
class ActiveStripData2 : public fl::EngineEvents::Listener {
  public:
    typedef fl::SortedHeapMap<int, SliceUint8> StripDataMap;
    typedef fl::SortedHeapMap<int, fl::ScreenMap> ScreenMapMap;

    static ActiveStripData2 &Instance();
    void update(int id, uint32_t now, const uint8_t *pixel_data, size_t size);
    void updateScreenMap(int id, const fl::ScreenMap &screenmap);
    size_t getPixelDataSize_C(int stripIndex);
    const uint8_t *getPixelData_Uint8_C(int stripIndex);
    fl::Str infoJsonString();

    const StripDataMap &getData() const { return mStripMap; }

    ~ActiveStripData2() { fl::EngineEvents::removeListener(this); }

    void onBeginFrame() override { mStripMap.clear(); }

    void onCanvasUiSet(CLEDController *strip,
                       const fl::ScreenMap &screenmap) override {
        int id = StripIdMap::addOrGetId(strip);
        updateScreenMap(id, screenmap);
    }

    bool hasScreenMap(int id) const { return mScreenMap.has(id); }

  private:
    friend class fl::Singleton<ActiveStripData2>;
    ActiveStripData2() {
        fl::EngineEvents::Listener *listener = this;
        fl::EngineEvents::addListener(listener);
    }

    StripDataMap mStripMap;
    ScreenMapMap mScreenMap;
};

} // namespace fl
