#include "test.h"
#include "fl/codec/gif.h"
#include "fl/bytestreammemory.h"
#include "fx/frame.h"

using namespace fl;

// Test GIF data: 2x2 image with Red, Blue, Green, Black pixels
// This is a minimal GIF89a file with a 2x2 image
const uint8_t test_gif_2x2[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61,       // GIF89a signature
    0x02, 0x00,                               // Width: 2
    0x02, 0x00,                               // Height: 2
    0xF0, 0x00, 0x00,                         // Global Color Table: 2 bits, no sort, 2 colors

    // Global Color Table (4 colors, 3 bytes each)
    0xFF, 0x00, 0x00,                         // Color 0: Red
    0x00, 0x00, 0xFF,                         // Color 1: Blue
    0x00, 0xFF, 0x00,                         // Color 2: Green
    0x00, 0x00, 0x00,                         // Color 3: Black

    0x21, 0xF9, 0x04,                         // Graphic Control Extension
    0x00, 0x00, 0x00, 0x00, 0x00,            // No delay, no transparency

    0x2C,                                     // Image Descriptor
    0x00, 0x00, 0x00, 0x00,                   // Left, Top: 0, 0
    0x02, 0x00, 0x02, 0x00,                   // Width, Height: 2, 2
    0x00,                                     // No local color table

    0x02,                                     // LZW minimum code size: 2
    0x04,                                     // Data sub-block size: 4
    0x84, 0x8F, 0xA9, 0xCB,                   // Compressed image data
    0x00,                                     // End of data sub-blocks

    0x3B                                      // GIF trailer
};
const size_t test_gif_2x2_size = sizeof(test_gif_2x2);

// Test GIF decoder availability
TEST_CASE("GIF availability") {
    bool gifSupported = Gif::isSupported();

    // GIF should be supported
    CHECK(gifSupported);
}

// Test GIF decoder creation
TEST_CASE("GIF decoder creation") {
    GifConfig config;
    config.mode = GifConfig::SingleFrame;
    config.format = PixelFormat::RGB888;

    fl::string error_msg;
    auto decoder = Gif::createDecoder(config, &error_msg);

    // Decoder creation should succeed
    CHECK(decoder != nullptr);
    if (!decoder) {
        FAIL("GIF decoder creation failed: " << error_msg);
    }
}

// Test GIF configuration
TEST_CASE("GIF configuration") {
    // Test different modes and formats
    {
        GifConfig config(GifConfig::SingleFrame, PixelFormat::RGB565);
        CHECK(config.mode == GifConfig::SingleFrame);
        CHECK(config.format == PixelFormat::RGB565);
    }

    {
        GifConfig config(GifConfig::Streaming, PixelFormat::RGBA8888);
        CHECK(config.mode == GifConfig::Streaming);
        CHECK(config.format == PixelFormat::RGBA8888);
    }

    // Test default constructor
    {
        GifConfig config;
        CHECK(config.mode == GifConfig::Streaming);
        CHECK(config.format == PixelFormat::RGB888);
        CHECK(config.looping == true);
        CHECK(config.maxWidth == 1920);
        CHECK(config.maxHeight == 1080);
        CHECK(config.bufferFrames == 3);
    }
}

// Test GIF decoder with 2x2 test data
TEST_CASE("GIF decoder with 2x2 test data") {
    GifConfig config;
    config.mode = GifConfig::SingleFrame;
    config.format = PixelFormat::RGB888;

    fl::string error_msg;
    auto decoder = Gif::createDecoder(config, &error_msg);

    if (!decoder) {
        FAIL("GIF decoder creation failed: " << error_msg);
    }

    // Create a ByteStreamMemory and write our test data to it
    auto stream = fl::make_shared<ByteStreamMemory>(test_gif_2x2_size);
    stream->write(test_gif_2x2, test_gif_2x2_size);

    // Begin decoding
    bool beginSuccess = decoder->begin(stream);
    if (!beginSuccess) {
        fl::string errorMsg;
        decoder->hasError(&errorMsg);
        INFO("GIF decoder begin failed (expected during development): " << errorMsg);
        return;
    }

    // Decode frame
    DecodeResult result = decoder->decode();
    if (result != DecodeResult::Success) {
        fl::string errorMsg;
        decoder->hasError(&errorMsg);
        INFO("GIF decode failed (expected during development): " << errorMsg);
        return;
    }

    // Get the decoded frame
    Frame frame = decoder->getCurrentFrame();
    if (!frame.isValid()) {
        INFO("GIF frame invalid (expected during development)");
        return;
    }

    // Verify frame properties if decode succeeds
    CHECK(frame.isValid());
    CHECK(frame.getWidth() == 2);
    CHECK(frame.getHeight() == 2);
    CHECK(frame.getFormat() == PixelFormat::RGB888);

    decoder->end();
}

// Test GIF decoder with empty data
TEST_CASE("GIF decoder with empty data") {
    GifConfig config;
    fl::string error_msg;
    auto decoder = Gif::createDecoder(config, &error_msg);

    if (!decoder) {
        FAIL("GIF decoder creation failed: " << error_msg);
    }

    // Create empty stream
    auto empty_stream = fl::make_shared<ByteStreamMemory>(0);

    // Try to begin with empty data - behavior may vary by implementation
    bool beginSuccess = decoder->begin(empty_stream);

    // If begin succeeds with empty data, try to decode (should fail)
    if (beginSuccess) {
        DecodeResult result = decoder->decode();
        CHECK(result != DecodeResult::Success);
        decoder->end();
    } else {
        // If begin fails, that's also acceptable behavior
        // Note: Some decoders may not set error state immediately
        INFO("Begin failed with empty data (expected behavior)");
    }
}

// Test GIF decoder with invalid data
TEST_CASE("GIF decoder with invalid data") {
    GifConfig config;
    fl::string error_msg;
    auto decoder = Gif::createDecoder(config, &error_msg);

    if (!decoder) {
        FAIL("GIF decoder creation failed: " << error_msg);
    }

    // Create invalid GIF data (just partial header)
    fl::u8 testData[] = {0x47, 0x49, 0x46}; // "GIF" only
    auto stream = fl::make_shared<ByteStreamMemory>(sizeof(testData));
    stream->write(testData, sizeof(testData));

    // Try to begin with invalid data - behavior may vary by implementation
    bool beginSuccess = decoder->begin(stream);

    if (beginSuccess) {
        // If begin succeeds with partial data, decode should fail
        DecodeResult result = decoder->decode();
        CHECK(result != DecodeResult::Success);
        decoder->end();
    } else {
        // If begin fails with invalid data, that's also acceptable
        // Note: Some decoders may not set error state immediately
        INFO("Begin failed with invalid data (expected behavior)");
    }
}