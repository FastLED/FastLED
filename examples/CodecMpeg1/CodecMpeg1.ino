// CodecMpeg1.ino - MPEG1 Video Playback on LED Matrix
// This example demonstrates how to decode and play MPEG1 video on an LED matrix

#include <FastLED.h>
#include "fl/codec/mpeg1.h"
#include "fl/bytestreammemory.h"

#define NUM_LEDS 32*32  // 32x32 LED matrix for video
#define DATA_PIN 6
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define TARGET_FPS 15   // Target video playback FPS

CRGB leds[NUM_LEDS];

// Forward declarations
bool playNextFrame();
void displayVideoFrame(const fl::Frame& frame);
CRGB getPixelFromVideoFrame(const fl::Frame& frame, int x, int y);
void showErrorPattern();
void printVideoStats();

// Sample MPEG1 data (minimal header - replace with actual MPEG1 data)
const uint8_t sampleMpeg1Data[] = {
    0x00, 0x00, 0x01, 0xB3, // Sequence header start code
    0x00, 0x14, 0x00, 0x0F, // Width=20 (320), Height=15 (240) encoded
    0x12, 0x34, 0x56, 0x78, // Additional header data
    // ... add more MPEG1 data here
    0x00, 0x00, 0x01, 0xB7  // Sequence end code
};

fl::shared_ptr<fl::IDecoder> videoDecoder;
unsigned long lastFrameTime = 0;
unsigned long frameInterval;
bool videoLoaded = false;

void setup() {
    Serial.begin(115200);
    Serial.println("FastLED MPEG1 Video Example");

    // Initialize LED strip
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(80);

    // Calculate frame interval for target FPS
    frameInterval = 1000 / TARGET_FPS;

    // Check if MPEG1 decoding is supported
    if (!fl::mpeg1::isSupported()) {
        Serial.println("MPEG1 decoding not supported on this platform");
        showErrorPattern();
        return;
    }

    Serial.println("MPEG1 decoding supported - loading video");

    // Configure MPEG1 decoder
    fl::Mpeg1Config config;
    config.mode = fl::Mpeg1Config::Streaming;
    config.targetFps = TARGET_FPS;
    config.looping = true;
    config.skipAudio = true;
    config.bufferFrames = 2;

    // Create decoder
    fl::string error_msg;
    videoDecoder = fl::mpeg1::createDecoder(config, &error_msg);

    if (!videoDecoder) {
        Serial.println("Failed to create MPEG1 decoder: " + error_msg);
        showErrorPattern();
        return;
    }

    // Create byte stream from sample data
    auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(sampleMpeg1Data));
    stream->write(sampleMpeg1Data, sizeof(sampleMpeg1Data));

    // Begin decoding
    if (!videoDecoder->begin(stream)) {
        fl::string error;
        videoDecoder->hasError(&error);
        Serial.println("Failed to begin video decoding: " + error);
        showErrorPattern();
        return;
    }

    Serial.println("Video loaded successfully - starting playback");
    videoLoaded = true;
    lastFrameTime = millis();
}

void loop() {
    if (!videoLoaded) {
        // Show error pattern if video failed to load
        showErrorPattern();
        FastLED.show();
        delay(100);
        return;
    }

    unsigned long currentTime = millis();

    // Check if it's time for the next frame
    if (currentTime - lastFrameTime >= frameInterval) {
        if (playNextFrame()) {
            lastFrameTime = currentTime;
        }
    }

    FastLED.show();
    delay(10); // Small delay to prevent excessive CPU usage
}

bool playNextFrame() {
    if (!videoDecoder || !videoDecoder->hasMoreFrames()) {
        // If looping is enabled, this will restart automatically
        // For this example, we'll just show a completion message
        Serial.println("Video playback completed");
        return false;
    }

    // Decode the next frame
    fl::DecodeResult result = videoDecoder->decode();

    if (result == fl::DecodeResult::Success) {
        // Get the current frame
        fl::Frame frame = videoDecoder->getCurrentFrame();

        if (frame.isValid()) {
            // Display the frame on the LED matrix
            displayVideoFrame(frame);
            return true;
        } else {
            Serial.println("Invalid frame received");
        }
    } else if (result == fl::DecodeResult::EndOfStream) {
        Serial.println("End of video stream reached");
        return false;
    } else {
        fl::string error;
        videoDecoder->hasError(&error);
        Serial.println("Frame decode error: " + error);
        return false;
    }

    return false;
}

void displayVideoFrame(const fl::Frame& frame) {
    // Calculate LED matrix dimensions
    int ledWidth = 32;   // Assuming 32x32 matrix
    int ledHeight = 32;

    for (int y = 0; y < ledHeight; y++) {
        for (int x = 0; x < ledWidth; x++) {
            // Calculate source pixel coordinates (with scaling)
            int srcX = (x * frame.getWidth()) / ledWidth;
            int srcY = (y * frame.getHeight()) / ledHeight;

            // Get pixel from frame using Frame's rgb() method
            CRGB color = getPixelFromVideoFrame(frame, srcX, srcY);

            // Set LED color
            int ledIndex = y * ledWidth + x;
            if (ledIndex < NUM_LEDS) {
                leds[ledIndex] = color;
            }
        }
    }
}

CRGB getPixelFromVideoFrame(const fl::Frame& frame, int x, int y) {
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

void showErrorPattern() {
    // Show a red/blue alternating pattern to indicate error
    static bool alternate = false;
    alternate = !alternate;

    for (int i = 0; i < NUM_LEDS; i++) {
        if ((i + (alternate ? 0 : 1)) % 2) {
            leds[i] = CRGB::Red;
        } else {
            leds[i] = CRGB::Blue;
        }
    }
}

// Optional: Function to show video statistics
void printVideoStats() {
    if (videoDecoder) {
        Serial.print("Frame: ");
        Serial.print(videoDecoder->getCurrentFrameIndex());
        Serial.print(", Total: ");
        Serial.println(videoDecoder->getFrameCount());
    }
}