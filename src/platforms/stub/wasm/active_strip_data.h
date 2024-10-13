#pragma once

#include <memory>

#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <map>

#include "slice.h"
#include "singleton.h"

#include "namespace.h"
#include "engine_events.h"

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
        mUpdateMap.clear();
    }

    static ActiveStripData& Instance() {
        return Singleton<ActiveStripData>::instance();
    }

    void update(int id, uint32_t now, const uint8_t* data, size_t size) {
        mStripMap[id] = SliceUint8(data, size);
        mUpdateMap[id] = now;
    }

    emscripten::val getPixelData_Uint8(int stripIndex) {
        // Efficient, zero copy conversion from internal data to JavaScript.
        auto find = mStripMap.find(stripIndex);
        if (find != mStripMap.end()) {
            SliceUint8 stripData = find->second;
            const uint8_t* data = stripData.data();
            uint8_t* data_mutable = const_cast<uint8_t*>(data);
            size_t size = stripData.size();
            return emscripten::val(emscripten::typed_memory_view(size, data_mutable));
        }
        return emscripten::val::undefined();
    }

    emscripten::val getFirstPixelData_Uint8() {
        // Efficient, zero copy conversion from internal data to JavaScript.
        if (mStripMap.size() > 0) {
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
            std::advance(it, n);
            SliceUint8 stripData = it->second;
            const uint8_t* data = stripData.data();
            uint8_t* data_mutable = const_cast<uint8_t*>(data);
            size_t size = stripData.size();
            return emscripten::val(emscripten::typed_memory_view(size, data_mutable));
        }
        return emscripten::val::undefined();
    }

    emscripten::val GetPixelDataTimeStamp(int stripIndex) {
        auto find = mUpdateMap.find(stripIndex);
        if (find != mUpdateMap.end()) {
            return emscripten::val(find->second);
        }
        return emscripten::val::undefined();
    }

    emscripten::val GetActiveIndices() {
        std::vector<int> indices;
        for (auto& pair : mStripMap) {
            indices.push_back(pair.first);
        }
        return emscripten::val(indices);
    }

    
private:
    friend class Singleton<ActiveStripData>;
    ActiveStripData() {}
    typedef std::map<int, SliceUint8> StripDataMap;
    typedef std::map<int, uint32_t> UpdateMap;
    StripDataMap mStripMap;
    UpdateMap mUpdateMap;
};
