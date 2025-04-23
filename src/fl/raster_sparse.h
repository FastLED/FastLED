
#pragma once

#include <stdint.h>

#include "fl/grid.h"
#include "fl/namespace.h"
#include "fl/point.h"
#include "fl/slice.h"
#include "fl/hash_map.h"
#include "fl/xymap.h"

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END

namespace fl {

class XYMap;
class XYDrawUint8Visitor;
class SubPixel2x2;

class XYRasterSparse {
  public:
    XYRasterSparse() = default;
    XYRasterSparse(const XYRasterSparse &) = delete;

    XYRasterSparse &reset() {
        mSparseGrid.clear();
        mBounds.mMin = point_xy<int>(0, 0);
        mBounds.mMax = point_xy<int>(0, 0);
        return *this;
    }

    void add(const point_xy<int> &pt, uint8_t value) {
        mSparseGrid.insert(pt, value);
        mBounds.expand(pt);
    }

    // Renders the subpixel tiles to the raster. Any previous data is cleared.
    // Memory will only be allocated if the size of the raster increased.
    // void rasterize(const Slice<const SubPixel2x2> &tiles);
    // uint8_t &at(uint16_t x, uint16_t y) { return mGrid.at(x, y); }
    // const uint8_t &at(uint16_t x, uint16_t y) const { return mGrid.at(x, y); }

    Pair<bool, uint8_t> at(uint16_t x, uint16_t y) const {
        const uint16_t* val = mSparseGrid.find(point_xy<int>(x, y));
        // if (it != mSparseGrid.end()) {
        //     return {true, it.second};
        // }
        if (val != nullptr) {
            return {true, *val};
        }
        return {false, 0};
    }

    const point_xy<int> origin() const { return mBounds.mMin; }
    point_xy<int> global_min() const { return origin(); }
    point_xy<int> global_max() const {
        return mBounds.mMax;
    }

    rect_xy<int> bounds() const {
        point_xy<int> min = origin();
        point_xy<int> max = global_max();
        return rect_xy<int>(min, max);
    }

    uint16_t width() const { return mBounds.width(); }
    uint16_t height() const { return mBounds.height(); }

    void draw(const CRGB &color, const XYMap &xymap, CRGB *out) const;

    // Inlined, yet customizable drawing access. This will only send you pixels
    // that are within the bounds of the XYMap.
    template <typename XYVisitor>
    void draw(const XYMap &xymap, XYVisitor& visitor) const {
        for (const auto& it : mSparseGrid) {
            auto pt = it.first;
            if (!xymap.has(pt.x, pt.y)) {
                continue;
            }
            uint32_t index = xymap(pt.x, pt.y);
            uint8_t value = it.second;
            if (value > 0) {  // Something wrote here.
                visitor.draw(pt, index, value);
            }
        }
    }

  private:
    fl::HashMap<point_xy<int>, uint16_t> mSparseGrid;
    fl::rect_xy<int> mBounds;
};

} // namespace fl