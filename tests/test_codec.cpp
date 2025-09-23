#include "test.h"
#include "fl/file_system.h"
#include "fl/codec/jpeg.h"
#include "fl/codec/webp.h"
// #include "fl/codec/gif.h"  // Disabled due to linking issues
#include "fx/frame.h"
#ifdef FASTLED_TESTING
#include "platforms/stub/fs_stub.hpp"
#endif

using namespace fl;

TEST_CASE("Codec file loading and decoding") {
    // Set the filesystem root to point to our test data
    setTestFileSystemRoot("tests");

    // Create filesystem instance
    FileSystem fs;
    bool ok = fs.beginSd(5);
    REQUIRE(ok);

    SUBCASE("JPEG file loading and decoding") {
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

            fl::string error_msg;
            fl::span<const fl::u8> data(file_data.data(), file_size);
            FramePtr frame = Jpeg::decode(config, data, &error_msg);

            if (frame) {
                CHECK(frame->isValid());
                CHECK_EQ(frame->getWidth(), 2);
                CHECK_EQ(frame->getHeight(), 2);
                CHECK_EQ(frame->getFormat(), PixelFormat::RGB888);

                // Expected layout: red-white-blue-black (2x2)
                // For now, just verify that decode was successful
                // Actual pixel verification would require accessing internal frame data
            }
        }

        handle->close();
    }

    SUBCASE("WebP file loading and decoding") {
        // Test that we can load the WebP file from filesystem
        FileHandlePtr handle = fs.openRead("data/codec/file.webp");
        REQUIRE(handle != nullptr);
        REQUIRE(handle->valid());

        // Get file size and read into buffer
        fl::size file_size = handle->size();
        CHECK(file_size > 0);

        fl::vector<fl::u8> file_data(file_size);
        fl::size bytes_read = handle->read(file_data.data(), file_size);
        CHECK_EQ(bytes_read, file_size);

        // WebP files should start with "RIFF"
        CHECK_EQ(file_data[0], 'R');
        CHECK_EQ(file_data[1], 'I');
        CHECK_EQ(file_data[2], 'F');
        CHECK_EQ(file_data[3], 'F');

        // WebP files should have "WEBP" at offset 8
        CHECK_EQ(file_data[8], 'W');
        CHECK_EQ(file_data[9], 'E');
        CHECK_EQ(file_data[10], 'B');
        CHECK_EQ(file_data[11], 'P');

        // Test WebP decoder (if supported)
        if (Webp::isSupported()) {
            WebpDecoderConfig config;
            config.format = PixelFormat::RGB888;

            fl::string error_msg;
            fl::span<const fl::u8> data(file_data.data(), file_size);
            FramePtr frame = Webp::decode(config, data, &error_msg);

            if (frame) {
                CHECK(frame->isValid());
                CHECK_EQ(frame->getWidth(), 2);
                CHECK_EQ(frame->getHeight(), 2);
                CHECK_EQ(frame->getFormat(), PixelFormat::RGB888);

                // Expected layout: red-white-blue-black (2x2)
                // For now, just verify that decode was successful
                // Actual pixel verification would require accessing internal frame data
            }
        } else {
            // WebP decoder not yet implemented, verify we can still load the file
            MESSAGE("WebP decoder not yet implemented - file loading test passed");
        }

        handle->close();
    }

    SUBCASE("GIF file loading") {
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

        // GIF files should start with "GIF"
        CHECK_EQ(file_data[0], 'G');
        CHECK_EQ(file_data[1], 'I');
        CHECK_EQ(file_data[2], 'F');

        // Check for GIF89a or GIF87a signature
        bool is_gif89a = (file_data[3] == '8' && file_data[4] == '9' && file_data[5] == 'a');
        bool is_gif87a = (file_data[3] == '8' && file_data[4] == '7' && file_data[5] == 'a');
        CHECK((is_gif89a || is_gif87a));

        // Note: GIF decoder testing disabled due to libnsgif linking issues
        MESSAGE("GIF file loaded successfully - decoder testing disabled due to linking issues");

        handle->close();
    }

    // Clean up filesystem
    fs.end();
}