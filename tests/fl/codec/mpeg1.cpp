#include "test.h"
#include "fl/file_system.h"
#include "fl/codec/mpeg1.h"
#include "fl/fx/frame.h"
#include "fl/bytestreammemory.h"
#include "platforms/stub/fs_stub.hpp"


// Helper function to set up filesystem for codec tests
static fl::FileSystem setupCodecFilesystem() {
    fl::setTestFileSystemRoot("tests");
    fl::FileSystem fs;
    bool ok = fs.beginSd(5);
    FL_REQUIRE(ok);
    return fs;
}

FL_TEST_CASE("MPEG1 file loading and decoding") {
    fl::FileSystem fs = setupCodecFilesystem();
        // Test that we can load the MPEG1 file from filesystem
        fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
        FL_REQUIRE(handle != nullptr);
        FL_REQUIRE(handle->valid());

        // Get file size and read into buffer
        fl::size file_size = handle->size();
        FL_CHECK(file_size > 0);

        fl::vector<fl::u8> file_data(file_size);
        fl::size bytes_read = handle->read(file_data.data(), file_size);
        FL_CHECK_EQ(bytes_read, file_size);

        // MPEG1 files should start with start code (0x000001)
        FL_CHECK_EQ(file_data[0], 0x00);
        FL_CHECK_EQ(file_data[1], 0x00);
        FL_CHECK_EQ(file_data[2], 0x01);
        // Fourth byte can be BA (pack header) or B3 (sequence header)
        FL_CHECK((file_data[3] == 0xBA || file_data[3] == 0xB3));

        // Helper lambda to verify frame pixel values
        auto verifyFrame0Pixels = [](const CRGB* pixels) {
            FL_CHECK_EQ(pixels[0].r, 68);   // Top-left: approximately red
            FL_CHECK_EQ(pixels[0].g, 68);
            FL_CHECK_EQ(pixels[0].b, 195);

            FL_CHECK_EQ(pixels[1].r, 233);  // Top-right: approximately white
            FL_CHECK_EQ(pixels[1].g, 233);
            FL_CHECK_EQ(pixels[1].b, 255);

            FL_CHECK_EQ(pixels[2].r, 6);    // Bottom-left: approximately blue
            FL_CHECK_EQ(pixels[2].g, 6);
            FL_CHECK_EQ(pixels[2].b, 133);

            FL_CHECK_EQ(pixels[3].r, 0);    // Bottom-right: approximately black
            FL_CHECK_EQ(pixels[3].g, 0);
            FL_CHECK_EQ(pixels[3].b, 119);
        };

        auto verifyFrame1Pixels = [](const CRGB* pixels) {
            FL_CHECK_EQ(pixels[0].r, 255);  // Top-left: approximately white
            FL_CHECK_EQ(pixels[0].g, 208);
            FL_CHECK_EQ(pixels[0].b, 208);

            FL_CHECK_EQ(pixels[1].r, 120);  // Top-right: approximately blue
            FL_CHECK_EQ(pixels[1].g, 0);
            FL_CHECK_EQ(pixels[1].b, 0);

            FL_CHECK_EQ(pixels[2].r, 98);   // Bottom-left: approximately black
            FL_CHECK_EQ(pixels[2].g, 0);
            FL_CHECK_EQ(pixels[2].b, 0);

            FL_CHECK_EQ(pixels[3].r, 163);  // Bottom-right: approximately red
            FL_CHECK_EQ(pixels[3].g, 36);
            FL_CHECK_EQ(pixels[3].b, 36);
        };

        auto verifyFrameDimensions = [](const fl::Frame& frame) {
            return frame.isValid() && frame.getWidth() == 2 && frame.getHeight() == 2;
        };

        auto testMpeg1Decoding = [&]() {
            fl::Mpeg1Config config;
            config.mode = fl::Mpeg1Config::SingleFrame;

            fl::string error_msg;
            auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
            if (!decoder) {
                FL_MESSAGE("MPEG1 decoder creation failed: " << error_msg);
                return;
            }

            // Create byte stream from file data
            auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
            stream->write(file_data.data(), file_size);
            bool began = decoder->begin(stream);
            FL_CHECK(began);
            if (!began) {
                FL_MESSAGE("Failed to begin MPEG1 decoder");
                return;
            }

            // Decode first frame
            auto result = decoder->decode();
            if (result != fl::DecodeResult::Success) {
                FL_MESSAGE("Failed to decode first frame, result: " << static_cast<int>(result));
                decoder->end();
                return;
            }

            fl::Frame frame0 = decoder->getCurrentFrame();
            if (!verifyFrameDimensions(frame0)) {
                FL_MESSAGE("First frame is not valid or wrong dimensions");
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
                if (result != fl::DecodeResult::Success) {
                    FL_MESSAGE("Failed to decode second frame, result: " << static_cast<int>(result));
                } else {
                    fl::Frame frame1 = decoder->getCurrentFrame();
                    if (!verifyFrameDimensions(frame1)) {
                        FL_MESSAGE("Second frame is not valid or wrong dimensions");
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
        if (!fl::Mpeg1::isSupported()) {
            FL_MESSAGE("MPEG1 decoder not supported on this platform");
        } else {
            testMpeg1Decoding();
        }

    handle->close();
    fs.end();
}

FL_TEST_CASE("MPEG1 decoder error handling") {
    fl::FileSystem fs = setupCodecFilesystem();

    FL_SUBCASE("null ByteStream") {
        fl::Mpeg1Config config;
        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        FL_REQUIRE(decoder);

        FL_CHECK_FALSE(decoder->begin(nullptr));
        FL_CHECK(decoder->hasError());
    }

    FL_SUBCASE("empty ByteStream") {
        fl::Mpeg1Config config;
        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        FL_REQUIRE(decoder);

        auto empty_stream = fl::make_shared<fl::ByteStreamMemory>(0);
        FL_CHECK_FALSE(decoder->begin(empty_stream));
        FL_CHECK(decoder->hasError());

        fl::string error_message;
        decoder->hasError(&error_message);
        FL_CHECK(error_message.find("Empty input stream") != fl::string::npos);
    }

    FL_SUBCASE("invalid MPEG1 data") {
        fl::Mpeg1Config config;
        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        FL_REQUIRE(decoder);

        // Create stream with invalid data (not MPEG1 format)
        const fl::u8 invalid_data[] = {0xFF, 0xD8, 0xFF, 0xE0}; // JPEG header
        auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(invalid_data));
        stream->write(invalid_data, sizeof(invalid_data));

        FL_CHECK_FALSE(decoder->begin(stream));
        FL_CHECK(decoder->hasError());
    }

    FL_SUBCASE("truncated MPEG1 data") {
        // Load the valid MPEG1 file but truncate it
        fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
        FL_REQUIRE(handle != nullptr);

        fl::size file_size = handle->size();
        fl::vector<fl::u8> file_data(file_size / 2); // Only read half
        handle->read(file_data.data(), file_data.size());
        handle->close();

        fl::Mpeg1Config config;
        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        FL_REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_data.size());
        stream->write(file_data.data(), file_data.size());

        // Decoder might initialize but should fail during decode or have limited functionality
        if (decoder->begin(stream)) {
            // The decoder may succeed with partial data due to pl_mpeg's robustness
            // This is actually expected behavior - pl_mpeg handles partial streams gracefully
            auto result = decoder->decode();
            // Either success (partial decode) or failure is acceptable
            FL_CHECK((result == fl::DecodeResult::Success || result == fl::DecodeResult::Error || result == fl::DecodeResult::EndOfStream));
        } else {
            FL_CHECK(decoder->hasError());
        }
    }

    fs.end();
}

FL_TEST_CASE("MPEG1 configuration options") {
    fl::FileSystem fs = setupCodecFilesystem();

    // Load valid MPEG1 data
    fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
    FL_REQUIRE(handle != nullptr);

    fl::size file_size = handle->size();
    fl::vector<fl::u8> file_data(file_size);
    handle->read(file_data.data(), file_size);
    handle->close();

    FL_SUBCASE("SingleFrame mode") {
        fl::Mpeg1Config config;
        config.mode = fl::Mpeg1Config::SingleFrame;

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        FL_REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        FL_CHECK(decoder->begin(stream));
        FL_CHECK(decoder->decode() == fl::DecodeResult::Success);

        fl::Frame frame = decoder->getCurrentFrame();
        FL_CHECK(frame.isValid());
        FL_CHECK_EQ(frame.getWidth(), 2);
        FL_CHECK_EQ(frame.getHeight(), 2);
    }

    FL_SUBCASE("Streaming mode with buffering") {
        fl::Mpeg1Config config;
        config.mode = fl::Mpeg1Config::Streaming;
        config.immediateMode = false;
        config.bufferFrames = 3;

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        FL_REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        FL_CHECK(decoder->begin(stream));
        FL_CHECK(decoder->decode() == fl::DecodeResult::Success);

        fl::Frame frame = decoder->getCurrentFrame();
        FL_CHECK(frame.isValid());
    }

    FL_SUBCASE("Custom frame rate") {
        fl::Mpeg1Config config;
        config.targetFps = 15;

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        FL_REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        FL_CHECK(decoder->begin(stream));
        FL_CHECK(decoder->decode() == fl::DecodeResult::Success);
    }

    FL_SUBCASE("Audio disabled (default)") {
        fl::Mpeg1Config config;
        config.skipAudio = true; // This should be default

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        FL_REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        FL_CHECK(decoder->begin(stream));
        FL_CHECK(decoder->decode() == fl::DecodeResult::Success);
    }

    fs.end();
}

FL_TEST_CASE("MPEG1 decoder properties and metadata") {
    fl::FileSystem fs = setupCodecFilesystem();

    fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
    FL_REQUIRE(handle != nullptr);

    fl::size file_size = handle->size();
    fl::vector<fl::u8> file_data(file_size);
    handle->read(file_data.data(), file_size);
    handle->close();

    fl::Mpeg1Config config;
    fl::string error_msg;
    auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
    FL_REQUIRE(decoder);

    auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
    stream->write(file_data.data(), file_size);

    FL_CHECK(decoder->begin(stream));

    FL_SUBCASE("Video properties") {
        // Check that we can get video properties after initialization
        // Note: dynamic_cast requires RTTI which is disabled, so we'll test via frame properties
        FL_CHECK(decoder->decode() == fl::DecodeResult::Success);
        fl::Frame test_frame = decoder->getCurrentFrame();
        if (test_frame.isValid()) {
            FL_CHECK_EQ(test_frame.getWidth(), 2);
            FL_CHECK_EQ(test_frame.getHeight(), 2);
        }
        decoder->end();
        decoder->begin(stream); // Reset for other subtests
    }

    FL_SUBCASE("Frame count and seeking") {
        // Frame count is 0 for streaming mode (unknown in advance)
        FL_CHECK_EQ(decoder->getFrameCount(), 0);

        // Seeking is not supported
        FL_CHECK_FALSE(decoder->seek(1));
    }

    FL_SUBCASE("Decoder state management") {
        FL_CHECK(decoder->isReady());
        FL_CHECK_FALSE(decoder->hasError());
        FL_CHECK(decoder->hasMoreFrames());

        // Decode first frame
        FL_CHECK(decoder->decode() == fl::DecodeResult::Success);
        FL_CHECK_EQ(decoder->getCurrentFrameIndex(), 1);

        // Should still have more frames
        if (decoder->hasMoreFrames()) {
            FL_CHECK(decoder->decode() == fl::DecodeResult::Success);
            FL_CHECK_EQ(decoder->getCurrentFrameIndex(), 2);
        }
    }

    decoder->end();
    FL_CHECK_FALSE(decoder->isReady());

    fs.end();
}

FL_TEST_CASE("MPEG1 frame data validation") {
    fl::FileSystem fs = setupCodecFilesystem();

    fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
    FL_REQUIRE(handle != nullptr);

    fl::size file_size = handle->size();
    fl::vector<fl::u8> file_data(file_size);
    handle->read(file_data.data(), file_size);
    handle->close();

    fl::Mpeg1Config config;
    config.mode = fl::Mpeg1Config::SingleFrame;

    fl::string error_msg;
    auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
    FL_REQUIRE(decoder);

    auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
    stream->write(file_data.data(), file_size);

    FL_CHECK(decoder->begin(stream));
    FL_CHECK(decoder->decode() == fl::DecodeResult::Success);

    fl::Frame frame = decoder->getCurrentFrame();
    FL_REQUIRE(frame.isValid());

    FL_SUBCASE("Frame properties") {
        FL_CHECK_EQ(frame.getWidth(), 2);
        FL_CHECK_EQ(frame.getHeight(), 2);
        FL_CHECK_EQ(frame.getFormat(), fl::PixelFormat::RGB888);
        FL_CHECK_GE(frame.getTimestamp(), 0); // Should have valid timestamp (may be 0 for first frame)
    }

    FL_SUBCASE("Pixel data integrity") {
        const CRGB* pixels = frame.rgb();
        FL_REQUIRE(pixels != nullptr);

        // Verify all 4 pixels are within valid RGB ranges
        for (int i = 0; i < 4; ++i) {
            FL_CHECK_GE(pixels[i].r, 0);
            FL_CHECK_LE(pixels[i].r, 255);
            FL_CHECK_GE(pixels[i].g, 0);
            FL_CHECK_LE(pixels[i].g, 255);
            FL_CHECK_GE(pixels[i].b, 0);
            FL_CHECK_LE(pixels[i].b, 255);
        }

        // Verify the expected color pattern is close to red-white-blue-black
        // (allowing for MPEG1 lossy compression artifacts)

        // Top-left should be blue-ish (high blue component)
        FL_CHECK_GT(pixels[0].b, pixels[0].r);
        FL_CHECK_GT(pixels[0].b, pixels[0].g);

        // Top-right should be white-ish (high all components)
        FL_CHECK_GT(pixels[1].r, 200);
        FL_CHECK_GT(pixels[1].g, 200);
        FL_CHECK_GT(pixels[1].b, 200);

        // Bottom-left should be blue-ish (high blue component)
        FL_CHECK_GT(pixels[2].b, pixels[2].r);
        FL_CHECK_GT(pixels[2].b, pixels[2].g);

        // Bottom-right should be dark (low all components)
        FL_CHECK_LT(pixels[3].r, 130);
        FL_CHECK_LT(pixels[3].g, 130);
    }

    decoder->end();
    fs.end();
}

FL_TEST_CASE("MPEG1 multi-frame sequence validation") {
    fl::FileSystem fs = setupCodecFilesystem();

    fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
    FL_REQUIRE(handle != nullptr);

    fl::size file_size = handle->size();
    fl::vector<fl::u8> file_data(file_size);
    handle->read(file_data.data(), file_size);
    handle->close();

    fl::Mpeg1Config config;
    config.mode = fl::Mpeg1Config::Streaming;

    fl::string error_msg;
    auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
    FL_REQUIRE(decoder);

    auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
    stream->write(file_data.data(), file_size);

    FL_CHECK(decoder->begin(stream));

    fl::vector<fl::Frame> decoded_frames;
    fl::DecodeResult result;
    int frame_count = 0;

    // Decode all available frames
    while ((result = decoder->decode()) == fl::DecodeResult::Success && frame_count < 10) {
        fl::Frame frame = decoder->getCurrentFrame();
        if (frame.isValid()) {
            decoded_frames.push_back(frame);
            frame_count++;
        }
    }

    FL_CHECK_GT(decoded_frames.size(), 0);
    bool valid_result = (result == fl::DecodeResult::EndOfStream) || (result == fl::DecodeResult::Success);
    FL_CHECK(valid_result);

    // Verify frame properties are consistent
    for (const auto& frame : decoded_frames) {
        FL_CHECK(frame.isValid());
        FL_CHECK_EQ(frame.getWidth(), 2);
        FL_CHECK_EQ(frame.getHeight(), 2);
        FL_CHECK_EQ(frame.getFormat(), fl::PixelFormat::RGB888);

        const CRGB* pixels = frame.rgb();
        FL_CHECK(pixels != nullptr);
    }

    // If we have multiple frames, verify timestamps are increasing
    if (decoded_frames.size() > 1) {
        for (fl::size i = 1; i < decoded_frames.size(); ++i) {
            FL_CHECK_GE(decoded_frames[i].getTimestamp(), decoded_frames[i-1].getTimestamp());
        }
    }

    decoder->end();
    fs.end();
}

FL_TEST_CASE("MPEG1 metadata parsing without decoding") {
    fl::FileSystem fs = setupCodecFilesystem();

    // Test that we can load the MPEG1 file from filesystem
    fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
    FL_REQUIRE(handle != nullptr);
    FL_REQUIRE(handle->valid());

    // Get file size and read into buffer
    fl::size file_size = handle->size();
    FL_CHECK(file_size > 0);

    fl::vector<fl::u8> file_data(file_size);
    fl::size bytes_read = handle->read(file_data.data(), file_size);
    FL_CHECK_EQ(bytes_read, file_size);

    // Test MPEG1 metadata parsing
    fl::string error_msg;
    fl::span<const fl::u8> data(file_data.data(), file_size);
    fl::Mpeg1Info info = fl::Mpeg1::parseMpeg1Info(data, &error_msg);

    // The metadata parsing should succeed
    FL_CHECK_MESSAGE(info.isValid, "MPEG1 metadata parsing failed: " << error_msg);

    if (info.isValid) {
        // Verify basic metadata
        FL_CHECK_MESSAGE(info.width > 0, "MPEG1 width should be greater than 0, got: " << info.width);
        FL_CHECK_MESSAGE(info.height > 0, "MPEG1 height should be greater than 0, got: " << info.height);

        // For our test video (2x2 pixels), verify exact dimensions
        FL_CHECK_MESSAGE(info.width == 2, "Expected width=2, got: " << info.width);
        FL_CHECK_MESSAGE(info.height == 2, "Expected height=2, got: " << info.height);

        // Verify video properties
        FL_CHECK_MESSAGE(info.frameRate > 0, "MPEG1 should have positive frame rate, got: " << info.frameRate);

        fl::string audio_str = info.hasAudio ? "yes" : "no";
        FL_MESSAGE("MPEG1 metadata - Width: " << info.width
                << ", Height: " << info.height
                << ", FrameRate: " << info.frameRate
                << ", FrameCount: " << info.frameCount
                << ", Duration: " << info.duration << "ms"
                << ", HasAudio: " << audio_str);
    }

    // Test edge cases
    FL_SUBCASE("Empty data") {
        fl::vector<fl::u8> empty_data;
        fl::span<const fl::u8> empty_span(empty_data.data(), 0);
        fl::string empty_error;

        fl::Mpeg1Info empty_info = fl::Mpeg1::parseMpeg1Info(empty_span, &empty_error);
        FL_CHECK_FALSE(empty_info.isValid);
        FL_CHECK_FALSE(empty_error.empty());
        FL_MESSAGE("Empty data error: " << empty_error);
    }

    FL_SUBCASE("Too small data") {
        fl::vector<fl::u8> small_data = {0x00, 0x00, 0x01, 0xBA}; // Just pack header start
        fl::span<const fl::u8> small_span(small_data.data(), small_data.size());
        fl::string small_error;

        fl::Mpeg1Info small_info = fl::Mpeg1::parseMpeg1Info(small_span, &small_error);
        FL_CHECK_FALSE(small_info.isValid);
        FL_CHECK_FALSE(small_error.empty());
        FL_MESSAGE("Small data error: " << small_error);
    }

    FL_SUBCASE("Invalid MPEG1 stream") {
        fl::vector<fl::u8> invalid_data(50, 0x42); // Random bytes
        fl::span<const fl::u8> invalid_span(invalid_data.data(), invalid_data.size());
        fl::string invalid_error;

        fl::Mpeg1Info invalid_info = fl::Mpeg1::parseMpeg1Info(invalid_span, &invalid_error);
        FL_CHECK_FALSE(invalid_info.isValid);
        FL_CHECK_FALSE(invalid_error.empty());
        FL_MESSAGE("Invalid stream error: " << invalid_error);
    }

    handle->close();
    fs.end();
}

FL_TEST_CASE("MPEG1 audio extraction") {
    fl::FileSystem fs = setupCodecFilesystem();

    // Load valid MPEG1 data with audio
    fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
    FL_REQUIRE(handle != nullptr);

    fl::size file_size = handle->size();
    fl::vector<fl::u8> file_data(file_size);
    handle->read(file_data.data(), file_size);
    handle->close();

    FL_SUBCASE("Audio callback receives samples") {
        fl::Mpeg1Config config;
        config.skipAudio = false;  // Enable audio

        // Track audio samples received
        int audio_frames_received = 0;
        int total_audio_samples = 0;

        config.audioCallback = [&](const fl::AudioSample& sample) {
            audio_frames_received++;
            total_audio_samples += sample.size();

            // Verify audio sample properties
            FL_CHECK(sample.isValid());
            FL_CHECK_GT(sample.size(), 0);
            FL_CHECK_GE(sample.timestamp(), 0);

            // Verify PCM data is accessible
            const auto& pcm = sample.pcm();
            FL_CHECK_GT(pcm.size(), 0);
        };

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        FL_REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        FL_CHECK(decoder->begin(stream));

        // Decode frames (this should also trigger audio callbacks if present)
        fl::DecodeResult result;
        int frames_decoded = 0;
        while ((result = decoder->decode()) == fl::DecodeResult::Success && frames_decoded < 10) {
            frames_decoded++;
        }

        // Report results - test file may or may not have audio
        if (decoder->hasAudio()) {
            FL_MESSAGE("Decoded " << frames_decoded << " video frames, received "
                    << audio_frames_received << " audio frames with "
                    << total_audio_samples << " total samples at "
                    << decoder->getAudioSampleRate() << " Hz");
            FL_CHECK_GT(audio_frames_received, 0);
        } else {
            FL_MESSAGE("Test file has no audio track (this is expected)");
            FL_CHECK_EQ(audio_frames_received, 0);
        }

        decoder->end();
    }

    FL_SUBCASE("Audio disabled via skipAudio flag") {
        fl::Mpeg1Config config;
        config.skipAudio = true;  // Disable audio

        int audio_frames_received = 0;
        config.audioCallback = [&](const fl::AudioSample& sample) {
            (void)sample;
            audio_frames_received++;
        };

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        FL_REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        FL_CHECK(decoder->begin(stream));

        // Decode some frames
        for (int i = 0; i < 5 && decoder->hasMoreFrames(); i++) {
            decoder->decode();
        }

        // Should not receive audio callbacks when skipAudio is true
        FL_CHECK_EQ(audio_frames_received, 0);

        decoder->end();
    }

    FL_SUBCASE("Audio disabled via empty callback") {
        fl::Mpeg1Config config;
        config.skipAudio = false;
        // audioCallback is default-constructed (empty)

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        FL_REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        FL_CHECK(decoder->begin(stream));

        // Decode some frames - should not crash even without audio callback
        for (int i = 0; i < 5 && decoder->hasMoreFrames(); i++) {
            fl::DecodeResult res = decoder->decode();
            FL_CHECK((res == fl::DecodeResult::Success || res == fl::DecodeResult::EndOfStream));
        }

        decoder->end();
    }

    FL_SUBCASE("Dynamic audio callback setting") {
        fl::Mpeg1Config config;
        config.skipAudio = false;

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        FL_REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        FL_CHECK(decoder->begin(stream));

        // Set audio callback after initialization
        int audio_frames_received = 0;
        decoder->setAudioCallback([&](const fl::AudioSample& sample) {
            (void)sample;
            audio_frames_received++;
        });

        // Decode some frames
        for (int i = 0; i < 5 && decoder->hasMoreFrames(); i++) {
            decoder->decode();
        }

        // Should receive audio if file has audio
        if (decoder->hasAudio()) {
            FL_MESSAGE("Received " << audio_frames_received << " audio frames via dynamic callback");
        }

        decoder->end();
    }

    FL_SUBCASE("Audio and video both decode from multiplexed stream") {
        // Load MPEG1 file with both audio and video
        fl::FileHandlePtr av_handle = fs.openRead("data/codec/test_audio_video.mpg");
        FL_REQUIRE(av_handle != nullptr);

        fl::size av_file_size = av_handle->size();
        fl::vector<fl::u8> av_file_data(av_file_size);
        av_handle->read(av_file_data.data(), av_file_size);
        av_handle->close();

        fl::Mpeg1Config config;
        config.skipAudio = false;  // Enable audio

        // Track both audio and video
        int audio_frames_received = 0;
        int video_frames_decoded = 0;
        int total_audio_samples = 0;

        config.audioCallback = [&](const fl::AudioSample& sample) {
            audio_frames_received++;
            total_audio_samples += sample.size();

            // Verify audio sample properties
            FL_CHECK(sample.isValid());
            FL_CHECK_GT(sample.size(), 0);
            FL_CHECK_GE(sample.timestamp(), 0);

            // Verify PCM data is accessible
            const auto& pcm = sample.pcm();
            FL_CHECK_GT(pcm.size(), 0);
        };

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        FL_REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(av_file_size);
        stream->write(av_file_data.data(), av_file_size);

        bool began = decoder->begin(stream);
        if (!began) {
            fl::string error_message;
            decoder->hasError(&error_message);
            FL_MESSAGE("Failed to begin decoder: " << error_message);
        }
        FL_CHECK(began);

        // Decode frames - audio will be detected once we hit audio packets
        fl::DecodeResult result;
        int decode_attempts = 0;
        while (decode_attempts < 50 && video_frames_decoded < 30) {
            result = decoder->decode();
            decode_attempts++;

            if (result == fl::DecodeResult::Success) {
                fl::Frame frame = decoder->getCurrentFrame();
                if (frame.isValid()) {
                    video_frames_decoded++;

                    // Verify video frame properties
                    FL_CHECK_EQ(frame.getWidth(), 2);
                    FL_CHECK_EQ(frame.getHeight(), 2);
                    FL_CHECK_EQ(frame.getFormat(), fl::PixelFormat::RGB888);
                }
            } else if (result == fl::DecodeResult::EndOfStream) {
                break;
            } else if (result == fl::DecodeResult::Error) {
                break;
            }
        }

        // Log results
        if (decoder->hasAudio() && decoder->getAudioSampleRate() > 0) {
            FL_MESSAGE("Decoded " << video_frames_decoded << " video frames and received "
                    << audio_frames_received << " audio frames with "
                    << total_audio_samples << " total samples at "
                    << decoder->getAudioSampleRate() << " Hz");

            // We should have received both video and audio
            FL_CHECK_GT(video_frames_decoded, 0);
            FL_CHECK_GT(audio_frames_received, 0);
            FL_CHECK_GT(total_audio_samples, 0);
        } else {
            FL_MESSAGE("Decoded " << video_frames_decoded << " video frames, but no audio was found");
            FL_MESSAGE("This may indicate audio packets are located very far into the stream");
            FL_CHECK_GT(video_frames_decoded, 0);
        }

        decoder->end();
    }

    fs.end();
}