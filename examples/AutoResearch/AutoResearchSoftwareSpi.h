#pragma once

// Compile/link audit surface. setup retains function addresses through RAM
// accesses; it never calls the GPIO probes or drives attached pins.
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

// Separate leaf symbols let the real-target disassembly oracle check each
// boundary independently. They use the actual selected production policy.
#define FL_SPI_DELAY_PROBE(COUNT) \
    extern "C" FL_KEEP_ALIVE FL_NO_INLINE void software_spi_probe_delay_##COUNT() { \
        fl::BitbangPin<fl::FastPin<1>>::template delay<COUNT>(); \
    }
FL_SPI_DELAY_PROBE(0)
FL_SPI_DELAY_PROBE(1)
FL_SPI_DELAY_PROBE(2)
FL_SPI_DELAY_PROBE(3)
FL_SPI_DELAY_PROBE(15)
FL_SPI_DELAY_PROBE(16)
FL_SPI_DELAY_PROBE(17)
FL_SPI_DELAY_PROBE(55)
FL_SPI_DELAY_PROBE(57)
FL_SPI_DELAY_PROBE(40000)
FL_SPI_DELAY_PROBE(2147483648)
#undef FL_SPI_DELAY_PROBE

// Retained but never called: real vendor codegen for zero/tiny/odd/long
// defaults, including a phase above AVR's signed16 cycle_t range.
extern "C" FL_KEEP_ALIVE FL_NO_INLINE void software_spi_probe_delay_boundaries() {
    using Timing = fl::BitbangPin<fl::FastPin<1>>;
#if !defined(ESP8266)
    Timing::template delay<-100>();
#endif
    Timing::template delay<0>();
    Timing::template delay<1>();
    Timing::template delay<2>();
    Timing::template delay<3>();
    Timing::template delay<55>();
    Timing::template delay<57>();
    Timing::template delay<40000>();
    Timing::template delay<2147483648ll>();
    fl::GenericSoftwareSPIOutput<0, 1, 80000>::writeByte(0x81);
}

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

// Defined in AutoResearchHelpers.cpp, whose separate translation unit checks
// FastLED-first include order rather than generated main.cpp's Arduino-first.
extern "C" void software_spi_probe_fastled_first_init();
extern "C" void software_spi_probe_fastled_first_scalar(fl::u8 value);

FASTLED_FORCE_INLINE void softwareSpiProbeKeepLinked() {
    // Volatile pointer writes/reads keep every relocation live through final
    // section GC, forcing dependency resolution without executing any probe.
    decltype(&software_spi_probe_init) volatile init[] = {
        &software_spi_probe_init, &software_spi_probe_delay_boundaries, &software_spi_probe_init_23,
        &software_spi_probe_fastled_first_init};
    decltype(&software_spi_probe_delay_0) volatile delays[] = {
        &software_spi_probe_delay_0, &software_spi_probe_delay_1,
        &software_spi_probe_delay_2, &software_spi_probe_delay_3,
        &software_spi_probe_delay_15, &software_spi_probe_delay_16,
        &software_spi_probe_delay_17, &software_spi_probe_delay_55,
        &software_spi_probe_delay_57, &software_spi_probe_delay_40000,
        &software_spi_probe_delay_2147483648};
    decltype(&software_spi_probe_scalar) volatile scalar[] = {
        &software_spi_probe_scalar, &software_spi_probe_scalar_23,
        &software_spi_probe_atomic, &software_spi_probe_atomic_23,
        &software_spi_probe_fastled_first_scalar};
    decltype(&software_spi_probe_bulk) volatile bulk[] = {
        &software_spi_probe_bulk, &software_spi_probe_bulk_23};
    decltype(&software_spi_probe_repeat) volatile repeat[] = {
        &software_spi_probe_repeat, &software_spi_probe_repeat_23};
    decltype(&software_spi_probe_pixels) volatile pixels[] = {
        &software_spi_probe_pixels, &software_spi_probe_pixels_23};
    for (unsigned i = 0; i < 4; ++i) { (void)init[i]; }
    for (unsigned i = 0; i < 11; ++i) { (void)delays[i]; }
    for (unsigned i = 0; i < 5; ++i) { (void)scalar[i]; }
    for (unsigned i = 0; i < 2; ++i) {
        (void)bulk[i];
        (void)repeat[i];
        (void)pixels[i];
    }
}
