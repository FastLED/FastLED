#pragma once

/* Screenmap maps strip indexes to x,y coordinates. This is used for FastLED.js
 * to map the 1D strip to a 2D grid. Note that the strip can have arbitrary
 * size. this was first motivated during the (attempted? Oct. 19th 2024) port of
 * the Chromancer project to FastLED.js.
 */

#include "force_inline.h"
#include "lut16.h"
#include "ptr.h"
#include <stdint.h>
#include <string.h>
#include "fixed_map.h"

class String;

// ScreenMap screen map maps strip indexes to x,y coordinates for a ui
// canvas in float format.
// This class is cheap to copy as it uses smart pointers for shared data.
class ScreenMap {
  public:
    ScreenMap() = default;

    // is_reverse is false by default for linear layout
    ScreenMap(uint32_t length) : length(length) {
        mLookUpTable = LUTXYFLOATPtr::New(length);
        LUTXYFLOAT &lut = *mLookUpTable.get();
        pair_xy_float *data = lut.getData();
        for (uint32_t x = 0; x < length; x++) {
            data[x] = {0, 0};
        }
    }

    ScreenMap(const pair_xy_float *lut, uint32_t length) : length(length) {
        mLookUpTable = LUTXYFLOATPtr::New(length);
        LUTXYFLOAT &lut16xy = *mLookUpTable.get();
        pair_xy_float *data = lut16xy.getData();
        for (uint32_t x = 0; x < length; x++) {
            data[x] = lut[x];
        }
    }

    template <uint32_t N> ScreenMap(const pair_xy_float (&lut)[N]) : length(N) {
        mLookUpTable = LUTXYFLOATPtr::New(length);
        LUTXYFLOAT &lut16xy = *mLookUpTable.get();
        pair_xy_float *data = lut16xy.getData();
        for (uint32_t x = 0; x < length; x++) {
            data[x] = lut[x];
        }
    }

    ScreenMap(const ScreenMap &other) {
        length = other.length;
        mLookUpTable = other.mLookUpTable;
    }

    const pair_xy_float &operator[](uint32_t x) const {
        if (x >= length || !mLookUpTable) {
            return empty(); // better than crashing.
        }
        LUTXYFLOAT &lut = *mLookUpTable.get();
        return lut[x];
    }

    void set(uint16_t index, const pair_xy_float &p) {
        if (mLookUpTable) {
            LUTXYFLOAT &lut = *mLookUpTable.get();
            auto *data = lut.getData();
            data[index] = p;
        }
    }

    // define the assignment operator
    ScreenMap &operator=(const ScreenMap &other) {
        if (this != &other) {
            length = other.length;
            mLookUpTable = other.mLookUpTable;
        }
        return *this;
    }

    pair_xy_float mapToIndex(uint32_t x) const {
        if (x >= length || !mLookUpTable) {
            return {0, 0};
        }
        LUTXYFLOAT &lut = *mLookUpTable.get();
        pair_xy_float screen_coords = lut[x];
        return screen_coords;
    }

    uint32_t getLength() const { return length; }

    static void ParseJson(const char *jsonStrOfMapFile,
                          FixedMap<String, ScreenMap, 16> *segmentMaps);

  private:
    static const pair_xy_float &empty() {
        static const pair_xy_float s_empty = pair_xy_float(0, 0);
        return s_empty;
    }
    uint32_t length = 0;
    LUTXYFLOATPtr mLookUpTable;
};
