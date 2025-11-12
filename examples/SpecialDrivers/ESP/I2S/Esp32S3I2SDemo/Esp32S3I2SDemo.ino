// @filter: (platform is esp32) and (target is ESP32S3)

/// ESP32-S3 I2S Parallel LED Demo
///
/// This example demonstrates using the I2S peripheral on ESP32-S3 to drive
/// 16 parallel LED strips using FastLED's standard API.
///
/// Originally based on Yves' I2S driver: https://github.com/hpwit/I2SClockLessLedDriveresp32s3
///
/// Hardware Notes:
/// - This is an advanced driver with certain ramifications:
///   - Once flashed, the ESP32-S3 might NOT want to be reprogrammed. Hold reset button during flash.
///   - Put a delay in setup() to make flashing easier during development.
///   - Serial output can interfere with DMA controller. Minimize Serial usage if device stops working.
///
/// PlatformIO Configuration:
/// Define your platformio.ini like so:
///
/// [env:esp32s3]
/// platform = https://github.com/pioarduino/platform-espressif32/releases/download/54.03.20/platform-espressif32.zip
/// framework = arduino
/// board = seeed_xiao_esp32s3
/// build_flags =
///     -DBOARD_HAS_PSRAM
///     -mfix-esp32-psram-cache-issue
///     -mfix-esp32-psram-cache-strategy=memw

#include <esp_psram.h>

#define FASTLED_USES_ESP32S3_I2S  // Must define this before including FastLED.h
#include "FastLED.h"

#define NUMSTRIPS 16
#define NUM_LEDS_PER_STRIP 256

// Pin definitions - carefully chosen to avoid conflicts
#define EXAMPLE_PIN_NUM_DATA0 1   // B0 - Safe pin (avoids USB-JTAG conflict with GPIO19)
#define EXAMPLE_PIN_NUM_DATA1 45  // B1
#define EXAMPLE_PIN_NUM_DATA2 21  // B2
#define EXAMPLE_PIN_NUM_DATA3 6   // B3
#define EXAMPLE_PIN_NUM_DATA4 7   // B4
#define EXAMPLE_PIN_NUM_DATA5 8   // G0
#define EXAMPLE_PIN_NUM_DATA6 9   // G1
#define EXAMPLE_PIN_NUM_DATA7 10  // G2
#define EXAMPLE_PIN_NUM_DATA8 11  // G3
#define EXAMPLE_PIN_NUM_DATA9 12  // G4
#define EXAMPLE_PIN_NUM_DATA10 13 // G5
#define EXAMPLE_PIN_NUM_DATA11 14 // R0
#define EXAMPLE_PIN_NUM_DATA12 15 // R1
#define EXAMPLE_PIN_NUM_DATA13 16 // R2
#define EXAMPLE_PIN_NUM_DATA14 17 // R3
#define EXAMPLE_PIN_NUM_DATA15 18 // R4

#define NUM_LEDS (NUM_LEDS_PER_STRIP * NUMSTRIPS)

CRGB leds[NUM_LEDS];

void setup() {
    psramInit();
    Serial.begin(115200);

    // This is used so that you can see if PSRAM is enabled. If not, we will crash in setup() or in loop().
    log_d("Total heap: %d", ESP.getHeapSize());
    log_d("Free heap: %d", ESP.getFreeHeap());
    log_d("Total PSRAM: %d", ESP.getPsramSize());  // If this prints out 0, then PSRAM is not enabled.
    log_d("Free PSRAM: %d", ESP.getFreePsram());

    log_d("waiting 6 seconds before startup");
    delay(6000);  // The long reset time here is to make it easier to flash the device during the development process.

    // Add each LED strip - FastLED will automatically use I2S for parallel output on ESP32-S3
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA0, GRB>(leds + (NUM_LEDS_PER_STRIP * 0), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA1, GRB>(leds + (NUM_LEDS_PER_STRIP * 1), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA2, GRB>(leds + (NUM_LEDS_PER_STRIP * 2), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA3, GRB>(leds + (NUM_LEDS_PER_STRIP * 3), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA4, GRB>(leds + (NUM_LEDS_PER_STRIP * 4), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA5, GRB>(leds + (NUM_LEDS_PER_STRIP * 5), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA6, GRB>(leds + (NUM_LEDS_PER_STRIP * 6), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA7, GRB>(leds + (NUM_LEDS_PER_STRIP * 7), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA8, GRB>(leds + (NUM_LEDS_PER_STRIP * 8), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA9, GRB>(leds + (NUM_LEDS_PER_STRIP * 9), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA10, GRB>(leds + (NUM_LEDS_PER_STRIP * 10), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA11, GRB>(leds + (NUM_LEDS_PER_STRIP * 11), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA12, GRB>(leds + (NUM_LEDS_PER_STRIP * 12), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA13, GRB>(leds + (NUM_LEDS_PER_STRIP * 13), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA14, GRB>(leds + (NUM_LEDS_PER_STRIP * 14), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA15, GRB>(leds + (NUM_LEDS_PER_STRIP * 15), NUM_LEDS_PER_STRIP);

    FastLED.setBrightness(32);
}

int offset = 0;

void loop() {
    // Create rainbow pattern across all strips
    for (int j = 0; j < NUMSTRIPS; j++) {
        for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
            int idx = (i + offset) % NUM_LEDS_PER_STRIP;
            leds[idx + NUM_LEDS_PER_STRIP * j] = CHSV(idx, 255, 255);
        }
        // Add white markers to show strip number
        for (int i = 0; i < j + 1; i++) {
            leds[i % NUM_LEDS_PER_STRIP + NUM_LEDS_PER_STRIP * j] = CRGB::White;
        }
    }

    FastLED.show();
    offset++;
}
