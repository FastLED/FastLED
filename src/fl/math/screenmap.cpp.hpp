/* Screenmap maps strip indexes to x,y coordinates. This is used for FastLED Web
 * to map the 1D strip to a 2D grid. Note that the strip can have arbitrary
 * size. this was first motivated during the (attempted? Oct. 19th 2024) port of
 * the Chromancer project to FastLED Web.
 */

#include "fl/math/screenmap.h"

// Heavy includes moved from header to reduce compilation time
#include "fl/math/lut.h"       // Full LUT definitions needed for implementation
#include "fl/stl/string.h"       // 129.4ms - only needed for string parameters in implementations
#include "fl/stl/flat_map.h"  // 12.4ms - only needed for flat_map parameters in implementations
// IWYU pragma: begin_keep
#include "fl/stl/function.h"
// IWYU pragma: end_keep  // ~5ms - only needed for function<> constructor implementation
#include "fl/stl/static_assert.h"

// Other implementation dependencies
#include "fl/math/math.h"
#include "fl/math/math.h"
#include "fl/stl/vector.h"
#include "fl/log/log.h"
#include "fl/stl/noexcept.h"


namespace fl {

namespace {

// Every value that reaches `mDiameter` from a caller passes through here.
//
// The class accepts any `float` -- three constructors and `setDiameter` all
// take one unfiltered -- but only two kinds of value mean anything: a
// positive size, and the -1 sentinel for "unset". A caller who stores 0.0f
// or -2.0f has named a diameter no LED has. The emitter cannot publish
// those (a consumer reading `"diameter": -2` learns nothing it can use) and
// the parsers read an absent key back as -1, so before this normalization a
// stored 0.0f came back from a round trip as -1.0f: silently different from
// what was set, and different in a way `getDiameter()` did not show until
// the file had been through JSON.
//
// Folding them to the sentinel at the boundary makes the two agree. A
// non-positive diameter now reads back as -1 immediately, from the setter
// onward, so the round trip is the identity on every value the class holds.
float normalizedDiameter(float diameter) FL_NO_EXCEPT {
    return diameter > 0.0f ? diameter : -1.0f;
}

}  // namespace

// Default constructor and destructor - must be in .cpp for proper smart_ptr handling
ScreenMap::ScreenMap() FL_NO_EXCEPT = default;
ScreenMap::~ScreenMap() FL_NO_EXCEPT = default;

ScreenMap ScreenMap::Circle(int numLeds, float cm_between_leds,
                            float cm_led_diameter, float completion) FL_NO_EXCEPT {
    ScreenMap screenMap(numLeds);

    // radius from LED spacing
    float circumference = numLeds * cm_between_leds;
    float radius = circumference / (2 * FL_PI);

    // how big an arc we light vs leave dark
    float totalAngle = completion * 2 * FL_PI;
    float gapAngle = 2 * FL_PI - totalAngle;

    // shift so the dark gap is centered at the bottom (–π/2)
    float startAngle = -FL_PI / 2 + gapAngle / 2.0f;

    // if partial, land last LED exactly at startAngle+totalAngle
    float divisor =
        (completion < 1.0f && numLeds > 1) ? (numLeds - 1) : numLeds;

    for (int i = 0; i < numLeds; ++i) {
        float angle = startAngle + (i * totalAngle) / divisor;
        float x = radius * cos(angle) * 2;
        float y = radius * sin(angle) * 2;
        screenMap[i] = {x, y};
    }

    screenMap.setDiameter(cm_led_diameter);
    return screenMap;
}

ScreenMap ScreenMap::DefaultStrip(int numLeds, float cm_between_leds,
                                  float cm_led_diameter, float completion) FL_NO_EXCEPT {
    return Circle(numLeds, cm_between_leds, cm_led_diameter, completion);
}

ScreenMap::ScreenMap(u32 length, float mDiameter) FL_NO_EXCEPT
    : length(length), mDiameter(normalizedDiameter(mDiameter)) {
    if (length > 0) {
        mLookUpTable = fl::make_shared<LUTXYFLOAT>(length);
        LUTXYFLOAT &lut = *mLookUpTable.get();
        vec2f *data = lut.getDataMutable();
        for (u32 x = 0; x < length; x++) {
            data[x] = {0, 0};
        }
    }
}

ScreenMap::ScreenMap(const vec2f *lut, u32 length, float diameter) FL_NO_EXCEPT
    : length(length), mDiameter(normalizedDiameter(diameter)) {
    mLookUpTable = fl::make_shared<LUTXYFLOAT>(length);
    LUTXYFLOAT &lut16xy = *mLookUpTable.get();
    vec2f *data = lut16xy.getDataMutable();
    for (u32 x = 0; x < length; x++) {
        data[x] = lut[x];
    }
}

ScreenMap::ScreenMap(int count, float diameter, fl::function<void(int, vec2f& pt_out)> func) FL_NO_EXCEPT
    : length(count), mDiameter(normalizedDiameter(diameter)) {
    if (count > 0) {
        mLookUpTable = fl::make_shared<LUTXYFLOAT>(count);
        LUTXYFLOAT &lut = *mLookUpTable.get();
        vec2f *data = lut.getDataMutable();
        for (int i = 0; i < count; i++) {
            func(i, data[i]);
        }
    }
}

ScreenMap::ScreenMap(const ScreenMap &other) FL_NO_EXCEPT {
    mDiameter = other.mDiameter;
    length = other.length;
    mLookUpTable = other.mLookUpTable;
    mSourceXYMap = other.mSourceXYMap;
    mShapes = other.mShapes;
}

ScreenMap::ScreenMap(ScreenMap&& other) FL_NO_EXCEPT {
    mDiameter = other.mDiameter;
    length = other.length;
    fl::swap(mLookUpTable, other.mLookUpTable);
    fl::swap(mSourceXYMap, other.mSourceXYMap);
    fl::swap(mShapes, other.mShapes);
    other.mLookUpTable.reset();
    other.mSourceXYMap.reset();
}

void ScreenMap::set(u16 index, const vec2f &p) FL_NO_EXCEPT {
    if (mLookUpTable) {
        LUTXYFLOAT &lut = *mLookUpTable.get();
        auto *data = lut.getDataMutable();
        data[index] = p;
    }
}

void ScreenMap::setDiameter(float diameter) FL_NO_EXCEPT {
    mDiameter = normalizedDiameter(diameter);
}

vec2f ScreenMap::mapToIndex(u32 x) const FL_NO_EXCEPT {
    if (x >= length || !mLookUpTable) {
        return {0, 0};
    }
    LUTXYFLOAT &lut = *mLookUpTable.get();
    vec2f screen_coords = lut[x];
    return screen_coords;
}

u32 ScreenMap::getLength() const FL_NO_EXCEPT { return length; }

float ScreenMap::getDiameter() const FL_NO_EXCEPT { return mDiameter; }

vec2f ScreenMap::getBounds() const FL_NO_EXCEPT {

    if (length == 0 || !mLookUpTable) {
        return {0, 0};
    }

    LUTXYFLOAT &lut = *mLookUpTable.get();

    fl::vec2f *data = lut.getDataMutable();
    // float minX = lut[0].x;
    // float maxX = lut[0].x;
    // float minY = lut[0].y;
    // float maxY = lut[0].y;
    float minX = data[0].x;
    float maxX = data[0].x;
    float minY = data[0].y;
    float maxY = data[0].y;

    for (u32 i = 1; i < length; i++) {
        const vec2f &p = lut[i];
        minX = fl::min(minX, p.x);
        maxX = fl::max(maxX, p.x);
        minY = fl::min(minY, p.y);
        maxY = fl::max(maxY, p.y);
    }

    return {maxX - minX, maxY - minY};
}

void ScreenMap::setShape(u32 index, Shape::Type type, const vec2f *vertices, u32 count,
                         float thickness) FL_NO_EXCEPT {
    if (!vertices || count == 0) return;
    if (index >= mShapes.size()) mShapes.resize(index + 1);
    Shape &shape = mShapes[index];
    shape.type = type;
    shape.thickness = thickness;
    shape.vertices.clear();
    shape.vertices.reserve(count);
    for (u32 i = 0; i < count; ++i) shape.vertices.push_back(vertices[i]);
}

bool ScreenMap::hasShapes() const FL_NO_EXCEPT { return !mShapes.empty(); }
u32 ScreenMap::getShapeCount() const FL_NO_EXCEPT { return static_cast<u32>(mShapes.size()); }

const ScreenMap::Shape &ScreenMap::getShape(u32 index) const FL_NO_EXCEPT {
    static const Shape emptyShape = [] {
        Shape shape;
        shape.type = Shape::EL_PANEL;
        return shape;
    }();
    return index < mShapes.size() ? mShapes[index] : emptyShape;
}

const vec2f &ScreenMap::empty() FL_NO_EXCEPT {
    static const vec2f s_empty = vec2f(0, 0); // okay static in header
    return s_empty;
}

const vec2f &ScreenMap::operator[](u32 x) const FL_NO_EXCEPT {
    if (x >= length || !mLookUpTable) {
        return empty(); // better than crashing.
    }
    LUTXYFLOAT &lut = *mLookUpTable.get();
    return lut[x];
}

vec2f &ScreenMap::operator[](u32 x) FL_NO_EXCEPT {
    if (x >= length || !mLookUpTable) {
        return const_cast<vec2f &>(empty()); // better than crashing.
    }
    LUTXYFLOAT &lut = *mLookUpTable.get();
    auto *data = lut.getDataMutable();
    return data[x];
}

ScreenMap &ScreenMap::operator=(const ScreenMap &other) FL_NO_EXCEPT {
    if (this != &other) {
        mDiameter = other.mDiameter;
        length = other.length;
        mLookUpTable = other.mLookUpTable;
        mSourceXYMap = other.mSourceXYMap;
        mShapes = other.mShapes;
    }
    return *this;
}

ScreenMap &ScreenMap::operator=(ScreenMap &&other) FL_NO_EXCEPT {
    if (this != &other) {
        mDiameter = other.mDiameter;
        length = other.length;
        mLookUpTable = fl::move(other.mLookUpTable);
        mSourceXYMap = fl::move(other.mSourceXYMap);
        mShapes = fl::move(other.mShapes);
        other.length = 0;
        other.mDiameter = -1.0f;
    }
    return *this;
}

void ScreenMap::setSourceXYMap(const fl::shared_ptr<XYMap>& xymap) FL_NO_EXCEPT {
    mSourceXYMap = xymap;
}

const XYMapPtr& ScreenMap::getSourceXYMapPtr() const FL_NO_EXCEPT {
    return mSourceXYMap;
}

const XYMap* ScreenMap::getXYMap() const FL_NO_EXCEPT {
    return mSourceXYMap.get();
}

bool ScreenMap::hasSourceXYMap() const FL_NO_EXCEPT {
    return mSourceXYMap != nullptr;
}

void ScreenMap::addOffset(const vec2f &p) FL_NO_EXCEPT {
    vec2f *data = mLookUpTable->getDataMutable();
    for (u32 i = 0; i < length; i++) {
        vec2f &curr = data[i];
        curr.x += p.x;
        curr.y += p.y;
    }
}

ScreenMap& ScreenMap::addOffsetX(float x) FL_NO_EXCEPT { addOffset({x, 0}); return *this; }
ScreenMap& ScreenMap::addOffsetY(float y) FL_NO_EXCEPT { addOffset({0, y}); return *this; }

} // namespace fl
