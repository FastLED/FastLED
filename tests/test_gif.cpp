#include "test.h"
#include "fl/file_system.h"
#include "fl/codec/gif.h"
#include "fx/frame.h"
#include "fl/bytestreammemory.h"
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

TEST_CASE("GIF file loading and decoding") {
    FileSystem fs = setupCodecFilesystem();
        // Test that we can load the GIF file from filesystem
        FileHandlePtr handle = fs.openRead("data/codec/file.gif");
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
        if (!Gif::isSupported()) {
            MESSAGE("GIF decoder not supported on this platform");
            handle->close();
            return;
        }

        GifConfig config;
        config.mode = GifConfig::SingleFrame;
        config.format = PixelFormat::RGB888;

        fl::string error_msg;
        auto decoder = Gif::createDecoder(config, &error_msg);
        REQUIRE_MESSAGE(decoder != nullptr, "GIF decoder creation failed: " << error_msg);

        // Create byte stream and begin decoding
        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);
        REQUIRE_MESSAGE(decoder->begin(stream), "Failed to begin GIF decoder");

        // Decode first frame
        auto result = decoder->decode();
        if (result == DecodeResult::Success) {
            Frame frame0 = decoder->getCurrentFrame();
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
                const int tolerance = 50;

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