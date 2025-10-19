#include <Arduino.h>
#include <FastLED.h>

#include "codec_processor.h"
#include "inlined_data.h"

namespace CodecProcessor {

// Configuration constants
const int TARGET_FPS = 30;

// LED array pointers - set from main sketch
CRGB* leds = nullptr;
int numLeds = 0;
int ledWidth = 64;
int ledHeight = 64;

void processCodecWithTiming(const char* codecName, fl::function<void()> codecFunc) {
    FL_WARN("Starting format " << codecName);

    fl::u32 start_time = millis();

    // Execute the codec processing function
    codecFunc();

    fl::u32 diff_time = millis() - start_time;

    FL_WARN("Format took " << diff_time << "ms to process " << codecName);

    // Now build up the message to display the current leds
    fl::sstream message;
    message << "Format: " << codecName << "\n";
    message << "LEDs: " << numLeds << " (" << ledWidth << "x" << ledHeight << ")\n";

    // For 32x32, just show a summary rather than all pixels
    if (numLeds > 16) {
        // Show first few pixels as a sample
        message << "First 4 pixels: ";
        for (int i = 0; i < 4 && i < numLeds; i++) {
            message << "RGB(" << (int)leds[i].r << "," << (int)leds[i].g << "," << (int)leds[i].b << ") ";
        }
        message << "\n";
    } else {
        // For small displays, show all LEDs
        for (int i = 0; i < numLeds; i++) {
            message << "LED " << i << ": " << leds[i] << "\n";
        }
    }
    FL_WARN(message.str().c_str());
    FastLED.show();
}

void processJpeg() {
    Serial.println("\n=== Processing JPEG ===");

    if (!fl::Jpeg::isSupported()) {
        Serial.println("JPEG decoding not supported on this platform");
        return;
    }

    // Copy data from PROGMEM to RAM
    fl::vector<fl::u8> jpegData(CodecData::sampleJpegDataLength);
    fl::memcopy(jpegData.data(), CodecData::sampleJpegData, CodecData::sampleJpegDataLength);

    // Configure JPEG decoder
    fl::JpegConfig config;
    config.format = fl::PixelFormat::RGB888;
    config.quality = fl::JpegConfig::Medium;

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


void processGif() {
    Serial.println("\n=== Processing GIF ===");

    if (!fl::Gif::isSupported()) {
        Serial.println("GIF decoding not supported on this platform");
        return;
    }

    // Copy data from PROGMEM to RAM
    fl::vector<fl::u8> gifData(CodecData::sampleGifDataLength);
    fl::memcopy(gifData.data(), CodecData::sampleGifData, CodecData::sampleGifDataLength);

    // Configure GIF decoder
    fl::GifConfig config;
    config.mode = fl::GifConfig::SingleFrame;
    config.format = fl::PixelFormat::RGB888;

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
    fl::vector<fl::u8> mpegData(CodecData::sampleMpeg1DataLength);
    fl::memcopy(mpegData.data(), CodecData::sampleMpeg1Data, CodecData::sampleMpeg1DataLength);

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
    for (int y = 0; y < ledHeight; y++) {
        for (int x = 0; x < ledWidth; x++) {
            // Calculate source pixel coordinates (with scaling)
            int srcX = (x * (int)frame.getWidth()) / ledWidth;
            int srcY = (y * (int)frame.getHeight()) / ledHeight;

            // Get pixel from frame
            CRGB color = getPixelFromFrame(frame, srcX, srcY);

            // Set LED color
            int ledIndex = y * ledWidth + x;
            if (ledIndex < numLeds) {
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

} // namespace CodecProcessor
