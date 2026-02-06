/// @file vorbis.cpp
/// @brief Unit tests for stb_vorbis decoder integration

#include "test.h"
#include "fl/codec/vorbis.h"
#include "fl/bytestream.h"

// Include low-level stb_vorbis API for pushdata and FILE tests
#include "third_party/stb/stb_vorbis.h"

#ifndef FL_STB_VORBIS_NO_STDIO
#include "fl/stl/detail/file_io.h"
#endif

using namespace fl;

// Test audio file: 1 second 440Hz sine wave at 8kHz mono
// OGG file created with: ffmpeg -f lavfi -i "sine=frequency=440:duration=1" -ar 8000 -ac 1 -c:a libvorbis -q:a 0 test_audio.ogg
static const char* TEST_AUDIO_PATH = "tests/fl/data/test_audio.ogg";

// Helper function to load test audio file into memory
static fl::vector<fl::u8> loadTestAudioFile() {
    fl::FILE* f = fl::fopen(TEST_AUDIO_PATH, "rb");
    if (!f) {
        return fl::vector<fl::u8>();
    }

    // Get file size
    fl::fseek(f, 0, fl::io::seek_end);
    long fileSize = fl::ftell(f);
    fl::fseek(f, 0, fl::io::seek_set);

    if (fileSize <= 0) {
        fl::fclose(f);
        return fl::vector<fl::u8>();
    }

    // Read file into vector
    fl::vector<fl::u8> data(static_cast<fl::size>(fileSize));
    fl::size bytesRead = fl::fread(data.data(), 1, static_cast<fl::size>(fileSize), f);
    fl::fclose(f);

    if (bytesRead != static_cast<fl::size>(fileSize)) {
        return fl::vector<fl::u8>();
    }

    return data;
}

TEST_CASE("Vorbis - factory creation") {
    // Test that the factory can create a decoder
    fl::string error;
    VorbisDecoderPtr decoder = Vorbis::createDecoder(&error);
    FL_CHECK(decoder != nullptr);
    FL_CHECK(error.empty());
}

TEST_CASE("Vorbis - isSupported") {
    // stb_vorbis is always supported
    FL_CHECK(Vorbis::isSupported());
}

TEST_CASE("Vorbis - decoder initial state") {
    VorbisDecoderPtr decoder = Vorbis::createDecoder();
    FL_REQUIRE(decoder != nullptr);

    // Decoder should not be ready before begin() is called
    FL_CHECK_FALSE(decoder->isReady());

    // Should not have an error in initial state
    FL_CHECK_FALSE(decoder->hasError());
}

TEST_CASE("Vorbis - parseVorbisInfo with invalid data") {
    // Test error handling with invalid data (not valid Vorbis)
    fl::u8 invalidData[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
    fl::string error;

    VorbisInfo info = Vorbis::parseVorbisInfo(
        fl::span<const fl::u8>(invalidData, sizeof(invalidData)),
        &error
    );

    // Should fail with invalid data
    FL_CHECK_FALSE(info.isValid);
    FL_CHECK_FALSE(error.empty());
}

TEST_CASE("Vorbis - decodeAll with invalid data") {
    // Test error handling with invalid data
    fl::u8 invalidData[] = { 0xFF, 0xFE, 0xFD, 0xFC };
    fl::string error;

    fl::vector<AudioSample> samples = Vorbis::decodeAll(
        fl::span<const fl::u8>(invalidData, sizeof(invalidData)),
        &error
    );

    // Should return empty vector for invalid data
    FL_CHECK(samples.empty());
    FL_CHECK_FALSE(error.empty());
}

TEST_CASE("Vorbis - StbVorbisDecoder low-level API") {
    StbVorbisDecoder decoder;

    // Initial state
    FL_CHECK_FALSE(decoder.isOpen());
    FL_CHECK_EQ(decoder.getTotalSamples(), 0);
    FL_CHECK_EQ(decoder.getSampleOffset(), 0);

    // Opening invalid data should fail
    fl::u8 invalidData[] = { 0xAB, 0xCD, 0xEF };
    FL_CHECK_FALSE(decoder.openMemory(invalidData));
    FL_CHECK_FALSE(decoder.isOpen());

    // getInfo on closed decoder should return invalid info
    VorbisInfo info = decoder.getInfo();
    FL_CHECK_FALSE(info.isValid);
}

TEST_CASE("Vorbis - VorbisInfo default construction") {
    VorbisInfo info;

    FL_CHECK_EQ(info.sampleRate, 0);
    FL_CHECK_EQ(info.channels, 0);
    FL_CHECK_EQ(info.totalSamples, 0);
    FL_CHECK_EQ(info.maxFrameSize, 0);
    FL_CHECK_FALSE(info.isValid);
}

TEST_CASE("Vorbis - VorbisInfo parameterized construction") {
    VorbisInfo info(44100, 2);

    FL_CHECK_EQ(info.sampleRate, 44100);
    FL_CHECK_EQ(info.channels, 2);
    FL_CHECK(info.isValid);
}

TEST_CASE("Vorbis - decoder seek on closed decoder") {
    StbVorbisDecoder decoder;

    // Seek on closed decoder should fail gracefully
    FL_CHECK_FALSE(decoder.seek(0));
    FL_CHECK_FALSE(decoder.seek(1000));
}

TEST_CASE("Vorbis - decoder getSamples on closed decoder") {
    StbVorbisDecoder decoder;

    // getSamplesShortInterleaved on closed decoder should return 0
    fl::i16 buffer[256];
    fl::i32 result = decoder.getSamplesShortInterleaved(2, buffer, 256);
    FL_CHECK_EQ(result, 0);
}

TEST_CASE("Vorbis - VorbisDecoder with null stream") {
    VorbisDecoder decoder;

    // begin with null stream should fail
    FL_CHECK_FALSE(decoder.begin(nullptr));
    FL_CHECK_FALSE(decoder.isReady());
    FL_CHECK(decoder.hasError());

    fl::string errorMsg;
    FL_CHECK(decoder.hasError(&errorMsg));
    FL_CHECK_FALSE(errorMsg.empty());
}

TEST_CASE("Vorbis - decode OGG file and verify metadata") {
    // Load test audio data from file
    fl::vector<fl::u8> oggData = loadTestAudioFile();
    FL_REQUIRE(!oggData.empty());

    fl::string error;
    VorbisInfo info = Vorbis::parseVorbisInfo(oggData, &error);

    FL_REQUIRE(info.isValid);
    FL_CHECK_EQ(info.sampleRate, 8000);  // 8kHz as created
    FL_CHECK_EQ(info.channels, 1);        // mono
    // Total samples should be approximately 8000 (1 second at 8kHz)
    FL_CHECK(info.totalSamples >= 7500);
    FL_CHECK(info.totalSamples <= 8500);
}

TEST_CASE("Vorbis - decode and verify 440Hz sine wave") {
    // Load test audio data from file (440Hz sine wave at 8kHz)
    fl::vector<fl::u8> oggData = loadTestAudioFile();
    FL_REQUIRE(!oggData.empty());

    // Decode the OGG file using our stb_vorbis decoder
    StbVorbisDecoder decoder;
    FL_REQUIRE(decoder.openMemory(oggData));
    FL_REQUIRE(decoder.isOpen());

    VorbisInfo info = decoder.getInfo();
    FL_REQUIRE(info.isValid);
    FL_CHECK_EQ(info.channels, 1);
    FL_CHECK_EQ(info.sampleRate, 8000);

    // Decode all samples to i16
    fl::vector<fl::i16> decodedSamples;
    constexpr fl::size CHUNK_SIZE = 1024;
    fl::i16 buffer[CHUNK_SIZE];

    while (true) {
        fl::i32 samplesRead = decoder.getSamplesShortInterleaved(1, buffer, CHUNK_SIZE);
        if (samplesRead == 0) break;
        for (fl::i32 i = 0; i < samplesRead; ++i) {
            decodedSamples.push_back(buffer[i]);
        }
    }

    FL_REQUIRE(decodedSamples.size() >= 7000);  // At least most of 1 second at 8kHz

    // Validate sine wave characteristics
    // 440Hz at 8kHz sample rate = period of ~18.18 samples
    // Expected period range allowing for Vorbis compression artifacts
    constexpr fl::i32 MIN_EXPECTED_AMPLITUDE = 1000;   // Should be well above noise floor (Vorbis is lossy)
    constexpr fl::i32 MAX_EXPECTED_AMPLITUDE = 32767;  // Max i16 value

    // Find peak amplitude
    fl::i32 maxAmplitude = 0;
    for (fl::size i = 0; i < decodedSamples.size(); ++i) {
        fl::i32 absValue = decodedSamples[i];
        if (absValue < 0) absValue = -absValue;
        if (absValue > maxAmplitude) maxAmplitude = absValue;
    }

    // Verify reasonable amplitude for a sine wave
    FL_CHECK(maxAmplitude >= MIN_EXPECTED_AMPLITUDE);
    FL_CHECK(maxAmplitude <= MAX_EXPECTED_AMPLITUDE);

    // Count zero crossings to estimate frequency
    // For 440Hz at 8kHz, expect ~880 zero crossings per second (2 per period)
    fl::size zeroCrossings = 0;
    for (fl::size i = 1; i < decodedSamples.size(); ++i) {
        if ((decodedSamples[i-1] <= 0 && decodedSamples[i] > 0) ||
            (decodedSamples[i-1] >= 0 && decodedSamples[i] < 0)) {
            ++zeroCrossings;
        }
    }

    // Expect approximately 880 zero crossings for 1 second of 440Hz sine wave
    // Allow generous tolerance for Vorbis compression artifacts
    FL_CHECK(zeroCrossings >= 700);   // Lower bound
    FL_CHECK(zeroCrossings <= 1100);  // Upper bound
}

TEST_CASE("Vorbis - decodeAll convenience function") {
    // Load test audio data from file
    fl::vector<fl::u8> oggData = loadTestAudioFile();
    FL_REQUIRE(!oggData.empty());

    fl::string error;
    fl::vector<AudioSample> samples = Vorbis::decodeAll(oggData, &error);

    FL_REQUIRE(error.empty());
    FL_REQUIRE(!samples.empty());

    // Each AudioSample contains a chunk of decoded audio
    // Verify we got some audio data
    fl::size totalSamples = 0;
    for (const auto& sample : samples) {
        totalSamples += sample.size();
    }

    // Should have approximately 8000 samples (1 second at 8kHz)
    FL_CHECK(totalSamples >= 7000);
    FL_CHECK(totalSamples <= 9000);
}

#ifndef FL_STB_VORBIS_NO_PUSHDATA_API
TEST_CASE("Vorbis - pushdata streaming mode") {
    // Test the pushdata (streaming buffer callback) mode
    // This mode allows decoding from a stream of buffers without needing the entire file in memory

    using namespace fl::third_party::vorbis;

    // Load test audio data from file
    fl::vector<fl::u8> oggData = loadTestAudioFile();
    FL_REQUIRE(!oggData.empty());

    // Open decoder in pushdata mode
    int32_t error = 0;
    int32_t used = 0;
    stb_vorbis* v = stb_vorbis_open_pushdata(
        oggData.data(),
        static_cast<int32_t>(oggData.size()),
        &used,
        &error,
        nullptr
    );

    FL_REQUIRE(v != nullptr);
    FL_CHECK_EQ(error, 0);
    FL_CHECK(used > 0);  // Should have consumed some bytes

    // Get stream info
    stb_vorbis_info info = stb_vorbis_get_info(v);
    FL_CHECK_EQ(info.sample_rate, 8000);
    FL_CHECK_EQ(info.channels, 1);

    // Decode frames in pushdata mode
    fl::vector<float> decodedSamples;
    const fl::u8* dataPtr = oggData.data() + used;
    int32_t dataRemaining = static_cast<int32_t>(oggData.size()) - used;

    while (dataRemaining > 0) {
        int32_t channels = 0;
        float** output = nullptr;
        int32_t samplesOut = 0;

        int32_t bytesUsed = stb_vorbis_decode_frame_pushdata(
            v,
            dataPtr,
            dataRemaining,
            &channels,
            &output,
            &samplesOut
        );

        // Collect samples from this frame
        if (samplesOut > 0 && output != nullptr) {
            FL_CHECK_EQ(channels, 1);  // Mono
            for (int32_t i = 0; i < samplesOut; ++i) {
                decodedSamples.push_back(output[0][i]);
            }
        }

        // No bytes consumed means end of stream or need more data
        if (bytesUsed == 0) {
            break;
        }

        dataPtr += bytesUsed;
        dataRemaining -= bytesUsed;
    }

    // Close decoder
    stb_vorbis_close(v);

    // Verify we decoded some samples
    FL_CHECK(!decodedSamples.empty());
    FL_CHECK(decodedSamples.size() >= 7000);
    FL_CHECK(decodedSamples.size() <= 9000);

    // Verify sample values are in valid range (float: -1.0 to 1.0)
    for (float sample : decodedSamples) {
        FL_CHECK(sample >= -1.0f);
        FL_CHECK(sample <= 1.0f);
    }
}

TEST_CASE("Vorbis - pushdata flush") {
    // Test flush operation in pushdata mode
    using namespace fl::third_party::vorbis;

    // Load test audio data from file
    fl::vector<fl::u8> oggData = loadTestAudioFile();
    FL_REQUIRE(!oggData.empty());

    int32_t error = 0;
    int32_t used = 0;
    stb_vorbis* v = stb_vorbis_open_pushdata(
        oggData.data(),
        static_cast<int32_t>(oggData.size()),
        &used,
        &error,
        nullptr
    );

    FL_REQUIRE(v != nullptr);

    // Flush should reset internal state for seeking
    stb_vorbis_flush_pushdata(v);

    // After flush, we can start decoding from the beginning again
    const fl::u8* dataPtr = oggData.data();
    int32_t dataRemaining = static_cast<int32_t>(oggData.size());
    int32_t channels = 0;
    float** output = nullptr;
    int32_t samplesOut = 0;

    int32_t bytesUsed = stb_vorbis_decode_frame_pushdata(
        v,
        dataPtr,
        dataRemaining,
        &channels,
        &output,
        &samplesOut
    );

    // Should successfully decode after flush
    FL_CHECK(bytesUsed >= 0);

    stb_vorbis_close(v);
}
#endif // !FL_STB_VORBIS_NO_PUSHDATA_API

#ifndef FL_STB_VORBIS_NO_STDIO
TEST_CASE("Vorbis - FILE-based decoding") {
    // Test decoding from fl::FILE*
    using namespace fl::third_party::vorbis;

    // Verify the file exists (it's in the repo)
    fl::FILE* f = fl::fopen(TEST_AUDIO_PATH, "rb");
    FL_REQUIRE(f != nullptr);

    // Open decoder from FILE*
    int32_t error = 0;
    stb_vorbis* v = stb_vorbis_open_file(f, true, &error, nullptr);

    FL_REQUIRE(v != nullptr);
    FL_CHECK_EQ(error, 0);

    // Get stream info
    stb_vorbis_info info = stb_vorbis_get_info(v);
    FL_CHECK_EQ(info.sample_rate, 8000);
    FL_CHECK_EQ(info.channels, 1);

    // Get total samples
    uint32_t totalSamples = stb_vorbis_stream_length_in_samples(v);
    FL_CHECK(totalSamples >= 7500);
    FL_CHECK(totalSamples <= 8500);

    // Decode some samples
    fl::i16 buffer[1024];
    int32_t samplesRead = stb_vorbis_get_samples_short_interleaved(v, 1, buffer, 1024);
    FL_CHECK(samplesRead > 0);
    FL_CHECK(samplesRead <= 1024);

    // Close decoder (will also close file since close_handle_on_close was true)
    stb_vorbis_close(v);
}

TEST_CASE("Vorbis - FILE-based seeking") {
    // Test seeking with FILE-based decoder
    using namespace fl::third_party::vorbis;

    fl::FILE* f = fl::fopen(TEST_AUDIO_PATH, "rb");
    FL_REQUIRE(f != nullptr);

    int32_t error = 0;
    stb_vorbis* v = stb_vorbis_open_file(f, true, &error, nullptr);
    FL_REQUIRE(v != nullptr);

    // Seek to middle of file
    uint32_t totalSamples = stb_vorbis_stream_length_in_samples(v);
    uint32_t midPoint = totalSamples / 2;

    int32_t seekResult = stb_vorbis_seek(v, midPoint);
    FL_CHECK(seekResult != 0);  // Non-zero = success

    // Verify we're at approximately the right position
    uint32_t currentPos = stb_vorbis_get_sample_offset(v);
    // Allow some tolerance due to frame boundaries
    FL_CHECK(currentPos >= midPoint - 1000);
    FL_CHECK(currentPos <= midPoint + 1000);

    // Seek back to start
    seekResult = stb_vorbis_seek_start(v);
    FL_CHECK(seekResult != 0);

    currentPos = stb_vorbis_get_sample_offset(v);
    FL_CHECK(currentPos < 1000);  // Should be near start

    stb_vorbis_close(v);
}

TEST_CASE("Vorbis - FILE section decoding") {
    // Test stb_vorbis_open_file_section with limited length
    using namespace fl::third_party::vorbis;

    fl::FILE* f = fl::fopen(TEST_AUDIO_PATH, "rb");
    FL_REQUIRE(f != nullptr);

    // Get file size
    fl::fseek(f, 0, fl::io::seek_end);
    long fileSize = fl::ftell(f);
    fl::fseek(f, 0, fl::io::seek_set);

    FL_REQUIRE(fileSize > 0);

    // Open with limited section (full file in this case)
    int32_t error = 0;
    stb_vorbis* v = stb_vorbis_open_file_section(
        f,
        true,  // close_handle_on_close
        &error,
        nullptr,
        fileSize
    );

    FL_REQUIRE(v != nullptr);
    FL_CHECK_EQ(error, 0);

    // Verify we can decode
    fl::i16 buffer[512];
    int32_t samplesRead = stb_vorbis_get_samples_short_interleaved(v, 1, buffer, 512);
    FL_CHECK(samplesRead > 0);

    stb_vorbis_close(v);
}
#endif // !FL_STB_VORBIS_NO_STDIO
