#include "test.h"
#include "fl/file_system.h"
#include "fl/codec/mpeg1.h"
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

TEST_CASE("MPEG1 file loading and decoding") {
    FileSystem fs = setupCodecFilesystem();
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
    fs.end();
}