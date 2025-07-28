/// @file    FxGfx2Video.ino
/// @brief   Demonstrates graphics to video conversion
/// @example FxGfx2Video.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

/// @file    Gfx2Video.ino
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
#include "fl/bytestreammemory.h"
#include "fx/fx_engine.h"
#include "fl/memory.h"
#include "fx/video.h"
#include "fl/dbg.h"

using namespace fl;

#define LED_PIN 2
#define BRIGHTNESS 96
#define LED_TYPE WS2811
#define COLOR_ORDER GRB

#define MATRIX_WIDTH 22
#define MATRIX_HEIGHT 22
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

CRGB leds[NUM_LEDS];

const int BYTES_PER_FRAME = 3 * NUM_LEDS;
const int NUM_FRAMES = 2;
const uint32_t BUFFER_SIZE = BYTES_PER_FRAME * NUM_FRAMES;

ByteStreamMemoryPtr memoryStream;
FxEngine fxEngine(NUM_LEDS);
// Create and initialize Video object
XYMap xymap(MATRIX_WIDTH, MATRIX_HEIGHT);
Video video(NUM_LEDS, 2.0f);

void write_one_frame(ByteStreamMemoryPtr memoryStream) {
    //memoryStream->seek(0);  // Reset to the beginning of the stream
    uint32_t total_bytes_written = 0;
    bool toggle = (millis() / 500) % 2 == 0;
    FASTLED_DBG("Writing frame data, toggle = " << toggle);
    for (uint32_t i = 0; i < NUM_LEDS; ++i) {
        CRGB color = (toggle ^ i%2) ? CRGB::Black : CRGB::Red;
        size_t bytes_written = memoryStream->writeCRGB(&color, 1);
        if (bytes_written != 1) {
            FASTLED_DBG("Failed to write frame data, wrote " << bytes_written << " bytes");
            break;
        }
        total_bytes_written += bytes_written;
    }
    if (total_bytes_written) {
        FASTLED_DBG("Frame written, total bytes: " << total_bytes_written);
    }
}

void setup() {
    delay(1000); // sanity delay
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(xymap);
    FastLED.setBrightness(BRIGHTNESS);

    // Create and fill the ByteStreamMemory with test data
    memoryStream = fl::make_shared<ByteStreamMemory>(BUFFER_SIZE*sizeof(CRGB));

    video.beginStream(memoryStream);
    // Add the video effect to the FxEngine
    fxEngine.addFx(video);
}

void loop() {
    EVERY_N_MILLISECONDS(500) {
        write_one_frame(memoryStream);  // Write next frame data
    }
    // write_one_frame(memoryStream);  // Write next frame data
    // Draw the frame
    fxEngine.draw(millis(), leds);
    // Show the LEDs
    FastLED.show();
    delay(20); // Adjust this delay to control frame rate
}
#else
void setup() {}
void loop() {}
#endif // COMPILE_VIDEO_STREAM
