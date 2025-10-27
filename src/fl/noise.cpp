/// @file noise.cpp
/// Ring, sphere, and cylinder noise functions in the fl namespace

#include "fl/fastled.h"
#include "fl/hsv16.h"
#include "fl/map_range.h"
#include "fl/noise.h"

// Forward declarations from src/noise.cpp
uint16_t inoise16(uint32_t x, uint32_t y, uint32_t z, uint32_t t);
uint16_t inoise16(uint32_t x, uint32_t y, uint32_t z);



namespace fl {

/// Rescale raw inoise16() output to full 16-bit range [0, 65535]
/// Curries in the global NOISE16_EXTENT_MIN/MAX extents for clean, reusable rescaling.
/// @param raw_value Raw noise value from inoise16()
/// @return Rescaled value spanning the full 16-bit range (0-65535)
FASTLED_FORCE_INLINE uint16_t rescaleNoiseValue16(uint16_t raw_value) {
  return fl::map_range_clamped(raw_value, fl::NOISE16_EXTENT_MIN, fl::NOISE16_EXTENT_MAX,
                               uint16_t(0), uint16_t(65535));
}

/// Ring noise functions - sample three z-slices for independent component evolution
HSV16 noiseRingHSV16(float angle, uint32_t time, float radius) {
  // Convert angle to cartesian coordinates
  float x = fl::cosf(angle);
  float y = fl::sinf(angle);

  // Map sin/cos values from [-1, 1] to [0, 0xFFFF]
  // This ensures positive values for uint32_t conversion
  uint32_t nx = static_cast<uint32_t>((x + 1.0f) * 0.5f * radius * 0xffff);
  uint32_t ny = static_cast<uint32_t>((y + 1.0f) * 0.5f * radius * 0xffff);

  // Sample three different z-slices for H, S, V components
  // Using offsets 0x0, 0x10000, 0x20000 to separate them in noise space
  uint16_t h_raw = inoise16(nx, ny, time);
  uint16_t s_raw = inoise16(nx, ny, time + 0x10000);
  uint16_t v_raw = inoise16(nx, ny, time + 0x20000);

  // Rescale from observed noise range to full 0-65535 range using global extents
  uint16_t h = rescaleNoiseValue16(h_raw);
  uint16_t s = rescaleNoiseValue16(s_raw);
  uint16_t v = rescaleNoiseValue16(v_raw);

  return HSV16(h, s, v);
}

CHSV noiseRingHSV8(float angle, uint32_t time, float radius) {
  fl::HSV16 hsv16 = noiseRingHSV16(angle, time, radius);

  // Scale 16-bit components down to 8-bit using bit shift with rounding
  // This preserves the relative position in the value range
  uint8_t h = (hsv16.h + 128) >> 8;
  uint8_t s = (hsv16.s + 128) >> 8;
  uint8_t v = (hsv16.v + 128) >> 8;

  return CHSV(h, s, v);
}

CRGB noiseRingCRGB(float angle, uint32_t time, float radius) {
  // Convert angle to cartesian coordinates
  float x = fl::cosf(angle);
  float y = fl::sinf(angle);

  // Map sin/cos values from [-1, 1] to [0, 0xFFFF]
  // This ensures positive values for uint32_t conversion
  uint32_t nx = static_cast<uint32_t>((x + 1.0f) * 0.5f * radius * 0xffff);
  uint32_t ny = static_cast<uint32_t>((y + 1.0f) * 0.5f * radius * 0xffff);

  // Sample three different z-slices for R, G, B components (direct RGB)
  // Using offsets 0x0, 0x10000, 0x20000 to separate them in noise space
  uint16_t r16_raw = inoise16(nx, ny, time);
  uint16_t g16_raw = inoise16(nx, ny, time + 0x10000);
  uint16_t b16_raw = inoise16(nx, ny, time + 0x20000);

  // Rescale from observed noise range to full 0-65535 range using global extents
  uint16_t r16 = rescaleNoiseValue16(r16_raw);
  uint16_t g16 = rescaleNoiseValue16(g16_raw);
  uint16_t b16 = rescaleNoiseValue16(b16_raw);

  // Scale down to 8-bit
  uint8_t r = r16 >> 8;
  uint8_t g = g16 >> 8;
  uint8_t b = b16 >> 8;

  return CRGB(r, g, b);
}


/// Sphere noise functions - sample three z-slices for independent component evolution
HSV16 noiseSphereHSV16(float angle, float phi, uint32_t time, float radius) {
  // Convert spherical coordinates to cartesian
  // angle: azimuth (0 to 2π), phi: polar angle from north pole (0 to π)
  // x = sin(phi) * cos(angle)
  // y = sin(phi) * sin(angle)
  // z = cos(phi)
  float sin_phi = fl::sinf(phi);
  float cos_phi = fl::cosf(phi);
  float x = sin_phi * fl::cosf(angle);
  float y = sin_phi * fl::sinf(angle);
  float z = cos_phi;

  // Map cartesian values from [-1, 1] to [0, 0xFFFF]
  // This ensures positive values for uint32_t conversion
  uint32_t nx = static_cast<uint32_t>((x + 1.0f) * 0.5f * radius * 0xffff);
  uint32_t ny = static_cast<uint32_t>((y + 1.0f) * 0.5f * radius * 0xffff);
  uint32_t nz = static_cast<uint32_t>((z + 1.0f) * 0.5f * radius * 0xffff);

  // Sample three different t-slices for H, S, V components
  // Using offsets 0x0, 0x10000, 0x20000 to separate them in noise space
  uint16_t h = inoise16(nx, ny, nz, time);
  uint16_t s = inoise16(nx, ny, nz, time + 0x10000);
  uint16_t v = inoise16(nx, ny, nz, time + 0x20000);

  return HSV16(h, s, v);
}

CHSV noiseSphereHSV8(float angle, float phi, uint32_t time, float radius) {
  HSV16 hsv16 = noiseSphereHSV16(angle, phi, time, radius);

  // Scale 16-bit components down to 8-bit using bit shift with rounding
  // This preserves the relative position in the value range
  uint8_t h = (hsv16.h + 128) >> 8;
  uint8_t s = (hsv16.s + 128) >> 8;
  uint8_t v = (hsv16.v + 128) >> 8;

  return CHSV(h, s, v);
}

CRGB noiseSphereCRGB(float angle, float phi, uint32_t time, float radius) {
  // Convert spherical coordinates to cartesian
  // angle: azimuth (0 to 2π), phi: polar angle from north pole (0 to π)
  // x = sin(phi) * cos(angle)
  // y = sin(phi) * sin(angle)
  // z = cos(phi)
  float sin_phi = fl::sinf(phi);
  float cos_phi = fl::cosf(phi);
  float x = sin_phi * fl::cosf(angle);
  float y = sin_phi * fl::sinf(angle);
  float z = cos_phi;

  // Map cartesian values from [-1, 1] to [0, 0xFFFF]
  // This ensures positive values for uint32_t conversion
  uint32_t nx = static_cast<uint32_t>((x + 1.0f) * 0.5f * radius * 0xffff);
  uint32_t ny = static_cast<uint32_t>((y + 1.0f) * 0.5f * radius * 0xffff);
  uint32_t nz = static_cast<uint32_t>((z + 1.0f) * 0.5f * radius * 0xffff);

  // Sample three different t-slices for R, G, B components (direct RGB)
  // Using offsets 0x0, 0x10000, 0x20000 to separate them in noise space
  uint16_t r16 = inoise16(nx, ny, nz, time);
  uint16_t g16 = inoise16(nx, ny, nz, time + 0x10000);
  uint16_t b16 = inoise16(nx, ny, nz, time + 0x20000);

  uint8_t r = fl::int_scale<uint16_t, uint8_t>(r16);
  uint8_t g = fl::int_scale<uint16_t, uint8_t>(g16);
  uint8_t b = fl::int_scale<uint16_t, uint8_t>(b16);

  return CRGB(r, g, b);
}


/// Cylinder noise functions - sample three z-slices for independent component evolution
HSV16 noiseCylinderHSV16(float angle, float height, uint32_t time, float radius) {
  // Convert cylindrical coordinates to cartesian
  // angle: azimuth around cylinder (0 to 2π)
  // height: vertical position (used directly)
  // x = cos(angle)
  // y = sin(angle)
  // z = height
  float x = fl::cosf(angle);
  float y = fl::sinf(angle);

  // Map sin/cos values from [-1, 1] to [0, 0xFFFF]
  // This ensures positive values for uint32_t conversion
  uint32_t nx = static_cast<uint32_t>((x + 1.0f) * 0.5f * radius * 0xffff);
  uint32_t ny = static_cast<uint32_t>((y + 1.0f) * 0.5f * radius * 0xffff);
  uint32_t nz = static_cast<uint32_t>(height * radius * 0xffff);

  // Sample three different t-slices for H, S, V components
  // Using offsets 0x0, 0x10000, 0x20000 to separate them in noise space
  uint16_t h_raw = inoise16(nx, ny, nz, time);
  uint16_t s_raw = inoise16(nx, ny, nz, time + 0x10000);
  uint16_t v_raw = inoise16(nx, ny, nz, time + 0x20000);

  // Rescale from observed noise range to full 0-65535 range using global extents
  uint16_t h = rescaleNoiseValue16(h_raw);
  uint16_t s = rescaleNoiseValue16(s_raw);
  uint16_t v = rescaleNoiseValue16(v_raw);

  return HSV16(h, s, v);
}

CHSV noiseCylinderHSV8(float angle, float height, uint32_t time, float radius) {
  HSV16 hsv16 = noiseCylinderHSV16(angle, height, time, radius);

  // Scale 16-bit components down to 8-bit using bit shift with rounding
  // This preserves the relative position in the value range
  uint8_t h = (hsv16.h + 128) >> 8;
  uint8_t s = (hsv16.s + 128) >> 8;
  uint8_t v = (hsv16.v + 128) >> 8;

  return CHSV(h, s, v);
}

CRGB noiseCylinderCRGB(float angle, float height, uint32_t time, float radius) {
  // Convert cylindrical coordinates to cartesian
  // angle: azimuth around cylinder (0 to 2π)
  // height: vertical position (used directly)
  // x = cos(angle)
  // y = sin(angle)
  // z = height
  float x = fl::cosf(angle);
  float y = fl::sinf(angle);

  // Map sin/cos values from [-1, 1] to [0, 0xFFFF]
  // This ensures positive values for uint32_t conversion
  uint32_t nx = static_cast<uint32_t>((x + 1.0f) * 0.5f * radius * 0xffff);
  uint32_t ny = static_cast<uint32_t>((y + 1.0f) * 0.5f * radius * 0xffff);
  uint32_t nz = static_cast<uint32_t>(height * radius * 0xffff);

  // Sample three different t-slices for R, G, B components (direct RGB)
  // Using offsets 0x0, 0x10000, 0x20000 to separate them in noise space
  uint16_t r16_raw = inoise16(nx, ny, nz, time);
  uint16_t g16_raw = inoise16(nx, ny, nz, time + 0x10000);
  uint16_t b16_raw = inoise16(nx, ny, nz, time + 0x20000);

  // Rescale from observed noise range to full 0-65535 range using global extents
  uint16_t r16 = rescaleNoiseValue16(r16_raw);
  uint16_t g16 = rescaleNoiseValue16(g16_raw);
  uint16_t b16 = rescaleNoiseValue16(b16_raw);

  // Scale down to 8-bit
  uint8_t r = fl::int_scale<uint16_t, uint8_t>(r16);
  uint8_t g = fl::int_scale<uint16_t, uint8_t>(g16);
  uint8_t b = fl::int_scale<uint16_t, uint8_t>(b16);

  return CRGB(r, g, b);
}

} // namespace fl
