/// @file _build.hpp
/// @brief Unity build header for fl/fx/2d/animartrix2_detail/ directory
/// Includes all implementation files in alphabetical order

#include "fl/fx/2d/animartrix2_detail/perlin_i16_optimized.cpp.hpp"
#include "fl/fx/2d/animartrix2_detail/perlin_q16.cpp.hpp"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16.cpp.hpp"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16_simd.cpp.hpp"
#include "fl/fx/2d/animartrix2_detail/perlin_s8x8.cpp.hpp"

// Visualizer implementations (engine.h needed by viz files that use Engine directly)
#include "fl/fx/2d/animartrix2_detail/engine.h"
#include "fl/fx/2d/animartrix2_detail/viz/_build.cpp.hpp"
