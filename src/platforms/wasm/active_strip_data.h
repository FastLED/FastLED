#pragma once

#include <memory>

#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "slice.h"
#include "singleton.h"
#include "fixed_map.h"

#include "namespace.h"
#include "engine_events.h"
#include "fixed_map.h"
#include "active_strip_data.h"

FASTLED_NAMESPACE_BEGIN

typedef Slice<const uint8_t> SliceUint8;
struct StripData {
    int index = 0;
    SliceUint8 slice;
};

// Zero copy data transfer of strip information from C++ to JavaScript.
class ActiveStripData: public EngineEvents::Listener {
public:
    void onBeginFrame() override {
        mStripMap.clear();
    }
    static ActiveStripData& Instance();
    void update(int id, uint32_t now, const uint8_t* pixel_data, size_t size);
    emscripten::val getPixelData_Uint8(int stripIndex);
    std::string infoJsonString();


    static constexpr size_t MAX_STRIPS = 16; // Adjust this value based on your needs
    typedef FixedMap<int, SliceUint8, MAX_STRIPS> StripDataMap;
    const StripDataMap& getData() const  {
        return mStripMap;
    }
    
    ~ActiveStripData() {
        EngineEvents::removeListener(this);
    }

private:
    friend class Singleton<ActiveStripData>;
    ActiveStripData() {
        EngineEvents::addListener(this);
    }


    StripDataMap mStripMap;
};
