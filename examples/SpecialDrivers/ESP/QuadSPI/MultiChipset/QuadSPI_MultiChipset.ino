/// @file QuadSPI_MultiChipset.ino
/// @brief Advanced example: Different LED chipsets on each Quad-SPI lane
///
/// This example demonstrates the flexibility of Quad-SPI by using different
/// LED chipset types on each data lane. Each chipset has its own protocol
/// requirements and padding bytes, all handled automatically.
///
/// Hardware Requirements:
/// - ESP32 variant (ESP32, S2, S3, C3, P4, H2)
/// - Mixed LED strips:
///   * Lane 0: APA102 (Dotstar) - 60 LEDs
///   * Lane 1: LPD8806 - 40 LEDs
///   * Lane 2: WS2801 - 80 LEDs
///   * Lane 3: APA102 (Dotstar) - 100 LEDs
///
/// Wiring (automatically selected based on ESP32 variant):
/// ESP32:    CLK=14, D0=13 (APA102), D1=12 (LPD8806), D2=2 (WS2801), D3=4 (APA102)
/// ESP32-S2: CLK=12, D0=11 (APA102), D1=13 (LPD8806), D2=14 (WS2801), D3=9 (APA102)
/// ESP32-S3: CLK=12, D0=11 (APA102), D1=13 (LPD8806), D2=14 (WS2801), D3=9 (APA102)
/// ESP32-C3: CLK=6,  D0=7  (APA102), D1=2  (LPD8806), D2=5  (WS2801), D3=4 (APA102)
/// ESP32-P4: CLK=9,  D0=8  (APA102), D1=10 (LPD8806), D2=11 (WS2801), D3=6 (APA102)
/// ESP32-H2: CLK=4,  D0=5  (APA102), D1=0  (LPD8806), D2=2  (WS2801), D3=3 (APA102)
/// ESP32-C5: CLK=12, D0=11 (APA102), D1=5  (LPD8806), D2=4  (WS2801), D3=3 (APA102)
/// ESP32-C6: CLK=6,  D0=7  (APA102), D1=2  (LPD8806), D2=5  (WS2801), D3=4 (APA102)
/// ESP32-C2: CLK=10, D0=0  (APA102), D1=1  (LPD8806), D2=2  (WS2801), D3=3 (APA102)
///
/// Important Notes:
/// - All chipsets must tolerate the same clock speed
/// - APA102 typically runs at 6-40 MHz
/// - LPD8806 max 2 MHz
/// - WS2801 max 25 MHz
/// - Use the slowest chipset's max speed (2 MHz for this example)
///
/// @warning This is an advanced use case. Mixing chipsets requires careful
///          consideration of clock speed compatibility!
///
/// IMPLEMENTATION NOTE:
/// The actual implementation is in quad_spi_multichipset.cpp/.h to work around
/// PlatformIO's Library Dependency Finder (LDF). The LDF aggressively scans
/// .ino files and can pull in unwanted dependencies (like TinyUSB audio code
/// which triggers malloc errors on NRF52), but it does NOT traverse into .cpp
/// files the same way. By keeping implementation in .cpp, we avoid these issues.

#include "quad_spi_multichipset.h"

void setup() {
    quad_spi_multichipset_setup();
}

void loop() {
    quad_spi_multichipset_loop();
}
