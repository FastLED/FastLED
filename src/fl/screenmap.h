#pragma once

#include "fl/stdint.h"

#include "fl/force_inline.h"
#include "fl/lut.h"
#include "fl/memory.h"

#include "fl/map.h"
#include "fl/namespace.h"
#include "fl/str.h"
#include "fl/json.h"

/* Screenmap maps strip indexes to x,y coordinates. This is used for FastLED Web
 * to map the 1D strip to a 2D screen. Note that the strip can have arbitrary
 * size. this was first motivated by the effort to port theChromancer project to
 * FastLED for the browser.
 */

 // CONVERT JSON TO JSON2
 // DON'T USE JSON

namespace fl {

class string;
class Json;

// Forward declaration for internal helper function
fl::vector<float> jsonArrayToFloatVector(const fl::Json& jsonArray);

// ScreenMap screen map maps strip indexes to x,y coordinates for a ui
// canvas in float format.
// This class is cheap to copy as it uses smart pointers for shared data.
class ScreenMap {
  public:
    static ScreenMap Circle(int numLeds, float cm_between_leds = 1.5f,
                            float cm_led_diameter = 0.5f,
                            float completion = 1.0f);

    static ScreenMap DefaultStrip(int numLeds, float cm_between_leds = 1.5f,
                                  float cm_led_diameter = 0.2f,
                                  float completion = .9f) {
        return Circle(numLeds, cm_between_leds, cm_led_diameter, completion);
    }

    ScreenMap() = default;

    // is_reverse is false by default for linear layout
    ScreenMap(u32 length, float mDiameter = -1.0f);

    ScreenMap(const vec2f *lut, u32 length, float diameter = -1.0);

    template <u32 N>
    ScreenMap(const vec2f (&lut)[N], float diameter = -1.0)
        : ScreenMap(lut, N, diameter) {}

    ScreenMap(const ScreenMap &other);
    ScreenMap(ScreenMap&& other);

    const vec2f &operator[](u32 x) const;

    void set(u16 index, const vec2f &p);

    void addOffset(const vec2f &p);
    void addOffsetX(float x);
    void addOffsetY(float y);

    vec2f &operator[](u32 x);

    // TODO: change this name to setDiameterLed. Default should be .5f
    // for 5 mm ws lense.
    void setDiameter(float diameter);

    // define the assignment operator
    ScreenMap &operator=(const ScreenMap &other);
    ScreenMap &operator=(ScreenMap &&other);

    vec2f mapToIndex(u32 x) const;

    u32 getLength() const;
    // The diameter each point represents.
    float getDiameter() const;

    // Get the bounding box of all points in the screen map
    vec2f getBounds() const;

    static bool ParseJson(const char *jsonStrScreenMap,
                          fl::fl_map<string, ScreenMap> *segmentMaps,
                          string *err = nullptr);

    static bool ParseJson(const char *jsonStrScreenMap,
                          const char *screenMapName, ScreenMap *screenmap,
                          string *err = nullptr);

    static void toJsonStr(const fl::fl_map<string, ScreenMap> &,
                          string *jsonBuffer);
    static void toJson(const fl::fl_map<string, ScreenMap> &, fl::Json *doc);

  private:
    static const vec2f &empty();
    u32 length = 0;
    float mDiameter = -1.0f; // Only serialized if it's not > 0.0f.
    LUTXYFLOATPtr mLookUpTable;
};

} // namespace fl
