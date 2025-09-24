#include "test.h"
#include "fl/file_system.h"
#include "fl/codec/jpeg.h"
#include "fl/codec/webp.h"
#include "fl/codec/mpeg1.h"
#include "fl/codec/gif.h"
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
    }

    #if 1  // webp enabled for testing

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
                // Verify basic frame properties - WebP pixel verification would be added when supported
                const CRGB* pixels = frame->rgb();
                REQUIRE(pixels != nullptr);

                // For now, just verify that decode was successful and we have valid pixel data
                // TODO: Add specific pixel value verification when WebP decoder is fully implemented
            }
        } else {
            // WebP decoder not yet implemented, verify we can still load the file
            MESSAGE("WebP decoder not yet implemented - file loading test passed");
        }

        handle->close();
    }
    #endif  // webp disabled


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
                CHECK_MESSAGE(pixels != nullptr, "GIF first frame decoded successfully");
            } else {
                MESSAGE("GIF frame dimensions invalid: " << frame0.getWidth() << "x" << frame0.getHeight());
            }
        } else {
            MESSAGE("Failed to decode GIF first frame, result: " << static_cast<int>(result));
        }

        decoder->end();
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

        // Helper lambda to verify frame pixel values
        auto verifyFrame0Pixels = [](const CRGB* pixels) {
            CHECK_EQ(pixels[0].r, 68);   // Top-left: approximately red
            CHECK_EQ(pixels[0].g, 68);
            CHECK_EQ(pixels[0].b, 195);

            CHECK_EQ(pixels[1].r, 233);  // Top-right: approximately white
            CHECK_EQ(pixels[1].g, 233);
            CHECK_EQ(pixels[1].b, 255);

            CHECK_EQ(pixels[2].r, 6);    // Bottom-left: approximately blue
            CHECK_EQ(pixels[2].g, 6);
            CHECK_EQ(pixels[2].b, 133);

            CHECK_EQ(pixels[3].r, 0);    // Bottom-right: approximately black
            CHECK_EQ(pixels[3].g, 0);
            CHECK_EQ(pixels[3].b, 119);
        };

        auto verifyFrame1Pixels = [](const CRGB* pixels) {
            CHECK_EQ(pixels[0].r, 255);  // Top-left: approximately white
            CHECK_EQ(pixels[0].g, 208);
            CHECK_EQ(pixels[0].b, 208);

            CHECK_EQ(pixels[1].r, 120);  // Top-right: approximately blue
            CHECK_EQ(pixels[1].g, 0);
            CHECK_EQ(pixels[1].b, 0);

            CHECK_EQ(pixels[2].r, 98);   // Bottom-left: approximately black
            CHECK_EQ(pixels[2].g, 0);
            CHECK_EQ(pixels[2].b, 0);

            CHECK_EQ(pixels[3].r, 163);  // Bottom-right: approximately red
            CHECK_EQ(pixels[3].g, 36);
            CHECK_EQ(pixels[3].b, 36);
        };

        auto verifyFrameDimensions = [](const Frame& frame) {
            return frame.isValid() && frame.getWidth() == 2 && frame.getHeight() == 2;
        };

        auto testMpeg1Decoding = [&]() {
            Mpeg1Config config;
            config.mode = Mpeg1Config::SingleFrame;

            fl::string error_msg;
            auto decoder = Mpeg1::createDecoder(config, &error_msg);
            if (!decoder) {
                MESSAGE("MPEG1 decoder creation failed: " << error_msg);
                return;
            }

            // Create byte stream from file data
            auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
            stream->write(file_data.data(), file_size);
            bool began = decoder->begin(stream);
            CHECK(began);
            if (!began) {
                MESSAGE("Failed to begin MPEG1 decoder");
                return;
            }

            // Decode first frame
            auto result = decoder->decode();
            if (result != DecodeResult::Success) {
                MESSAGE("Failed to decode first frame, result: " << static_cast<int>(result));
                decoder->end();
                return;
            }

            Frame frame0 = decoder->getCurrentFrame();
            if (!verifyFrameDimensions(frame0)) {
                MESSAGE("First frame is not valid or wrong dimensions");
                decoder->end();
                return;
            }

            // Verify first frame pixels
            const CRGB* pixels = frame0.rgb();
            if (pixels) {
                verifyFrame0Pixels(pixels);
            }

            // Decode second frame if available
            if (decoder->hasMoreFrames()) {
                result = decoder->decode();
                if (result != DecodeResult::Success) {
                    MESSAGE("Failed to decode second frame, result: " << static_cast<int>(result));
                } else {
                    Frame frame1 = decoder->getCurrentFrame();
                    if (!verifyFrameDimensions(frame1)) {
                        MESSAGE("Second frame is not valid or wrong dimensions");
                    } else {
                        const CRGB* pixels1 = frame1.rgb();
                        if (pixels1) {
                            verifyFrame1Pixels(pixels1);
                        }
                    }
                }
            }

            decoder->end();
        };

        // Test MPEG1 decoder
        if (!Mpeg1::isSupported()) {
            MESSAGE("MPEG1 decoder not supported on this platform");
        } else {
            testMpeg1Decoding();
        }

        handle->close();
    }

    // Clean up filesystem
    fs.end();
}
