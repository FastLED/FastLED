#include "fl/gfx/sample.h"

#include "fl/math_macros.h"
#include "fl/xymap.h"
#include "crgb.h"

namespace fl {

CRGB sample(const CRGB *grid, const XYMap &xyMap, float x, float y,
            SampleMode mode) {
    if (mode == SAMPLE_BILINEAR) {
        return sampleBilinear(grid, xyMap, x, y);
    }
    return sampleNearest(grid, xyMap, x, y);
}

CRGB sampleBilinear(const CRGB *grid, const XYMap &xyMap, float x, float y) {
    int x0 = static_cast<int>(x);
    int y0 = static_cast<int>(y);
    int x1 = min(x0 + 1, static_cast<int>(xyMap.getWidth()) - 1);
    int y1 = min(y0 + 1, static_cast<int>(xyMap.getHeight()) - 1);

    // Clamp x0 and y0 to valid range
    x0 = max(0, min(x0, static_cast<int>(xyMap.getWidth()) - 1));
    y0 = max(0, min(y0, static_cast<int>(xyMap.getHeight()) - 1));

    float fx = x - x0;
    float fy = y - y0;

    // Get four neighboring pixels
    CRGB c00 = grid[xyMap.mapToIndex(x0, y0)];
    CRGB c10 = grid[xyMap.mapToIndex(x1, y0)];
    CRGB c01 = grid[xyMap.mapToIndex(x0, y1)];
    CRGB c11 = grid[xyMap.mapToIndex(x1, y1)];

    // Interpolate
    CRGB c0 = CRGB::blend(c00, c10, static_cast<u8>(fx * 255));
    CRGB c1 = CRGB::blend(c01, c11, static_cast<u8>(fx * 255));
    return CRGB::blend(c0, c1, static_cast<u8>(fy * 255));
}

CRGB sampleNearest(const CRGB *grid, const XYMap &xyMap, float x, float y) {
    int xi = static_cast<int>(x + 0.5f); // Round to nearest
    int yi = static_cast<int>(y + 0.5f);
    xi = max(0, min(xi, static_cast<int>(xyMap.getWidth()) - 1));
    yi = max(0, min(yi, static_cast<int>(xyMap.getHeight()) - 1));
    return grid[xyMap.mapToIndex(xi, yi)];
}

} // namespace fl
