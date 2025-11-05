
#ifdef ESP32

/// ESP32-S3 I2S Bulk LED Controller Demo
///
/// This example demonstrates the BulkClockless API for managing multiple LED strips
/// using the I2S peripheral on ESP32/ESP32-S3 for parallel output.
///
/// Originally based on Yves' I2S driver: https://github.com/hpwit/I2SClockLessLedDriveresp32s3
///
/// Key Features:
/// - Uses the new BulkClockless API with I2S peripheral
/// - Manages 16 LED strips sharing a single I2S controller
/// - Each strip can have individual color correction, temperature, dither, and RGBW settings
/// - ScreenMap integration for spatial positioning
///
/// What's Changed (New API):
/// - Instead of 16 separate FastLED.addLeds() calls, uses single FastLED.addBulkLeds<WS2812, I2S>()
/// - Individual strip buffers instead of one large contiguous buffer
/// - Per-strip settings via get(pin)->setCorrection(), etc.
/// - Spatial positioning via ScreenMap for each strip
///
/// Hardware Notes:
/// - This is an advanced driver with certain ramifications:
///   - Once flashed, the ESP32-S3 might NOT want to be reprogrammed. Hold reset button during flash.
///   - Put a delay in setup() to make flashing easier during development.
///   - Serial output can interfere with DMA controller. Remove printfs if device stops working.
///
/// Supported Features:
/// - RGBW: Yes (via setRgbw())
/// - Overclocking: Yes (to support WS281X variants and new WS2812-5VB with 250us timing)
/// - Per-strip color correction and temperature
/// - Dynamic add/remove of strips (NOT during show())
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
#include "fl/screenmap.h"

using namespace fl;

#define NUMSTRIPS 16
#define NUM_LEDS_PER_STRIP 256
#define NUM_LEDS (NUM_LEDS_PER_STRIP * NUMSTRIPS)

// Note that you can use less strips than this.

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

// Individual LED buffers for each strip
CRGB strip0[NUM_LEDS_PER_STRIP];
CRGB strip1[NUM_LEDS_PER_STRIP];
CRGB strip2[NUM_LEDS_PER_STRIP];
CRGB strip3[NUM_LEDS_PER_STRIP];
CRGB strip4[NUM_LEDS_PER_STRIP];
CRGB strip5[NUM_LEDS_PER_STRIP];
CRGB strip6[NUM_LEDS_PER_STRIP];
CRGB strip7[NUM_LEDS_PER_STRIP];
CRGB strip8[NUM_LEDS_PER_STRIP];
CRGB strip9[NUM_LEDS_PER_STRIP];
CRGB strip10[NUM_LEDS_PER_STRIP];
CRGB strip11[NUM_LEDS_PER_STRIP];
CRGB strip12[NUM_LEDS_PER_STRIP];
CRGB strip13[NUM_LEDS_PER_STRIP];
CRGB strip14[NUM_LEDS_PER_STRIP];
CRGB strip15[NUM_LEDS_PER_STRIP];

// Bulk controller reference
BulkClockless<WS2812, I2S>* i2s_bulk = nullptr;

void setup_i2s() {
    // I2S bulk controller: All strips share the I2S peripheral
    // On ESP32/S3: Uses I2S peripheral for parallel output
    // On other platforms: Uses CPU fallback (warning will be printed)

    // Create screen maps for each strip's spatial position
    // Arranging strips in a 4x4 grid layout
    ScreenMap map0 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map0.addOffsetX(0.0f).addOffsetY(0.0f);

    ScreenMap map1 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map1.addOffsetX(100.0f).addOffsetY(0.0f);

    ScreenMap map2 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map2.addOffsetX(200.0f).addOffsetY(0.0f);

    ScreenMap map3 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map3.addOffsetX(300.0f).addOffsetY(0.0f);

    ScreenMap map4 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map4.addOffsetX(0.0f).addOffsetY(50.0f);

    ScreenMap map5 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map5.addOffsetX(100.0f).addOffsetY(50.0f);

    ScreenMap map6 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map6.addOffsetX(200.0f).addOffsetY(50.0f);

    ScreenMap map7 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map7.addOffsetX(300.0f).addOffsetY(50.0f);

    ScreenMap map8 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map8.addOffsetX(0.0f).addOffsetY(100.0f);

    ScreenMap map9 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map9.addOffsetX(100.0f).addOffsetY(100.0f);

    ScreenMap map10 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map10.addOffsetX(200.0f).addOffsetY(100.0f);

    ScreenMap map11 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map11.addOffsetX(300.0f).addOffsetY(100.0f);

    ScreenMap map12 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map12.addOffsetX(0.0f).addOffsetY(150.0f);

    ScreenMap map13 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map13.addOffsetX(100.0f).addOffsetY(150.0f);

    ScreenMap map14 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map14.addOffsetX(200.0f).addOffsetY(150.0f);

    ScreenMap map15 = ScreenMap::DefaultStrip(NUM_LEDS_PER_STRIP, 1.0f, 0.4f);
    map15.addOffsetX(300.0f).addOffsetY(150.0f);

    // Create bulk controller with all 16 strips
    // Initializer format: {pin, buffer_ptr, num_leds, screenMap}
    auto& i2s_ref = FastLED.addBulkLeds<WS2812, I2S>({
        {EXAMPLE_PIN_NUM_DATA0, strip0, NUM_LEDS_PER_STRIP, map0},
        {EXAMPLE_PIN_NUM_DATA1, strip1, NUM_LEDS_PER_STRIP, map1},
        {EXAMPLE_PIN_NUM_DATA2, strip2, NUM_LEDS_PER_STRIP, map2},
        {EXAMPLE_PIN_NUM_DATA3, strip3, NUM_LEDS_PER_STRIP, map3},
        {EXAMPLE_PIN_NUM_DATA4, strip4, NUM_LEDS_PER_STRIP, map4},
        {EXAMPLE_PIN_NUM_DATA5, strip5, NUM_LEDS_PER_STRIP, map5},
        {EXAMPLE_PIN_NUM_DATA6, strip6, NUM_LEDS_PER_STRIP, map6},
        {EXAMPLE_PIN_NUM_DATA7, strip7, NUM_LEDS_PER_STRIP, map7},
        {EXAMPLE_PIN_NUM_DATA8, strip8, NUM_LEDS_PER_STRIP, map8},
        {EXAMPLE_PIN_NUM_DATA9, strip9, NUM_LEDS_PER_STRIP, map9},
        {EXAMPLE_PIN_NUM_DATA10, strip10, NUM_LEDS_PER_STRIP, map10},
        {EXAMPLE_PIN_NUM_DATA11, strip11, NUM_LEDS_PER_STRIP, map11},
        {EXAMPLE_PIN_NUM_DATA12, strip12, NUM_LEDS_PER_STRIP, map12},
        {EXAMPLE_PIN_NUM_DATA13, strip13, NUM_LEDS_PER_STRIP, map13},
        {EXAMPLE_PIN_NUM_DATA14, strip14, NUM_LEDS_PER_STRIP, map14},
        {EXAMPLE_PIN_NUM_DATA15, strip15, NUM_LEDS_PER_STRIP, map15}
    });

    // Set global settings for all strips
    i2s_ref.setCorrection(UncorrectedColor)
           .setTemperature(UncorrectedTemperature)
           .setDither(BINARY_DITHER);

    i2s_bulk = &i2s_ref;

    // Example: Configure individual strips with different settings
    // Uncomment to see per-strip settings in action:
    /*
    auto* strip_0 = i2s_bulk->get(EXAMPLE_PIN_NUM_DATA0);
    if (strip_0) {
        strip_0->setCorrection(TypicalLEDStrip)
               .setTemperature(Tungsten100W)
               .setDither(BINARY_DITHER);
    }

    auto* strip_1 = i2s_bulk->get(EXAMPLE_PIN_NUM_DATA1);
    if (strip_1) {
        strip_1->setCorrection(TypicalSMD5050)
               .setTemperature(Candle)
               .setDither(DISABLE_DITHER)
               .setRgbw(Rgbw(6000, kRGBWExactColors, W3));  // RGBW mode
    }
    */

    FL_WARN("I2S bulk controller initialized with " << i2s_bulk->stripCount() << " strips");
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

void fill_rainbow_strips() {
    static int s_offset = 0;

    // Array of pointers to all strip buffers
    CRGB* strips[] = {
        strip0, strip1, strip2, strip3, strip4, strip5, strip6, strip7,
        strip8, strip9, strip10, strip11, strip12, strip13, strip14, strip15
    };

    // Fill each strip with rainbow pattern
    for (int j = 0; j < NUMSTRIPS; j++) {
        for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
            int idx = (i + s_offset) % NUM_LEDS_PER_STRIP;
            strips[j][i] = CHSV(idx, 255, 255);
        }
    }
    s_offset++;
}

void loop() {
    fill_rainbow_strips();
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
