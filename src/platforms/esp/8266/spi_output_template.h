#pragma once

/// @file spi_output_template.h
/// @brief ESP8266 SPIOutput template definition

#include "fl/int.h"
#include "platforms/esp/8266/fastspi_esp8266.h"

namespace fl {

#if defined(FASTLED_ESP8266) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)

/// ESP8266 hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public ESP8266HardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

#endif  // ESP8266 with FASTLED_ALL_PINS_HARDWARE_SPI

}  // namespace fl
