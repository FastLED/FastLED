#pragma once

/* Screenmap maps strip indexes to x,y coordinates. This is used for FastLED.js
 * to map the 1D strip to a 2D grid. Note that the strip can have arbitrary size.
 * this was first motivated during the (attempted? Oct. 19th 2024) port of the
 * Chromancer project to FastLED.js.
*/

#include <stdint.h>
#include <string.h>
#include "ptr.h"
#include "force_inline.h"
#include "lut16.h"


// ScreenMap holds either a function or a look up table to map x coordinates to a 1D index.
class ScreenMap {
public:
    // is_reverse is false by default for linear layout
    ScreenMap(uint32_t length): length(length) {
        mLookUpTable = LUTXY16Ptr::New(length);
        LUTXY16& lut = *mLookUpTable.get();
        pair_xy16* data = lut.getData();
        for (uint32_t x = 0; x < length; x++) {
            data[x] = {0, 0};
        }
    }

    ScreenMap(const pair_xy16* lut, uint32_t length): length(length) {
        mLookUpTable = LUTXY16Ptr::New(length);
        LUTXY16& lut16xy = *mLookUpTable.get();
        pair_xy16* data = lut16xy.getData();
        for (uint32_t x = 0; x < length; x++) {
            data[x] = lut[x];
        }
    }

    template<uint32_t N>
    ScreenMap(const pair_xy16 (&lut)[N]): length(N) {
        mLookUpTable = LUTXY16Ptr::New(length);
        LUTXY16& lut16xy = *mLookUpTable.get();
        pair_xy16* data = lut16xy.getData();
        for (uint32_t x = 0; x < length; x++) {
            data[x] = lut[x];
        }
    }

    ScreenMap(const ScreenMap &other) {
        length = other.length;
        mLookUpTable = other.mLookUpTable;
    }

    const pair_xy16& operator[](uint32_t x) const {
        if (x >= length || !mLookUpTable) {
            return empty();  // better than crashing.
        }
        LUTXY16& lut = *mLookUpTable.get();
        return lut[x];
    }

    // define the assignment operator
    ScreenMap& operator=(const ScreenMap &other) {
        if (this != &other) {
            length = other.length;
            mLookUpTable = other.mLookUpTable;
        }
        return *this;
    }

    pair_xy16 mapToIndex(uint32_t x) const {
        if (x >= length || !mLookUpTable) {
            return {0, 0};
        }
        LUTXY16& lut = *mLookUpTable.get();
        pair_xy16 screen_coords = lut[x];
        return screen_coords;
    }

    uint32_t getLength() const {
        return length;
    }

private:
    static const pair_xy16& empty() {
        static const pair_xy16 s_empty = {0, 0};
        return s_empty;
    }
    uint32_t length = 0;
    LUTXY16Ptr mLookUpTable;
};
