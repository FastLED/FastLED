/// This is a work in progress to bring up the I2S driver for the ESP32-S3.


#include <esp_psram.h>

#define FASTLED_USES_ESP32S3_I2S
#include "FastLED.h"
// #include "third_party/yves/I2SClockLessLedDriveresp32s3/driver.h"

// Define your platformio.ino like so:
//
// [env:esp32s3]
// # Developement branch of the open source espressif32 platform
// platform =
// https://github.com/pioarduino/platform-espressif32/releases/download/51.03.04/platform-espressif32.zip
// framework = arduino
// upload_protocol = esptool
// monitor_filters =
// 	default
// 	esp32_exception_decoder  ; Decode exceptions so that they are human
// readable. ; Symlink in the FastLED library so that changes to the library are
// reflected in the project ; build immediatly. lib_deps = FastLED board =
// esp32-s3-devkitc-1 build_flags =
//     -DBOARD_HAS_PSRAM
//     -mfix-esp32-psram-cache-issue
//     -mfix-esp32-psram-cache-strategy=memw
// board_build.partitions = huge_app.csv

#define NUMSTRIPS 16
#define NUM_LEDS_PER_STRIP 256

#define EXAMPLE_PIN_NUM_DATA0 19  // B0
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


// #define USE_FASTLED_I2S

int pins[] = {
    EXAMPLE_PIN_NUM_DATA0,  EXAMPLE_PIN_NUM_DATA1,  EXAMPLE_PIN_NUM_DATA2,
    EXAMPLE_PIN_NUM_DATA3,  EXAMPLE_PIN_NUM_DATA4,  EXAMPLE_PIN_NUM_DATA5,
    EXAMPLE_PIN_NUM_DATA6,  EXAMPLE_PIN_NUM_DATA7,  EXAMPLE_PIN_NUM_DATA8,
    EXAMPLE_PIN_NUM_DATA9,  EXAMPLE_PIN_NUM_DATA10, EXAMPLE_PIN_NUM_DATA11,
    EXAMPLE_PIN_NUM_DATA12, EXAMPLE_PIN_NUM_DATA13, EXAMPLE_PIN_NUM_DATA14,
    EXAMPLE_PIN_NUM_DATA15};

#ifndef USE_FASTLED_I2S
fl::InternalI2SDriver *driver = fl::InternalI2SDriver::create();
#endif

#define NUM_LEDS (NUM_LEDS_PER_STRIP * NUMSTRIPS)

CRGB leds[NUM_LEDS];
void setup() {
    psramInit();
    // put your setup code here, to run once:
    Serial.begin(115200);

    log_d("Total heap: %d", ESP.getHeapSize());
    log_d("Free heap: %d", ESP.getFreeHeap());
    log_d("Total PSRAM: %d", ESP.getPsramSize());
    log_d("Free PSRAM: %d", ESP.getFreePsram());

    Serial.println("waiting 3 second before startup");
    delay(3000);

    #ifdef USE_FASTLED_I2S
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA3, GRB>(leds, NUM_LEDS_PER_STRIP);
    #if 0
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA0, GRB>(leds, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA1, GRB>(leds, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA2, GRB>(leds, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA4, GRB>(leds, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA5, GRB>(leds, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA6, GRB>(leds, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA7, GRB>(leds, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA8, GRB>(leds, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA9, GRB>(leds, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA10, GRB>(leds, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA11, GRB>(leds, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA12, GRB>(leds, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA13, GRB>(leds, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA14, GRB>(leds, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA15, GRB>(leds, NUM_LEDS_PER_STRIP);
    FastLED.setBrightness(32);
    #endif

    #else
    driver->initled((uint8_t *)leds, pins, NUMSTRIPS, NUM_LEDS_PER_STRIP);
    driver->setBrightness(32);
    #endif

}
int off = 0;

void loop() {

    for (int j = 0; j < NUMSTRIPS; j++) {

        for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {

            leds[(i + off) % NUM_LEDS_PER_STRIP + NUM_LEDS_PER_STRIP * j] =
                CHSV(i, 255, 255);
        }
        for (int i = 0; i < j + 1; i++) {

            leds[i % NUM_LEDS_PER_STRIP + NUM_LEDS_PER_STRIP * j] = CRGB::White;
        }
    }
    #ifdef USE_FASTLED_I2S
    FastLED.show();
    #else
    driver->show();
    #endif
    off++;
}