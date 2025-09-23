// codec.ino - Comprehensive Multimedia Codec Demonstration
// Cycles through JPEG, WebP, GIF, and MPEG1 formats every 250ms
// All media files embedded as PROGMEM data

#include <FastLED.h>
#include "fl/sketch_macros.h"
#include <string.h>

// Only compile if we have enough memory
#if SKETCH_HAS_LOTS_OF_MEMORY == 0
#error "This sketch requires lots of memory and cannot run on low-memory devices"
#endif

#include "fl/codec/jpeg.h"
#include "fl/codec/webp.h"
#include "fl/codec/gif.h"
#include "fl/codec/mpeg1.h"
#include "fl/bytestreammemory.h"

#include "sample_jpeg_data.h"
#include "sample_webp_data.h"
#include "sample_gif_data.h"
#include "sample_mpeg1_data.h"

#define NUM_LEDS 64*64  // 64x64 LED matrix
#define DATA_PIN 6
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define TARGET_FPS 30

CRGB leds[NUM_LEDS];

// Forward declarations
void processJpeg();
void processWebp();
void processGif();
void processMpeg1();
void displayFrameOnLEDs(const fl::Frame& frame);
CRGB getPixelFromFrame(const fl::Frame& frame, int x, int y);
void showDecodedMessage(const char* format);


// Global state
int currentFormat = 0; // 0=JPEG, 1=WebP, 2=GIF, 3=MPEG1
const unsigned long FORMAT_INTERVAL = 250; // 250ms between format changes

void setup() {
    Serial.begin(115200);
    Serial.println("FastLED Multimedia Codec Demonstration");
    Serial.println("Cycling through JPEG, WebP, GIF, and MPEG1 every 250ms");

    // Initialize LED strip
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(50);

    // Clear LEDs
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();

    Serial.println("System initialized - starting codec demonstration...");
}

void loop() {
    EVERY_N_MILLISECONDS(FORMAT_INTERVAL) {
        switch(currentFormat) {
            case 0:
                processJpeg();
                break;
            case 1:
                processWebp();
                break;
            case 2:
                processGif();
                break;
            case 3:
                processMpeg1();
                break;
        }

        // Move to next format
        currentFormat = (currentFormat + 1) % 4;
    }

    FastLED.show();
    delay(10); // Small delay to prevent overwhelming the system
}

void processJpeg() {
    Serial.println("\n=== Processing JPEG ===");

    if (!fl::Jpeg::isSupported()) {
        Serial.println("JPEG decoding not supported on this platform");
        return;
    }

    // Copy data from PROGMEM to RAM
    fl::vector<fl::u8> jpegData(sampleJpegDataLength);
    memcpy(jpegData.data(), sampleJpegData, sampleJpegDataLength);

    // Configure JPEG decoder
    fl::JpegDecoderConfig config;
    config.format = fl::PixelFormat::RGB888;
    config.quality = fl::JpegDecoderConfig::Medium;
    config.maxWidth = 64;
    config.maxHeight = 64;

    // Create data span
    fl::span<const fl::u8> data(jpegData.data(), jpegData.size());

    // Decode the JPEG
    fl::string error_msg;
    fl::FramePtr framePtr = fl::Jpeg::decode(config, data, &error_msg);

    if (framePtr && framePtr->isValid()) {
        displayFrameOnLEDs(*framePtr);
        showDecodedMessage("JPEG decoded successfully!");
    } else {
        Serial.print("Failed to decode JPEG: ");
        Serial.println(error_msg.c_str());
    }
}

void processWebp() {
    Serial.println("\n=== Processing WebP ===");

    if (!fl::Webp::isSupported()) {
        Serial.println("WebP decoding not yet implemented");
        return;
    }

    // Copy data from PROGMEM to RAM
    fl::vector<fl::u8> webpData(sampleWebpDataLength);
    memcpy(webpData.data(), sampleWebpData, sampleWebpDataLength);

    // Configure WebP decoder
    fl::WebpDecoderConfig config;
    config.format = fl::PixelFormat::RGB888;

    // Create data span
    fl::span<const fl::u8> data(webpData.data(), webpData.size());

    // Decode the WebP
    fl::string error_msg;
    fl::FramePtr framePtr = fl::Webp::decode(config, data, &error_msg);

    if (framePtr && framePtr->isValid()) {
        displayFrameOnLEDs(*framePtr);
        showDecodedMessage("WebP decoded successfully!");
    } else {
        Serial.print("Failed to decode WebP: ");
        Serial.println(error_msg.c_str());
    }
}

void processGif() {
    Serial.println("\n=== Processing GIF ===");

    if (!fl::Gif::isSupported()) {
        Serial.println("GIF decoding not supported on this platform");
        return;
    }

    // Copy data from PROGMEM to RAM
    fl::vector<fl::u8> gifData(sampleGifDataLength);
    memcpy(gifData.data(), sampleGifData, sampleGifDataLength);

    // Configure GIF decoder
    fl::GifConfig config;
    config.mode = fl::GifConfig::SingleFrame;
    config.format = fl::PixelFormat::RGB888;
    config.maxWidth = 64;
    config.maxHeight = 64;

    // Create decoder
    fl::string error_msg;
    fl::shared_ptr<fl::IDecoder> decoder = fl::Gif::createDecoder(config, &error_msg);

    if (!decoder) {
        Serial.print("Failed to create GIF decoder: ");
        Serial.println(error_msg.c_str());
        return;
    }

    // Create byte stream from data
    auto stream = fl::make_shared<fl::ByteStreamMemory>(gifData.size());
    stream->write(gifData.data(), gifData.size());

    // Begin decoding
    if (!decoder->begin(stream)) {
        fl::string error;
        decoder->hasError(&error);
        Serial.print("Failed to begin GIF decoding: ");
        Serial.println(error.c_str());
        return;
    }

    // Decode first frame
    fl::DecodeResult result = decoder->decode();

    if (result == fl::DecodeResult::Success) {
        fl::Frame frame = decoder->getCurrentFrame();
        if (frame.isValid()) {
            displayFrameOnLEDs(frame);
            showDecodedMessage("GIF decoded successfully!");
        } else {
            Serial.println("Invalid GIF frame received");
        }
    } else {
        fl::string error;
        decoder->hasError(&error);
        Serial.print("GIF frame decode error: ");
        Serial.println(error.c_str());
    }

    decoder->end();
}

void processMpeg1() {
    Serial.println("\n=== Processing MPEG1 ===");

    if (!fl::Mpeg1::isSupported()) {
        Serial.println("MPEG1 decoding not supported on this platform");
        return;
    }

    // Copy data from PROGMEM to RAM
    fl::vector<fl::u8> mpegData(sampleMpeg1DataLength);
    memcpy(mpegData.data(), sampleMpeg1Data, sampleMpeg1DataLength);

    // Configure MPEG1 decoder
    fl::Mpeg1Config config;
    config.mode = fl::Mpeg1Config::SingleFrame;
    config.targetFps = TARGET_FPS;
    config.looping = false;
    config.skipAudio = true;

    // Create decoder
    fl::string error_msg;
    fl::shared_ptr<fl::IDecoder> decoder = fl::Mpeg1::createDecoder(config, &error_msg);

    if (!decoder) {
        Serial.print("Failed to create MPEG1 decoder: ");
        Serial.println(error_msg.c_str());
        return;
    }

    // Create byte stream from data
    auto stream = fl::make_shared<fl::ByteStreamMemory>(mpegData.size());
    stream->write(mpegData.data(), mpegData.size());

    // Begin decoding
    if (!decoder->begin(stream)) {
        fl::string error;
        decoder->hasError(&error);
        Serial.print("Failed to begin MPEG1 decoding: ");
        Serial.println(error.c_str());
        return;
    }

    // Decode first frame
    fl::DecodeResult result = decoder->decode();

    if (result == fl::DecodeResult::Success) {
        fl::Frame frame = decoder->getCurrentFrame();
        if (frame.isValid()) {
            displayFrameOnLEDs(frame);
            showDecodedMessage("MPEG1 decoded successfully!");
        } else {
            Serial.println("Invalid MPEG1 frame received");
        }
    } else {
        fl::string error;
        decoder->hasError(&error);
        Serial.print("MPEG1 frame decode error: ");
        Serial.println(error.c_str());
    }

    decoder->end();
}

void displayFrameOnLEDs(const fl::Frame& frame) {
    // Calculate LED matrix dimensions (assuming square matrix)
    int ledWidth = 64;
    int ledHeight = 64;

    for (int y = 0; y < ledHeight; y++) {
        for (int x = 0; x < ledWidth; x++) {
            // Calculate source pixel coordinates (with scaling)
            int srcX = (x * (int)frame.getWidth()) / ledWidth;
            int srcY = (y * (int)frame.getHeight()) / ledHeight;

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
    if (x >= (int)frame.getWidth() || y >= (int)frame.getHeight() || !frame.isValid()) {
        return CRGB::Black;
    }

    int pixelIndex = y * (int)frame.getWidth() + x;

    // Frame stores data as CRGB
    if (pixelIndex < (int)frame.size()) {
        return frame.rgb()[pixelIndex];
    }

    return CRGB::Black;
}

void showDecodedMessage(const char* format) {
    Serial.println(format);
}