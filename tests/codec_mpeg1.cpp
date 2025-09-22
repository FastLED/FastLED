#include "test.h"
#include "fl/codec/mpeg1.h"
#include "fl/bytestreammemory.h"

using namespace fl;

// Helper function to create test data
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
    bool mpeg1Supported = mpeg1::isSupported();

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
    auto decoder = mpeg1::createDecoder(config, &error_msg);

    CHECK(decoder != nullptr);
    CHECK_FALSE(decoder->isReady());

    // Create stream with test data
    fl::u8 testData[4096];
    setupTestData(testData);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(testData));
    stream->write(testData, sizeof(testData));

    bool beginResult = decoder->begin(stream);

    // Should succeed on platforms that support MPEG1
    if (mpeg1::isSupported()) {
        CHECK(beginResult);
        CHECK(decoder->isReady());
        CHECK_FALSE(decoder->hasError());
    }
}

// Test frame decoding
TEST_CASE("MPEG1 frame decoding") {
    if (!mpeg1::isSupported()) {
        // Skip test if MPEG1 not supported
        return;
    }

    Mpeg1Config config(Mpeg1Config::SingleFrame);
    auto decoder = mpeg1::createDecoder(config);

    fl::u8 testData[4096];
    setupTestData(testData);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(testData));
    stream->write(testData, sizeof(testData));

    CHECK(decoder->begin(stream));

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
    if (!mpeg1::isSupported()) {
        return;
    }

    Mpeg1Config config;
    config.mode = Mpeg1Config::Streaming;
    config.bufferFrames = 2;

    auto decoder = mpeg1::createDecoder(config);

    fl::u8 testData[4096];
    setupTestData(testData);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(testData));
    stream->write(testData, sizeof(testData));

    CHECK(decoder->begin(stream));

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
    if (!mpeg1::isSupported()) {
        return;
    }

    Mpeg1Config config;
    config.mode = Mpeg1Config::SingleFrame;

    auto decoder = mpeg1::createDecoder(config);

    fl::u8 testData[4096];
    setupTestData(testData);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(testData));
    stream->write(testData, sizeof(testData));

    CHECK(decoder->begin(stream));

    // In single frame mode, should decode one frame
    DecodeResult result = decoder->decode();

    // Test data may not be valid MPEG1, so we just check it doesn't crash
    CHECK((result == DecodeResult::Success ||
           result == DecodeResult::Error ||
           result == DecodeResult::NeedsMoreData));

    decoder->end();
}

// Test decoder lifecycle
TEST_CASE("MPEG1 decoder lifecycle") {
    if (!mpeg1::isSupported()) {
        return;
    }

    Mpeg1Config config;
    auto decoder = mpeg1::createDecoder(config);

    // Initial state
    CHECK_FALSE(decoder->isReady());
    CHECK_FALSE(decoder->hasError());

    // Create test stream
    fl::u8 testData[4096];
    setupTestData(testData);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(testData));
    stream->write(testData, sizeof(testData));

    // Begin
    CHECK(decoder->begin(stream));
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
    if (!mpeg1::isSupported()) {
        return;
    }

    Mpeg1Config config;
    auto decoder = mpeg1::createDecoder(config);

    // Test with null stream
    CHECK_FALSE(decoder->begin(fl::ByteStreamPtr()));
    CHECK(decoder->hasError());

    // Test with empty stream
    auto emptyStream = fl::make_shared<fl::ByteStreamMemory>(0);
    CHECK_FALSE(decoder->begin(emptyStream));
    CHECK(decoder->hasError());
}