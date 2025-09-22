// CodecJpeg.ino - JPEG Image Display on LED Matrix
// This example demonstrates how to decode and display JPEG images on an LED matrix

#include <FastLED.h>
#include "fl/codec/jpeg.h"
#include "fl/bytestreammemory.h"

#define NUM_LEDS 64*64  // 64x64 LED matrix
#define DATA_PIN 6
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

// Forward declarations
void displayFrameOnLEDs(const fl::Frame& frame);
CRGB getPixelFromFrame(const fl::Frame& frame, int x, int y);

// Sample JPEG data (minimal header - replace with actual JPEG data)
const uint8_t sampleJpegData[] = {
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46,
    0x49, 0x46, 0x00, 0x01, 0x01, 0x01, 0x00, 0x48,
    // ... add more JPEG data here
    0xFF, 0xD9  // End of JPEG marker
};

void setup() {
    Serial.begin(115200);
    Serial.println("FastLED JPEG Display Example");

    // Initialize LED strip
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(50);

    // Check if JPEG decoding is supported
    if (!fl::jpeg::isSupported()) {
        Serial.println("JPEG decoding not supported on this platform");
        return;
    }

    Serial.println("JPEG decoding supported - starting decode");

    // Configure JPEG decoder
    fl::JpegConfig config;
    config.format = fl::PixelFormat::RGB888;
    config.quality = fl::JpegConfig::Medium;
    config.maxWidth = 64;
    config.maxHeight = 64;

    // Create decoder
    fl::string error_msg;
    auto decoder = fl::jpeg::createDecoder(config, &error_msg);

    if (!decoder) {
        Serial.print("Failed to create JPEG decoder: ");
        Serial.println(error_msg.c_str());
        return;
    }

    // Create byte stream from sample data
    auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(sampleJpegData));
    stream->write(sampleJpegData, sizeof(sampleJpegData));

    // Begin decoding
    if (!decoder->begin(stream)) {
        fl::string error;
        decoder->hasError(&error);
        Serial.print("Failed to begin decoding: ");
        Serial.println(error.c_str());
        return;
    }

    // Decode the image
    fl::DecodeResult result = decoder->decode();

    if (result == fl::DecodeResult::Success) {
        Serial.println("JPEG decoded successfully!");

        // Get the decoded frame
        fl::Frame frame = decoder->getCurrentFrame();

        if (frame.isValid()) {
            Serial.print("Frame size: ");
            Serial.print(frame.getWidth());
            Serial.print("x");
            Serial.print(frame.getHeight());
            Serial.print(", format: ");
            Serial.println((int)frame.getFormat());

            // Copy frame data to LEDs
            displayFrameOnLEDs(frame);
        } else {
            Serial.println("Invalid frame received");
        }
    } else {
        Serial.println("JPEG decoding failed");
    }

    decoder->end();
}

void loop() {
    // Show the LEDs
    FastLED.show();
    delay(100);

    // You could add animation or update logic here
    // For example, cycling through multiple images
}

void displayFrameOnLEDs(const fl::Frame& frame) {
    // Calculate scaling if frame size doesn't match LED matrix
    int ledWidth = 64;   // Assuming 64x64 matrix
    int ledHeight = 64;

    for (int y = 0; y < ledHeight; y++) {
        for (int x = 0; x < ledWidth; x++) {
            // Calculate source pixel coordinates (with scaling)
            int srcX = (x * frame.getWidth()) / ledWidth;
            int srcY = (y * frame.getHeight()) / ledHeight;

            // Get pixel from frame
            CRGB color = getPixelFromFrame(frame, srcX, srcY);

            // Set LED color
            int ledIndex = y * ledWidth + x;
            if (ledIndex < NUM_LEDS) {
                leds[ledIndex] = color;
            }
        }
    }
}

CRGB getPixelFromFrame(const fl::Frame& frame, int x, int y) {
    if (x >= frame.getWidth() || y >= frame.getHeight() || !frame.isValid()) {
        return CRGB::Black;
    }

    int pixelIndex = y * frame.getWidth() + x;

    // Frame now stores data as CRGB, so we can directly access it
    if (pixelIndex < frame.size()) {
        return frame.rgb()[pixelIndex];
    }

    return CRGB::Black;
}