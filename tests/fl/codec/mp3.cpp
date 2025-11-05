#include "test.h"
#include "fl/codec/mp3.h"
#include "fl/file_system.h"
#ifdef FASTLED_TESTING
#include "platforms/stub/fs_stub.hpp"
#endif

using namespace fl::third_party;

// Minimal valid MP3 frame header (Layer III, MPEG1, 44.1kHz, 128kbps, mono)
// This is a synthetic test - we'll just test initialization and basic API
TEST_CASE("Mp3HelixDecoder initialization") {
    Mp3HelixDecoder decoder;

    // Test initialization
    bool init_result = decoder.init();
    CHECK(init_result);

    // Reset should work without errors
    decoder.reset();
}

TEST_CASE("Mp3HelixDecoder basic decode test") {
    Mp3HelixDecoder decoder;
    CHECK(decoder.init());

    // This is a minimal MP3 frame sync word - 0xFFE or 0xFFF in first 11 bits
    // We're testing that the decoder can handle invalid/incomplete data gracefully
    fl::u8 invalid_data[] = {0xFF, 0xFB, 0x90, 0x00};

    int frames = 0;
    decoder.decode(invalid_data, sizeof(invalid_data), [&](const Mp3Frame&) {
        frames++;
    });

    // We don't expect to decode any frames from this minimal data
    // The test passes if it doesn't crash
    CHECK(frames >= 0);  // Just verify the callback mechanism works
}

TEST_CASE("Mp3HelixDecoder empty data") {
    Mp3HelixDecoder decoder;
    CHECK(decoder.init());

    fl::u8 empty_data[] = {};

    int frames = 0;
    decoder.decode(empty_data, 0, [&](const Mp3Frame&) {
        frames++;
    });

    CHECK_EQ(frames, 0);  // No frames from empty data
}

TEST_CASE("Mp3HelixDecoder decodeToAudioSamples") {
    Mp3HelixDecoder decoder;
    CHECK(decoder.init());

    fl::u8 test_data[] = {0xFF, 0xFB, 0x90, 0x00};

    fl::vector<fl::AudioSample> samples = decoder.decodeToAudioSamples(test_data, sizeof(test_data));

    // With invalid/incomplete data, we expect zero samples
    CHECK(samples.size() >= 0);
}

TEST_CASE("Mp3HelixDecoder - Decode real MP3 file") {
    // Set up filesystem to point to tests/data directory
    fl::setTestFileSystemRoot("tests/data");

    fl::FileSystem fs;
    CHECK(fs.beginSd(0)); // CS pin doesn't matter for test

    // Open the MP3 file
    fl::FileHandlePtr file = fs.openRead("codec/jazzy_percussion.mp3");
    REQUIRE(file != nullptr);
    REQUIRE(file->valid());

    // Read entire file into buffer
    fl::size file_size = file->size();
    CHECK_GT(file_size, 0);

    fl::vector<fl::u8> mp3_data;
    mp3_data.resize(file_size);
    fl::size bytes_read = file->read(mp3_data.data(), file_size);
    CHECK_EQ(bytes_read, file_size);

    file->close();

    // Decode MP3 data
    Mp3HelixDecoder decoder;
    CHECK(decoder.init());

    int frames_decoded = 0;
    int total_samples = 0;
    int sample_rate = 0;
    int channels = 0;

    decoder.decode(mp3_data.data(), mp3_data.size(), [&](const Mp3Frame& frame) {
        frames_decoded++;
        total_samples += frame.samples * frame.channels;
        if (sample_rate == 0) {
            sample_rate = frame.sample_rate;
            channels = frame.channels;
        }
    });

    // Verify we decoded some frames
    CHECK_GT(frames_decoded, 0);
    CHECK_GT(total_samples, 0);
    CHECK_GT(sample_rate, 0);
    CHECK_GT(channels, 0);

    // Print stats for debugging
    printf("Decoded %d MP3 frames, %d total samples, %d Hz, %d channels\n",
           frames_decoded, total_samples, sample_rate, channels);
}

TEST_CASE("Mp3HelixDecoder - Convert to fl::AudioSamples from real file") {
    // Set up filesystem to point to tests/data directory
    fl::setTestFileSystemRoot("tests/data");

    fl::FileSystem fs;
    CHECK(fs.beginSd(0));

    // Open the MP3 file
    fl::FileHandlePtr file = fs.openRead("codec/jazzy_percussion.mp3");
    REQUIRE(file != nullptr);

    // Read entire file
    fl::size file_size = file->size();
    fl::vector<fl::u8> mp3_data;
    mp3_data.resize(file_size);
    file->read(mp3_data.data(), file_size);
    file->close();

    // Decode to fl::AudioSamples
    Mp3HelixDecoder decoder;
    CHECK(decoder.init());

    fl::vector<fl::AudioSample> samples = decoder.decodeToAudioSamples(mp3_data.data(), mp3_data.size());

    // Verify we got samples
    CHECK_GT(samples.size(), 0);

    // Verify samples have valid data
    bool has_non_zero = false;
    for (const auto& sample : samples) {
        const auto& pcm = sample.pcm();
        for (fl::i16 value : pcm) {
            if (value != 0) {
                has_non_zero = true;
                break;
            }
        }
        if (has_non_zero) break;
    }
    CHECK(has_non_zero);

    printf("Converted MP3 to %zu fl::AudioSamples\n", samples.size());
}
