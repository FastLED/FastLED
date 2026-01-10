// Unit tests for XYMap basic functionality and LUT vs user-function compatibility

#include "FastLED.h"
#include "doctest.h"
#include "fl/xymap.h"
#include "fl/int.h"


namespace {
// Helper to fill a LUT for serpentine layout
void fill_serpentine_lut(fl::u16* out, fl::u16 width, fl::u16 height) {
    for (fl::u16 y = 0; y < height; ++y) {
        for (fl::u16 x = 0; x < width; ++x) {
            const fl::u16 idx = y * width + x;
            out[idx] = fl::xy_serpentine(x, y, width, height);
        }
    }
}

// Helper to fill a custom irregular LUT (not serpentine or linear) to simulate
// non-standard layouts users often provide.
void fill_custom_irregular_lut(fl::u16* out, fl::u16 width, fl::u16 height) {
    // Simple reversible scramble: reverse X on even rows, reverse Y on odd rows
    // then map to line-by-line index. Deterministic and non-trivial.
    for (fl::u16 y = 0; y < height; ++y) {
        for (fl::u16 x = 0; x < width; ++x) {
            const fl::u16 idx = y * width + x;
            const fl::u16 xx = (y % 2 == 0) ? (fl::u16)(width - 1 - x) : x;
            const fl::u16 yy = (y % 2 == 1) ? (fl::u16)(height - 1 - y) : y;
            out[idx] = (fl::u16)(yy * width + xx);
        }
    }
}
} // namespace

TEST_CASE("XYMap - LUT and wrapped user function mappings are identical (serpentine)") {
    constexpr fl::u16 W = 5;
    constexpr fl::u16 H = 4;

    fl::u16 lut[W * H];
    fill_serpentine_lut(lut, W, H);

    // Construct directly from LUT
    XYMap mapFromLut = XYMap::constructWithLookUpTable(W, H, lut);

    // Wrap via a pure formula user function (no external state)
    auto xy_from_serpentine_formula = +[](fl::u16 x, fl::u16 y, fl::u16 width, fl::u16 height) -> fl::u16 {
        return fl::xy_serpentine(x, y, width, height);
    };
    XYMap mapFromWrapped = XYMap::constructWithUserFunction(W, H, xy_from_serpentine_formula);

    // Validate indices match for all in-bounds coordinates
    for (fl::u16 y = 0; y < H; ++y) {
        for (fl::u16 x = 0; x < W; ++x) {
            CHECK_EQ(mapFromLut.mapToIndex(x, y), mapFromWrapped.mapToIndex(x, y));
        }
    }

    // Also validate that applying the same positive offset keeps them identical
    constexpr fl::u16 OFFSET = 7;
    XYMap mapFromLutOffset = XYMap::constructWithLookUpTable(W, H, lut, OFFSET);
    XYMap mapFromWrappedOffset = XYMap::constructWithUserFunction(W, H, xy_from_serpentine_formula, OFFSET);
    for (fl::u16 y = 0; y < H; ++y) {
        for (fl::u16 x = 0; x < W; ++x) {
            CHECK_EQ(mapFromLutOffset.mapToIndex(x, y), mapFromWrappedOffset.mapToIndex(x, y));
        }
    }
}

TEST_CASE("XYMap - LUT and wrapped user function mappings are identical (custom irregular)") {
    constexpr fl::u16 W = 6;
    constexpr fl::u16 H = 5;

    fl::u16 lut[W * H];
    fill_custom_irregular_lut(lut, W, H);

    XYMap mapFromLut = XYMap::constructWithLookUpTable(W, H, lut);

    auto xy_from_irregular_formula = +[](fl::u16 x, fl::u16 y, fl::u16 width, fl::u16 height) -> fl::u16 {
        const fl::u16 xx = (y % 2 == 0) ? (fl::u16)(width - 1 - x) : x;
        const fl::u16 yy = (y % 2 == 1) ? (fl::u16)(height - 1 - y) : y;
        return (fl::u16)(yy * width + xx);
    };
    XYMap mapFromWrapped = XYMap::constructWithUserFunction(W, H, xy_from_irregular_formula);

    for (fl::u16 y = 0; y < H; ++y) {
        for (fl::u16 x = 0; x < W; ++x) {
            CHECK_EQ(mapFromLut.mapToIndex(x, y), mapFromWrapped.mapToIndex(x, y));
        }
    }
}

TEST_CASE("XYMap - composing two 4x3 serpentine segments into a 4x6 matrix") {
    // Goal: Validate how two serpentine 4x3 segments (offsets 0 and 12) compose
    // into a 4x6 matrix, and whether they match a single 4x6 serpentine map.
    // Observation: With the built-in serpentine mapping, row parity resets per
    // segment (because y is reduced modulo height internally), which breaks
    // continuity across the segment boundary. Offset alone does not fix this.

    constexpr fl::u16 W = 4;
    constexpr fl::u16 H_SEG = 3;
    constexpr fl::u16 H_FULL = 6;

    // Reference: a single 4x6 serpentine mapping
    XYMap fullSerp = XYMap::constructSerpentine(W, H_FULL);

    // Two 4x3 serpentine segments, stacked vertically, with offsets 0 and 12
    XYMap segTop = XYMap::constructSerpentine(W, H_SEG, /*offset=*/0);
    XYMap segBottom = XYMap::constructSerpentine(W, H_SEG, /*offset=*/W * H_SEG);

    // Helper to compose index from the two segments using absolute (x,y)
    auto composedIndexSerp = [&](fl::u16 x, fl::u16 y) -> fl::u16 {
        if (y < H_SEG) {
            return segTop.mapToIndex(x, y);
        }
        return segBottom.mapToIndex(x, y);
    };

    SUBCASE("Default serpentine segments: top half matches; rows 3-5 mismatch due to parity reset") {
        // Rows 0..2 should match the single 4x6 map exactly
        for (fl::u16 y = 0; y < H_SEG; ++y) {
            for (fl::u16 x = 0; x < W; ++x) {
                CHECK_EQ(composedIndexSerp(x, y), fullSerp.mapToIndex(x, y));
            }
        }

        // Row 3 (y=3) is expected to mismatch due to parity reset
        {
            fl::u16 y = 3;
            CHECK_NE(composedIndexSerp(0, y), fullSerp.mapToIndex(0, y));
            CHECK_NE(composedIndexSerp(W - 1, y), fullSerp.mapToIndex(W - 1, y));
        }

        // Row 4 (y=4) also mismatches due to parity reset within the segment
        {
            fl::u16 y = 4;
            CHECK_NE(composedIndexSerp(0, y), fullSerp.mapToIndex(0, y));
            CHECK_NE(composedIndexSerp(W - 1, y), fullSerp.mapToIndex(W - 1, y));
        }

        // Row 5 (y=5) mismatches due to parity reset
        {
            fl::u16 y = 5;
            CHECK_NE(composedIndexSerp(0, y), fullSerp.mapToIndex(0, y));
            CHECK_NE(composedIndexSerp(W - 1, y), fullSerp.mapToIndex(W - 1, y));
        }
    }

    SUBCASE("User-function segments honoring absolute row parity match the 4x6 serpentine") {
        // Define a mapping that uses absolute y parity, but still indexes within the segment
        // height via y % height. This preserves boustrophedon continuity across segments.
        auto xy_abs_parity_serp = +[](fl::u16 x, fl::u16 y, fl::u16 width, fl::u16 height) -> fl::u16 {
            fl::u16 yy = static_cast<fl::u16>(y % height);
            fl::u16 base = static_cast<fl::u16>(yy * width);
            bool odd = (y & 1u) != 0u; // absolute row parity
            if (odd) {
                return static_cast<fl::u16>(base + (width - 1 - x));
            }
            return static_cast<fl::u16>(base + x);
        };

        XYMap segTopUF = XYMap::constructWithUserFunction(W, H_SEG, xy_abs_parity_serp, /*offset=*/0);
        XYMap segBottomUF = XYMap::constructWithUserFunction(W, H_SEG, xy_abs_parity_serp, /*offset=*/W * H_SEG);

        auto composedIndexUF = [&](fl::u16 x, fl::u16 y) -> fl::u16 {
            if (y < H_SEG) {
                return segTopUF.mapToIndex(x, y);
            }
            return segBottomUF.mapToIndex(x, y);
        };

        // With absolute parity honored, the composed mapping should match the 4x6 serpentine
        for (fl::u16 y = 0; y < H_FULL; ++y) {
            for (fl::u16 x = 0; x < W; ++x) {
                CHECK_EQ(composedIndexUF(x, y), fullSerp.mapToIndex(x, y));
            }
        }
    }
}
