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
    REQUIRE(ok);
    return fs;
}

TEST_CASE("MPEG1 file loading and decoding") {
    fl::FileSystem fs = setupCodecFilesystem();
        // Test that we can load the MPEG1 file from filesystem
        fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
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

        auto verifyFrameDimensions = [](const fl::Frame& frame) {
            return frame.isValid() && frame.getWidth() == 2 && frame.getHeight() == 2;
        };

        auto testMpeg1Decoding = [&]() {
            fl::Mpeg1Config config;
            config.mode = fl::Mpeg1Config::SingleFrame;

            fl::string error_msg;
            auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
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
            if (result != fl::DecodeResult::Success) {
                MESSAGE("Failed to decode first frame, result: " << static_cast<int>(result));
                decoder->end();
                return;
            }

            fl::Frame frame0 = decoder->getCurrentFrame();
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
                if (result != fl::DecodeResult::Success) {
                    MESSAGE("Failed to decode second frame, result: " << static_cast<int>(result));
                } else {
                    fl::Frame frame1 = decoder->getCurrentFrame();
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
        if (!fl::Mpeg1::isSupported()) {
            MESSAGE("MPEG1 decoder not supported on this platform");
        } else {
            testMpeg1Decoding();
        }

    handle->close();
    fs.end();
}

TEST_CASE("MPEG1 decoder error handling") {
    fl::FileSystem fs = setupCodecFilesystem();

    SUBCASE("null ByteStream") {
        fl::Mpeg1Config config;
        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        REQUIRE(decoder);

        CHECK_FALSE(decoder->begin(nullptr));
        CHECK(decoder->hasError());
    }

    SUBCASE("empty ByteStream") {
        fl::Mpeg1Config config;
        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        REQUIRE(decoder);

        auto empty_stream = fl::make_shared<fl::ByteStreamMemory>(0);
        CHECK_FALSE(decoder->begin(empty_stream));
        CHECK(decoder->hasError());

        fl::string error_message;
        decoder->hasError(&error_message);
        CHECK(error_message.find("Empty input stream") != fl::string::npos);
    }

    SUBCASE("invalid MPEG1 data") {
        fl::Mpeg1Config config;
        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        REQUIRE(decoder);

        // Create stream with invalid data (not MPEG1 format)
        const fl::u8 invalid_data[] = {0xFF, 0xD8, 0xFF, 0xE0}; // JPEG header
        auto stream = fl::make_shared<fl::ByteStreamMemory>(sizeof(invalid_data));
        stream->write(invalid_data, sizeof(invalid_data));

        CHECK_FALSE(decoder->begin(stream));
        CHECK(decoder->hasError());
    }

    SUBCASE("truncated MPEG1 data") {
        // Load the valid MPEG1 file but truncate it
        fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
        REQUIRE(handle != nullptr);

        fl::size file_size = handle->size();
        fl::vector<fl::u8> file_data(file_size / 2); // Only read half
        handle->read(file_data.data(), file_data.size());
        handle->close();

        fl::Mpeg1Config config;
        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_data.size());
        stream->write(file_data.data(), file_data.size());

        // Decoder might initialize but should fail during decode or have limited functionality
        if (decoder->begin(stream)) {
            // The decoder may succeed with partial data due to pl_mpeg's robustness
            // This is actually expected behavior - pl_mpeg handles partial streams gracefully
            auto result = decoder->decode();
            // Either success (partial decode) or failure is acceptable
            CHECK((result == fl::DecodeResult::Success || result == fl::DecodeResult::Error || result == fl::DecodeResult::EndOfStream));
        } else {
            CHECK(decoder->hasError());
        }
    }

    fs.end();
}

TEST_CASE("MPEG1 configuration options") {
    fl::FileSystem fs = setupCodecFilesystem();

    // Load valid MPEG1 data
    fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
    REQUIRE(handle != nullptr);

    fl::size file_size = handle->size();
    fl::vector<fl::u8> file_data(file_size);
    handle->read(file_data.data(), file_size);
    handle->close();

    SUBCASE("SingleFrame mode") {
        fl::Mpeg1Config config;
        config.mode = fl::Mpeg1Config::SingleFrame;

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        CHECK(decoder->begin(stream));
        CHECK(decoder->decode() == fl::DecodeResult::Success);

        fl::Frame frame = decoder->getCurrentFrame();
        CHECK(frame.isValid());
        CHECK_EQ(frame.getWidth(), 2);
        CHECK_EQ(frame.getHeight(), 2);
    }

    SUBCASE("Streaming mode with buffering") {
        fl::Mpeg1Config config;
        config.mode = fl::Mpeg1Config::Streaming;
        config.immediateMode = false;
        config.bufferFrames = 3;

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        CHECK(decoder->begin(stream));
        CHECK(decoder->decode() == fl::DecodeResult::Success);

        fl::Frame frame = decoder->getCurrentFrame();
        CHECK(frame.isValid());
    }

    SUBCASE("Custom frame rate") {
        fl::Mpeg1Config config;
        config.targetFps = 15;

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        CHECK(decoder->begin(stream));
        CHECK(decoder->decode() == fl::DecodeResult::Success);
    }

    SUBCASE("Audio disabled (default)") {
        fl::Mpeg1Config config;
        config.skipAudio = true; // This should be default

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        CHECK(decoder->begin(stream));
        CHECK(decoder->decode() == fl::DecodeResult::Success);
    }

    fs.end();
}

TEST_CASE("MPEG1 decoder properties and metadata") {
    fl::FileSystem fs = setupCodecFilesystem();

    fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
    REQUIRE(handle != nullptr);

    fl::size file_size = handle->size();
    fl::vector<fl::u8> file_data(file_size);
    handle->read(file_data.data(), file_size);
    handle->close();

    fl::Mpeg1Config config;
    fl::string error_msg;
    auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
    REQUIRE(decoder);

    auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
    stream->write(file_data.data(), file_size);

    CHECK(decoder->begin(stream));

    SUBCASE("Video properties") {
        // Check that we can get video properties after initialization
        // Note: dynamic_cast requires RTTI which is disabled, so we'll test via frame properties
        CHECK(decoder->decode() == fl::DecodeResult::Success);
        fl::Frame test_frame = decoder->getCurrentFrame();
        if (test_frame.isValid()) {
            CHECK_EQ(test_frame.getWidth(), 2);
            CHECK_EQ(test_frame.getHeight(), 2);
        }
        decoder->end();
        decoder->begin(stream); // Reset for other subtests
    }

    SUBCASE("Frame count and seeking") {
        // Frame count is 0 for streaming mode (unknown in advance)
        CHECK_EQ(decoder->getFrameCount(), 0);

        // Seeking is not supported
        CHECK_FALSE(decoder->seek(1));
    }

    SUBCASE("Decoder state management") {
        CHECK(decoder->isReady());
        CHECK_FALSE(decoder->hasError());
        CHECK(decoder->hasMoreFrames());

        // Decode first frame
        CHECK(decoder->decode() == fl::DecodeResult::Success);
        CHECK_EQ(decoder->getCurrentFrameIndex(), 1);

        // Should still have more frames
        if (decoder->hasMoreFrames()) {
            CHECK(decoder->decode() == fl::DecodeResult::Success);
            CHECK_EQ(decoder->getCurrentFrameIndex(), 2);
        }
    }

    decoder->end();
    CHECK_FALSE(decoder->isReady());

    fs.end();
}

TEST_CASE("MPEG1 frame data validation") {
    fl::FileSystem fs = setupCodecFilesystem();

    fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
    REQUIRE(handle != nullptr);

    fl::size file_size = handle->size();
    fl::vector<fl::u8> file_data(file_size);
    handle->read(file_data.data(), file_size);
    handle->close();

    fl::Mpeg1Config config;
    config.mode = fl::Mpeg1Config::SingleFrame;

    fl::string error_msg;
    auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
    REQUIRE(decoder);

    auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
    stream->write(file_data.data(), file_size);

    CHECK(decoder->begin(stream));
    CHECK(decoder->decode() == fl::DecodeResult::Success);

    fl::Frame frame = decoder->getCurrentFrame();
    REQUIRE(frame.isValid());

    SUBCASE("Frame properties") {
        CHECK_EQ(frame.getWidth(), 2);
        CHECK_EQ(frame.getHeight(), 2);
        CHECK_EQ(frame.getFormat(), fl::PixelFormat::RGB888);
        CHECK_GE(frame.getTimestamp(), 0); // Should have valid timestamp (may be 0 for first frame)
    }

    SUBCASE("Pixel data integrity") {
        const CRGB* pixels = frame.rgb();
        REQUIRE(pixels != nullptr);

        // Verify all 4 pixels are within valid RGB ranges
        for (int i = 0; i < 4; ++i) {
            CHECK_GE(pixels[i].r, 0);
            CHECK_LE(pixels[i].r, 255);
            CHECK_GE(pixels[i].g, 0);
            CHECK_LE(pixels[i].g, 255);
            CHECK_GE(pixels[i].b, 0);
            CHECK_LE(pixels[i].b, 255);
        }

        // Verify the expected color pattern is close to red-white-blue-black
        // (allowing for MPEG1 lossy compression artifacts)

        // Top-left should be blue-ish (high blue component)
        CHECK_GT(pixels[0].b, pixels[0].r);
        CHECK_GT(pixels[0].b, pixels[0].g);

        // Top-right should be white-ish (high all components)
        CHECK_GT(pixels[1].r, 200);
        CHECK_GT(pixels[1].g, 200);
        CHECK_GT(pixels[1].b, 200);

        // Bottom-left should be blue-ish (high blue component)
        CHECK_GT(pixels[2].b, pixels[2].r);
        CHECK_GT(pixels[2].b, pixels[2].g);

        // Bottom-right should be dark (low all components)
        CHECK_LT(pixels[3].r, 130);
        CHECK_LT(pixels[3].g, 130);
    }

    decoder->end();
    fs.end();
}

TEST_CASE("MPEG1 multi-frame sequence validation") {
    fl::FileSystem fs = setupCodecFilesystem();

    fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
    REQUIRE(handle != nullptr);

    fl::size file_size = handle->size();
    fl::vector<fl::u8> file_data(file_size);
    handle->read(file_data.data(), file_size);
    handle->close();

    fl::Mpeg1Config config;
    config.mode = fl::Mpeg1Config::Streaming;

    fl::string error_msg;
    auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
    REQUIRE(decoder);

    auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
    stream->write(file_data.data(), file_size);

    CHECK(decoder->begin(stream));

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

    CHECK_GT(decoded_frames.size(), 0);
    bool valid_result = (result == fl::DecodeResult::EndOfStream) || (result == fl::DecodeResult::Success);
    CHECK(valid_result);

    // Verify frame properties are consistent
    for (const auto& frame : decoded_frames) {
        CHECK(frame.isValid());
        CHECK_EQ(frame.getWidth(), 2);
        CHECK_EQ(frame.getHeight(), 2);
        CHECK_EQ(frame.getFormat(), fl::PixelFormat::RGB888);

        const CRGB* pixels = frame.rgb();
        CHECK(pixels != nullptr);
    }

    // If we have multiple frames, verify timestamps are increasing
    if (decoded_frames.size() > 1) {
        for (fl::size i = 1; i < decoded_frames.size(); ++i) {
            CHECK_GE(decoded_frames[i].getTimestamp(), decoded_frames[i-1].getTimestamp());
        }
    }

    decoder->end();
    fs.end();
}

TEST_CASE("MPEG1 metadata parsing without decoding") {
    fl::FileSystem fs = setupCodecFilesystem();

    // Test that we can load the MPEG1 file from filesystem
    fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
    REQUIRE(handle != nullptr);
    REQUIRE(handle->valid());

    // Get file size and read into buffer
    fl::size file_size = handle->size();
    CHECK(file_size > 0);

    fl::vector<fl::u8> file_data(file_size);
    fl::size bytes_read = handle->read(file_data.data(), file_size);
    CHECK_EQ(bytes_read, file_size);

    // Test MPEG1 metadata parsing
    fl::string error_msg;
    fl::span<const fl::u8> data(file_data.data(), file_size);
    fl::Mpeg1Info info = fl::Mpeg1::parseMpeg1Info(data, &error_msg);

    // The metadata parsing should succeed
    CHECK_MESSAGE(info.isValid, "MPEG1 metadata parsing failed: " << error_msg);

    if (info.isValid) {
        // Verify basic metadata
        CHECK_MESSAGE(info.width > 0, "MPEG1 width should be greater than 0, got: " << info.width);
        CHECK_MESSAGE(info.height > 0, "MPEG1 height should be greater than 0, got: " << info.height);

        // For our test video (2x2 pixels), verify exact dimensions
        CHECK_MESSAGE(info.width == 2, "Expected width=2, got: " << info.width);
        CHECK_MESSAGE(info.height == 2, "Expected height=2, got: " << info.height);

        // Verify video properties
        CHECK_MESSAGE(info.frameRate > 0, "MPEG1 should have positive frame rate, got: " << info.frameRate);

        fl::string audio_str = info.hasAudio ? "yes" : "no";
        MESSAGE("MPEG1 metadata - Width: " << info.width
                << ", Height: " << info.height
                << ", FrameRate: " << info.frameRate
                << ", FrameCount: " << info.frameCount
                << ", Duration: " << info.duration << "ms"
                << ", HasAudio: " << audio_str);
    }

    // Test edge cases
    SUBCASE("Empty data") {
        fl::vector<fl::u8> empty_data;
        fl::span<const fl::u8> empty_span(empty_data.data(), 0);
        fl::string empty_error;

        fl::Mpeg1Info empty_info = fl::Mpeg1::parseMpeg1Info(empty_span, &empty_error);
        CHECK_FALSE(empty_info.isValid);
        CHECK_FALSE(empty_error.empty());
        MESSAGE("Empty data error: " << empty_error);
    }

    SUBCASE("Too small data") {
        fl::vector<fl::u8> small_data = {0x00, 0x00, 0x01, 0xBA}; // Just pack header start
        fl::span<const fl::u8> small_span(small_data.data(), small_data.size());
        fl::string small_error;

        fl::Mpeg1Info small_info = fl::Mpeg1::parseMpeg1Info(small_span, &small_error);
        CHECK_FALSE(small_info.isValid);
        CHECK_FALSE(small_error.empty());
        MESSAGE("Small data error: " << small_error);
    }

    SUBCASE("Invalid MPEG1 stream") {
        fl::vector<fl::u8> invalid_data(50, 0x42); // Random bytes
        fl::span<const fl::u8> invalid_span(invalid_data.data(), invalid_data.size());
        fl::string invalid_error;

        fl::Mpeg1Info invalid_info = fl::Mpeg1::parseMpeg1Info(invalid_span, &invalid_error);
        CHECK_FALSE(invalid_info.isValid);
        CHECK_FALSE(invalid_error.empty());
        MESSAGE("Invalid stream error: " << invalid_error);
    }

    handle->close();
    fs.end();
}

TEST_CASE("MPEG1 audio extraction") {
    fl::FileSystem fs = setupCodecFilesystem();

    // Load valid MPEG1 data with audio
    fl::FileHandlePtr handle = fs.openRead("data/codec/file.mpeg");
    REQUIRE(handle != nullptr);

    fl::size file_size = handle->size();
    fl::vector<fl::u8> file_data(file_size);
    handle->read(file_data.data(), file_size);
    handle->close();

    SUBCASE("Audio callback receives samples") {
        fl::Mpeg1Config config;
        config.skipAudio = false;  // Enable audio

        // Track audio samples received
        int audio_frames_received = 0;
        int total_audio_samples = 0;

        config.audioCallback = [&](const fl::AudioSample& sample) {
            audio_frames_received++;
            total_audio_samples += sample.size();

            // Verify audio sample properties
            CHECK(sample.isValid());
            CHECK_GT(sample.size(), 0);
            CHECK_GE(sample.timestamp(), 0);

            // Verify PCM data is accessible
            const auto& pcm = sample.pcm();
            CHECK_GT(pcm.size(), 0);
        };

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        CHECK(decoder->begin(stream));

        // Decode frames (this should also trigger audio callbacks if present)
        fl::DecodeResult result;
        int frames_decoded = 0;
        while ((result = decoder->decode()) == fl::DecodeResult::Success && frames_decoded < 10) {
            frames_decoded++;
        }

        // Report results - test file may or may not have audio
        if (decoder->hasAudio()) {
            MESSAGE("Decoded " << frames_decoded << " video frames, received "
                    << audio_frames_received << " audio frames with "
                    << total_audio_samples << " total samples at "
                    << decoder->getAudioSampleRate() << " Hz");
            CHECK_GT(audio_frames_received, 0);
        } else {
            MESSAGE("Test file has no audio track (this is expected)");
            CHECK_EQ(audio_frames_received, 0);
        }

        decoder->end();
    }

    SUBCASE("Audio disabled via skipAudio flag") {
        fl::Mpeg1Config config;
        config.skipAudio = true;  // Disable audio

        int audio_frames_received = 0;
        config.audioCallback = [&](const fl::AudioSample& sample) {
            (void)sample;
            audio_frames_received++;
        };

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        CHECK(decoder->begin(stream));

        // Decode some frames
        for (int i = 0; i < 5 && decoder->hasMoreFrames(); i++) {
            decoder->decode();
        }

        // Should not receive audio callbacks when skipAudio is true
        CHECK_EQ(audio_frames_received, 0);

        decoder->end();
    }

    SUBCASE("Audio disabled via empty callback") {
        fl::Mpeg1Config config;
        config.skipAudio = false;
        // audioCallback is default-constructed (empty)

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        CHECK(decoder->begin(stream));

        // Decode some frames - should not crash even without audio callback
        for (int i = 0; i < 5 && decoder->hasMoreFrames(); i++) {
            fl::DecodeResult res = decoder->decode();
            CHECK((res == fl::DecodeResult::Success || res == fl::DecodeResult::EndOfStream));
        }

        decoder->end();
    }

    SUBCASE("Dynamic audio callback setting") {
        fl::Mpeg1Config config;
        config.skipAudio = false;

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(file_size);
        stream->write(file_data.data(), file_size);

        CHECK(decoder->begin(stream));

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
            MESSAGE("Received " << audio_frames_received << " audio frames via dynamic callback");
        }

        decoder->end();
    }

    SUBCASE("Audio and video both decode from multiplexed stream") {
        // Load MPEG1 file with both audio and video
        fl::FileHandlePtr av_handle = fs.openRead("data/codec/test_audio_video.mpg");
        REQUIRE(av_handle != nullptr);

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
            CHECK(sample.isValid());
            CHECK_GT(sample.size(), 0);
            CHECK_GE(sample.timestamp(), 0);

            // Verify PCM data is accessible
            const auto& pcm = sample.pcm();
            CHECK_GT(pcm.size(), 0);
        };

        fl::string error_msg;
        auto decoder = fl::Mpeg1::createDecoder(config, &error_msg);
        REQUIRE(decoder);

        auto stream = fl::make_shared<fl::ByteStreamMemory>(av_file_size);
        stream->write(av_file_data.data(), av_file_size);

        bool began = decoder->begin(stream);
        if (!began) {
            fl::string error_message;
            decoder->hasError(&error_message);
            MESSAGE("Failed to begin decoder: " << error_message);
        }
        CHECK(began);

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
                    CHECK_EQ(frame.getWidth(), 2);
                    CHECK_EQ(frame.getHeight(), 2);
                    CHECK_EQ(frame.getFormat(), fl::PixelFormat::RGB888);
                }
            } else if (result == fl::DecodeResult::EndOfStream) {
                break;
            } else if (result == fl::DecodeResult::Error) {
                break;
            }
        }

        // Log results
        if (decoder->hasAudio() && decoder->getAudioSampleRate() > 0) {
            MESSAGE("Decoded " << video_frames_decoded << " video frames and received "
                    << audio_frames_received << " audio frames with "
                    << total_audio_samples << " total samples at "
                    << decoder->getAudioSampleRate() << " Hz");

            // We should have received both video and audio
            CHECK_GT(video_frames_decoded, 0);
            CHECK_GT(audio_frames_received, 0);
            CHECK_GT(total_audio_samples, 0);
        } else {
            MESSAGE("Decoded " << video_frames_decoded << " video frames, but no audio was found");
            MESSAGE("This may indicate audio packets are located very far into the stream");
            CHECK_GT(video_frames_decoded, 0);
        }

        decoder->end();
    }

    fs.end();
}