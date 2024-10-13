#pragma once

#include "slice.h"

#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>


typedef Slice<const uint8_t> SliceUint8;
struct StripData {
    int index = 0;
    SliceUint8 slice;
};

class FastLED_ChannelData {
public:
    FastLED_ChannelData() {
    }

    void update(Slice<StripData> data) {
        mStripMap.clear();
        for (const StripData& stripData : data) {
            mStripMap[stripData.index] = stripData.slice;
        }
    }

    emscripten::val getPixelData_Uint8(int stripIndex) {
        SliceUint8 stripData = getStripData(stripIndex);
        const uint8_t* data = stripData.data();
        uint8_t* data_mutable = const_cast<uint8_t*>(data);
        size_t size = stripData.size();
        return emscripten::val(emscripten::typed_memory_view(size, data_mutable));
    }
    
private:
    typedef std::map<int, SliceUint8> StripDataMap;
    StripDataMap mStripMap;
    SliceUint8 getStripData(int stripIndex) {
        // search through the vector and look for the first element matching the stripIndex
        // strip map
        StripDataMap::const_iterator it = mStripMap.find(stripIndex);
        if (it != mStripMap.end()) {
            SliceUint8 slice = it->second;
            return slice;
        }
        return SliceUint8();
    }
};