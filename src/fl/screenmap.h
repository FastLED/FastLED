#pragma once

#include <stdint.h>

#include "fl/force_inline.h"
#include "fl/lut.h"
#include "fl/ptr.h"


#include "fl/map.h"
#include "fl/str.h"
#include "fl/namespace.h"

/* Screenmap maps strip indexes to x,y coordinates. This is used for FastLED Web
 * to map the 1D strip to a 2D screen. Note that the strip can have arbitrary
 * size. this was first motivated by the effort to port theChromancer project to
 * FastLED for the browser.
 */

namespace fl {

class Str;
class JsonDocument;

// ScreenMap screen map maps strip indexes to x,y coordinates for a ui
// canvas in float format.
// This class is cheap to copy as it uses smart pointers for shared data.
class ScreenMap {
  public:
    static ScreenMap Circle(int numLeds, float cm_between_leds = 1.5f,
                            float cm_led_diameter = 0.5f);

    ScreenMap() = default;

    // is_reverse is false by default for linear layout
    ScreenMap(uint32_t length, float mDiameter = -1.0f);

    ScreenMap(const pair_xy_float *lut, uint32_t length,
              float diameter = -1.0);

    template <uint32_t N>
    ScreenMap(const pair_xy_float (&lut)[N], float diameter = -1.0)
        : ScreenMap(lut, N, diameter) {}

    ScreenMap(const ScreenMap &other);

    const pair_xy_float &operator[](uint32_t x) const;

    void set(uint16_t index, const pair_xy_float &p);

    pair_xy_float &operator[](uint32_t x);

    // TODO: change this name to setDiameterLed. Default should be .5f
    // for 5 mm ws lense.
    void setDiameter(float diameter);

    // define the assignment operator
    ScreenMap &operator=(const ScreenMap &other);

    pair_xy_float mapToIndex(uint32_t x) const;

    uint32_t getLength() const;
    // The diameter each point represents.
    float getDiameter() const;

    static bool ParseJson(const char *jsonStrScreenMap,
                          FixedMap<Str, ScreenMap, 16> *segmentMaps,
                          Str *err = nullptr);

    static bool ParseJson(const char *jsonStrScreenMap,
                          const char *screenMapName, ScreenMap *screenmap,
                          Str *err = nullptr);

    static void toJsonStr(const FixedMap<Str, ScreenMap, 16> &,
                          Str *jsonBuffer);
    static void toJson(const FixedMap<Str, ScreenMap, 16> &,
                       JsonDocument *doc);

  private:
    static const pair_xy_float &empty();
    uint32_t length = 0;
    float mDiameter = -1.0f; // Only serialized if it's not > 0.0f.
    LUTXYFLOATPtr mLookUpTable;
};

} // namespace fl
