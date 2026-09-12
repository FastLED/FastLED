
#include "noise.h"
#include "fl/stl/stdint.h"
#include "test.h"
#include "fl/log/log.h"
#include "fl/stl/strstream.h"

FL_TEST_FILE(FL_FILEPATH) {
using namespace fl;

FL_TEST_CASE("Noise Range Analysis") {
    // Test 1D noise function
    uint8_t min_1d = 255;
    uint8_t max_1d = 0;
    
    // Test representative range (optimized for speed while maintaining coverage)
    for (uint32_t x = 0; x < 65536; x += 509) {  // ~129 samples
        uint8_t noise_val = inoise8(x);
        if (noise_val < min_1d) min_1d = noise_val;
        if (noise_val > max_1d) max_1d = noise_val;
    }

    // Test 2D noise function
    uint8_t min_2d = 255;
    uint8_t max_2d = 0;

    for (uint16_t x = 0; x < 1024; x += 127) {  // ~8x8 = 64 samples
        for (uint16_t y = 0; y < 1024; y += 127) {
            uint8_t noise_val = inoise8(x, y);
            if (noise_val < min_2d) min_2d = noise_val;
            if (noise_val > max_2d) max_2d = noise_val;
        }
    }

    // Test 3D noise function
    uint8_t min_3d = 255;
    uint8_t max_3d = 0;

    for (uint16_t x = 0; x < 256; x += 127) {  // ~2x2x2 = 8 samples minimum
        for (uint16_t y = 0; y < 256; y += 127) {
            for (uint16_t z = 0; z < 256; z += 127) {
                uint8_t noise_val = inoise8(x, y, z);
                if (noise_val < min_3d) min_3d = noise_val;
                if (noise_val > max_3d) max_3d = noise_val;
            }
        }
    }

    // Test raw noise functions for comparison
    int8_t min_raw_1d = 127;
    int8_t max_raw_1d = -128;

    for (uint32_t x = 0; x < 65536; x += 509) {
        int8_t raw_val = inoise8_raw(x);
        if (raw_val < min_raw_1d) min_raw_1d = raw_val;
        if (raw_val > max_raw_1d) max_raw_1d = raw_val;
    }

    int8_t min_raw_2d = 127;
    int8_t max_raw_2d = -128;

    for (uint16_t x = 0; x < 1024; x += 127) {
        for (uint16_t y = 0; y < 1024; y += 127) {
            int8_t raw_val = inoise8_raw(x, y);
            if (raw_val < min_raw_2d) min_raw_2d = raw_val;
            if (raw_val > max_raw_2d) max_raw_2d = raw_val;
        }
    }

    int8_t min_raw_3d = 127;
    int8_t max_raw_3d = -128;

    for (uint16_t x = 0; x < 256; x += 127) {
        for (uint16_t y = 0; y < 256; y += 127) {
            for (uint16_t z = 0; z < 256; z += 127) {
                int8_t raw_val = inoise8_raw(x, y, z);
                if (raw_val < min_raw_3d) min_raw_3d = raw_val;
                if (raw_val > max_raw_3d) max_raw_3d = raw_val;
            }
        }
    }
    
    // Report findings
    FL_WARN("=== NOISE RANGE ANALYSIS RESULTS ===");
    FL_WARN("Expected u8 range: 0-255 (full range)");
    FL_WARN("Expected raw range: -64 to +64 (from comments)");
    FL_WARN("");
    FL_WARN("1D inoise8 range: " << (int)min_1d << " to " << (int)max_1d << " (span: " << (int)(max_1d - min_1d) << ")");
    FL_WARN("2D inoise8 range: " << (int)min_2d << " to " << (int)max_2d << " (span: " << (int)(max_2d - min_2d) << ")");
    FL_WARN("3D inoise8 range: " << (int)min_3d << " to " << (int)max_3d << " (span: " << (int)(max_3d - min_3d) << ")");
    FL_WARN("");
    FL_WARN("1D inoise8_raw range: " << (int)min_raw_1d << " to " << (int)max_raw_1d << " (span: " << (int)(max_raw_1d - min_raw_1d) << ")");
    FL_WARN("2D inoise8_raw range: " << (int)min_raw_2d << " to " << (int)max_raw_2d << " (span: " << (int)(max_raw_2d - min_raw_2d) << ")");
    FL_WARN("3D inoise8_raw range: " << (int)min_raw_3d << " to " << (int)max_raw_3d << " (span: " << (int)(max_raw_3d - min_raw_3d) << ")");
    FL_WARN("");
    
    // Calculate utilization percentages
    float utilization_1d = (float)(max_1d - min_1d) / 255.0f * 100.0f;
    float utilization_2d = (float)(max_2d - min_2d) / 255.0f * 100.0f;
    float utilization_3d = (float)(max_3d - min_3d) / 255.0f * 100.0f;
    
    FL_WARN("Range utilization:");
    FL_WARN("1D: " << utilization_1d << "% of full u8 range");
    FL_WARN("2D: " << utilization_2d << "% of full u8 range");
    FL_WARN("3D: " << utilization_3d << "% of full u8 range");
    FL_WARN("");
    
    // Test if the documented range of 16-238 is accurate
    bool matches_documented_range = (min_1d >= 16 && min_1d <= 20) && (max_1d >= 235 && max_1d <= 240);
    FL_WARN("Does 1D range match documented 'roughly 16-238'? " << (matches_documented_range ? "YES" : "NO"));
    
    // Perform basic sanity checks
    FL_CHECK_GT(max_1d, min_1d);  // Range should be non-zero
    FL_CHECK_GT(max_2d, min_2d);
    FL_CHECK_GT(max_3d, min_3d);
    FL_CHECK_GT(max_raw_1d, min_raw_1d);
    FL_CHECK_GT(max_raw_2d, min_raw_2d);
    FL_CHECK_GT(max_raw_3d, min_raw_3d);
    
    // Test if we're not using the full u8 range (this should likely fail given the user's report)
    if (min_1d > 0 || max_1d < 255) {
        // This is expected behavior - inoise8 typically doesn't use the full 0-255 range
        // The noise function uses a subset for more natural looking noise patterns
        FL_WARN("INFO: inoise8 range is " << static_cast<int>(min_1d) << " to " << static_cast<int>(max_1d) 
                << " (not using full 0-255 range, which is expected)");
    }
    
    // Test if raw values are within expected -64 to +64 range
    FL_CHECK_GE(min_raw_1d, -64);
    FL_CHECK_LE(max_raw_1d, 64);
    FL_CHECK_GE(min_raw_2d, -64);
    FL_CHECK_LE(max_raw_2d, 64);
    FL_CHECK_GE(min_raw_3d, -64);
    FL_CHECK_LE(max_raw_3d, 64);
    
    FL_WARN("=== END NOISE RANGE ANALYSIS ===");
}

// Disable non-critical detailed analysis tests for faster test runs
#if 0

FL_TEST_CASE("[.]Noise Distribution Analysis") {
    FL_WARN("=== NOISE DISTRIBUTION ANALYSIS ===");
    
    // Create histogram of noise values
    uint32_t histogram[256] = {0};
    uint32_t total_samples = 0;
    
    // Sample 1D noise extensively
    for (uint32_t x = 0; x < 65536; x += 7) {  // Prime step
        uint8_t noise_val = inoise8(x);
        histogram[noise_val]++;
        total_samples++;
    }
    
    // Find first and last non-zero histogram bins
    uint8_t first_nonzero = 0;
    uint8_t last_nonzero = 255;
    
    for (int i = 0; i < 256; i++) {
        if (histogram[i] > 0) {
            first_nonzero = i;
            break;
        }
    }
    
    for (int i = 255; i >= 0; i--) {
        if (histogram[i] > 0) {
            last_nonzero = i;
            break;
        }
    }
    
    FL_WARN("Distribution analysis from " << total_samples << " samples:");
    FL_WARN("First non-zero bin: " << (int)first_nonzero);
    FL_WARN("Last non-zero bin: " << (int)last_nonzero);
    FL_WARN("Actual range: " << (int)(last_nonzero - first_nonzero));
    FL_WARN("");
    
    // Show the first few and last few non-zero bins
    FL_WARN("First 10 non-zero values and their counts:");
    int shown = 0;
    for (int i = first_nonzero; i <= last_nonzero && shown < 10; i++) {
        if (histogram[i] > 0) {
            FL_WARN("  Value " << i << ": " << histogram[i] << " samples");
            shown++;
        }
    }
    
    FL_WARN("Last 10 non-zero values and their counts:");
    shown = 0;
    for (int i = last_nonzero; i >= first_nonzero && shown < 10; i--) {
        if (histogram[i] > 0) {
            FL_WARN("  Value " << i << ": " << histogram[i] << " samples");
            shown++;
        }
    }
    
    FL_WARN("=== END DISTRIBUTION ANALYSIS ===");
}

FL_TEST_CASE("[.]Noise Range Analysis Summary") {
    FL_WARN("=== NOISE RANGE ANALYSIS SUMMARY ===");
    FL_WARN("");
    FL_WARN("USER REPORT CONFIRMED: u8 noise functions do NOT use the full u8 range");
    FL_WARN("");
    FL_WARN("FINDINGS:");
    FL_WARN("- 1D inoise8(): ~99.6% utilization - excellent range coverage");
    FL_WARN("- 2D inoise8(): ~98.4% utilization - excellent range coverage");
    FL_WARN("- 3D inoise8(): ~88.6% utilization - good range coverage after optimization");
    FL_WARN("");
    FL_WARN("ROOT CAUSE:");
    FL_WARN("- 3D gradient function was using suboptimal gradient vector selection");
    FL_WARN("- Fixed by implementing industry-standard 12 edge vectors of a cube");
    FL_WARN("- Higher dimensions have inherently more interpolation steps, reducing extremes");
    FL_WARN("");
    FL_WARN("RECOMMENDATIONS:");
    FL_WARN("- Use inoise16() and scale down if full 0-255 range is critical");
    FL_WARN("- Current 3D performance is suitable for most LED applications");
    FL_WARN("- Update documentation to reflect actual ranges vs theoretical 0-255");
    FL_WARN("");
    FL_WARN("=== END SUMMARY ===");
}

FL_TEST_CASE("[.]3D Gradient Behavior Demonstration") {
    FL_WARN("=== 3D GRADIENT BEHAVIOR DEMONSTRATION ===");
    FL_WARN("");
    FL_WARN("Demonstrating 3D noise behavior with different coordinate patterns:");
    FL_WARN("");
    
    // Test some specific 3D coordinates 
    FL_WARN("Testing 3D noise with identical coordinates:");
    FL_WARN("inoise8(100, 100, 100) = " << (int)inoise8(100, 100, 100));
    FL_WARN("inoise8(200, 200, 200) = " << (int)inoise8(200, 200, 200));
    FL_WARN("inoise8(300, 300, 300) = " << (int)inoise8(300, 300, 300));
    FL_WARN("");
    
    FL_WARN("Testing 3D noise with diverse coordinates:");
    FL_WARN("inoise8(0, 32767, 65535) = " << (int)inoise8(0, 32767, 65535));
    FL_WARN("inoise8(65535, 0, 32767) = " << (int)inoise8(65535, 0, 32767));
    FL_WARN("inoise8(32767, 65535, 0) = " << (int)inoise8(32767, 65535, 0));
    FL_WARN("");
    
    FL_WARN("Compare with 2D noise:");
    FL_WARN("inoise8(0, 32767) = " << (int)inoise8(0, 32767));
    FL_WARN("inoise8(32767, 0) = " << (int)inoise8(32767, 0));
    FL_WARN("inoise8(65535, 32767) = " << (int)inoise8(65535, 32767));
    FL_WARN("");
    
    FL_WARN("Compare with 1D noise:");
    FL_WARN("inoise8(0) = " << (int)inoise8(0));
    FL_WARN("inoise8(32767) = " << (int)inoise8(32767));
    FL_WARN("inoise8(65535) = " << (int)inoise8(65535));
    FL_WARN("");
    
    FL_WARN("CONCLUSION:");
    FL_WARN("3D noise function now uses industry-standard gradient vectors");
    FL_WARN("for optimal range utilization suitable for LED applications.");
    FL_WARN("");
    FL_WARN("=== END 3D GRADIENT DEMONSTRATION ===");
}

#endif  // End disabled noise tests



// ===========================================================================
// Regression tests for the one-dimensional Perlin gradient. FastLED#1114.
// ===========================================================================
///
/// `grad8(hash, x)` and `grad16(hash, x)` used to be the two/three-dimensional
/// gradient with the second coordinate missing. Feeding x to both terms and then
/// flipping their signs independently produced `avg7(x, -x)`, which is zero for
/// every x, so a quarter of the hash table had no gradient at all. A lattice cube
/// whose two corners both hashed into that quarter interpolated zero against zero
/// and went flat across every one of its inputs.
///
/// Measured before the fix:
///
///   inoise8   longest constant run 1063 samples (at x = 24048, value 128)
///             value at lattice points 126..130, not a single value
///   inoise16  the whole 65536-sample cube at x = 24064 << 8 constant at 34616
///
/// and after:
///
///   inoise8   longest constant run 17 samples; every lattice point exactly 128
///   inoise16  longest constant run 182 samples in that cube; lattice 34616
///
/// Range (0..255) and maximum step between neighbours (4) are unchanged by the
/// fix, which is the point: this removes flat spots without making the function
/// less smooth.

namespace {
/// Samples per lattice cube: `inoise8` takes the cube index from the high byte.
constexpr u32 kCubeSamples8 = 256;
constexpr u32 kCubes8 = 256;
}  // namespace

FL_TEST_CASE("inoise8 has no dead lattice cube") {
    // The direct statement of the defect: a cube both of whose corner hashes had
    // a zero gradient produced one value for all 256 of its inputs. 21 of the 256
    // cubes did. Not one may.
    u32 dead = 0;
    u32 first_dead = kCubes8;
    for (u32 cube = 0; cube < kCubes8; ++cube) {
        const u8 base = inoise8(static_cast<u16>(cube * kCubeSamples8));
        bool flat = true;
        for (u32 off = 1; off < kCubeSamples8; ++off) {
            if (inoise8(static_cast<u16>(cube * kCubeSamples8 + off)) != base) {
                flat = false;
                break;
            }
        }
        if (flat) {
            ++dead;
            if (first_dead == kCubes8) {
                first_dead = cube;
            }
        }
    }
    FL_CHECK_EQ(dead, 0u);
    FL_CHECK_EQ(first_dead, kCubes8);
}

FL_TEST_CASE("inoise8 passes through its base value at every lattice point") {
    // Perlin noise is defined to equal its base value where the fractional part
    // is zero. The old gradient's `avg7(+-1, x)` branches contributed +-1 at x = 0,
    // so it did not: lattice values ranged 126..130.
    for (u32 cube = 0; cube < kCubes8; ++cube) {
        FL_CHECK_EQ(inoise8(static_cast<u16>(cube * kCubeSamples8)), 128);
    }
}

FL_TEST_CASE("inoise8 keeps its range and its smoothness") {
    u8 lo = 255;
    u8 hi = 0;
    u32 max_step = 0;
    u32 longest_flat = 1;
    u32 run = 1;
    u8 prev = inoise8(0);
    for (u32 x = 1; x < 65536; ++x) {
        const u8 v = inoise8(static_cast<u16>(x));
        if (v < lo) { lo = v; }
        if (v > hi) { hi = v; }
        const u32 step = static_cast<u32>(v > prev ? v - prev : prev - v);
        if (step > max_step) { max_step = step; }
        if (v == prev) {
            ++run;
            if (run > longest_flat) { longest_flat = run; }
        } else {
            run = 1;
        }
        prev = v;
    }

    // Full 8-bit range, as before the fix.
    FL_CHECK_EQ(static_cast<int>(lo), 0);
    FL_CHECK_EQ(static_cast<int>(hi), 255);

    // Neighbouring samples still differ by at most 4, so nothing was traded for
    // the flat spots: the function is no less continuous than it was.
    FL_CHECK_EQ(max_step, 4u);

    // Measured 17. The bound is a quarter of a cube -- far enough above the
    // measurement to survive incidental change, far enough below the 1063 of the
    // defect that a regression cannot hide under it.
    FL_CHECK(longest_flat <= 64u);
}

FL_TEST_CASE("the range FastLED#1114 reported is no longer flat") {
    // "inoise8(i) returns a constant value of 128 from i = 3061 to 3599."
    u32 changes = 0;
    u8 prev = inoise8(3061);
    for (u32 x = 3062; x <= 3599; ++x) {
        const u8 v = inoise8(static_cast<u16>(x));
        if (v != prev) { ++changes; }
        prev = v;
    }
    // Measured 245 over the wider 3072..3606 window; the reported window is
    // narrower. One change would still have been a flat cube with an edge in it.
    FL_CHECK(changes > 100u);
}

FL_TEST_CASE("inoise16 has no dead lattice cube either") {
    // Same gradient, same defect, 65536 samples per cube instead of 256. Walking
    // all 256 cubes at full resolution is 16M calls, so this checks the cube the
    // 8-bit scan identified as the worst, plus a stride over the rest.
    constexpr u32 kDeadCube = 94;  // 24048 >> 8, the old 1063-sample plateau
    const u16 base = inoise16(static_cast<u32>(kDeadCube) << 16);
    bool flat = true;
    for (u32 off = 1; off < 65536; off += 7) {
        if (inoise16((static_cast<u32>(kDeadCube) << 16) + off) != base) {
            flat = false;
            break;
        }
    }
    FL_CHECK(!flat);

    u32 dead = 0;
    for (u32 cube = 0; cube < 256; ++cube) {
        const u16 cube_base = inoise16(static_cast<u32>(cube) << 16);
        bool cube_flat = true;
        for (u32 off = 251; off < 65536; off += 251) {
            if (inoise16((static_cast<u32>(cube) << 16) + off) != cube_base) {
                cube_flat = false;
                break;
            }
        }
        if (cube_flat) { ++dead; }
    }
    FL_CHECK_EQ(dead, 0u);
}

FL_TEST_CASE("inoise16 passes through its base value at every lattice point") {
    // 34616 rather than 32768 because inoise16 offsets by 17308 before doubling;
    // what matters is that it is one value and not the 34614..34618 spread the
    // old gradient produced.
    for (u32 cube = 0; cube < 256; ++cube) {
        FL_CHECK_EQ(inoise16(static_cast<u32>(cube) << 16), 34616);
    }
}

} // FL_TEST_FILE
