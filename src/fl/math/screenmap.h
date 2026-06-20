#pragma once


#include "fl/stl/flat_map.h"  // Required: fl_map is a template alias, cannot be forward declared
#include "fl/stl/shared_ptr.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
/* Screenmap maps strip indexes to x,y coordinates. This is used for FastLED Web
 * to map the 1D strip to a 2D screen. Note that the strip can have arbitrary
 * size. this was first motivated by the effort to port theChromancer project to
 * FastLED for the browser.
 */

namespace fl {

// Forward declarations - full definitions only needed in .cpp
class string;
class json;
template<typename Signature> class function;

// Forward declare geometry types
template<typename T> struct vec2;
using vec2f = vec2<float>;

// Forward declare XYMap for optional source storage
class XYMap;

// Forward declare LUT types and smart pointers
template<typename T> class LUT;
using LUTXYFLOAT = LUT<vec2f>;
template<typename T> class shared_ptr;  // IWYU pragma: keep
using LUTXYFLOATPtr = shared_ptr<LUTXYFLOAT>;
using XYMapPtr = shared_ptr<XYMap>;

// ScreenMap screen map maps strip indexes to x,y coordinates for a ui
// canvas in float format.
// This class is cheap to copy as it uses smart pointers for shared data.
class ScreenMap {
  public:
    static ScreenMap Circle(int numLeds, float cm_between_leds = 1.5f,
                            float cm_led_diameter = 0.5f,
                            float completion = 1.0f) FL_NO_EXCEPT;

    static ScreenMap DefaultStrip(int numLeds, float cm_between_leds = 1.5f,
                                  float cm_led_diameter = 0.2f,
                                  float completion = .9f) FL_NO_EXCEPT;

    // Constructors and destructor - implemented in .cpp for proper smart_ptr handling
    ScreenMap() FL_NO_EXCEPT;
    ~ScreenMap() FL_NO_EXCEPT;

    // is_reverse is false by default for linear layout
    ScreenMap(u32 length, float mDiameter = -1.0f) FL_NO_EXCEPT;

    ScreenMap(const vec2f *lut, u32 length, float diameter = -1.0) FL_NO_EXCEPT;

    template <u32 N>
    ScreenMap(const vec2f (&lut)[N], float diameter = -1.0) FL_NO_EXCEPT
        : ScreenMap(lut, N, diameter) {}

    // Constructor with functor to fill in values
    ScreenMap(int count, float diameter, fl::function<void(int, vec2f& pt_out)> func) FL_NO_EXCEPT;

    ScreenMap(const ScreenMap &other) FL_NO_EXCEPT;
    ScreenMap(ScreenMap&& other) FL_NO_EXCEPT;

    const vec2f &operator[](u32 x) const FL_NO_EXCEPT;

    void set(u16 index, const vec2f &p) FL_NO_EXCEPT;

    void addOffset(const vec2f &p) FL_NO_EXCEPT;
    ScreenMap& addOffsetX(float x) FL_NO_EXCEPT;
    ScreenMap& addOffsetY(float y) FL_NO_EXCEPT;

    vec2f &operator[](u32 x) FL_NO_EXCEPT;

    // TODO: change this name to setDiameterLed. Default should be .5f
    // for 5 mm ws lense.
    void setDiameter(float diameter) FL_NO_EXCEPT;

    // define the assignment operator
    ScreenMap &operator=(const ScreenMap &other) FL_NO_EXCEPT;
    ScreenMap &operator=(ScreenMap &&other) FL_NO_EXCEPT;

    vec2f mapToIndex(u32 x) const FL_NO_EXCEPT;

    u32 getLength() const FL_NO_EXCEPT;
    // The diameter each point represents.
    float getDiameter() const FL_NO_EXCEPT;

    // Get the bounding box of all points in the screen map
    vec2f getBounds() const FL_NO_EXCEPT;

    static bool ParseJson(const char *jsonStrScreenMap,
                          fl::flat_map<string, ScreenMap> *segmentMaps,
                          string *err = nullptr) FL_NO_EXCEPT;

    static bool ParseJson(const char *jsonStrScreenMap,
                          const char *screenMapName, ScreenMap *screenmap,
                          string *err = nullptr) FL_NO_EXCEPT;

    static void toJsonStr(const fl::flat_map<string, ScreenMap> &,
                          string *jsonBuffer) FL_NO_EXCEPT;
    static void toJson(const fl::flat_map<string, ScreenMap> &, fl::json *doc) FL_NO_EXCEPT;

    /// @brief Set the source XYMap (used for pixel transformation during encoding)
    /// @param xymap Source XYMap that this ScreenMap was created from (can be nullptr)
    void setSourceXYMap(const fl::shared_ptr<XYMap>& xymap) FL_NO_EXCEPT;

    /// @brief Get the source XYMap shared pointer if available
    /// @return Shared pointer to source XYMap or nullptr if not set
    const XYMapPtr& getSourceXYMapPtr() const FL_NO_EXCEPT;

    /// @brief Get the source XYMap as a raw const pointer
    /// @return Raw const pointer to source XYMap or nullptr if not set
    const XYMap* getXYMap() const FL_NO_EXCEPT;

    /// @brief Check if source XYMap is available
    /// @return true if source XYMap was stored
    bool hasSourceXYMap() const FL_NO_EXCEPT;

  private:
    static const vec2f &empty() FL_NO_EXCEPT;
    u32 length = 0;
    float mDiameter = -1.0f; // Only serialized if it's not > 0.0f.
    LUTXYFLOATPtr mLookUpTable;
    XYMapPtr mSourceXYMap;  // Optional: source XYMap for encoding pipeline
};

} // namespace fl
