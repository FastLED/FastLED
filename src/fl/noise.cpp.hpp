/// @file noise.cpp
/// Ring, sphere, and cylinder noise functions in the fl namespace

#include "fl/fastled.h"
#include "fl/hsv16.h"
#include "fl/map_range.h"
#include "fl/noise.h"

// Forward declarations from src/noise.cpp
fl::u16 inoise16(fl::u32 x, fl::u32 y, fl::u32 z, fl::u32 t);
fl::u16 inoise16(fl::u32 x, fl::u32 y, fl::u32 z);



namespace fl {

/// Rescale raw inoise16() output to full 16-bit range [0, 65535]
/// Curries in the global NOISE16_EXTENT_MIN/MAX extents for clean, reusable rescaling.
/// @param raw_value Raw noise value from inoise16()
/// @return Rescaled value spanning the full 16-bit range (0-65535)
FASTLED_FORCE_INLINE u16 rescaleNoiseValue16(u16 raw_value) {
  return fl::map_range_clamped(raw_value, fl::NOISE16_EXTENT_MIN, fl::NOISE16_EXTENT_MAX,
                               u16(0), u16(65535));
}

/// Ring noise functions - sample three z-slices for independent component evolution
HSV16 noiseRingHSV16(float angle, u32 time, float radius) {
  // Convert angle to cartesian coordinates
  float x = fl::cosf(angle);
  float y = fl::sinf(angle);

  // Map sin/cos values from [-1, 1] to [0, 0xFFFF]
  // This ensures positive values for uint32_t conversion
  u32 nx = static_cast<u32>((x + 1.0f) * 0.5f * radius * 0xffff);
  u32 ny = static_cast<u32>((y + 1.0f) * 0.5f * radius * 0xffff);

  // Sample three different z-slices for H, S, V components
  // Using offsets 0x0, 0x10000, 0x20000 to separate them in noise space
  u16 h_raw = inoise16(nx, ny, time);
  u16 s_raw = inoise16(nx, ny, time + 0x10000);
  u16 v_raw = inoise16(nx, ny, time + 0x20000);

  // Rescale from observed noise range to full 0-65535 range using global extents
  u16 h = rescaleNoiseValue16(h_raw);
  u16 s = rescaleNoiseValue16(s_raw);
  u16 v = rescaleNoiseValue16(v_raw);

  return HSV16(h, s, v);
}

CHSV noiseRingHSV8(float angle, u32 time, float radius) {
  fl::HSV16 hsv16 = noiseRingHSV16(angle, time, radius);

  // Scale 16-bit components down to 8-bit using bit shift with rounding
  // This preserves the relative position in the value range
  u8 h = (hsv16.h + 128) >> 8;
  u8 s = (hsv16.s + 128) >> 8;
  u8 v = (hsv16.v + 128) >> 8;

  return CHSV(h, s, v);
}

CRGB noiseRingCRGB(float angle, u32 time, float radius) {
  // Convert angle to cartesian coordinates
  float x = fl::cosf(angle);
  float y = fl::sinf(angle);

  // Map sin/cos values from [-1, 1] to [0, 0xFFFF]
  // This ensures positive values for uint32_t conversion
  u32 nx = static_cast<u32>((x + 1.0f) * 0.5f * radius * 0xffff);
  u32 ny = static_cast<u32>((y + 1.0f) * 0.5f * radius * 0xffff);

  // Sample three different z-slices for R, G, B components (direct RGB)
  // Using offsets 0x0, 0x10000, 0x20000 to separate them in noise space
  u16 r16_raw = inoise16(nx, ny, time);
  u16 g16_raw = inoise16(nx, ny, time + 0x10000);
  u16 b16_raw = inoise16(nx, ny, time + 0x20000);

  // Rescale from observed noise range to full 0-65535 range using global extents
  u16 r16 = rescaleNoiseValue16(r16_raw);
  u16 g16 = rescaleNoiseValue16(g16_raw);
  u16 b16 = rescaleNoiseValue16(b16_raw);

  // Scale down to 8-bit
  u8 r = r16 >> 8;
  u8 g = g16 >> 8;
  u8 b = b16 >> 8;

  return CRGB(r, g, b);
}


/// Sphere noise functions - sample three z-slices for independent component evolution
HSV16 noiseSphereHSV16(float angle, float phi, u32 time, float radius) {
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
  u32 nx = static_cast<u32>((x + 1.0f) * 0.5f * radius * 0xffff);
  u32 ny = static_cast<u32>((y + 1.0f) * 0.5f * radius * 0xffff);
  u32 nz = static_cast<u32>((z + 1.0f) * 0.5f * radius * 0xffff);

  // Sample three different t-slices for H, S, V components
  // Using offsets 0x0, 0x10000, 0x20000 to separate them in noise space
  u16 h = inoise16(nx, ny, nz, time);
  u16 s = inoise16(nx, ny, nz, time + 0x10000);
  u16 v = inoise16(nx, ny, nz, time + 0x20000);

  return HSV16(h, s, v);
}

CHSV noiseSphereHSV8(float angle, float phi, u32 time, float radius) {
  HSV16 hsv16 = noiseSphereHSV16(angle, phi, time, radius);

  // Scale 16-bit components down to 8-bit using bit shift with rounding
  // This preserves the relative position in the value range
  u8 h = (hsv16.h + 128) >> 8;
  u8 s = (hsv16.s + 128) >> 8;
  u8 v = (hsv16.v + 128) >> 8;

  return CHSV(h, s, v);
}

CRGB noiseSphereCRGB(float angle, float phi, u32 time, float radius) {
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
  u32 nx = static_cast<u32>((x + 1.0f) * 0.5f * radius * 0xffff);
  u32 ny = static_cast<u32>((y + 1.0f) * 0.5f * radius * 0xffff);
  u32 nz = static_cast<u32>((z + 1.0f) * 0.5f * radius * 0xffff);

  // Sample three different t-slices for R, G, B components (direct RGB)
  // Using offsets 0x0, 0x10000, 0x20000 to separate them in noise space
  u16 r16 = inoise16(nx, ny, nz, time);
  u16 g16 = inoise16(nx, ny, nz, time + 0x10000);
  u16 b16 = inoise16(nx, ny, nz, time + 0x20000);

  u8 r = fl::int_scale<u16, u8>(r16);
  u8 g = fl::int_scale<u16, u8>(g16);
  u8 b = fl::int_scale<u16, u8>(b16);

  return CRGB(r, g, b);
}


/// Cylinder noise functions - sample three z-slices for independent component evolution
HSV16 noiseCylinderHSV16(float angle, float height, u32 time, float radius) {
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
  u32 nx = static_cast<u32>((x + 1.0f) * 0.5f * radius * 0xffff);
  u32 ny = static_cast<u32>((y + 1.0f) * 0.5f * radius * 0xffff);
  u32 nz = static_cast<u32>(height * radius * 0xffff);

  // Sample three different t-slices for H, S, V components
  // Using offsets 0x0, 0x10000, 0x20000 to separate them in noise space
  u16 h_raw = inoise16(nx, ny, nz, time);
  u16 s_raw = inoise16(nx, ny, nz, time + 0x10000);
  u16 v_raw = inoise16(nx, ny, nz, time + 0x20000);

  // Rescale from observed noise range to full 0-65535 range using global extents
  u16 h = rescaleNoiseValue16(h_raw);
  u16 s = rescaleNoiseValue16(s_raw);
  u16 v = rescaleNoiseValue16(v_raw);

  return HSV16(h, s, v);
}

CHSV noiseCylinderHSV8(float angle, float height, u32 time, float radius) {
  HSV16 hsv16 = noiseCylinderHSV16(angle, height, time, radius);

  // Scale 16-bit components down to 8-bit using bit shift with rounding
  // This preserves the relative position in the value range
  u8 h = (hsv16.h + 128) >> 8;
  u8 s = (hsv16.s + 128) >> 8;
  u8 v = (hsv16.v + 128) >> 8;

  return CHSV(h, s, v);
}

CRGB noiseCylinderCRGB(float angle, float height, u32 time, float radius) {
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
  u32 nx = static_cast<u32>((x + 1.0f) * 0.5f * radius * 0xffff);
  u32 ny = static_cast<u32>((y + 1.0f) * 0.5f * radius * 0xffff);
  u32 nz = static_cast<u32>(height * radius * 0xffff);

  // Sample three different t-slices for R, G, B components (direct RGB)
  // Using offsets 0x0, 0x10000, 0x20000 to separate them in noise space
  u16 r16_raw = inoise16(nx, ny, nz, time);
  u16 g16_raw = inoise16(nx, ny, nz, time + 0x10000);
  u16 b16_raw = inoise16(nx, ny, nz, time + 0x20000);

  // Rescale from observed noise range to full 0-65535 range using global extents
  u16 r16 = rescaleNoiseValue16(r16_raw);
  u16 g16 = rescaleNoiseValue16(g16_raw);
  u16 b16 = rescaleNoiseValue16(b16_raw);

  // Scale down to 8-bit
  u8 r = fl::int_scale<u16, u8>(r16);
  u8 g = fl::int_scale<u16, u8>(g16);
  u8 b = fl::int_scale<u16, u8>(b16);

  return CRGB(r, g, b);
}

} // namespace fl
