/// @file    VideoTest.ino
/// @brief   Demonstrates a simple video test using alternating black/red pixels
/// @example VideoTest.ino

#include <FastLED.h>
#include "fx/storage/bytestreammemory.h"
#include "fx/2d/video.hpp"
#include "fx/fx_engine.h"
#include "ptr.h"
#include <iostream>

#define LED_PIN 2
#define BRIGHTNESS 96
#define LED_TYPE WS2811
#define COLOR_ORDER GRB

#define MATRIX_WIDTH 22
#define MATRIX_HEIGHT 22
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

CRGB leds[NUM_LEDS];

const int BYTES_PER_FRAME = 3 * MATRIX_WIDTH * MATRIX_HEIGHT;
const int NUM_FRAMES = 1;
const uint32_t BUFFER_SIZE = BYTES_PER_FRAME * NUM_FRAMES;

ByteStreamMemoryPtr memoryStream;
VideoPtr videoFx;
FxEngine fxEngine(NUM_LEDS);


void write_one_frame(ByteStreamMemoryPtr memoryStream) {
    for (uint32_t i = 0; i < BUFFER_SIZE / 3; ++i) {
        CRGB color = (i % 6 < 3) ? CRGB::Black : CRGB::Red;
        size_t bytes_written = memoryStream->write(color.raw, 3);
        if (bytes_written != 3) {
            // Serial.println("Error writing to memory stream");
            std::cout << "Error writing to memory stream" << std::endl;
        }
    }
}

void setup() {
    delay(1000); // sanity delay
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);

    // Create and fill the ByteStreamMemory with test data
    memoryStream = Ptr::New<ByteStreamMemory>(BUFFER_SIZE);
    // Create and initialize Video fx object
    XYMap xymap(MATRIX_WIDTH, MATRIX_HEIGHT);
    videoFx = Fx::make<VideoPtr>(xymap);
    //videoFx = RefPtr<Video>::FromHeap(new Video(xymap));
    videoFx->beginStream(memoryStream);

    // Add the video effect to the FxEngine
    fxEngine.addFx(videoFx);
}

void loop() {
    write_one_frame(memoryStream);
    fxEngine.draw(millis(), leds);
    FastLED.show();
    delay(100); // Adjust this delay to control frame rate
}
