#pragma once

// ESP platform-specific Quad-SPI detection and configuration
// This header is included by platforms/quad_spi_platform.h when ESP32 is detected

namespace fl {

// ESP8266: No hardware Quad-SPI support
#if defined(ARDUINO_ARCH_ESP8266) || defined(ESP8266)
    #define FASTLED_HAS_QUAD_SPI 0
    #define FASTLED_QUAD_SPI_MAX_LANES 0
    #define FASTLED_QUAD_SPI_NUM_BUSES 0
#endif

}  // namespace fl
