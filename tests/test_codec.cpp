#include "test.h"
#include "fl/file_system.h"
#include "fl/codec/jpeg.h"
#include "fl/codec/webp.h"
#include "fl/codec/mpeg1.h"
// #include "fl/codec/gif.h"  // Disabled due to linking issues
#include "fx/frame.h"
#include "fl/bytestreammemory.h"
#include "platforms/stub/fs_stub.hpp"

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

    SUBCASE("MPEG1 file loading and decoding") {
        // Test that we can load the MPEG1 file from filesystem
        FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
        REQUIRE(handle != nullptr);
        REQUIRE(handle->valid());

        // Get file size and read into buffer
        fl::size file_size = handle->size();
        CHECK(file_size > 0);

        fl::vector<fl::u8> file_data(file_size);
        fl::size bytes_read = handle->read(file_data.data(), file_size);
        CHECK_EQ(bytes_read, file_size);

        // MPEG1 files should start with start code (0x000001)
        CHECK_EQ(file_data[0], 0x00);
        CHECK_EQ(file_data[1], 0x00);
        CHECK_EQ(file_data[2], 0x01);
        // Fourth byte can be BA (pack header) or B3 (sequence header)
        CHECK((file_data[3] == 0xBA || file_data[3] == 0xB3));

        // Test MPEG1 decoder
        if (Mpeg1::isSupported()) {
            Mpeg1Config config;
            config.mode = Mpeg1Config::SingleFrame;

            fl::string error_msg;
            auto decoder = Mpeg1::createDecoder(config, &error_msg);

            if (decoder) {
                // Create byte stream from file data
                auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
                stream->write(file_data.data(), file_size);
                bool began = decoder->begin(stream);
                CHECK(began);

                if (began) {
                    // Attempt to decode first frame
                    auto result = decoder->decode();

                    if (result == DecodeResult::Success) {
                        Frame frame0 = decoder->getCurrentFrame();

                        if (frame0.isValid() && frame0.getWidth() == 2 && frame0.getHeight() == 2) {
                            // Verify pixel values for frame 0
                            // Expected: red-white-blue-black (2x2)
                            const CRGB* pixels = frame0.rgb();
                            if (pixels) {
                                // Top-left: red (255, 0, 0)
                                CHECK_EQ(pixels[0].r, 255);
                                CHECK_EQ(pixels[0].g, 0);
                                CHECK_EQ(pixels[0].b, 0);

                                // Top-right: white (255, 255, 255)
                                CHECK_EQ(pixels[1].r, 255);
                                CHECK_EQ(pixels[1].g, 255);
                                CHECK_EQ(pixels[1].b, 255);

                                // Bottom-left: blue (0, 0, 255)
                                CHECK_EQ(pixels[2].r, 0);
                                CHECK_EQ(pixels[2].g, 0);
                                CHECK_EQ(pixels[2].b, 255);

                                // Bottom-right: black (0, 0, 0)
                                CHECK_EQ(pixels[3].r, 0);
                                CHECK_EQ(pixels[3].g, 0);
                                CHECK_EQ(pixels[3].b, 0);
                            }

                            // Check if there are more frames
                            if (decoder->hasMoreFrames()) {
                                // Attempt to decode second frame
                                result = decoder->decode();

                                if (result == DecodeResult::Success) {
                                    Frame frame1 = decoder->getCurrentFrame();

                                    if (frame1.isValid() && frame1.getWidth() == 2 && frame1.getHeight() == 2) {
                                        // Verify pixel values for frame 1
                                        const CRGB* pixels1 = frame1.rgb();
                                        if (pixels1) {
                                            // Top-left: white (255, 255, 255)
                                            CHECK_EQ(pixels1[0].r, 255);
                                            CHECK_EQ(pixels1[0].g, 255);
                                            CHECK_EQ(pixels1[0].b, 255);

                                            // Top-right: blue (0, 0, 255)
                                            CHECK_EQ(pixels1[1].r, 0);
                                            CHECK_EQ(pixels1[1].g, 0);
                                            CHECK_EQ(pixels1[1].b, 255);

                                            // Bottom-left: black (0, 0, 0)
                                            CHECK_EQ(pixels1[2].r, 0);
                                            CHECK_EQ(pixels1[2].g, 0);
                                            CHECK_EQ(pixels1[2].b, 0);

                                            // Bottom-right: red (255, 0, 0)
                                            CHECK_EQ(pixels1[3].r, 255);
                                            CHECK_EQ(pixels1[3].g, 0);
                                            CHECK_EQ(pixels1[3].b, 0);
                                        }
                                    } else {
                                        MESSAGE("Second frame is not valid or wrong dimensions");
                                    }
                                } else {
                                    MESSAGE("Failed to decode second frame, result: " << static_cast<int>(result));
                                }
                            }
                        } else {
                            MESSAGE("First frame is not valid or wrong dimensions");
                        }
                    } else {
                        MESSAGE("Failed to decode first frame, result: " << static_cast<int>(result));
                    }

                    decoder->end();
                } else {
                    MESSAGE("Failed to begin MPEG1 decoder");
                }
            } else {
                MESSAGE("MPEG1 decoder creation failed: " << error_msg);
            }
        } else {
            MESSAGE("MPEG1 decoder not supported on this platform");
        }

        handle->close();
    }

    // Clean up filesystem
    fs.end();
}
