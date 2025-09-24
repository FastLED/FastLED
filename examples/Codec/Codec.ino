// codec.ino - Comprehensive Multimedia Codec Demonstration
// Click buttons to decode JPEG, WebP, GIF, and MPEG1 formats
// All media files embedded as PROGMEM data

#include <FastLED.h>
#include "fl/sketch_macros.h"
#include "fl/ui.h"
#include <string.h>

// Only compile if we have enough memory
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
#error "This sketch requires lots of memory and cannot run on low-memory devices"
#endif

#include "fl/xymap.h"
#include "inlined_data.h"
#include "codec_processor.h"

#define WIDTH 2
#define HEIGHT 2

#define NUM_LEDS WIDTH*HEIGHT  // 2x2 LED matrix
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
    // Simple 2x2 grid for WASM display using XYMap
    // Just display the first 4 LEDs in a 2x2 grid
    fl::XYMap xymap = fl::XYMap::constructRectangularGrid(2, 2);  // 2x2 grid

    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, 4)
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
    CodecProcessor::ledWidth = 64;  // Keep original 64x64 for scaling
    CodecProcessor::ledHeight = 64;

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
