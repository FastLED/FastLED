#pragma once

// IWYU pragma: private

/// @file spi_output_template.h
/// @brief ESP8266 SPIOutput template definition
///
/// Default: register-level HSPI when the strip is on GPIO13 (data) and
/// GPIO14 (clock), the only pins HSPI can drive; bit-bang on any other pair
/// (#4925). FASTLED_ALL_PINS_HARDWARE_SPI keeps the legacy Arduino <SPI.h>
/// driver, whose SPI_SPEED is a frequency in Hz rather than a divider.

#include "fl/stl/int.h"

#if defined(FASTLED_ALL_PINS_HARDWARE_SPI)
#include "platforms/esp/8266/fastspi_esp8266.h"
#else
#include "fl/stl/type_traits.h"
#include "platforms/esp/8266/fastspi_esp8266_hspi.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"
#endif

namespace fl {

#if defined(FASTLED_ALL_PINS_HARDWARE_SPI)
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public ESP8266SPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#else
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput
    : public fl::conditional<_DATA_PIN == 13 && _CLOCK_PIN == 14,
                             ESP8266HspiOutput<13, 14, _SPI_CLOCK_DIVIDER>,
                             GenericSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER>>::type {};
#endif

}  // namespace fl
