
#ifdef ESP32

/// The Yves ESP32_S3 I2S driver is a driver that uses the I2S peripheral on the ESP32-S3 to drive leds.
/// Originally from: https://github.com/hpwit/I2SClockLessLedDriveresp32s3
///
///
/// This is an advanced driver. It has certain ramifications.
///   - Once flashed, the ESP32-S3 might NOT want to be reprogrammed again. To get around
///     this hold the reset button and release when the flash tool is looking for an
///     an upload port.
///   - Put a delay in the setup function. This is to make it easier to flash the device during developement.
///   - Serial output will mess up the DMA controller. I'm not sure why this is happening
///     but just be aware of it. If your device suddenly stops working, remove the printfs and see if that fixes the problem.
///
/// Is RGBW supported? Yes.
///
/// Is Overclocking supported? Yes. Use this to bend the timeings to support other WS281X variants. Fun fact, just overclock the
/// chipset until the LED starts working.
///
/// What about the new WS2812-5VB leds? Yes, they have 250us timing.
///
/// Why use this?
/// Raw YVes driver needs a perfect parallel rectacngle buffer for operation. In this code we've provided FastLED
/// type bindings.
///
// ArduinoIDE
//  Should already be enabled.
//
// PLATFORMIO BUILD FLAGS:
// Define your platformio.ini like so:
//
// PlatformIO
// [env:esp32s3]
// platform = https://github.com/pioarduino/platform-espressif32/releases/download/54.03.20/platform-espressif32.zip
// framework = arduino
// board = seeed_xiao_esp32s3


#define FASTLED_USES_ESP32S3_I2S  // Must define this before including FastLED.h


#include "FastLED.h"
#include "fl/assert.h"


#define NUMSTRIPS 16
#define NUM_LEDS_PER_STRIP 256
#define NUM_LEDS (NUM_LEDS_PER_STRIP * NUMSTRIPS)

// Note that you can use less strips than this.

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


// Users say you can use a lot less strips. Experiment around and find out!
// Please comment at reddit.com/r/fastled and let us know if you have problems.
// Or send us a picture of your Triumps!
int PINS[] = {
    EXAMPLE_PIN_NUM_DATA0,
    EXAMPLE_PIN_NUM_DATA1,
    EXAMPLE_PIN_NUM_DATA2,
    EXAMPLE_PIN_NUM_DATA3,
    EXAMPLE_PIN_NUM_DATA4,
    EXAMPLE_PIN_NUM_DATA5,
    EXAMPLE_PIN_NUM_DATA6,
    EXAMPLE_PIN_NUM_DATA7,
    EXAMPLE_PIN_NUM_DATA8,
    EXAMPLE_PIN_NUM_DATA9,
    EXAMPLE_PIN_NUM_DATA10,
    EXAMPLE_PIN_NUM_DATA11,
    EXAMPLE_PIN_NUM_DATA12,
    EXAMPLE_PIN_NUM_DATA13,
    EXAMPLE_PIN_NUM_DATA14,
    EXAMPLE_PIN_NUM_DATA15
};

CRGB leds[NUM_LEDS];

void setup_i2s() {
    // Note, in this case we are using contingious memory for the leds. But this is not required.
    // Each strip can be a different size and the FastLED api will upscale the smaller strips to the largest strip.
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA0, GRB>(
        leds + (0 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA1, GRB>(
        leds + (1 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA2, GRB>(
        leds + (2 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA3, GRB>(
        leds + (3 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA4, GRB>(
        leds + (4 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA5, GRB>(
        leds + (5 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA6, GRB>(
        leds + (6 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA7, GRB>(
        leds + (7 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA8, GRB>(
        leds + (8 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA9, GRB>(
        leds + (9 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA10, GRB>(
        leds + (10 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA11, GRB>(
        leds + (11 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA12, GRB>(
        leds + (12 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA13, GRB>(
        leds + (13 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA14, GRB>(
        leds + (14 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
    FastLED.addLeds<WS2812, EXAMPLE_PIN_NUM_DATA15, GRB>(
        leds + (15 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP
    );
}



void setup() {
    // put your setup code here, to run once:
    Serial.begin(57600);

    // This is used so that you can see if PSRAM is enabled. If not, we will crash in setup() or in loop().
    log_d("Total heap: %d", ESP.getHeapSize());
    log_d("Free heap: %d", ESP.getFreeHeap());
    log_d("Total PSRAM: %d", ESP.getPsramSize());  // If this prints out 0, then PSRAM is not enabled.
    log_d("Free PSRAM: %d", ESP.getFreePsram());

    log_d("waiting 6 seconds before startup");
    delay(6000);  // The long reset time here is to make it easier to flash the device during the development process.

    setup_i2s();
    FastLED.setBrightness(32);
   
}

void fill_rainbow(CRGB* all_leds) {
    static int s_offset = 0;
    for (int j = 0; j < NUMSTRIPS; j++) {
        for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
            int idx = (i + s_offset) % NUM_LEDS_PER_STRIP + NUM_LEDS_PER_STRIP * j;
            all_leds[idx] = CHSV(i, 255, 255);
        }
    }
    s_offset++;
}

void loop() {
    fill_rainbow(leds);
    FastLED.show();
    
}

#else  // ESP32

// Non-ESP32 platform - provide minimal example for compilation testing
#include "FastLED.h"

#define NUM_LEDS 16
#define DATA_PIN 3

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
}

void loop() {
    fill_rainbow(leds, NUM_LEDS, 0, 7);
    FastLED.show();
    delay(50);
}

#endif  // ESP32
