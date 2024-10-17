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

    static ActiveStripData& Instance() {
        return Singleton<ActiveStripData>::instance();
    }

    void update(int id, uint32_t now, const uint8_t* data, size_t size) {
        mStripMap.update(id, SliceUint8(data, size));
    }

    emscripten::val getPixelData_Uint8(int stripIndex) {
        // Efficient, zero copy conversion from internal data to JavaScript.
        SliceUint8 stripData;
        if (mStripMap.get(stripIndex, &stripData)) {
            const uint8_t* data = stripData.data();
            uint8_t* data_mutable = const_cast<uint8_t*>(data);
            size_t size = stripData.size();
            return emscripten::val(emscripten::typed_memory_view(size, data_mutable));
        }
        return emscripten::val::undefined();
    }

    emscripten::val getFirstPixelData_Uint8() {
        // Efficient, zero copy conversion from internal data to JavaScript.
        if (!mStripMap.empty()) {
            SliceUint8 stripData = mStripMap.begin()->second;
            const uint8_t* data = stripData.data();
            uint8_t* data_mutable = const_cast<uint8_t*>(data);
            size_t size = stripData.size();
            return emscripten::val(emscripten::typed_memory_view(size, data_mutable));
        }
        return emscripten::val::undefined();
    }

    emscripten::val getNthPixelStripData_Uint8(int n) {
        // Efficient, zero copy conversion from internal data to JavaScript.
        if (mStripMap.size() > n) {
            auto it = mStripMap.begin();
            for (int i = 0; i < n && it != mStripMap.end(); ++i, ++it) {}
            if (it != mStripMap.end()) {
                SliceUint8 stripData = it->second;
                const uint8_t* data = stripData.data();
                uint8_t* data_mutable = const_cast<uint8_t*>(data);
                size_t size = stripData.size();
                return emscripten::val(emscripten::typed_memory_view(size, data_mutable));
            }
        }
        return emscripten::val::undefined();
    }

    emscripten::val GetActiveIndices() {
        std::vector<int> indices;
        for (const auto& pair : mStripMap) {
            indices.push_back(pair.first);
        }
        return emscripten::val(indices);
    }
    ~ActiveStripData() {
        EngineEvents::removeListener(this);
    }
    
private:
    friend class Singleton<ActiveStripData>;
    ActiveStripData() {
        EngineEvents::addListener(this);
    }
    static constexpr size_t MAX_STRIPS = 16; // Adjust this value based on your needs
    typedef FixedMap<int, SliceUint8, MAX_STRIPS> StripDataMap;
    StripDataMap mStripMap;
};
