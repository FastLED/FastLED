#pragma once

#include "fl/hsv.h"
#include "fl/hsv16.h"
#include "chsv.h"
#include "crgb.h"

/// @file noise.h
/// Functions to generate noise patterns on rings and spheres.

namespace fl {

/// Observed min/max extents for inoise16() output.
/// These values represent the practical range of the Perlin noise function
/// across all radius and parameter combinations, optimized to maximize color coverage.
/// Used for rescaling 16-bit noise output to span the full 16-bit range (0 to 65535).
/// @note Determined by comprehensive stress testing with 100k+ random samples at radius=1.0
///       and 100k samples (10 trials × 10k each) at extreme radius=1000.
///       Bounds [9000, 59500] capture ~98%+ hue coverage across all test conditions,
///       ensuring no significant color bands are lost even at extreme radius values.
/// @see noiseRingHSV16(), noiseRingCRGB(), noiseSphereHSV16(), noiseSphereCRGB()
constexpr uint16_t NOISE16_EXTENT_MIN = 9000;
constexpr uint16_t NOISE16_EXTENT_MAX = 59500;

/// @defgroup ShapeNoise Shape Noise Functions
/// Convenience functions for generating noise on geometric shapes.
/// Each function samples multiple z-slices of the noise space to generate
/// independent values for each color component (H/S/V or R/G/B).
/// @{

/// @defgroup RingNoise Ring Noise Functions
/// Convenience functions for generating noise on circular rings.
/// Each function samples three z-slices of the noise space to generate
/// independent values for each color component (H/S/V or R/G/B).
/// @{

/// Generate HSV16 noise for a ring pattern.
/// Samples three z-slices of 3D Perlin noise (at time, time+0x10000, time+0x20000)
/// to create independent hue, saturation, and value components.
/// @param angle Position around the ring (radians, 0 to 2π)
/// @param time Animation time parameter
/// @param radius Noise zoom level (level of detail). Larger values = coarser pattern, smaller = more detail (default 1.0)
/// @return HSV16 color with 16-bit components
fl::HSV16 noiseRingHSV16(float angle, uint32_t time, float radius = 1.0f);

/// Generate HSV8 (8-bit) noise for a ring pattern.
/// Calls noiseRingHSV16() and scales each component down to 8-bit.
/// @param angle Position around the ring (radians, 0 to 2π)
/// @param time Animation time parameter
/// @param radius Noise zoom level (level of detail). Larger values = coarser pattern, smaller = more detail (default 1.0)
/// @return HSV8 (CHSV) color with 8-bit components
CHSV noiseRingHSV8(float angle, uint32_t time, float radius = 1.0f);

/// Generate CRGB noise for a ring pattern.
/// Samples three z-slices of 3D Perlin noise to create independent
/// red, green, and blue components (direct RGB, not HSV conversion).
/// @param angle Position around the ring (radians, 0 to 2π)
/// @param time Animation time parameter
/// @param radius Noise zoom level (level of detail). Larger values = coarser pattern, smaller = more detail (default 1.0)
/// @return CRGB color with 8-bit components
CRGB noiseRingCRGB(float angle, uint32_t time, float radius = 1.0f);

/// @} Ring Noise Functions


/// @defgroup SphereNoise Sphere Noise Functions
/// Convenience functions for generating noise on spheres.
/// Each function samples three z-slices of the noise space to generate
/// independent values for each color component (H/S/V or R/G/B).
/// @{

/// Generate HSV16 noise for a sphere pattern.
/// Samples three z-slices of 3D Perlin noise (at time, time+0x10000, time+0x20000)
/// to create independent hue, saturation, and value components.
/// @param angle Azimuth angle around the sphere (radians, 0 to 2π)
/// @param phi Polar angle from the north pole (radians, 0 to π)
/// @param time Animation time parameter
/// @param radius Noise zoom level (level of detail). Larger values = coarser pattern, smaller = more detail (default 1.0)
/// @return HSV16 color with 16-bit components
fl::HSV16 noiseSphereHSV16(float angle, float phi, uint32_t time, float radius = 1.0f);

/// Generate HSV8 (8-bit) noise for a sphere pattern.
/// Calls noiseSphereHSV16() and scales each component down to 8-bit.
/// @param angle Azimuth angle around the sphere (radians, 0 to 2π)
/// @param phi Polar angle from the north pole (radians, 0 to π)
/// @param time Animation time parameter
/// @param radius Noise zoom level (level of detail). Larger values = coarser pattern, smaller = more detail (default 1.0)
/// @return HSV8 (CHSV) color with 8-bit components
CHSV noiseSphereHSV8(float angle, float phi, uint32_t time, float radius = 1.0f);

/// Generate CRGB noise for a sphere pattern.
/// Samples three z-slices of 3D Perlin noise to create independent
/// red, green, and blue components (direct RGB, not HSV conversion).
/// @param angle Azimuth angle around the sphere (radians, 0 to 2π)
/// @param phi Polar angle from the north pole (radians, 0 to π)
/// @param time Animation time parameter
/// @param radius Noise zoom level (level of detail). Larger values = coarser pattern, smaller = more detail (default 1.0)
/// @return CRGB color with 8-bit components
CRGB noiseSphereCRGB(float angle, float phi, uint32_t time, float radius = 1.0f);

/// @} Sphere Noise Functions


/// @defgroup CylinderNoise Cylinder Noise Functions
/// Convenience functions for generating noise on cylindrical surfaces.
/// Each function samples three z-slices of the noise space to generate
/// independent values for each color component (H/S/V or R/G/B).
/// @{

/// Generate HSV16 noise for a cylinder pattern.
/// Samples three z-slices of 3D Perlin noise (at time, time+0x10000, time+0x20000)
/// to create independent hue, saturation, and value components.
/// Maps the angle around the circumference using sin/cos, and samples height directly.
/// @param angle Position around the cylinder (radians, 0 to 2π)
/// @param height Vertical position on the cylinder
/// @param time Animation time parameter
/// @param radius Noise zoom level (level of detail). Larger values = coarser pattern, smaller = more detail (default 1.0)
/// @return HSV16 color with 16-bit components
fl::HSV16 noiseCylinderHSV16(float angle, float height, uint32_t time, float radius = 1.0f);

/// Generate HSV8 (8-bit) noise for a cylinder pattern.
/// Calls noiseCylinderHSV16() and scales each component down to 8-bit.
/// @param angle Position around the cylinder (radians, 0 to 2π)
/// @param height Vertical position on the cylinder
/// @param time Animation time parameter
/// @param radius Noise zoom level (level of detail). Larger values = coarser pattern, smaller = more detail (default 1.0)
/// @return HSV8 (CHSV) color with 8-bit components
CHSV noiseCylinderHSV8(float angle, float height, uint32_t time, float radius = 1.0f);

/// Generate CRGB noise for a cylinder pattern.
/// Samples three z-slices of 3D Perlin noise to create independent
/// red, green, and blue components (direct RGB, not HSV conversion).
/// Maps the angle around the circumference using sin/cos, and samples height directly.
/// @param angle Position around the cylinder (radians, 0 to 2π)
/// @param height Vertical position on the cylinder
/// @param time Animation time parameter
/// @param radius Noise zoom level (level of detail). Larger values = coarser pattern, smaller = more detail (default 1.0)
/// @return CRGB color with 8-bit components
CRGB noiseCylinderCRGB(float angle, float height, uint32_t time, float radius = 1.0f);

/// @} Cylinder Noise Functions

/// @} Shape Noise Functions

} // namespace fl
