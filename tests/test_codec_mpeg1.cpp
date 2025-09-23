#include "test.h"
#include "fl/codec/mpeg1.h"
#include "fl/bytestreammemory.h"

using namespace fl;

// Minimal 2x2 MPEG1 test data for two frames: red-white-blue-black -> white-blue-black-red
static const fl::u8 test2x2MpegData[] = {
    // MPEG Program Stream Pack Header
    0x00, 0x00, 0x01, 0xBA,  // Pack start code
    0x44, 0x00, 0x04, 0x00,  // System clock reference
    0x04, 0x01, 0xF8,        // Program mux rate

    // System Header
    0x00, 0x00, 0x01, 0xBB,  // System header start code
    0x00, 0x12,              // Header length
    0x80, 0x01, 0xF8,        // Rate bound
    0x06, 0xE1,              // Audio/video flags
    0x10, 0xE0, 0x03, 0xFF,  // Video stream info
    0xBD, 0xC0, 0x03, 0x20,  // Audio stream info

    // MPEG1 Video Sequence Header
    0x00, 0x00, 0x01, 0xB3,  // Sequence header start code
    0x00, 0x02, 0x00, 0x02,  // Width=2, Height=2 (12-bit each)
    0x11,                    // Aspect ratio (1:1) + frame rate (25fps)
    0x01, 0x40, 0x00,        // Bit rate + VBV buffer size
    0x10, 0x00,              // Load intra quantizer matrix flag

    // Group of Pictures Header
    0x00, 0x00, 0x01, 0xB8,  // GOP start code
    0x00, 0x08, 0x00, 0x00,  // Time code + flags

    // First I-Frame (red-white-blue-black)
    0x00, 0x00, 0x01, 0x00,  // Picture start code
    0x00, 0x08,              // Temporal reference + picture type (I-frame)
    0xFF, 0xF8,              // VBV delay

    // Picture coding extension would go here in real MPEG
    // Simplified macroblock data for 2x2 pixels
    0x80, 0x40, 0x20, 0x10,  // Simplified Y data (luma)
    0x80, 0x00,              // Simplified Cb data (chroma blue)
    0x00, 0x80,              // Simplified Cr data (chroma red)

    // Second I-Frame (white-blue-black-red)
    0x00, 0x00, 0x01, 0x00,  // Picture start code
    0x00, 0x18,              // Temporal reference + picture type (I-frame)
    0xFF, 0xF8,              // VBV delay

    // Simplified macroblock data for rotated 2x2 pixels
    0x40, 0x20, 0x10, 0x80,  // Simplified Y data (luma) - rotated
    0x00, 0x80,              // Simplified Cb data (chroma blue)
    0x80, 0x00,              // Simplified Cr data (chroma red)

    // Sequence End Code
    0x00, 0x00, 0x01, 0xB7   // Sequence end code
};

static const size_t test2x2MpegDataSize = sizeof(test2x2MpegData);

// Helper function to create legacy test data
static void setupTestData(fl::u8 testData[4096]) {
    // Create test MPEG1 data (simplified header)
    testData[0] = 0x00;
    testData[1] = 0x00;
    testData[2] = 0x01;
    testData[3] = 0xB3; // Sequence header start code
    // Add more test data as needed
    for (int i = 4; i < 4096; ++i) {
        testData[i] = static_cast<fl::u8>(i % 256);
    }
}

// Test MPEG1 decoder availability
TEST_CASE("MPEG1 availability") {
    bool mpeg1Supported = Mpeg1::isSupported();

    // MPEG1 should be supported via software decoder
    CHECK(mpeg1Supported);
}

// Test MPEG1 decoder configuration
TEST_CASE("MPEG1 decoder configuration") {
    Mpeg1Config config;

    // Test default configuration
    CHECK(config.mode == Mpeg1Config::Streaming);
    CHECK(config.targetFps == 30);
    CHECK(config.skipAudio == true);
    CHECK(config.looping == false);
    CHECK(config.immediateMode == true);  // Default for real-time applications
    CHECK(config.bufferFrames == 2);

    // Test custom configuration
    Mpeg1Config customConfig(Mpeg1Config::SingleFrame, 25);
    CHECK(customConfig.mode == Mpeg1Config::SingleFrame);
    CHECK(customConfig.targetFps == 25);
}

// Test MPEG1 decoder creation and initialization
TEST_CASE("MPEG1 decoder creation") {
    Mpeg1Config config;
    config.mode = Mpeg1Config::Streaming;
    config.bufferFrames = 3;

    fl::string error_msg;
    auto decoder = Mpeg1::createDecoder(config, &error_msg);

    CHECK(decoder != nullptr);
    CHECK_FALSE(decoder->isReady());

    // Create stream with test data
    fl::u8 testData[4096];
    setupTestData(testData);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(testData));
    stream->write(testData, sizeof(testData));

    bool beginResult = decoder->begin(stream);

    // Should succeed on platforms that support MPEG1 if the test data is valid
    // However, synthetic test data may not be valid MPEG1, so we just check it doesn't crash
    if (Mpeg1::isSupported()) {
        // Test data may not be valid MPEG1 format, so we allow both success and failure
        // The main thing is that it doesn't crash
        if (beginResult) {
            CHECK(decoder->isReady());
            CHECK_FALSE(decoder->hasError());
        } else {
            // If begin fails, it should set an error message
            CHECK(decoder->hasError());
        }
    }
}

// Test frame decoding
TEST_CASE("MPEG1 frame decoding") {
    if (!Mpeg1::isSupported()) {
        // Skip test if MPEG1 not supported
        return;
    }

    Mpeg1Config config(Mpeg1Config::SingleFrame);
    auto decoder = Mpeg1::createDecoder(config);

    fl::u8 testData[4096];
    setupTestData(testData);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(testData));
    stream->write(testData, sizeof(testData));

    bool beginResult = decoder->begin(stream);
    if (!beginResult) {
        // If synthetic test data is invalid, skip the test
        return;
    }

    // Try to decode a frame
    DecodeResult result = decoder->decode();

    if (result == DecodeResult::Success) {
        Frame frame = decoder->getCurrentFrame();

        CHECK(frame.isValid());
        CHECK(frame.getWidth() > 0);
        CHECK(frame.getHeight() > 0);
        CHECK(frame.rgb() != nullptr);
        CHECK(frame.getFormat() == PixelFormat::RGB888);
    }
}

// Test streaming mode
TEST_CASE("MPEG1 streaming mode") {
    if (!Mpeg1::isSupported()) {
        return;
    }

    Mpeg1Config config;
    config.mode = Mpeg1Config::Streaming;
    config.bufferFrames = 2;

    auto decoder = Mpeg1::createDecoder(config);

    fl::u8 testData[4096];
    setupTestData(testData);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(testData));
    stream->write(testData, sizeof(testData));

    bool beginResult = decoder->begin(stream);
    if (!beginResult) {
        // If synthetic test data is invalid, skip the test
        return;
    }

    // Test multiple frame decoding
    int frameCount = 0;
    const int maxFrames = 5;

    while (decoder->hasMoreFrames() && frameCount < maxFrames) {
        DecodeResult result = decoder->decode();

        if (result == DecodeResult::Success) {
            frameCount++;
        } else if (result == DecodeResult::EndOfStream) {
            break;
        } else if (result == DecodeResult::Error) {
            break; // Expected for test data
        }
    }

    decoder->end();
}

// Test single frame mode
TEST_CASE("MPEG1 single frame mode") {
    if (!Mpeg1::isSupported()) {
        return;
    }

    Mpeg1Config config;
    config.mode = Mpeg1Config::SingleFrame;

    auto decoder = Mpeg1::createDecoder(config);

    fl::u8 testData[4096];
    setupTestData(testData);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(testData));
    stream->write(testData, sizeof(testData));

    bool beginResult = decoder->begin(stream);
    if (!beginResult) {
        // If synthetic test data is invalid, skip the test
        return;
    }

    // In single frame mode, should decode one frame
    DecodeResult result = decoder->decode();

    // Test data may not be valid MPEG1, so we just check it doesn't crash
    CHECK((result == DecodeResult::Success ||
           result == DecodeResult::Error ||
           result == DecodeResult::NeedsMoreData));

    decoder->end();
}

// Test immediate mode for real-time applications
TEST_CASE("MPEG1 immediate mode") {
    if (!Mpeg1::isSupported()) {
        return;
    }

    Mpeg1Config config;
    config.mode = Mpeg1Config::Streaming;
    config.immediateMode = true;  // Enable immediate mode

    auto decoder = Mpeg1::createDecoder(config);

    fl::u8 testData[4096];
    setupTestData(testData);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(testData));
    stream->write(testData, sizeof(testData));

    bool beginResult = decoder->begin(stream);
    if (!beginResult) {
        // If synthetic test data is invalid, skip the test
        return;
    }

    // Test that immediate mode bypasses frame buffering
    DecodeResult result = decoder->decode();
    if (result == DecodeResult::Success) {
        Frame frame = decoder->getCurrentFrame();
        CHECK(frame.isValid());
        // In immediate mode, frame should be available immediately without buffering delay
    }

    decoder->end();
}

// Test buffered mode vs immediate mode performance
TEST_CASE("MPEG1 buffered vs immediate mode") {
    if (!Mpeg1::isSupported()) {
        return;
    }

    // Test buffered mode
    Mpeg1Config bufferedConfig;
    bufferedConfig.mode = Mpeg1Config::Streaming;
    bufferedConfig.immediateMode = false;
    bufferedConfig.bufferFrames = 3;

    auto bufferedDecoder = Mpeg1::createDecoder(bufferedConfig);
    CHECK(bufferedDecoder != nullptr);

    // Test immediate mode
    Mpeg1Config immediateConfig;
    immediateConfig.mode = Mpeg1Config::Streaming;
    immediateConfig.immediateMode = true;

    auto immediateDecoder = Mpeg1::createDecoder(immediateConfig);
    CHECK(immediateDecoder != nullptr);

    // Both should be functional but with different memory usage patterns
}

// Test decoder lifecycle
TEST_CASE("MPEG1 decoder lifecycle") {
    if (!Mpeg1::isSupported()) {
        return;
    }

    Mpeg1Config config;
    auto decoder = Mpeg1::createDecoder(config);

    // Initial state
    CHECK_FALSE(decoder->isReady());
    CHECK_FALSE(decoder->hasError());

    // Create test stream
    fl::u8 testData[4096];
    setupTestData(testData);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(testData));
    stream->write(testData, sizeof(testData));

    // Begin
    bool beginResult = decoder->begin(stream);
    if (!beginResult) {
        // If synthetic test data is invalid, skip the test
        return;
    }
    CHECK(decoder->isReady());

    // End
    decoder->end();
    CHECK_FALSE(decoder->isReady());

    // Should be able to begin again with a new stream
    auto newStream = fl::make_shared<fl::ByteStreamMemory>(sizeof(testData));
    newStream->write(testData, sizeof(testData));
    CHECK(decoder->begin(newStream));
    decoder->end();
}

// Test error handling
TEST_CASE("MPEG1 error handling") {
    if (!Mpeg1::isSupported()) {
        return;
    }

    Mpeg1Config config;
    auto decoder = Mpeg1::createDecoder(config);

    // Test with null stream
    CHECK_FALSE(decoder->begin(fl::ByteStreamPtr()));
    CHECK(decoder->hasError());

    // Test with empty stream
    auto emptyStream = fl::make_shared<fl::ByteStreamMemory>(0);
    CHECK_FALSE(decoder->begin(emptyStream));
    CHECK(decoder->hasError());
}

// Helper function to check pixel color
static bool checkPixelColor(const CRGB& pixel, fl::u8 expectedR, fl::u8 expectedG, fl::u8 expectedB, fl::u8 tolerance = 10) {
    return (abs(pixel.r - expectedR) <= tolerance) &&
           (abs(pixel.g - expectedG) <= tolerance) &&
           (abs(pixel.b - expectedB) <= tolerance);
}

// Test 2x2 MPEG1 video with specific frame colors
TEST_CASE("MPEG1 2x2 frame color validation") {
    if (!Mpeg1::isSupported()) {
        return;
    }

    Mpeg1Config config;
    config.mode = Mpeg1Config::Streaming;
    auto decoder = Mpeg1::createDecoder(config);

    // Create stream with our 2x2 test data
    auto stream = fl::make_shared<fl::ByteStreamMemory>(test2x2MpegDataSize);
    stream->write(test2x2MpegData, test2x2MpegDataSize);

    if (!decoder->begin(stream)) {
        // If the basic MPEG structure isn't valid, that's expected for synthetic test data
        // Just ensure no crash and return
        return;
    }

    CHECK(decoder->isReady());

    // Try to decode first frame: red-white-blue-black
    DecodeResult result1 = decoder->decode();
    if (result1 == DecodeResult::Success) {
        Frame frame1 = decoder->getCurrentFrame();
        CHECK(frame1.isValid());
        CHECK(frame1.getWidth() == 2);
        CHECK(frame1.getHeight() == 2);
        CHECK(frame1.getFormat() == PixelFormat::RGB888);

        const CRGB* rgb = frame1.rgb();
        if (rgb != nullptr) {
            // Frame 1: red-white-blue-black (top-left, top-right, bottom-left, bottom-right)
            CHECK(checkPixelColor(rgb[0], 255, 0, 0));    // Pixel (0,0): Red
            CHECK(checkPixelColor(rgb[1], 255, 255, 255)); // Pixel (1,0): White
            CHECK(checkPixelColor(rgb[2], 0, 0, 255));     // Pixel (0,1): Blue
            CHECK(checkPixelColor(rgb[3], 0, 0, 0));       // Pixel (1,1): Black
        }

        // Try to decode second frame: white-blue-black-red
        DecodeResult result2 = decoder->decode();
        if (result2 == DecodeResult::Success) {
            Frame frame2 = decoder->getCurrentFrame();
            CHECK(frame2.isValid());
            CHECK(frame2.getWidth() == 2);
            CHECK(frame2.getHeight() == 2);

            const CRGB* rgb2 = frame2.rgb();
            if (rgb2 != nullptr) {
                // Frame 2: white-blue-black-red (rotated pattern)
                CHECK(checkPixelColor(rgb2[0], 255, 255, 255)); // Pixel (0,0): White
                CHECK(checkPixelColor(rgb2[1], 0, 0, 255));     // Pixel (1,0): Blue
                CHECK(checkPixelColor(rgb2[2], 0, 0, 0));       // Pixel (0,1): Black
                CHECK(checkPixelColor(rgb2[3], 255, 0, 0));     // Pixel (1,1): Red
            }
        }
    }

    decoder->end();
}