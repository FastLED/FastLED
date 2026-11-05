#pragma once

// Compile-only audit surface. The functions are retained in the sketch object
// for target disassembly; setup/loop never call them or drive attached pins.
#include <FastLED.h>
#include "fl/stl/span.h"
#include "fl/stl/static_assert.h"

namespace software_spi_probe {
using Output = ::SoftwareSPIOutput<0, 1, DATA_RATE_MHZ(1)>;
using AlternateOutput = ::SoftwareSPIOutput<2, 3, DATA_RATE_MHZ(1)>;
#if !defined(FASTLED_FORCE_SOFTWARE_PINS)
// The forced common pin API lacks validpin(); keep real writer instantiation
// below so that this mode's register safety is still inspected (#5056).
FL_STATIC_ASSERT(fl::FastPin<0>::validpin(), "audit data pin must exist");
FL_STATIC_ASSERT(fl::FastPin<1>::validpin(), "audit clock pin must exist");
FL_STATIC_ASSERT(fl::FastPin<2>::validpin(), "alternate audit data pin must exist");
FL_STATIC_ASSERT(fl::FastPin<3>::validpin(), "alternate audit clock pin must exist");
#endif
} // namespace software_spi_probe

extern "C" FL_KEEP_ALIVE FL_NO_INLINE void software_spi_probe_init() {
    software_spi_probe::Output output;
    output.init();
}

extern "C" FL_KEEP_ALIVE FL_NO_INLINE void software_spi_probe_scalar(fl::u8 value) {
    software_spi_probe::Output::writeByte(value);
}

extern "C" FL_KEEP_ALIVE FL_NO_INLINE void software_spi_probe_bulk(fl::span<fl::u8> bytes) {
    software_spi_probe::Output output;
    output.writeBytes(bytes.data(), static_cast<int>(bytes.size()));
}

extern "C" FL_KEEP_ALIVE FL_NO_INLINE void software_spi_probe_repeat(fl::u8 value, int length) {
    software_spi_probe::Output::writeBytesValueRaw(value, length);
}

extern "C" FL_KEEP_ALIVE FL_NO_INLINE void software_spi_probe_pixels(::PixelController<RGB> pixels) {
    software_spi_probe::Output output;
    output.template writePixels<FLAG_START_BIT, DATA_NOP, RGB>(pixels);
}

extern "C" FL_KEEP_ALIVE FL_NO_INLINE void software_spi_probe_atomic(fl::u8 value) {
    fl::BitbangPin<fl::FastPin<0>>::template write<0>(value);
}

// A second pair exercises distinct physical masks on boards such as ClearCore
// (logical IO2/IO3 map to PA06/PA07). Lengths remain runtime inputs, including 0.
extern "C" FL_KEEP_ALIVE FL_NO_INLINE void software_spi_probe_init_23() {
    software_spi_probe::AlternateOutput output;
    output.init();
}

extern "C" FL_KEEP_ALIVE FL_NO_INLINE void software_spi_probe_scalar_23(fl::u8 value) {
    software_spi_probe::AlternateOutput::writeByte(value);
}

extern "C" FL_KEEP_ALIVE FL_NO_INLINE void software_spi_probe_bulk_23(fl::span<fl::u8> bytes) {
    software_spi_probe::AlternateOutput output;
    output.writeBytes(bytes.data(), static_cast<int>(bytes.size()));
}

extern "C" FL_KEEP_ALIVE FL_NO_INLINE void software_spi_probe_repeat_23(fl::u8 value, int length) {
    software_spi_probe::AlternateOutput::writeBytesValueRaw(value, length);
}

extern "C" FL_KEEP_ALIVE FL_NO_INLINE void software_spi_probe_pixels_23(::PixelController<RGB> pixels) {
    software_spi_probe::AlternateOutput output;
    output.template writePixels<FLAG_START_BIT, DATA_NOP, RGB>(pixels);
}

extern "C" FL_KEEP_ALIVE FL_NO_INLINE void software_spi_probe_atomic_23(fl::u8 value) {
    fl::BitbangPin<fl::FastPin<2>>::template write<0>(value);
}
