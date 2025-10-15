/// @file QuadSPI_Basic.ino
/// @brief Basic example of using Hardware Quad-SPI for parallel LED control on ESP32
///
/// This example demonstrates driving 4 APA102 LED strips simultaneously using
/// ESP32's hardware Quad-SPI peripheral with DMA for zero CPU overhead.
///
/// Hardware Requirements:
/// - ESP32 variant (ESP32, S2, S3, C3, P4, H2)
/// - 4× APA102 (Dotstar) LED strips
/// - Shared clock line connected to all 4 strips
/// - 4× separate data lines (one per strip)
///
/// Wiring (automatically selected based on ESP32 variant):
/// ESP32:    CLK=14, D0=13, D1=12, D2=2,  D3=4  (HSPI pins)
/// ESP32-S2: CLK=12, D0=11, D1=13, D2=14, D3=9  (SPI2 pins)
/// ESP32-S3: CLK=12, D0=11, D1=13, D2=14, D3=9  (SPI2 pins)
/// ESP32-C3: CLK=6,  D0=7,  D1=2,  D2=5,  D3=4  (SPI2 pins)
/// ESP32-P4: CLK=9,  D0=8,  D1=10, D2=11, D3=6  (SPI2 pins)
/// ESP32-H2: CLK=4,  D0=5,  D1=0,  D2=2,  D3=3  (SPI2 pins)
/// ESP32-C5: CLK=12, D0=11, D1=5,  D2=4,  D3=3  (Safe GPIO, avoid flash pins)
/// ESP32-C6: CLK=6,  D0=7,  D1=2,  D2=5,  D3=4  (SPI2 IO_MUX pins)
/// ESP32-C2: CLK=10, D0=0,  D1=1,  D2=2,  D3=3  (Safe GPIO, avoid flash pins 11-17)
///
/// Performance:
/// - 4×100 LEDs transmitted in ~0.08ms @ 40 MHz
/// - 0% CPU usage during transmission (DMA-driven)
/// - ~27× faster than sequential software SPI
///
/// @note This feature requires ESP32/S2/S3. Not supported on other platforms.
///
/// IMPLEMENTATION NOTE:
/// The actual implementation is in quad_spi_basic.cpp/.h to work around
/// PlatformIO's Library Dependency Finder (LDF). The LDF aggressively scans
/// .ino files and can pull in unwanted dependencies (like TinyUSB audio code
/// which triggers malloc errors on NRF52), but it does NOT traverse into .cpp
/// files the same way. By keeping implementation in .cpp, we avoid these issues.

#include "quad_spi_basic.h"

void setup() {
    quad_spi_basic_setup();
}

void loop() {
    quad_spi_basic_loop();
}
