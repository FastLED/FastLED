#pragma once



#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <memory>

#include "slice.h"
#include "singleton.h"
#include "fixed_map.h"

#include "namespace.h"
#include "engine_events.h"
#include "fixed_map.h"
#include "screenmap.h"
#include "strip_id_map.h"

FASTLED_NAMESPACE_BEGIN

typedef Slice<const uint8_t> SliceUint8;

// Zero copy data transfer of strip information from C++ to JavaScript.
class ActiveStripData: public EngineEvents::Listener {
public:


    static constexpr size_t MAX_STRIPS = 16; // Adjust this value based on your needs
    typedef FixedMap<int, SliceUint8, MAX_STRIPS> StripDataMap;
    typedef FixedMap<int, ScreenMap, MAX_STRIPS> ScreenMapMap;

    static ActiveStripData& Instance();
    void update(int id, uint32_t now, const uint8_t* pixel_data, size_t size);
    void updateScreenMap(int id, const ScreenMap& screenmap);
    emscripten::val getPixelData_Uint8(int stripIndex);
    Str infoJsonString();


    const StripDataMap& getData() const  {
        return mStripMap;
    }
    
    ~ActiveStripData() {
        EngineEvents::removeListener(this);
    }

    void onBeginFrame() override {
        mStripMap.clear();
    }
    
    void onCanvasUiSet(CLEDController *strip, const ScreenMap& screenmap) override {
        int id = StripIdMap::addOrGetId(strip);
        updateScreenMap(id, screenmap);
    }

    const bool hasScreenMap(int id) const {
        return mScreenMap.has(id);
    }


private:
    friend class Singleton<ActiveStripData>;
    ActiveStripData() {
        EngineEvents::Listener* listener = this;
        EngineEvents::addListener(listener);
    }


    StripDataMap mStripMap;
    ScreenMapMap mScreenMap;
};

FASTLED_NAMESPACE_END
