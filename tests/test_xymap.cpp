// Unit tests for XYMap basic functionality and LUT vs user-function compatibility

#include "test.h"
#include "FastLED.h"

using namespace fl;

namespace {
// Helper to fill a LUT for serpentine layout
void fill_serpentine_lut(u16* out, u16 width, u16 height) {
    for (u16 y = 0; y < height; ++y) {
        for (u16 x = 0; x < width; ++x) {
            const u16 idx = y * width + x;
            out[idx] = xy_serpentine(x, y, width, height);
        }
    }
}

// Helper to fill a custom irregular LUT (not serpentine or linear) to simulate
// non-standard layouts users often provide.
void fill_custom_irregular_lut(u16* out, u16 width, u16 height) {
    // Simple reversible scramble: reverse X on even rows, reverse Y on odd rows
    // then map to line-by-line index. Deterministic and non-trivial.
    for (u16 y = 0; y < height; ++y) {
        for (u16 x = 0; x < width; ++x) {
            const u16 idx = y * width + x;
            const u16 xx = (y % 2 == 0) ? (u16)(width - 1 - x) : x;
            const u16 yy = (y % 2 == 1) ? (u16)(height - 1 - y) : y;
            out[idx] = (u16)(yy * width + xx);
        }
    }
}
} // namespace

TEST_CASE("XYMap - LUT and wrapped user function mappings are identical (serpentine)") {
    constexpr u16 W = 5;
    constexpr u16 H = 4;

    u16 lut[W * H];
    fill_serpentine_lut(lut, W, H);

    // Construct directly from LUT
    XYMap mapFromLut = XYMap::constructWithLookUpTable(W, H, lut);

    // Wrap via a pure formula user function (no external state)
    auto xy_from_serpentine_formula = +[](u16 x, u16 y, u16 width, u16 height) -> u16 {
        return xy_serpentine(x, y, width, height);
    };
    XYMap mapFromWrapped = XYMap::constructWithUserFunction(W, H, xy_from_serpentine_formula);

    // Validate indices match for all in-bounds coordinates
    for (u16 y = 0; y < H; ++y) {
        for (u16 x = 0; x < W; ++x) {
            CHECK_EQ(mapFromLut.mapToIndex(x, y), mapFromWrapped.mapToIndex(x, y));
        }
    }

    // Also validate that applying the same positive offset keeps them identical
    constexpr u16 OFFSET = 7;
    XYMap mapFromLutOffset = XYMap::constructWithLookUpTable(W, H, lut, OFFSET);
    XYMap mapFromWrappedOffset = XYMap::constructWithUserFunction(W, H, xy_from_serpentine_formula, OFFSET);
    for (u16 y = 0; y < H; ++y) {
        for (u16 x = 0; x < W; ++x) {
            CHECK_EQ(mapFromLutOffset.mapToIndex(x, y), mapFromWrappedOffset.mapToIndex(x, y));
        }
    }
}

TEST_CASE("XYMap - LUT and wrapped user function mappings are identical (custom irregular)") {
    constexpr u16 W = 6;
    constexpr u16 H = 5;

    u16 lut[W * H];
    fill_custom_irregular_lut(lut, W, H);

    XYMap mapFromLut = XYMap::constructWithLookUpTable(W, H, lut);

    auto xy_from_irregular_formula = +[](u16 x, u16 y, u16 width, u16 height) -> u16 {
        const u16 xx = (y % 2 == 0) ? (u16)(width - 1 - x) : x;
        const u16 yy = (y % 2 == 1) ? (u16)(height - 1 - y) : y;
        return (u16)(yy * width + xx);
    };
    XYMap mapFromWrapped = XYMap::constructWithUserFunction(W, H, xy_from_irregular_formula);

    for (u16 y = 0; y < H; ++y) {
        for (u16 x = 0; x < W; ++x) {
            CHECK_EQ(mapFromLut.mapToIndex(x, y), mapFromWrapped.mapToIndex(x, y));
        }
    }
}
