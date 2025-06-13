#include "fl/tile2x2.h"
#include "crgb.h"
#include "fl/draw_visitor.h"
#include "fl/raster.h"
#include "fl/raster_sparse.h"
#include "fl/unused.h"
#include "fl/warn.h"
#include "fl/xymap.h"

using namespace fl;

namespace fl {

namespace {
static vec2i16 wrap(const vec2i16 &v, const vec2i16 &size) {
    // Wrap the vector v around the size
    return vec2i16(v.x % size.x, v.y % size.y);
}

static vec2i16 wrap_x(const vec2i16 &v, const uint16_t width) {
    // Wrap the x component of the vector v around the size
    return vec2i16(v.x % width, v.y);
}
} // namespace


void Tile2x2_u8::Rasterize(const Slice<const Tile2x2_u8> &tiles,
                           XYRasterU8Sparse *out_raster) {
    out_raster->rasterize(tiles);
}

void Tile2x2_u8::draw(const CRGB &color, const XYMap &xymap, CRGB *out) const {
    XYDrawComposited visitor(color, xymap, out);
    draw(xymap, visitor);
}

void Tile2x2_u8::scale(uint8_t scale) {
    // scale the tile
    if (scale == 255) {
        return;
    }
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            uint16_t value = at(x, y);
            at(x, y) = (value * scale) >> 8;
        }
    }
}



Tile2x2_u8_wrap::Tile2x2_u8_wrap(const Tile2x2_u8 &from, uint16_t width) {
    const vec2i16 origin = from.origin();
    at(0, 0) = {wrap_x(vec2i16(origin.x, origin.y), width), from.at(0, 0)};
    at(0, 1) = {wrap_x(vec2i16(origin.x, origin.y + 1), width), from.at(0, 1)};
    at(1, 0) = {wrap_x(vec2i16(origin.x + 1, origin.y), width), from.at(1, 0)};
    at(1, 1) = {wrap_x(vec2i16(origin.x + 1, origin.y + 1), width),
                from.at(1, 1)};
}

Tile2x2_u8_wrap::Data &Tile2x2_u8_wrap::at(uint16_t x, uint16_t y) {
    // Wrap around the edges
    x = (x + 2) % 2;
    y = (y + 2) % 2;
    return tile[y][x];
}

const Tile2x2_u8_wrap::Data &Tile2x2_u8_wrap::at(uint16_t x, uint16_t y) const {
    // Wrap around the edges
    x = (x + 2) % 2;
    y = (y + 2) % 2;
    return tile[y][x];
}



Tile2x2_u8_wrap::Tile2x2_u8_wrap(const Tile2x2_u8 &from, uint16_t width, uint16_t height) {
    const vec2i16 origin = from.origin();
    at(0, 0) = {wrap(vec2i16(origin.x, origin.y), vec2i16(width, height)),
                from.at(0, 0)};
    at(0, 1) = {wrap(vec2i16(origin.x, origin.y + 1), vec2i16(width, height)),
                from.at(0, 1)};
    at(1, 0) = {wrap(vec2i16(origin.x + 1, origin.y), vec2i16(width, height)),
                from.at(1, 0)};
    at(1, 1) = {wrap(vec2i16(origin.x + 1, origin.y + 1), vec2i16(width, height)),
                from.at(1, 1)};
}

} // namespace fl
