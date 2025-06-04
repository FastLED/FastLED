
#pragma once

/*
A sparse path through an xy grid. When a value is set != 0, it will get stored
in the sparse grid. The raster will only store the values that are set, and will
not allocate memory for the entire grid. This is useful for large grids where
only a small number of pixels are set.
*/

#include <stdint.h>

#include "fl/geometry.h"
#include "fl/grid.h"
#include "fl/hash_map.h"
#include "fl/map.h"
#include "fl/namespace.h"
#include "fl/slice.h"
#include "fl/tile2x2.h"
#include "fl/xymap.h"

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END

#ifndef FASTLED_RASTER_SPARSE_INLINED_COUNT
#define FASTLED_RASTER_SPARSE_INLINED_COUNT 128
#endif

namespace fl {

class XYMap;
class Gradient;
class Tile2x2_u8;

// A raster of uint8_t values. This is a sparse raster, meaning that it will
// only store the values that are set.
class XYRasterU8Sparse {
  public:
    XYRasterU8Sparse() = default;
    XYRasterU8Sparse(int width, int height) {
        setBounds(rect<int16_t>(0, 0, width, height));
    }
    XYRasterU8Sparse(const XYRasterU8Sparse &) = delete;

    XYRasterU8Sparse &reset() {
        mSparseGrid.clear();
        mCache.clear();
        return *this;
    }

    XYRasterU8Sparse &clear() { return reset(); }

    // Rasterizes point with a value For best visual results, you'll want to
    // rasterize tile2x2 tiles, which are generated for you by the XYPathRenderer
    // to represent sub pixel / neightbor splatting positions along a path.
    // TODO: Bring the math from XYPathRenderer::at_subpixel(float alpha)
    // into a general purpose function.
    void rasterize(const vec2<int16_t> &pt, uint8_t value) {
        // Turn it into a Tile2x2_u8 tile and see if we can cache it.
        write(pt, value);
    }

    void setSize(uint16_t width, uint16_t height) {
        setBounds(rect<int16_t>(0, 0, width, height));
    }

    void setBounds(const rect<int16_t> &bounds) {
        mAbsoluteBounds = bounds;
        mAbsoluteBoundsSet = true;
    }

    using iterator = fl::HashMap<vec2<int16_t>, uint8_t>::iterator;
    using const_iterator = fl::HashMap<vec2<int16_t>, uint8_t>::const_iterator;

    iterator begin() { return mSparseGrid.begin(); }
    const_iterator begin() const { return mSparseGrid.begin(); }
    iterator end() { return mSparseGrid.end(); }
    const_iterator end() const { return mSparseGrid.end(); }
    size_t size() const { return mSparseGrid.size(); }
    bool empty() const { return mSparseGrid.empty(); }

    void rasterize(const Slice<const Tile2x2_u8> &tiles);
    void rasterize(const Tile2x2_u8 &tile) { rasterize_internal(tile); }

    void rasterize_internal(const Tile2x2_u8 &tile,
                            const rect<int16_t> *optional_bounds = nullptr);

    // Renders the subpixel tiles to the raster. Any previous data is
    // cleared. Memory will only be allocated if the size of the raster
    // increased. void rasterize(const Slice<const Tile2x2_u8> &tiles);
    // uint8_t &at(uint16_t x, uint16_t y) { return mGrid.at(x, y); }
    // const uint8_t &at(uint16_t x, uint16_t y) const { return mGrid.at(x,
    // y); }

    Pair<bool, uint8_t> at(uint16_t x, uint16_t y) const {
        const uint8_t *val = mSparseGrid.find_value(vec2<int16_t>(x, y));
        if (val != nullptr) {
            return {true, *val};
        }
        return {false, 0};
    }

    rect<int16_t> bounds() const {
        if (mAbsoluteBoundsSet) {
            return mAbsoluteBounds;
        }
        return bounds_pixels();
    }

    rect<int16_t> bounds_pixels() const {
        int min_x = 0;
        bool min_x_set = false;
        int min_y = 0;
        bool min_y_set = false;
        int max_x = 0;
        bool max_x_set = false;
        int max_y = 0;
        bool max_y_set = false;
        for (const auto &it : mSparseGrid) {
            const vec2<int16_t> &pt = it.first;
            if (!min_x_set || pt.x < min_x) {
                min_x = pt.x;
                min_x_set = true;
            }
            if (!min_y_set || pt.y < min_y) {
                min_y = pt.y;
                min_y_set = true;
            }
            if (!max_x_set || pt.x > max_x) {
                max_x = pt.x;
                max_x_set = true;
            }
            if (!max_y_set || pt.y > max_y) {
                max_y = pt.y;
                max_y_set = true;
            }
        }
        return rect<int16_t>(min_x, min_y, max_x + 1, max_y + 1);
    }

    // Warning! - SLOW.
    uint16_t width() const { return bounds().width(); }
    uint16_t height() const { return bounds().height(); }

    void draw(const CRGB &color, const XYMap &xymap, CRGB *out);

    void drawGradient(const Gradient &gradient, const XYMap &xymap, CRGB *out);

    // Inlined, yet customizable drawing access. This will only send you
    // pixels that are within the bounds of the XYMap.
    template <typename XYVisitor>
    void draw(const XYMap &xymap, XYVisitor &visitor) {
        for (const auto &it : mSparseGrid) {
            auto pt = it.first;
            if (!xymap.has(pt.x, pt.y)) {
                continue;
            }
            uint32_t index = xymap(pt.x, pt.y);
            uint8_t value = it.second;
            if (value > 0) { // Something wrote here.
                visitor.draw(pt, index, value);
            }
        }
    }

    static const int kMaxCacheSize = 8; // Max size for tiny cache.

    void write(const vec2<int16_t> &pt, uint8_t value) {
        // FASTLED_WARN("write: " << pt.x << "," << pt.y << " value: " <<
        // value); mSparseGrid.insert(pt, value);

        uint8_t **cached = mCache.find_value(pt);
        if (cached) {
            uint8_t *val = *cached;
            if (*val < value) {
                *val = value;
            }
            return;
        }
        if (mCache.size() <= kMaxCacheSize) {
            // cache it.
            uint8_t *v = mSparseGrid.find_value(pt);
            if (v == nullptr) {
                // FASTLED_WARN("write: " << pt.x << "," << pt.y << " value: "
                // << value);
                if (mSparseGrid.needs_rehash()) {
                    // mSparseGrid is about to rehash, so we need to clear the
                    // small cache because it shares pointers.
                    mCache.clear();
                }

                mSparseGrid.insert(pt, value);
                return;
            }
            mCache.insert(pt, v);
            if (*v < value) {
                *v = value;
            }
            return;
        } else {
            // overflow, clear cache and write directly.
            mCache.clear();
            mSparseGrid.insert(pt, value);
            return;
        }
    }

  private:
    using Key = vec2<int16_t>;
    using Value = uint8_t;
    using HashKey = Hash<Key>;
    using EqualToKey = EqualTo<Key>;
    using FastHashKey = FastHash<Key>;
    using HashMapLarge = fl::HashMap<Key, Value, HashKey, EqualToKey,
                                     FASTLED_HASHMAP_INLINED_COUNT>;
    HashMapLarge mSparseGrid;
    // Small cache for the last N writes to help performance.
    HashMap<vec2<int16_t>, uint8_t *, FastHashKey, EqualToKey, kMaxCacheSize>
        mCache;
    fl::rect<int16_t> mAbsoluteBounds;
    bool mAbsoluteBoundsSet = false;
};

} // namespace fl