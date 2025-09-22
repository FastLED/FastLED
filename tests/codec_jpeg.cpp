#include "test.h"
#include "fl/codec/jpeg.h"
#include "fl/bytestreammemory.h"

using namespace fl;

// Test JPEG decoder availability
TEST_CASE("JPEG availability") {
    bool jpegSupported = jpeg::isSupported();

    // For now, JPEG is not implemented
    CHECK_FALSE(jpegSupported);
}

// Test JPEG decoder creation
TEST_CASE("JPEG decoder creation") {
    JpegConfig config;
    config.format = PixelFormat::RGB888;
    config.quality = JpegConfig::Medium;

    fl::string error_msg;
    auto decoder = jpeg::createDecoder(config, &error_msg);

    // Since JPEG is not yet implemented, check expected behavior
    if (!jpeg::isSupported()) {
        // When not supported, decoder should be null and error message should be set
        CHECK(decoder == nullptr);
        CHECK(!error_msg.empty());
    } else {
        // If supported in future, should return valid decoder
        CHECK(decoder != nullptr);
        CHECK_FALSE(decoder->isReady());
    }
}

// Test JPEG decoder with null stream
TEST_CASE("JPEG decoder with null stream") {
    JpegConfig config;
    auto decoder = jpeg::createDecoder(config);

    // Skip this test if JPEG is not supported
    if (!jpeg::isSupported()) {
        CHECK(decoder == nullptr);
        return;
    }

    CHECK(decoder != nullptr);

    // Should fail with null stream
    CHECK_FALSE(decoder->begin(fl::ByteStreamPtr()));
    CHECK(decoder->hasError());
}

// Test JPEG decoder lifecycle
TEST_CASE("JPEG decoder lifecycle") {
    JpegConfig config;
    auto decoder = jpeg::createDecoder(config);

    // Skip this test if JPEG is not supported
    if (!jpeg::isSupported()) {
        CHECK(decoder == nullptr);
        return;
    }

    CHECK(decoder != nullptr);

    // Create a simple test stream
    fl::u8 testData[] = {0xFF, 0xD8, 0xFF, 0xE0}; // JPEG header
    auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(testData));
    stream->write(testData, sizeof(testData));

    // Test lifecycle
    CHECK_FALSE(decoder->isReady());

    // Begin should succeed with valid stream
    CHECK(decoder->begin(stream));

    // End should work
    decoder->end();
    CHECK_FALSE(decoder->isReady());
}

// Test JPEG configuration options
TEST_CASE("JPEG configuration") {
    // Test different quality settings
    {
        JpegConfig config(JpegConfig::Low, PixelFormat::RGB565);
        CHECK(config.quality == JpegConfig::Low);
        CHECK(config.format == PixelFormat::RGB565);
    }

    {
        JpegConfig config(JpegConfig::High, PixelFormat::RGBA8888);
        CHECK(config.quality == JpegConfig::High);
        CHECK(config.format == PixelFormat::RGBA8888);
    }

    // Test default constructor
    {
        JpegConfig config;
        CHECK(config.quality == JpegConfig::Medium);
        CHECK(config.format == PixelFormat::RGB888);
        CHECK(config.useHardwareAcceleration == true);
        CHECK(config.maxWidth == 1920);
        CHECK(config.maxHeight == 1080);
    }
}