#pragma once

#include <stdint.h>

#include "fl/force_inline.h"
#include "fl/lut.h"
#include "fl/ptr.h"

#include "fl/str.h"
#include "fl/map.h"
#include "fl/json.h"
#include "namespace.h"

/* Screenmap maps strip indexes to x,y coordinates. This is used for FastLED Web
 * to map the 1D strip to a 2D grid. Note that the strip can have arbitrary
 * size. this was first motivated during the (attempted? Oct. 19th 2024) port of
 * the Chromancer project to FastLED Web.
 */

namespace fl {

class Str;



// ScreenMap screen map maps strip indexes to x,y coordinates for a ui
// canvas in float format.
// This class is cheap to copy as it uses smart pointers for shared data.
class ScreenMap {
  public:

    static ScreenMap Circle(int numLeds, float cm_between_leds = 1.5f, float cm_led_diameter = 0.5f);

    ScreenMap() = default;

    // is_reverse is false by default for linear layout
    ScreenMap(uint32_t length, float mDiameter = -1.0f) : length(length), mDiameter(mDiameter) {
        mLookUpTable = fl::LUTXYFLOATPtr::New(length);
        fl::LUTXYFLOAT &lut = *mLookUpTable.get();
        fl::pair_xy_float *data = lut.getData();
        for (uint32_t x = 0; x < length; x++) {
            data[x] = {0, 0};
        }
    }

    ScreenMap(const fl::pair_xy_float *lut, uint32_t length, float diameter = -1.0) : length(length), mDiameter(diameter) {
        mLookUpTable = fl::LUTXYFLOATPtr::New(length);
        fl::LUTXYFLOAT &lut16xy = *mLookUpTable.get();
        fl::pair_xy_float *data = lut16xy.getData();
        for (uint32_t x = 0; x < length; x++) {
            data[x] = lut[x];
        }
    }

    template <uint32_t N> ScreenMap(const fl::pair_xy_float (&lut)[N], float diameter = -1.0) : length(N), mDiameter(diameter) {
        mLookUpTable = fl::LUTXYFLOATPtr::New(length);
        fl::LUTXYFLOAT &lut16xy = *mLookUpTable.get();
        fl::pair_xy_float *data = lut16xy.getData();
        for (uint32_t x = 0; x < length; x++) {
            data[x] = lut[x];
        }
    }

    ScreenMap(const ScreenMap &other) {
        mDiameter = other.mDiameter;
        length = other.length;
        mLookUpTable = other.mLookUpTable;
    }

    const fl::pair_xy_float &operator[](uint32_t x) const {
        if (x >= length || !mLookUpTable) {
            return empty(); // better than crashing.
        }
        fl::LUTXYFLOAT &lut = *mLookUpTable.get();
        return lut[x];
    }

    void set(uint16_t index, const fl::pair_xy_float &p) {
        if (mLookUpTable) {
            fl::LUTXYFLOAT &lut = *mLookUpTable.get();
            auto *data = lut.getData();
            data[index] = p;
        }
    }

    fl::pair_xy_float& operator[](uint32_t x) {
        if (x >= length || !mLookUpTable) {
            return const_cast<fl::pair_xy_float &>(empty()); // better than crashing.
        }
        fl::LUTXYFLOAT &lut = *mLookUpTable.get();
        auto *data = lut.getData();
        return data[x];
    }

    // TODO: change this name to setDiameterLed. Default should be .5f
    // for 5 mm ws lense.
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

    fl::pair_xy_float mapToIndex(uint32_t x) const {
        if (x >= length || !mLookUpTable) {
            return {0, 0};
        }
        fl::LUTXYFLOAT &lut = *mLookUpTable.get();
        fl::pair_xy_float screen_coords = lut[x];
        return screen_coords;
    }

    uint32_t getLength() const { return length; }
    // The diameter each point represents.
    float getDiameter() const { return mDiameter; }

    static bool ParseJson(const char *jsonStrScreenMap,
                          fl::FixedMap<fl::Str, ScreenMap, 16> *segmentMaps,
                          fl::Str *err = nullptr);

    static bool ParseJson(const char *jsonStrScreenMap,
                          const char* screenMapName,
                          ScreenMap *screenmap,
                          fl::Str *err = nullptr);

    static void toJsonStr(const fl::FixedMap<fl::Str, ScreenMap, 16>&, fl::Str* jsonBuffer);
    static void toJson(const fl::FixedMap<fl::Str, ScreenMap, 16>&, FLArduinoJson::JsonDocument* doc);

  private:
    static const fl::pair_xy_float &empty() {
        static const fl::pair_xy_float s_empty = fl::pair_xy_float(0, 0);
        return s_empty;
    }
    uint32_t length = 0;
    float mDiameter = -1.0f;  // Only serialized if it's not > 0.0f.
    fl::LUTXYFLOATPtr mLookUpTable;
};

}  // namespace fl
