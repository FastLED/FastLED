

/// The Yves ESP32_S3 I2S driver is a driver that uses the I2S peripheral on the ESP32-S3 to drive leds.
/// Originally from: https://github.com/hpwit/I2SClockLessLedDriveresp32s3
///
/// This is an advanced driver. It has certain ramifications.
///   - You probably aren't going to be able to use this in ArduinoIDE, because ArduinoIDE does not allow you to put in the necessary build flags.
///     You will need to use PlatformIO to build this.
///   - These flags enable PSRAM.
///   - Once flashed, the ESP32-S3 might NOT want to be reprogrammed again. To get around
///     this hold the reset button and release when the flash tool is looking for an
///     an upload port.
///   - Put a delay in the setup function. This is to make it easier to flash the device.
///   - Serial output will mess up the DMA controller. I'm not sure why this is happening
///     but just be aware of it. If your device suddenly stops works, remove the printfs and see if that fixes the problem.
///   - You MUST use all the available PINS specified in this demo. Anything less than that will cause FastLED to crash.
///   - Certain leds will turn white in debug mode. Probably has something to do with timing.
///
/// Is RGBW supported? Yes.
///
/// Is Overclocking supported? No.
///
/// What about the new WS2812-5VB leds? Kinda. We put in a hack to add the extra wait time of 300 uS.
///
/// What are the advantages of using the FastLED bindings over the raw driver?
///  - FastLED api is more user friendly since you don't have to combine all your leds into one rectangular block.
///  - FastLED api allows you to have different sized strips which will be upscaled to the largest strip internally.
///
/// What are the advantages of using the raw driver over the FastLED bindings?
///  - The raw driver uses less memory because it doesn't have a frame buffer copy.
///
// PLATFORMIO BUILD FLAGS:
// Define your platformio.ini like so:
//
// [env:esp32s3]
// platform = https://github.com/pioarduino/platform-espressif32/releases/download/51.03.04/platform-espressif32.zip
// framework = arduino
// board = seeed_xiao_esp32s3
// build_flags = 
//     ${env:generic-esp.build_flags}
//     -DBOARD_HAS_PSRAM
//     -mfix-esp32-psram-cache-issue
//     -mfix-esp32-psram-cache-strategy=memw
// board_build.partitions = huge_app.csv
//
// Then in your setup function you are going to want to call psramInit();
//
// Want to get a contributor badge for FastLED? This driver has only been lightly tested.
// There are certain open questions:
//  - Can the pins order for the strips be changed?
//  - Are there some combination of pins that can be ommitted?
//  - What other caveats are there?
//
//  If you know the answer to these questions then please submit a PR to the FastLED repo and
//  we will update the information for the community.

#include <esp_psram.h>

#define FASTLED_USES_ESP32S3_I2S

#include "FastLED.h"
#include "fl/assert.h"


#define NUMSTRIPS 16
#define NUM_LEDS_PER_STRIP 256
#define NUM_LEDS (NUM_LEDS_PER_STRIP * NUMSTRIPS)

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


const bool gUseFastLEDApi = true;  // Set this to false to use the raw driver.

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

fl::InternalI2SDriver *driver = nullptr;
CRGB leds[NUM_LEDS];

void setup_i2s_using_fastled_api() {
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
    FastLED.setBrightness(32);
}



void setup() {
    psramInit(); // IMPORTANT: This is required to enable PSRAM. If you don't do this, the driver will not work.
    // put your setup code here, to run once:
    Serial.begin(115200);

    // This is used so that you can see if PSRAM is enabled. If not, we will crash in setup() or in loop().
    log_d("Total heap: %d", ESP.getHeapSize());
    log_d("Free heap: %d", ESP.getFreeHeap());
    log_d("Total PSRAM: %d", ESP.getPsramSize());  // If this prints out 0, then PSRAM is not enabled.
    log_d("Free PSRAM: %d", ESP.getFreePsram());

    log_d("waiting 6 second before startup");
    delay(6000);  // The long reset time here is to make it easier to flash the device during the development process.
    if (gUseFastLEDApi) {
        setup_i2s_using_fastled_api();
    } else {
        driver = fl::InternalI2SDriver::create();
        driver->initled((uint8_t *)leds, PINS, NUMSTRIPS, NUM_LEDS_PER_STRIP);  // Skips extra frame buffer copy.
        driver->setBrightness(32);
    }
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
    if (gUseFastLEDApi) {
        FastLED.show();
    } else {
        FASTLED_ASSERT(driver != nullptr, "Did not expect driver to be null");
        driver->show();
    }
}