#pragma once

// Chasing_Spirals visualizer classes — three precision/performance tiers.
// Included from animartrix2_detail.h — do NOT include directly.

#include "fl/fx/2d/animartrix2_detail/chasing_spiral_state.h"
#include "fl/fx/2d/animartrix2_detail/viz/viz_base.h"

namespace fl {

// Original floating-point implementation (~210 µs/frame on 32×32).
// No cached state — all computation is per-frame.
class Chasing_Spirals_Float : public IAnimartrix2Viz {
public:
    void draw(Context &ctx) override;
};

// Fixed-point Q31 scalar implementation (~78 µs/frame, 2.7× speedup).
// Owns the SoA geometry cache; rebuilt only when grid dimensions change.
class Chasing_Spirals_Q31 : public IAnimartrix2Viz {
public:
    void draw(Context &ctx) override;
private:
    ChasingSpiralState mState;
};

// Fixed-point Q31 SIMD implementation (4-wide SSE2 vectorisation).
// Owns the SoA geometry cache; rebuilt only when grid dimensions change.
class Chasing_Spirals_Q31_SIMD : public IAnimartrix2Viz {
public:
    void draw(Context &ctx) override;
private:
    ChasingSpiralState mState;
};

} // namespace fl
