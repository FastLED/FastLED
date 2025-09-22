#include "test.h"
#include "fl/codec/common.h"
#include "fx/frame.h"

using namespace fl;

// Test Frame structure with codec functionality
TEST_CASE("Frame codec functionality") {
    // Test empty frame
    Frame frame(0);
    CHECK_FALSE(frame.isFromCodec());
    CHECK(frame.size() == 0);
    CHECK_FALSE(frame.isValid());

    // Test codec-created frame
    fl::u8 testPixels[12] = {255, 0, 0, 0, 255, 0, 0, 0, 255, 128, 128, 128}; // 4 RGB pixels
    Frame codecFrame(testPixels, 2, 2, PixelFormat::RGB888, 1000);
    CHECK(codecFrame.isFromCodec());
    CHECK(codecFrame.getWidth() == 2);
    CHECK(codecFrame.getHeight() == 2);
    CHECK(codecFrame.getFormat() == PixelFormat::RGB888);
    CHECK(codecFrame.getTimestamp() == 1000);
    CHECK(codecFrame.size() == 4);
    CHECK(codecFrame.isValid());
}

// Test pixel format utilities
TEST_CASE("Pixel format utilities") {
    CHECK(getBytesPerPixel(PixelFormat::RGB565) == 2);
    CHECK(getBytesPerPixel(PixelFormat::RGB888) == 3);
    CHECK(getBytesPerPixel(PixelFormat::RGBA8888) == 4);

    // Test RGB565 conversion
    fl::u16 rgb565 = rgb888ToRgb565(255, 128, 64);
    CHECK(rgb565 > 0);

    fl::u8 r, g, b;
    rgb565ToRgb888(rgb565, r, g, b);
    // Due to precision loss, values won't be exact
    CHECK(r > 240);  // Should be close to 255
    CHECK(g > 120);  // Should be close to 128
    CHECK(b > 60);   // Should be close to 64
}

// Test NullDecoder
TEST_CASE("NullDecoder functionality") {
    NullDecoder decoder;

    // Should always report not ready and has error
    CHECK_FALSE(decoder.begin(fl::ByteStreamPtr()));
    CHECK_FALSE(decoder.isReady());
    CHECK(decoder.hasError());

    fl::string errorMsg;
    CHECK(decoder.hasError(&errorMsg));
    CHECK(!errorMsg.empty());

    // Should return error for all operations
    CHECK(decoder.decode() == DecodeResult::UnsupportedFormat);
    CHECK_FALSE(decoder.hasMoreFrames());

    Frame frame = decoder.getCurrentFrame();
    CHECK_FALSE(frame.isValid());

    decoder.end(); // Should not crash
}

// Test DecodeResult enum
TEST_CASE("DecodeResult values") {
    // Just ensure the enum values exist and are distinct
    CHECK(DecodeResult::Success != DecodeResult::Error);
    CHECK(DecodeResult::NeedsMoreData != DecodeResult::EndOfStream);
    CHECK(DecodeResult::UnsupportedFormat != DecodeResult::Success);
}

// Test PixelFormat enum
TEST_CASE("PixelFormat values") {
    // Ensure pixel format values exist and are distinct
    CHECK(PixelFormat::RGB565 != PixelFormat::RGB888);
    CHECK(PixelFormat::RGB888 != PixelFormat::RGBA8888);
    CHECK(PixelFormat::RGBA8888 != PixelFormat::YUV420);
}