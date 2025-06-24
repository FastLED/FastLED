/// @file    VideoTest.ino
/// @brief   Demonstrates a simple video test using alternating black/red pixels
/// @example VideoTest.ino

#include <FastLED.h>
#include "fl/bytestreammemory.h"
#include "fx/2d/video.hpp"
#include "fx/fx_engine.h"
#include "fl/ptr.h"
#include <iostream>

using namespace fl;

#define LED_PIN 2
#define BRIGHTNESS 96
#define LED_TYPE WS2811
#define COLOR_ORDER GRB

#define MATRIX_WIDTH 22
#define MATRIX_HEIGHT 22
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

CRGB leds[NUM_LEDS];

const int BYTES_PER_FRAME = 3 * MATRIX_WIDTH * MATRIX_HEIGHT;
const int NUM_FRAMES = 2;
const uint32_t BUFFER_SIZE = BYTES_PER_FRAME * NUM_FRAMES;

ByteStreamMemoryPtr memoryStream;
VideoPtr videoFx;
FxEngine fxEngine(NUM_LEDS);

using namespace fl;


void write_one_frame(ByteStreamMemoryPtr memoryStream) {
    //memoryStream->seek(0);  // Reset to the beginning of the stream
    uint32_t total_bytes_written = 0;
    int toggle = (millis() / 500) % 2;
    for (uint32_t i = 0; i < NUM_LEDS; ++i) {
        CRGB color = (i % 2 == toggle) ? CRGB::Black : CRGB::Red;
        size_t bytes_written = memoryStream->write(color.raw, 3);
        if (bytes_written != 3) {
            std::cout << "Error writing to memory stream at LED " << i << std::endl;
        }
        total_bytes_written += bytes_written;
    }
    std::cout << "Total bytes written: " << total_bytes_written << " / " << BUFFER_SIZE << std::endl;
}

void setup() {
    delay(1000); // sanity delay
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);

    // Create and fill the ByteStreamMemory with test data
    memoryStream = Ptr::New<ByteStreamMemory>(BUFFER_SIZE);
    write_one_frame(memoryStream);  // Write initial frame data

    // Create and initialize Video fx object
    XYMap xymap(MATRIX_WIDTH, MATRIX_HEIGHT);
    videoFx = Fx::make<VideoPtr>(xymap);
    videoFx->beginStream(memoryStream);

    // Add the video effect to the FxEngine
    fxEngine.addFx(videoFx);

    std::cout << "Setup complete. Starting main loop." << std::endl;
}

void loop() {
    // Reset the memory stream position before reading
    //memoryStream->seek(0);
    write_one_frame(memoryStream);  // Write next frame data

    // Draw the frame
    fxEngine.draw(millis(), leds);

    // Debug output
    //std::cout << "First LED: R=" << (int)leds[0].r << " G=" << (int)leds[0].g << " B=" << (int)leds[0].b << std::endl;
    //std::cout << "Last LED: R=" << (int)leds[NUM_LEDS-1].r << " G=" << (int)leds[NUM_LEDS-1].g << " B=" << (int)leds[NUM_LEDS-1].b << std::endl;

    // Show the LEDs
    FastLED.show();

    delay(100); // Adjust this delay to control frame rate
}
