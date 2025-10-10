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

// ESP32-only example - do not compile on other platforms
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)

#include <FastLED.h>
#include "platforms/quad_spi_platform.h"

// Only compile if Quad-SPI is available on this platform
#if FASTLED_HAS_QUAD_SPI

// Pin definitions - hardware QuadSPI pins per ESP32 variant
// These use the IO_MUX pins for optimal performance (up to 80MHz)
#if CONFIG_IDF_TARGET_ESP32
// ESP32 (original) - Using HSPI (SPI2) QuadSPI pins
#define CLOCK_PIN 14   // HSPI CLK
#define DATA_PIN_0 13  // APA102 - HSPI MOSI (D0)
#define DATA_PIN_1 12  // LPD8806 - HSPI MISO (D1)
#define DATA_PIN_2 2   // WS2801 - HSPI WP (D2)
#define DATA_PIN_3 4   // APA102 - HSPI HD (D3)

#elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
// ESP32-S2/S3 - Using SPI2 QuadSPI pins
#define CLOCK_PIN 12   // SPI2 CLK
#define DATA_PIN_0 11  // APA102 - SPI2 MOSI (D0)
#define DATA_PIN_1 13  // LPD8806 - SPI2 MISO (D1)
#define DATA_PIN_2 14  // WS2801 - SPI2 WP (D2)
#define DATA_PIN_3 9   // APA102 - SPI2 HD (D3)

#elif CONFIG_IDF_TARGET_ESP32C3
// ESP32-C3 - Using SPI2 QuadSPI pins
#define CLOCK_PIN 6    // SPI2 CLK
#define DATA_PIN_0 7   // APA102 - SPI2 MOSI (D0)
#define DATA_PIN_1 2   // LPD8806 - SPI2 MISO (D1)
#define DATA_PIN_2 5   // WS2801 - SPI2 WP (D2)
#define DATA_PIN_3 4   // APA102 - SPI2 HD (D3)

#elif CONFIG_IDF_TARGET_ESP32P4
// ESP32-P4 - Using SPI2 QuadSPI pins
#define CLOCK_PIN 9    // SPI2 CLK
#define DATA_PIN_0 8   // APA102 - SPI2 MOSI (D0)
#define DATA_PIN_1 10  // LPD8806 - SPI2 MISO (D1)
#define DATA_PIN_2 11  // WS2801 - SPI2 WP (D2)
#define DATA_PIN_3 6   // APA102 - SPI2 HD (D3)

#else
// Fallback for unknown variants - use VSPI-like pins
#define CLOCK_PIN 18
#define DATA_PIN_0 23  // APA102
#define DATA_PIN_1 19  // LPD8806
#define DATA_PIN_2 22  // WS2801
#define DATA_PIN_3 21  // APA102
#endif

// LED counts for each strip
#define NUM_LEDS_LANE_0 60   // APA102
#define NUM_LEDS_LANE_1 40   // LPD8806
#define NUM_LEDS_LANE_2 80   // WS2801
#define NUM_LEDS_LANE_3 100  // APA102

// Use slowest chipset's max speed (LPD8806 @ 2 MHz)
#define SPI_SPEED DATA_RATE_MHZ(2)

// LED arrays (one per strip)
CRGB leds_lane0[NUM_LEDS_LANE_0];  // APA102
CRGB leds_lane1[NUM_LEDS_LANE_1];  // LPD8806
CRGB leds_lane2[NUM_LEDS_LANE_2];  // WS2801
CRGB leds_lane3[NUM_LEDS_LANE_3];  // APA102

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("FastLED Quad-SPI Multi-Chipset Example");
    Serial.println("======================================");
    Serial.println("WARNING: Advanced example with mixed chipsets!");
    Serial.println();

    // Add different chipset types on each lane
    // NOTE: All use same clock pin but different data pins
    // Clock speed limited to slowest chipset (LPD8806 @ 2 MHz)

    FastLED.addLeds<APA102, DATA_PIN_0, CLOCK_PIN, RGB, SPI_SPEED>(leds_lane0, NUM_LEDS_LANE_0);
    FastLED.addLeds<LPD8806, DATA_PIN_1, CLOCK_PIN, RGB, SPI_SPEED>(leds_lane1, NUM_LEDS_LANE_1);
    FastLED.addLeds<WS2801, DATA_PIN_2, CLOCK_PIN, RGB, SPI_SPEED>(leds_lane2, NUM_LEDS_LANE_2);
    FastLED.addLeds<APA102, DATA_PIN_3, CLOCK_PIN, RGB, SPI_SPEED>(leds_lane3, NUM_LEDS_LANE_3);

    Serial.println("LED strips initialized:");
    Serial.printf("  Lane 0: %d LEDs (APA102) on pin %d\n", NUM_LEDS_LANE_0, DATA_PIN_0);
    Serial.printf("  Lane 1: %d LEDs (LPD8806) on pin %d\n", NUM_LEDS_LANE_1, DATA_PIN_1);
    Serial.printf("  Lane 2: %d LEDs (WS2801) on pin %d\n", NUM_LEDS_LANE_2, DATA_PIN_2);
    Serial.printf("  Lane 3: %d LEDs (APA102) on pin %d\n", NUM_LEDS_LANE_3, DATA_PIN_3);
    Serial.printf("  Shared Clock: pin %d @ 2 MHz\n", CLOCK_PIN);
    Serial.println();

    Serial.println("Protocol-safe padding bytes:");
    Serial.println("  APA102: 0xFF (end frame continuation)");
    Serial.println("  LPD8806: 0x00 (latch continuation)");
    Serial.println("  WS2801: 0x00 (no protocol state)");
    Serial.println();

    Serial.println("Starting multi-chipset animation...");
}

void loop() {
    static uint8_t hue = 0;
    static uint8_t animation_mode = 0;
    static uint32_t last_mode_change = 0;

    // Change animation mode every 5 seconds
    if (millis() - last_mode_change > 5000) {
        animation_mode = (animation_mode + 1) % 4;
        last_mode_change = millis();

        switch(animation_mode) {
            case 0:
                Serial.println("Animation: Rainbow on all strips");
                break;
            case 1:
                Serial.println("Animation: Different colors per chipset");
                break;
            case 2:
                Serial.println("Animation: Color waves");
                break;
            case 3:
                Serial.println("Animation: Sparkles");
                break;
        }
    }

    switch(animation_mode) {
        case 0:
            // Rainbow on all strips (same pattern)
            fill_rainbow(leds_lane0, NUM_LEDS_LANE_0, hue, 7);
            fill_rainbow(leds_lane1, NUM_LEDS_LANE_1, hue, 7);
            fill_rainbow(leds_lane2, NUM_LEDS_LANE_2, hue, 7);
            fill_rainbow(leds_lane3, NUM_LEDS_LANE_3, hue, 7);
            break;

        case 1:
            // Different solid colors per chipset type
            fill_solid(leds_lane0, NUM_LEDS_LANE_0, CRGB::Red);      // APA102: Red
            fill_solid(leds_lane1, NUM_LEDS_LANE_1, CRGB::Green);    // LPD8806: Green
            fill_solid(leds_lane2, NUM_LEDS_LANE_2, CRGB::Blue);     // WS2801: Blue
            fill_solid(leds_lane3, NUM_LEDS_LANE_3, CRGB::Yellow);   // APA102: Yellow
            break;

        case 2:
            // Color waves (different phase per strip)
            fill_rainbow(leds_lane0, NUM_LEDS_LANE_0, hue, 7);
            fill_rainbow(leds_lane1, NUM_LEDS_LANE_1, hue + 64, 7);
            fill_rainbow(leds_lane2, NUM_LEDS_LANE_2, hue + 128, 7);
            fill_rainbow(leds_lane3, NUM_LEDS_LANE_3, hue + 192, 7);
            break;

        case 3:
            // Random sparkles
            fadeToBlackBy(leds_lane0, NUM_LEDS_LANE_0, 20);
            fadeToBlackBy(leds_lane1, NUM_LEDS_LANE_1, 20);
            fadeToBlackBy(leds_lane2, NUM_LEDS_LANE_2, 20);
            fadeToBlackBy(leds_lane3, NUM_LEDS_LANE_3, 20);

            if (random8() < 80) leds_lane0[random16(NUM_LEDS_LANE_0)] = CHSV(hue, 255, 255);
            if (random8() < 80) leds_lane1[random16(NUM_LEDS_LANE_1)] = CHSV(hue + 64, 255, 255);
            if (random8() < 80) leds_lane2[random16(NUM_LEDS_LANE_2)] = CHSV(hue + 128, 255, 255);
            if (random8() < 80) leds_lane3[random16(NUM_LEDS_LANE_3)] = CHSV(hue + 192, 255, 255);
            break;
    }

    // Show all strips
    // When Quad-SPI is fully implemented, this will transmit
    // all 4 strips in parallel via hardware DMA, despite using
    // different chipset protocols
    FastLED.show();

    hue += 2;
    delay(20);
}

#else

// Stub functions for platforms without Quad-SPI support
void setup() {}
void loop() {}

#endif  // FASTLED_HAS_QUAD_SPI

#else  // !ESP32

// Stub functions for non-ESP32 platforms
void setup() {}
void loop() {}

#endif  // ESP32
