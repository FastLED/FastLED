#include "test.h"
#include "fl/file_system.h"
#include "fl/codec/jpeg.h"
#include "fx/frame.h"
#include "platforms/stub/fs_stub.hpp"

using namespace fl;

// Helper function to set up filesystem for codec tests
static FileSystem setupCodecFilesystem() {
    setTestFileSystemRoot("tests");
    FileSystem fs;
    bool ok = fs.beginSd(5);
    REQUIRE(ok);
    return fs;
}

TEST_CASE("JPEG file loading and decoding") {
    FileSystem fs = setupCodecFilesystem();

    // Test that we can load the JPEG file from filesystem
    FileHandlePtr handle = fs.openRead("data/codec/file.jpg");
    REQUIRE(handle != nullptr);
    REQUIRE(handle->valid());

    // Get file size and read into buffer
    fl::size file_size = handle->size();
    CHECK(file_size > 0);

    fl::vector<fl::u8> file_data(file_size);
    fl::size bytes_read = handle->read(file_data.data(), file_size);
    CHECK_EQ(bytes_read, file_size);

    // JPEG files should start with FF D8 (JPEG SOI marker)
    CHECK_EQ(file_data[0], 0xFF);
    CHECK_EQ(file_data[1], 0xD8);

    // JPEG files should end with FF D9 (JPEG EOI marker)
    CHECK_EQ(file_data[file_size - 2], 0xFF);
    CHECK_EQ(file_data[file_size - 1], 0xD9);

    // Test JPEG decoder
    if (Jpeg::isSupported()) {
        JpegDecoderConfig config;
        config.format = PixelFormat::RGB888;
        config.quality = JpegDecoderConfig::Quality::High;  // Use 1:1 scaling for 2x2 test image

        fl::string error_msg;
        fl::span<const fl::u8> data(file_data.data(), file_size);
        FramePtr frame = Jpeg::decode(config, data, &error_msg);

        if (!frame) {
            MESSAGE("JPEG decode failed with error: " << error_msg);
            REQUIRE_MESSAGE(frame, "JPEG decoder returned null frame with error: " << error_msg);
        }

        if (frame) {
            CHECK(frame->isValid());
            CHECK_EQ(frame->getWidth(), 2);
            CHECK_EQ(frame->getHeight(), 2);
            CHECK_EQ(frame->getFormat(), PixelFormat::RGB888);

            // Expected layout: red-white-blue-black (2x2)
            // Verify pixel values match expected color pattern (JPEG compression affects exact values)
            const CRGB* pixels = frame->rgb();
            REQUIRE(pixels != nullptr);

            // Verify we have 4 pixels for a 2x2 image
            MESSAGE("Decoded pixel values - Red: (" << (int)pixels[0].r << "," << (int)pixels[0].g << "," << (int)pixels[0].b
                    << ") White: (" << (int)pixels[1].r << "," << (int)pixels[1].g << "," << (int)pixels[1].b
                    << ") Blue: (" << (int)pixels[2].r << "," << (int)pixels[2].g << "," << (int)pixels[2].b
                    << ") Black: (" << (int)pixels[3].r << "," << (int)pixels[3].g << "," << (int)pixels[3].b << ")");

            // For JPEG with compression artifacts, verify approximate color values
            // Expected layout: red-white-blue-black (2x2)
            // Allow tolerance for JPEG compression artifacts
            const int tolerance = 50; // JPEG compression can alter values significantly

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

            // Check if all pixels are black (indicating decoder failure)
            bool all_pixels_black = true;
            for (int i = 0; i < 4; i++) {
                if (pixels[i].r != 0 || pixels[i].g != 0 || pixels[i].b != 0) {
                    all_pixels_black = false;
                    break;
                }
            }

            // If all pixels are black, this indicates a decoder failure and should fail the test
            CHECK_MESSAGE(!all_pixels_black,
                "JPEG decoder returned all black pixels - decoder failure. "
                "Frame details: valid=" << frame->isValid()
                << ", width=" << frame->getWidth()
                << ", height=" << frame->getHeight());

            // Verify pixels are not all identical (decoder should produce varied output)
            bool all_pixels_identical = true;
            for (int i = 1; i < 4; i++) {
                if (pixels[i].r != pixels[0].r || pixels[i].g != pixels[0].g || pixels[i].b != pixels[0].b) {
                    all_pixels_identical = false;
                    break;
                }
            }
            CHECK_FALSE_MESSAGE(all_pixels_identical,
                "JPEG decoder returned all identical pixels - indicates improper decoding");
        }
    }

    handle->close();
    fs.end();
}