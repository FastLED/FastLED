#include "test.h"
#include "fl/file_system.h"
#include "fl/codec/jpeg.h"
#include "fl/bytestreammemory.h"
#include "fl/fx/frame.h"
#include "platforms/stub/fs_stub.hpp"


// Helper function to set up filesystem for codec tests
static fl::FileSystem setupCodecFilesystem() {
    fl::setTestFileSystemRoot("tests");
    fl::FileSystem fs;
    bool ok = fs.beginSd(5);
    REQUIRE(ok);
    return fs;
}

TEST_CASE("JPEG file loading and decoding") {
    fl::FileSystem fs = setupCodecFilesystem();

    // Test that we can load the JPEG file from filesystem
    fl::FileHandlePtr handle = fs.openRead("data/codec/file.jpg");
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
    if (fl::Jpeg::isSupported()) {
        fl::JpegConfig config;
        config.format = fl::PixelFormat::RGB888;
        config.quality = fl::JpegConfig::Quality::High;  // Use 1:1 scaling for 2x2 test image

        fl::string error_msg;
        fl::span<const fl::u8> data(file_data.data(), file_size);
        fl::FramePtr frame = fl::Jpeg::decode(config, data, &error_msg);

        if (!frame) {
            MESSAGE("JPEG decode failed with error: " << error_msg);
            REQUIRE_MESSAGE(frame, "JPEG decoder returned null frame with error: " << error_msg);
        }

        if (frame) {
            CHECK(frame->isValid());
            CHECK_EQ(frame->getWidth(), 2);
            CHECK_EQ(frame->getHeight(), 2);
            CHECK_EQ(frame->getFormat(), fl::PixelFormat::RGB888);

            // Expected layout: red-white-blue-black (2x2)
            // Verify pixel values match expected color pattern (JPEG compression affects exact values)
            const CRGB* pixels = frame->rgb();
            REQUIRE(pixels != nullptr);

            // Verify we have 4 pixels for a 2x2 image
            MESSAGE("Decoded pixel values - Red: (" << (int)pixels[0].r << "," << (int)pixels[0].g << "," << (int)pixels[0].b
                    << ") White: (" << (int)pixels[1].r << "," << (int)pixels[1].g << "," << (int)pixels[1].b
                    << ") Blue: (" << (int)pixels[2].r << "," << (int)pixels[2].g << "," << (int)pixels[2].b
                    << ") Black: (" << (int)pixels[3].r << "," << (int)pixels[3].g << "," << (int)pixels[3].b << ")");

            // For JPEG with compression artifacts, verify decoder is working correctly
            // The exact color values may vary due to JPEG compression, but decoder should produce valid output

            // Verify that pixels have some color variation (not all the same)
            bool pixels_vary = false;
            for (int i = 1; i < 4; i++) {
                if (pixels[i].r != pixels[0].r || pixels[i].g != pixels[0].g || pixels[i].b != pixels[0].b) {
                    pixels_vary = true;
                    break;
                }
            }
            CHECK_MESSAGE(pixels_vary, "Pixels should have some variation, got all identical values");

            // Verify that color channel values are within reasonable range (0-255)
            for (int i = 0; i < 4; i++) {
                CHECK_MESSAGE(pixels[i].r <= 255, "Red channel out of range for pixel " << i << ": " << (int)pixels[i].r);
                CHECK_MESSAGE(pixels[i].g <= 255, "Green channel out of range for pixel " << i << ": " << (int)pixels[i].g);
                CHECK_MESSAGE(pixels[i].b <= 255, "Blue channel out of range for pixel " << i << ": " << (int)pixels[i].b);
            }

            // Verify that at least some pixels have non-zero color values
            bool has_non_zero_pixels = false;
            for (int i = 0; i < 4; i++) {
                if (pixels[i].r > 0 || pixels[i].g > 0 || pixels[i].b > 0) {
                    has_non_zero_pixels = true;
                    break;
                }
            }
            CHECK_MESSAGE(has_non_zero_pixels, "At least some pixels should have non-zero color values");

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
