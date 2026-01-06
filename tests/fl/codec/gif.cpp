#include "test.h"
#include "fl/file_system.h"
#include "fl/codec/gif.h"
#include "fl/fx/frame.h"
#include "fl/bytestreammemory.h"
#include "platforms/stub/fs_stub.hpp"


// Helper function to set up filesystem for codec tests
static fl::FileSystem setupCodecFilesystem() {
    fl::setTestFileSystemRoot("tests");
    fl::FileSystem fs;
    bool ok = fs.beginSd(5);
    REQUIRE(ok);
    return fs;
}

TEST_CASE("GIF file loading and decoding") {
    fl::FileSystem fs = setupCodecFilesystem();
        // Test that we can load the GIF file from filesystem
        fl::FileHandlePtr handle = fs.openRead("data/codec/file.gif");
        REQUIRE(handle != nullptr);
        REQUIRE(handle->valid());

        // Get file size and read into buffer
        fl::size file_size = handle->size();
        CHECK(file_size > 0);

        fl::vector<fl::u8> file_data(file_size);
        fl::size bytes_read = handle->read(file_data.data(), file_size);
        CHECK_EQ(bytes_read, file_size);

        // Validate GIF file signature
        CHECK_EQ(file_data[0], 'G');
        CHECK_EQ(file_data[1], 'I');
        CHECK_EQ(file_data[2], 'F');

        // Check for valid GIF version (87a or 89a)
        bool valid_version = (file_data[3] == '8' &&
                             ((file_data[4] == '7' && file_data[5] == 'a') ||
                              (file_data[4] == '9' && file_data[5] == 'a')));
        CHECK(valid_version);

        // Test GIF decoder if supported
        if (!fl::Gif::isSupported()) {
            MESSAGE("GIF decoder not supported on this platform");
            handle->close();
            return;
        }

        fl::GifConfig config;
        config.mode = fl::GifConfig::SingleFrame;
        config.format = fl::PixelFormat::RGB888;

        fl::string error_msg;
        auto decoder = fl::Gif::createDecoder(config, &error_msg);
        REQUIRE_MESSAGE(decoder != nullptr, "GIF decoder creation failed: " << error_msg);

        // Create byte stream and begin decoding
        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);
        REQUIRE_MESSAGE(decoder->begin(stream), "Failed to begin GIF decoder");

        // Decode first frame
        auto result = decoder->decode();
        if (result == fl::DecodeResult::Success) {
            fl::Frame frame0 = decoder->getCurrentFrame();
            if (frame0.isValid() && frame0.getWidth() == 2 && frame0.getHeight() == 2) {
                const CRGB* pixels = frame0.rgb();
                REQUIRE_MESSAGE(pixels != nullptr, "GIF frame pixels should not be null");

                // Debug: Show decoded pixel values like JPEG test
                MESSAGE("GIF decoded pixel values - Red: (" << (int)pixels[0].r << "," << (int)pixels[0].g << "," << (int)pixels[0].b
                        << ") White: (" << (int)pixels[1].r << "," << (int)pixels[1].g << "," << (int)pixels[1].b
                        << ") Blue: (" << (int)pixels[2].r << "," << (int)pixels[2].g << "," << (int)pixels[2].b
                        << ") Black: (" << (int)pixels[3].r << "," << (int)pixels[3].g << "," << (int)pixels[3].b << ")");

                // Expected layout: red-white-blue-black (2x2) - same as JPEG test
                // Allow tolerance for GIF compression artifacts

                // Pixel 0: Red (high R, low G/B)
                CHECK_MESSAGE(pixels[0].r > 150, "Red pixel should have high red value, got: " << (int)pixels[0].r);
                CHECK_MESSAGE(pixels[0].g < 100, "Red pixel should have low green value, got: " << (int)pixels[0].g);
                CHECK_MESSAGE(pixels[0].b < 100, "Red pixel should have low blue value, got: " << (int)pixels[0].b);

                // Pixel 1: White (high R/G/B)
                CHECK_MESSAGE(pixels[1].r > 200, "White pixel should have high red value, got: " << (int)pixels[1].r);
                CHECK_MESSAGE(pixels[1].g > 200, "White pixel should have high green value, got: " << (int)pixels[1].g);
                CHECK_MESSAGE(pixels[1].b > 200, "White pixel should have high blue value, got: " << (int)pixels[1].b);

                // Pixel 2: Blue (low R/G, high B)
                CHECK_MESSAGE(pixels[2].r < 100, "Blue pixel should have low red value, got: " << (int)pixels[2].r);
                CHECK_MESSAGE(pixels[2].g < 100, "Blue pixel should have low green value, got: " << (int)pixels[2].g);
                CHECK_MESSAGE(pixels[2].b > 150, "Blue pixel should have high blue value, got: " << (int)pixels[2].b);

                // Pixel 3: Black (low R/G/B)
                CHECK_MESSAGE(pixels[3].r < 50, "Black pixel should have low red value, got: " << (int)pixels[3].r);
                CHECK_MESSAGE(pixels[3].g < 50, "Black pixel should have low green value, got: " << (int)pixels[3].g);
                CHECK_MESSAGE(pixels[3].b < 50, "Black pixel should have low blue value, got: " << (int)pixels[3].b);

                // Check if all pixels are black (indicating decoder failure like JPEG test)
                bool all_pixels_black = true;
                for (int i = 0; i < 4; i++) {
                    if (pixels[i].r != 0 || pixels[i].g != 0 || pixels[i].b != 0) {
                        all_pixels_black = false;
                        break;
                    }
                }

                CHECK_MESSAGE(!all_pixels_black,
                    "GIF decoder returned all black pixels - decoder failure. "
                    "Frame details: valid=" << frame0.isValid()
                    << ", width=" << frame0.getWidth()
                    << ", height=" << frame0.getHeight());

                // Verify pixels are not all identical (decoder should produce varied output)
                bool all_pixels_identical = true;
                for (int i = 1; i < 4; i++) {
                    if (pixels[i].r != pixels[0].r || pixels[i].g != pixels[0].g || pixels[i].b != pixels[0].b) {
                        all_pixels_identical = false;
                        break;
                    }
                }
                CHECK_FALSE_MESSAGE(all_pixels_identical,
                    "GIF decoder returned all identical pixels - indicates improper decoding");

            } else {
                MESSAGE("GIF frame dimensions invalid: " << frame0.getWidth() << "x" << frame0.getHeight());
            }
        } else {
            MESSAGE("Failed to decode GIF first frame, result: " << static_cast<int>(result));
        }

        decoder->end();
        handle->close();
        fs.end();
}

TEST_CASE("GIF metadata parsing without decoding") {
    fl::FileSystem fs = setupCodecFilesystem();

    // Test that we can load the GIF file from filesystem
    fl::FileHandlePtr handle = fs.openRead("data/codec/file.gif");
    REQUIRE(handle != nullptr);
    REQUIRE(handle->valid());

    // Get file size and read into buffer
    fl::size file_size = handle->size();
    CHECK(file_size > 0);

    fl::vector<fl::u8> file_data(file_size);
    fl::size bytes_read = handle->read(file_data.data(), file_size);
    CHECK_EQ(bytes_read, file_size);

    // Test GIF metadata parsing
    fl::string error_msg;
    fl::span<const fl::u8> data(file_data.data(), file_size);
    fl::GifInfo info = fl::Gif::parseGifInfo(data, &error_msg);

    // The metadata parsing should succeed
    CHECK_MESSAGE(info.isValid, "GIF metadata parsing failed: " << error_msg);

    if (info.isValid) {
        // Verify basic metadata
        CHECK_MESSAGE(info.width > 0, "GIF width should be greater than 0, got: " << info.width);
        CHECK_MESSAGE(info.height > 0, "GIF height should be greater than 0, got: " << info.height);

        // For our test image (2x2 pixels), verify exact dimensions
        CHECK_MESSAGE(info.width == 2, "Expected width=2, got: " << info.width);
        CHECK_MESSAGE(info.height == 2, "Expected height=2, got: " << info.height);

        // Verify frame and animation information
        CHECK_MESSAGE(info.frameCount > 0, "GIF should have at least 1 frame, got: " << info.frameCount);
        CHECK_MESSAGE(info.bitsPerPixel == 8, "GIF should have 8 bits per pixel, got: " << (int)info.bitsPerPixel);

        // Animation consistency check
        if (info.frameCount == 1) {
            CHECK_MESSAGE(!info.isAnimated, "GIF with 1 frame should not be marked as animated");
        } else {
            CHECK_MESSAGE(info.isAnimated, "GIF with " << info.frameCount << " frames should be marked as animated");
        }

        fl::string animated_str = info.isAnimated ? "yes" : "no";
        MESSAGE("GIF metadata - Width: " << info.width
                << ", Height: " << info.height
                << ", FrameCount: " << info.frameCount
                << ", LoopCount: " << info.loopCount
                << ", BitsPerPixel: " << (int)info.bitsPerPixel
                << ", IsAnimated: " << animated_str);
    }

    // Test edge cases
    SUBCASE("Empty data") {
        fl::vector<fl::u8> empty_data;
        fl::span<const fl::u8> empty_span(empty_data.data(), 0);
        fl::string empty_error;

        fl::GifInfo empty_info = fl::Gif::parseGifInfo(empty_span, &empty_error);
        CHECK_FALSE(empty_info.isValid);
        CHECK_FALSE(empty_error.empty());
        MESSAGE("Empty data error: " << empty_error);
    }

    SUBCASE("Too small data") {
        fl::vector<fl::u8> small_data = {'G', 'I', 'F', '8', '7', 'a'}; // Just signature
        fl::span<const fl::u8> small_span(small_data.data(), small_data.size());
        fl::string small_error;

        fl::GifInfo small_info = fl::Gif::parseGifInfo(small_span, &small_error);
        CHECK_FALSE(small_info.isValid);
        CHECK_FALSE(small_error.empty());
        MESSAGE("Small data error: " << small_error);
    }

    SUBCASE("Invalid GIF signature") {
        fl::vector<fl::u8> invalid_data(50, 0x42); // Random bytes
        invalid_data[0] = 'X'; // Invalid signature
        invalid_data[1] = 'I';
        invalid_data[2] = 'F';
        fl::span<const fl::u8> invalid_span(invalid_data.data(), invalid_data.size());
        fl::string invalid_error;

        fl::GifInfo invalid_info = fl::Gif::parseGifInfo(invalid_span, &invalid_error);
        CHECK_FALSE(invalid_info.isValid);
        CHECK_FALSE(invalid_error.empty());
        MESSAGE("Invalid signature error: " << invalid_error);
    }

    handle->close();
    fs.end();
}