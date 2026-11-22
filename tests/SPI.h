#pragma once

/// @file SPI.h
/// @brief Minimal host stand-in for the arduino-esp32 SPI.h, so the real opt-in
/// ESP32SPIOutput (platforms/arduino/spi/fastspi_esp32_arduino.h) can run on a host.
/// The test that includes it defines SPIClass's members and records what they send.
/// It is not on any device build's include path.

#include "fl/stl/stddef.h"
#include "fl/stl/stdint.h"

#define VSPI 3
#define SPI_MSBFIRST 1
#define SPI_MODE0 0

struct SPISettings {
    SPISettings(uint32_t, uint8_t, uint8_t) {}
};

class SPIClass {
  public:
    explicit SPIClass(uint8_t) {}
    void begin(int8_t sck, int8_t miso, int8_t mosi, int8_t ss);
    void beginTransaction(SPISettings settings);
    void endTransaction();
    uint8_t transfer(uint8_t data);
    void writePixels(const void* data, uint32_t size);
};
