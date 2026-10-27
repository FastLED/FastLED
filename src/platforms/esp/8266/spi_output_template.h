#pragma once

// IWYU pragma: private

/// @file spi_output_template.h
/// @brief ESP8266 SPIOutput selection
///
/// The ESP8266 has exactly one usable hardware SPI pin pair for LEDs:
/// GPIO13 (MOSI) / GPIO14 (SCK). Picks, per pin pair:
///
///   - declared hardware-SPI pair (FL_PIN_SPI_DATA_1 / FL_PIN_SPI_CLOCK_1)
///       -> native register-level HSPI (no Arduino <SPI.h>)
///   - otherwise -> generic software SPI bit-bang
///
/// The pair is expressed through the active pin-order mode and resolved to the
/// real GPIO pads via FastPin<>::kRealPin, because the peripheral pads are
/// fixed at GPIO13/14 while the pin numbers in the sketch are not:
///
///   FASTLED_ESP8266_RAW_PIN_ORDER (default) -> 13 / 14
///   FASTLED_ESP8266_D1_PIN_ORDER            ->  7 / 5   (GPIO13 / GPIO14)
///   FASTLED_ESP8266_NODEMCU_PIN_ORDER       ->  7 / 5   (GPIO13 / GPIO14)
///
/// With FASTLED_ALL_PINS_HARDWARE_SPI the legacy Arduino-SPI shim is kept for
/// non-HSPI pin pairs so existing sketches that opted in keep compiling; the
/// native HSPI driver still wins on the HSPI pair because it is the only path
/// that can actually drive those pads at the requested rate without <SPI.h>.

#include "fl/stl/int.h"
#include "platforms/esp/8266/fastpin_esp8266.h"  // FastPin<>::kRealPin  // IWYU pragma: keep
#include "platforms/esp/8266/fastspi_esp8266_hspi.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

#if defined(FASTLED_ALL_PINS_HARDWARE_SPI)
#include "platforms/esp/8266/fastspi_esp8266.h"
#endif

namespace fl {

#if defined(FASTLED_ALL_PINS_HARDWARE_SPI)
/// Legacy opt-in: Arduino-SPI shim for arbitrary pin numbers.
template <u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_SPEED>
class SPIOutput : public ESP8266SPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_SPEED> {};
#else
/// Default: generic software SPI bit-bang (SPI_SPEED is cycles per bit here,
/// matching fastspi.h's software-path FL_DATA_RATE_MHZ).
template <u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_SPEED>
class SPIOutput : public fl::GenericSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_SPEED> {};
#endif

/// The platform's declared hardware-SPI pair always uses the HSPI peripheral.
/// Mapping through kRealPin keeps D1/NodeMCU pin-order modes on the same pads.
template <u32 _SPI_SPEED>
class SPIOutput<FL_PIN_SPI_DATA_1, FL_PIN_SPI_CLOCK_1, _SPI_SPEED>
    : public ESP8266HSPIOutput<fl::FastPin<FL_PIN_SPI_DATA_1>::kRealPin,
                               fl::FastPin<FL_PIN_SPI_CLOCK_1>::kRealPin,
                               _SPI_SPEED> {};

}  // namespace fl
