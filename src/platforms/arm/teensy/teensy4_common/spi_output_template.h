// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

#pragma once

// IWYU pragma: private

/// @file spi_output_template.h
/// @brief Teensy 4 SPIOutput template definition

#include "fl/stl/int.h"
#include "platforms/arm/mxrt1062/spi_device_proxy.h"
#include "platforms/arm/teensy/teensy4_common/spi_output_selector.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace fl {

namespace detail {

template <fl::u8 DATA_PIN, fl::u8 CLOCK_PIN, fl::u32 SPI_CLOCK_RATE,
          bool IS_HARDWARE, int SPI_INDEX>
class Teensy4SPIOutputImpl
    : public fl::GenericSoftwareSPIOutput<DATA_PIN, CLOCK_PIN, SPI_CLOCK_RATE> {};

template <fl::u8 DATA_PIN, fl::u8 CLOCK_PIN, fl::u32 SPI_CLOCK_RATE,
          int SPI_INDEX>
class Teensy4SPIOutputImpl<DATA_PIN, CLOCK_PIN, SPI_CLOCK_RATE, true, SPI_INDEX>
    : public SPIDeviceProxy<DATA_PIN, CLOCK_PIN, SPI_CLOCK_RATE, SPI_INDEX> {};

} // namespace detail

/// Teensy 4 SPI output. Valid LPSPI pin pairs use hardware; all other pairs
/// retain the legacy generic bitbang behavior.
template <fl::u8 DATA_PIN, fl::u8 CLOCK_PIN, fl::u32 SPI_CLOCK_RATE>
class SPIOutput
    : public detail::Teensy4SPIOutputImpl<
          DATA_PIN, CLOCK_PIN, SPI_CLOCK_RATE,
          platforms::teensy::Teensy4SpiPinRoute<
              DATA_PIN, CLOCK_PIN,
#if defined(ARDUINO_TEENSY41)
              true
#else
              false
#endif
              >::is_hardware,
          platforms::teensy::Teensy4SpiPinRoute<
              DATA_PIN, CLOCK_PIN,
#if defined(ARDUINO_TEENSY41)
              true
#else
              false
#endif
              >::bus_index> {};

}  // namespace fl
