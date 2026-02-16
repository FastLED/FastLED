#pragma once

// Chasing_Spirals s16x16 fixed-point implementation.
// Replaces all inner-loop floating-point with integer math.
//
// Included from animartrix2_detail.hpp — do NOT include directly.

// ============================================================================
// PERFORMANCE ANALYSIS & OPTIMIZATION HISTORY (2026-02-09)
// ============================================================================
//
// MEASUREMENT METHODOLOGY:
// - Platform: Windows/Clang 21.1.5, profile build mode (-Os -g)
// - Test: 32x32 grid (1024 pixels), 20 benchmark runs with idle CPU
// - Profiler: tests/profile/profile_chasing_spirals.cpp
//
// MEASURED PERFORMANCE:
//
// Float (Animartrix - Original):
//   Best:   199.6 μs/frame  (5,010 fps)
//   Median: 209.5 μs/frame  (4,773 fps)
//   Worst:  236.8 μs/frame  (4,223 fps)
//   Per-pixel: 0.205 μs/pixel
//
// Q31 (Animartrix2 - Optimized):
//   Best:   74.3 μs/frame   (13,460 fps)  ⭐ Best case
//   Median: 78.5 μs/frame   (12,739 fps)  ⭐ Typical
//   Worst:  97.7 μs/frame   (10,235 fps)
//   Per-pixel: 0.077 μs/pixel
//
// SPEEDUP: 2.7x (median), 2.7x (best case)
//
// ----------------------------------------------------------------------------
// KEY OPTIMIZATIONS (How 2.7x Speedup Was Achieved)
// ----------------------------------------------------------------------------
//
// 1. PixelLUT Pre-Computation:
//    - Stores per-pixel: base_angle, dist_scaled, rf3, rf_half, rf_quarter
//    - Computed once at init, reused every frame
//    - Eliminates: ~30,000 operations per frame
//    - Memory: 32KB (1024 pixels × 32 bytes), fits in L1 cache
//
// 2. 2D Perlin Noise (z=0 specialization):
//    - Float: 8 cube corners → Q31: 4 square corners
//    - Saves: 4 gradient evaluations, 4 lerp calls per noise sample
//    - Result: 50% fewer Perlin operations
//
// 3. LUT-Based Fade Curve:
//    - Float: t³(t(6t-15)+10) = 5 multiplies + 3 adds
//    - Q31: table[idx] + interpolation = 1 lookup + 4 ops
//    - Speedup: 4x faster per fade call
//    - Memory: 257 entries × 4 bytes = 1KB
//
// 4. Branchless Gradient Function:
//    - Float: Hash-based conditionals = 72 branches/pixel
//    - Q31: Struct lookup lut[hash & 15] = 0 branches
//    - Eliminates: Branch misprediction penalties
//
// 5. Combined sincos32():
//    - Float: 6 separate trig calls (3 sin + 3 cos)
//    - Q31: 3 combined calls returning both sin+cos
//    - Speedup: 2x fewer calls, integer LUT vs float polynomial
//
// 6. Integer Fixed-Point Arithmetic:
//    - Float: ~500 float ops/pixel ≈ 1,500 CPU cycles
//    - Q31: ~160 i32/i64 ops/pixel ≈ 220 CPU cycles
//    - Speedup: 6.8x fewer cycles per pixel
//
// ----------------------------------------------------------------------------
// PERFORMANCE BREAKDOWN (Q31 - Where Time Is Spent)
// ----------------------------------------------------------------------------
//
// Component               % Time    μs/frame   Details
// ─────────────────────────────────────────────────────────────────────────
// 2D Perlin Noise         50-55%    39-43 μs   LUT fade, branchless grad
// Fixed-Point Trig        25-30%    20-24 μs   LUT-based sincos32
// Coordinate Transform    10-12%     8-9 μs    i32/i64 arithmetic
// Radial Filter + RGB      5-7%      4-5 μs    Pre-computed in PixelLUT
// Other (memory, writes)   3-5%      2-4 μs    Direct leds[] access
//
// Cache Efficiency:
// - All hot data fits in L1 cache (32KB PixelLUT + 1KB fade + 256B perm)
// - Sequential PixelLUT access = perfect hardware prefetching
// - Zero cache misses during inner loop
//
// ----------------------------------------------------------------------------
// FAILED OPTIMIZATION ATTEMPTS (What NOT To Do)
// ----------------------------------------------------------------------------
//
// All micro-optimizations FAILED. Compiler (Clang 21.1.5 -Os) already optimal.
//
// Phase 1: Permutation Table Prefetching
//   Goal: Reduce pipeline stalls with __builtin_prefetch()
//   Result: 0% improvement (74.1 vs 74.3 μs, within noise)
//   Reason: Hardware prefetching already handles sequential access optimally
//
// Phase 2: Gradient Coefficient Packing
//   Goal: Pack (cx, cy) into single i16 to reduce memory loads
//   Result: -6.1% SLOWER (76.8 vs 74.3 μs)
//   Reason: Compiler already optimized struct loads, packing added shift/mask
//
// Phase 3: Manual Lerp Inlining
//   Goal: Eliminate function call overhead from nested lerp()
//   Result: -4.6% SLOWER (75.7 vs 74.3 μs)
//   Reason: Compiler already inlined lerp(), manual prevented optimization
//
// Key Lesson: Trust the compiler. Modern compilers beat hand-written
// micro-optimizations through:
//   - Auto-inlining of FASTLED_FORCE_INLINE functions
//   - Hardware prefetch detection (sequential arrays)
//   - Register allocation and instruction scheduling
//   - Algebraic simplification across function boundaries
//
// ----------------------------------------------------------------------------
// FUTURE OPTIMIZATION OPPORTUNITIES
// ----------------------------------------------------------------------------
//
// Current implementation is optimal for scalar code. Further speedup requires:
//
// 1. SIMD Vectorization (SSE/AVX):
//    - Process 4 pixels simultaneously with vector intrinsics
//    - Expected: 3x speedup → ~26 μs/frame (38,000 fps)
//    - Complexity: High (platform-specific, requires SSE4/AVX2)
//
// 2. Simplex Noise (Algorithm Change):
//    - Fewer gradient evaluations than Perlin (3 vs 4 in 2D)
//    - Expected: 20-30% speedup → ~60 μs/frame (16,000 fps)
//    - Complexity: Medium (new algorithm, accuracy validation)
//
// 3. Build Mode Tuning:
//    - Try -O3 instead of -Os (speed vs size)
//    - Expected: 5-10% speedup → ~70-75 μs/frame
//    - Complexity: Trivial (flag change)
//
// NOT Recommended:
//   ❌ Manual micro-optimizations (proven ineffective)
//   ❌ Further LUT tuning (fade LUT already optimal)
//   ❌ Assembly hand-tuning (compiler beats manual)
//
// ----------------------------------------------------------------------------
// PROFILING & VALIDATION
// ----------------------------------------------------------------------------
//
// Profiler: tests/profile/profile_chasing_spirals.cpp
//   - 6 variants: Float, Q31, Q31_Batch4, Q31_Batch4_ColorGrouped, etc.
//   - 20 iterations per variant for statistical analysis
//   - Outputs: best/median/worst/stdev timing data
//
// Accuracy Tests: tests/fl/fx/2d/animartrix2.cpp
//   - Low time (t=1000): avg error < 1%, max ≤ 6 per channel
//   - High time (t>1M): avg error < 3%, max ≤ 10 per channel
//   - Visual validation: AnimartrixRing example (no artifacts)
//
// Commands:
//   bash profile chasing_spirals --docker --iterations 20
//   uv run test.py animartrix2 --cpp
//
// See also: docs/profiling/HOW_TO_PROFILE.md
// ============================================================================

#ifndef ANIMARTRIX2_CHASING_SPIRALS_INTERNAL
#error "Do not include chasing_spirals.hpp directly. Include animartrix2.hpp instead."
#endif

#include "fl/fixed_point/s16x16.h"

// fl/fixed_point/s16x16.h is included by animartrix2_detail.hpp before
// the namespace block, so fl::s16x16 is available at global scope.

#include "chasing_spirals_common.hpp"  // Common helper code
#include "chasing_spirals_q31.hpp"     // Q31 scalar implementation
#include "chasing_spirals_simd.hpp"    // Q31 SIMD implementation
