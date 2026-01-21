#pragma once

/// @file fastspi.h
/// Serial peripheral interface (SPI) definitions per platform
///
/// ARCHITECTURE:
/// This file provides:
/// 1. Common SPI macros and utilities (DATA_RATE_MHZ, DATA_RATE_KHZ)
/// 2. SoftwareSPIOutput template (generic bit-banging SPI)
/// 3. Includes all platform-specific implementations via dispatch headers
///
/// IMPORTANT: The SPIOutput template is NOT defined in this file.
/// Instead, it is defined by platform-specific spi_output_template.h files:
/// - Each platform (esp32, teensy, nrf52, etc.) has its own platform directory
/// - Each platform directory contains spi_output_template.h
/// - The dispatch header (platforms/spi_output_template.h) selects the right one
/// - This approach centralizes platform logic in platform-specific directories
///
/// The include pattern is:
/// #include "fastspi.h"  // Includes this file
/// // At this point, SoftwareSPIOutput is available
/// // SPIOutput is defined (by whichever platform-specific template was included)

#ifndef __INC_FASTSPI_H
#define __INC_FASTSPI_H

// ============================================================================
// ALL INCLUDES FIRST (before any namespace declarations)
// This satisfies the namespace-include-order linting requirement
// ============================================================================

#include "controller.h"
#include "lib8tion.h"
#include "fastspi_types.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"
#include "fl/int.h"

// Include platform-specific SPI device proxy implementations BEFORE FastLED.h
// These provide the hardware SPI abstractions for each platform
// This must happen before FastLED.h includes chipsets.h
#include "platforms/spi_device_proxy.h"

// Include platform-specific SPIOutput template implementations BEFORE FastLED.h
// The dispatch header selects the appropriate SPIOutput for the current platform
// This must happen BEFORE FastLED.h includes chipsets.h
#include "platforms/spi_output_template.h"

// NOW include the internal FastLED header, which defines core types without cycles
// At this point, SPIOutput is already defined for the current platform
// Note: We use fl/fastled.h instead of FastLED.h to avoid cyclic dependencies,
// since this file is included by FastLED.h
#include "fl/fastled.h"

// ============================================================================
// DATA RATE MACROS (platform-specific clock calculations)
// ============================================================================
//
// These macros provide platform-specific clock rate calculations for SPI:
// - Hardware SPI platforms (ESP32, Teensy4, etc.): Return frequency in Hz
// - Software SPI platforms (AVR, etc.): Return clock divider (CPU cycles per bit)
//
// New macros with semantic clarity (recommended):
//   FL_DATA_RATE_MHZ(X) - Returns either Hz (hardware) or divider (software) depending on platform
//   FL_DATA_RATE_KHZ(X) - Same as above but for kHz input
//   FL_TO_CLOCK_DIVIDER(FREQ_MHZ, CPU_FREQ_MHZ) - Explicit clock divider calculation
//
// Legacy macros (backwards compatibility):
//   DATA_RATE_MHZ(X) - Alias for FL_DATA_RATE_MHZ
//   DATA_RATE_KHZ(X) - Alias for FL_DATA_RATE_KHZ

#if defined(FASTLED_TEENSY3) && (F_CPU > 48000000)
// Teensy 3.x with overclocking: clock divider based on 48 MHz
#define FL_DATA_RATE_MHZ(X) (((48000000L / 1000000L) / X))
#define FL_DATA_RATE_KHZ(X) (((48000000L / 1000L) / X))
#define FL_TO_CLOCK_DIVIDER(FREQ_MHZ, CPU_FREQ_MHZ) ((CPU_FREQ_MHZ) / (FREQ_MHZ))

#elif defined(FASTLED_TEENSY4) || defined(ESP32) || (defined(ESP8266) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)) || defined(FASTLED_STUB_IMPL)
// Hardware SPI platforms: return frequency in Hz
// ESP32 always uses hardware SPI via GPIO matrix (no conditional needed)
#define FL_DATA_RATE_MHZ(X) (1000000 * (X))
#define FL_DATA_RATE_KHZ(X) (1000 * (X))
#define FL_TO_CLOCK_DIVIDER(FREQ_MHZ, CPU_FREQ_MHZ) ((CPU_FREQ_MHZ) / (FREQ_MHZ))

#else
// Software SPI platforms: return clock divider (CPU cycles per bit)
#define FL_DATA_RATE_MHZ(X) ((F_CPU / 1000000L) / X)
#define FL_DATA_RATE_KHZ(X) ((F_CPU / 1000L) / X)
#define FL_TO_CLOCK_DIVIDER(FREQ_MHZ, CPU_FREQ_MHZ) ((CPU_FREQ_MHZ) / (FREQ_MHZ))
#endif

// Backwards compatibility aliases
#define DATA_RATE_MHZ FL_DATA_RATE_MHZ
#define DATA_RATE_KHZ FL_DATA_RATE_KHZ

// ============================================================================
// GENERIC SOFTWARE SPI OUTPUT
// ============================================================================
// This template provides bit-banging SPI for any platform that supports GPIO
// It's a fallback for platforms without dedicated hardware SPI support

/// Software SPI output (generic cross-platform bit-banging)
/// NOTE: This is NOT in the fl namespace per fastspi.h design requirements
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SoftwareSPIOutput : public fl::GenericSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

#endif  // __INC_FASTSPI_H
