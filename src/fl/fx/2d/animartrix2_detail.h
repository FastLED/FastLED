#pragma once
// IWYU pragma: private, include "fl/fx/2d/animartrix2.hpp"
// allow-include-after-namespace

// Animartrix2 detail: Refactored version of animartrix_detail.hpp
// Original by Stefan Petrick 2023. Adapted to C++ by Netmindz 2023.
// Adapted to FastLED by Zach Vorhies 2024.
// Refactored to free-function architecture 2026.
//
// Licensed under Creative Commons Attribution License CC BY-NC 3.0
// https://creativecommons.org/licenses/by-nc/3.0/
//
// Architecture: Context struct holds all shared state. Each animation is an
// IAnimartrix2Viz subclass with a draw(Context&) method. Internally delegates
// to animartrix_detail::ANIMartRIX for bit-identical output.
//
// NOTE: This is an internal implementation header. Do not include directly.
//       Include "fl/fx/2d/animartrix2.hpp" instead.

// Include standalone Animartrix2 core functionality
#include "fl/fx/2d/animartrix2_detail/core_types.h"
#include "fl/fx/2d/animartrix2_detail/perlin_float.h"
#include "fl/fx/2d/animartrix2_detail/engine_core.h"

#include "crgb.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/optional.h"
#include "fl/stl/stdint.h"
#include "fl/sin32.h"
#include "fl/fixed_point/s16x16.h"
#include "fl/simd.h"


#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

#ifndef FL_ANIMARTRIX_USES_FAST_MATH
#define FL_ANIMARTRIX_USES_FAST_MATH 1
#endif

#if FL_ANIMARTRIX_USES_FAST_MATH
FL_FAST_MATH_BEGIN
FL_OPTIMIZATION_LEVEL_O3_BEGIN
#endif

// Helper structs extracted to separate headers for better organization
#include "fl/fx/2d/animartrix2_detail/context.h"
#include "fl/fx/2d/animartrix2_detail/chasing_spiral_pixel_lut.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16_simd.h"
#include "fl/fx/2d/animartrix2_detail/perlin_q16.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s8x8.h"
#include "fl/fx/2d/animartrix2_detail/perlin_i16_optimized.h"
#include "fl/fx/2d/animartrix2_detail/viz/chasing_spirals.h"
#include "fl/fx/2d/animartrix2_detail/engine.h"

// Visualizer declarations (implementations in viz/*.cpp.hpp via unity build)
#include "fl/fx/2d/animartrix2_detail/viz/rotating_blob.h"
#include "fl/fx/2d/animartrix2_detail/viz/rings.h"
#include "fl/fx/2d/animartrix2_detail/viz/waves.h"
#include "fl/fx/2d/animartrix2_detail/viz/center_field.h"
#include "fl/fx/2d/animartrix2_detail/viz/distance_experiment.h"
#include "fl/fx/2d/animartrix2_detail/viz/caleido1.h"
#include "fl/fx/2d/animartrix2_detail/viz/caleido2.h"
#include "fl/fx/2d/animartrix2_detail/viz/caleido3.h"
#include "fl/fx/2d/animartrix2_detail/viz/lava1.h"
#include "fl/fx/2d/animartrix2_detail/viz/scaledemo1.h"
#include "fl/fx/2d/animartrix2_detail/viz/yves.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiralus.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiralus2.h"
#include "fl/fx/2d/animartrix2_detail/viz/hot_blob.h"
#include "fl/fx/2d/animartrix2_detail/viz/zoom.h"
#include "fl/fx/2d/animartrix2_detail/viz/zoom2.h"
#include "fl/fx/2d/animartrix2_detail/viz/slow_fade.h"
#include "fl/fx/2d/animartrix2_detail/viz/polar_waves.h"
#include "fl/fx/2d/animartrix2_detail/viz/rgb_blobs.h"
#include "fl/fx/2d/animartrix2_detail/viz/rgb_blobs2.h"
#include "fl/fx/2d/animartrix2_detail/viz/rgb_blobs3.h"
#include "fl/fx/2d/animartrix2_detail/viz/rgb_blobs4.h"
#include "fl/fx/2d/animartrix2_detail/viz/rgb_blobs5.h"
#include "fl/fx/2d/animartrix2_detail/viz/big_caleido.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix1.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix2.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix3.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix4.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix5.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix6.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix8.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix9.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix10.h"
#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido.h"
#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido_2.h"
#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido_3.h"
#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido_4.h"
#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido_5.h"
#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido_6.h"
#include "fl/fx/2d/animartrix2_detail/viz/water.h"
#include "fl/fx/2d/animartrix2_detail/viz/parametric_water.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment1.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment2.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment3.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment4.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment5.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment6.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment7.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment8.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment9.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment10.h"
#include "fl/fx/2d/animartrix2_detail/viz/fluffy_blobs.h"

namespace fl {

// Namespace wrappers for backwards compatibility with test code

namespace q31 {
    using fl::Chasing_Spirals_Q31;
    using fl::Chasing_Spirals_Q31_SIMD;
}

namespace q16 {
    // Q16 implementation aliased to Q31 (Q16 was removed, use Q31 instead)
    inline void Chasing_Spirals_Q16_Batch4_ColorGrouped(Context &ctx) {
        fl::Chasing_Spirals_Q31().draw(ctx);
    }
}


#if FL_ANIMARTRIX_USES_FAST_MATH
FL_OPTIMIZATION_LEVEL_O3_END
FL_FAST_MATH_END
#endif

}  // namespace fl
