/// @file spi_quad_esp32.cpp
/// @brief ESP32 implementation of Quad-SPI factory
///
/// This file provides the strong definition of createInstances()
/// that overrides the weak default in platforms/shared/spi_quad.cpp

#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4)

#include "platforms/shared/spi_quad.h"
#include "platforms/esp/32/spi_quad_esp32.h"

namespace fl {

/// ESP32 factory override - returns available SPI bus instances
/// Strong definition overrides weak default
fl::vector<SPIQuad*> SPIQuad::createInstances() {
    fl::vector<SPIQuad*> controllers;

#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3)
    // ESP32 classic/S2/S3: 2 SPI buses (HSPI/bus 2, VSPI/bus 3)
    // Each bus supports full quad-SPI (4 data lines)
    static SPIQuadESP32 controller2(2, "HSPI");  // Bus 2 (HSPI) - static lifetime
    static SPIQuadESP32 controller3(3, "VSPI");  // Bus 3 (VSPI) - static lifetime
    controllers.push_back(&controller2);
    controllers.push_back(&controller3);

#elif defined(ESP32C3) || defined(ESP32C2) || defined(ESP32C6) || defined(ESP32H2)
    // ESP32-C3/C2/C6/H2: 1 SPI bus (bus 2)
    // Supports dual-SPI only (2 data lines max)
    static SPIQuadESP32 controller2(2, "SPI2");  // Bus 2 - static lifetime
    controllers.push_back(&controller2);

#elif defined(ESP32P4)
    // ESP32-P4: 2 SPI buses
    // Supports octal-SPI (8 data lines) - future enhancement
    static SPIQuadESP32 controller2(2, "SPI2");  // Bus 2 - static lifetime
    static SPIQuadESP32 controller3(3, "SPI3");  // Bus 3 - static lifetime
    controllers.push_back(&controller2);
    controllers.push_back(&controller3);

#endif

    return controllers;
}

}  // namespace fl

#endif  // ESP32 variants
