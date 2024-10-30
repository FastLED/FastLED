/// @file    Frame2Video.ino
/// @brief   Demonstrates drawing to a frame buffer, which is used as source video for the memory player.
///          The render pattern is alternating black/red pixels as a checkerboard.
/// @example VideoTest.ino


#ifndef COMPILE_VIDEO_STREAM
#if defined(__AVR__)
// This has grown too large for the AVR to handle.
#define COMPILE_VIDEO_STREAM 0
#else
#define COMPILE_VIDEO_STREAM 1
#endif
#endif // COMPILE_VIDEO_STREAM

#if COMPILE_VIDEO_STREAM


#include <FastLED.h>
#include "fx/storage/bytestreammemory.h"
#include "fx/2d/video.hpp"
#include "fx/fx_engine.h"
#include "ref.h"

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

ByteStreamMemoryRef memoryStream;
VideoRef videoFx;
FxEngine fxEngine(NUM_LEDS);


void write_one_frame(ByteStreamMemoryRef memoryStream) {
    //memoryStream->seek(0);  // Reset to the beginning of the stream
    uint32_t total_bytes_written = 0;
    int toggle = (millis() / 500) % 2;
    for (uint32_t i = 0; i < NUM_LEDS; ++i) {
        CRGB color = (i % 2 == toggle) ? CRGB::Black : CRGB::Red;
        size_t bytes_written = memoryStream->write(color.raw, 3);
        if (bytes_written != 3) {
        }
        total_bytes_written += bytes_written;
    }
}

void setup() {
    delay(1000); // sanity delay
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);

    // Create and fill the ByteStreamMemory with test data
    memoryStream = ByteStreamMemoryRef::New(BUFFER_SIZE);
    write_one_frame(memoryStream);  // Write initial frame data

    // Create and initialize Video fx object
    XYMap xymap(MATRIX_WIDTH, MATRIX_HEIGHT);
    videoFx = VideoRef::New(xymap);
    videoFx->beginStream(memoryStream);

    // Add the video effect to the FxEngine
    fxEngine.addFx(videoFx);
}

void loop() {
    // Reset the memory stream position before reading
    //memoryStream->seek(0);
    write_one_frame(memoryStream);  // Write next frame data

    // Draw the frame
    fxEngine.draw(millis(), leds);

    // Show the LEDs
    FastLED.show();

    delay(100); // Adjust this delay to control frame rate
}
#else
void setup() {}
void loop() {}
#endif // COMPILE_VIDEO_STREAM
