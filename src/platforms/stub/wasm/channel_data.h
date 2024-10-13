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

FASTLED_NAMESPACE_BEGIN

typedef Slice<const uint8_t> SliceUint8;
struct StripData {
    int index = 0;
    SliceUint8 slice;
};

class ChannelData {
public:
    ChannelData() {}

    static ChannelData& Instance() {
        return Singleton<ChannelData>::instance();
    }

    void update(Slice<StripData> data) {
        mStripMap.clear();
        for (const StripData& stripData : data) {
            mStripMap[stripData.index] = stripData.slice;
        }
    }

    void update(int id, uint32_t now, Slice<SliceUint8> data) {
        mStripMap[id] = data;
        mUpdateMap[id] = now;
    }

    emscripten::val getPixelData_Uint8(int stripIndex) {
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

    typedef std::map<int, SliceUint8> StripDataMap;
    typedef std::map<int, uint32_t> UpdateMap;
    StripDataMap mStripMap;
    UpdateMap mUpdateMap;

};
