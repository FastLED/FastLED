#pragma once

#include <stdint.h>

#include "force_inline.h"
#include "lut.h"
#include "ref.h"

#include "str.h"
#include "fixed_map.h"
#include "json.h"
#include "namespace.h"

/* Screenmap maps strip indexes to x,y coordinates. This is used for FastLED.js
 * to map the 1D strip to a 2D grid. Note that the strip can have arbitrary
 * size. this was first motivated during the (attempted? Oct. 19th 2024) port of
 * the Chromancer project to FastLED.js.
 */



FASTLED_NAMESPACE_BEGIN

class Str;

// ScreenMap screen map maps strip indexes to x,y coordinates for a ui
// canvas in float format.
// This class is cheap to copy as it uses smart pointers for shared data.
class ScreenMap {
  public:
    ScreenMap() = default;

    // is_reverse is false by default for linear layout
    ScreenMap(uint32_t length, float mDiameter = -1.0f) : length(length), mDiameter(mDiameter) {
        mLookUpTable = LUTXYFLOATRef::New(length);
        LUTXYFLOAT &lut = *mLookUpTable.get();
        pair_xy_float *data = lut.getData();
        for (uint32_t x = 0; x < length; x++) {
            data[x] = {0, 0};
        }
    }

    ScreenMap(const pair_xy_float *lut, uint32_t length, float diameter = -1.0) : length(length), mDiameter(diameter) {
        mLookUpTable = LUTXYFLOATRef::New(length);
        LUTXYFLOAT &lut16xy = *mLookUpTable.get();
        pair_xy_float *data = lut16xy.getData();
        for (uint32_t x = 0; x < length; x++) {
            data[x] = lut[x];
        }
    }

    template <uint32_t N> ScreenMap(const pair_xy_float (&lut)[N], float diameter = -1.0) : length(N), mDiameter(diameter) {
        mLookUpTable = LUTXYFLOATRef::New(length);
        LUTXYFLOAT &lut16xy = *mLookUpTable.get();
        pair_xy_float *data = lut16xy.getData();
        for (uint32_t x = 0; x < length; x++) {
            data[x] = lut[x];
        }
    }

    ScreenMap(const ScreenMap &other) {
        mDiameter = other.mDiameter;
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

    void setDiameter(float diameter) { mDiameter = diameter; }

    // define the assignment operator
    ScreenMap &operator=(const ScreenMap &other) {
        if (this != &other) {
            mDiameter = other.mDiameter;
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
    // The diameter each point represents.
    float getDiameter() const { return mDiameter; }

    static void ParseJson(const char *jsonStrOfMapFile,
                          FixedMap<Str, ScreenMap, 16> *segmentMaps);

    static void toJsonStr(const FixedMap<Str, ScreenMap, 16>&, Str* jsonBuffer);
    static void toJson(const FixedMap<Str, ScreenMap, 16>&, FLArduinoJson::JsonDocument* doc);

  private:
    static const pair_xy_float &empty() {
        static const pair_xy_float s_empty = pair_xy_float(0, 0);
        return s_empty;
    }
    uint32_t length = 0;
    float mDiameter = -1.0f;  // Only serialized if it's not > 0.0f.
    LUTXYFLOATRef mLookUpTable;
};


FASTLED_NAMESPACE_END
