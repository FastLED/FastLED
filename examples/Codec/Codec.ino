// @filter: (memory is high)

// codec.ino - Comprehensive Multimedia Codec Demonstration
// Click buttons to decode JPEG, WebP, GIF, and MPEG1 formats
// All media files embedded as PROGMEM data

#include <FastLED.h>
#include "fl/sketch_macros.h"
#include "fl/ui.h"
#include "fl/str.h"

#include "fl/xymap.h"
#include "inlined_data.h"
#include "codec_processor.h"

#define WIDTH 32
#define HEIGHT 32

#define NUM_LEDS WIDTH*HEIGHT  // 32x32 LED matrix
#define DATA_PIN 3
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];
fl::string formatNames[] = {"JPEG", "GIF", "MPEG1"};

// UI Button definitions
fl::UIButton jpegButton("JPEG");
fl::UIButton gifButton("GIF");
fl::UIButton mpeg1Button("MPEG1");


// Global state (no longer used for automatic cycling)

void setup() {
    Serial.begin(115200);
    Serial.println("FastLED Multimedia Codec Demonstration");
    Serial.println("Click buttons to decode JPEG, GIF, and MPEG1 formats");

    // xymap is for the wasm compiler and is otherwise a no-op.
    // 32x32 grid for WASM display using XYMap
    fl::XYMap xymap = fl::XYMap::constructRectangularGrid(WIDTH, HEIGHT);  // 32x32 grid

    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(xymap.toScreenMap());


    FastLED.setBrightness(50);

    // Clear LEDs
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();

    // UI buttons are initialized via constructors

    // Initialize codec processor with LED array
    CodecProcessor::leds = leds;
    CodecProcessor::numLeds = NUM_LEDS;
    CodecProcessor::ledWidth = WIDTH;  // 32x32 for 1:1 mapping
    CodecProcessor::ledHeight = HEIGHT;

    Serial.println("System initialized - codec demonstration ready. Click buttons to decode formats...");
}



void loop() {
    // Check for button clicks
    if (jpegButton.clicked()) {
        CodecProcessor::processCodecWithTiming("JPEG", []() { CodecProcessor::processJpeg(); });
    }

    if (gifButton.clicked()) {
        CodecProcessor::processCodecWithTiming("GIF", []() { CodecProcessor::processGif(); });
    }

    if (mpeg1Button.clicked()) {
        CodecProcessor::processCodecWithTiming("MPEG1", []() { CodecProcessor::processMpeg1(); });
    }
}
