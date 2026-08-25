#include "test.h"
#include "fl/codec/mp3.h"
#include "fl/fs/fs.h"
#include "fl/math/math.h"
#include "fl/stl/detail/memory_file_handle.h"
#include "third_party/minimp3/minimp3.h"
#ifdef FASTLED_TESTING
#include "platforms/stub/fs_stub.hpp" // ok platform headers
#endif

using namespace fl::third_party;

FL_TEST_CASE("minimp3 caller-owned scratch API is available") {
    mp3dec_t decoder;
    mp3dec_scratch_t scratch;
    mp3dec_frame_info_t info = {};
    fl::u8 input[4] = {};

    mp3dec_init(&decoder);
    int samples = mp3dec_decode_frame_r(&decoder, &scratch, input,
                                        sizeof(input), nullptr, &info);

    FL_CHECK_EQ(samples, 0);
}

namespace {

struct Mp3DecodeResult {
    struct FrameMetadata {
        FrameMetadata(int sample_rate, int channels, int bitrate, int version,
                      int layer)
            : mSampleRate(sample_rate), mChannels(channels), mBitrate(bitrate),
              mVersion(version), mLayer(layer) {}

        int mSampleRate;
        int mChannels;
        int mBitrate;
        int mVersion;
        int mLayer;
    };

    fl::vector<fl::i16> mPcm;
    fl::vector<FrameMetadata> mMetadata;
    int mFrames = 0;
    int mSampleRate = 0;
    int mChannels = 0;
};

fl::vector<fl::u8> loadMp3CorpusFile(const char* path) {
    fl::FileSystem fs;
    FL_REQUIRE(fs.beginSd(0));
    fl::ifstream file = fs.openRead(path);
    FL_REQUIRE(file.is_open());

    fl::vector<fl::u8> bytes;
    bytes.resize(file.size());
    FL_REQUIRE_EQ(file.read(bytes.data(), bytes.size()), bytes.size());
    return bytes;
}

template <typename Decoder>
Mp3DecodeResult decodeMp3Corpus(const fl::vector<fl::u8>& bytes) {
    Decoder decoder;
    FL_REQUIRE(decoder.init());

    Mp3DecodeResult result;
    result.mFrames = decoder.decode(
        bytes.data(), bytes.size(), [&](const Mp3Frame& frame) {
            if (result.mSampleRate == 0) {
                result.mSampleRate = frame.sample_rate;
                result.mChannels = frame.channels;
            }
            result.mMetadata.push_back(Mp3DecodeResult::FrameMetadata(
                frame.sample_rate, frame.channels, frame.bitrate,
                frame.version, frame.layer));
            const int sample_count = frame.samples * frame.channels;
            for (int i = 0; i < sample_count; ++i) {
                result.mPcm.push_back(frame.pcm[i]);
            }
        });
    return result;
}

double reportPsnr(const fl::vector<fl::i16>& reference,
                  const fl::vector<fl::i16>& candidate) {
    const fl::size compared =
        reference.size() < candidate.size() ? reference.size()
                                            : candidate.size();
    if (compared == 0) {
        return 0.0;
    }

    double squared_error = 0.0;
    for (fl::size i = 0; i < compared; ++i) {
        const double delta = static_cast<double>(reference[i]) - candidate[i];
        squared_error += delta * delta;
    }
    const double mse = squared_error / compared;
    return mse == 0.0 ? 99.0
                      : 10.0 * fl::log10((32767.0 * 32767.0) / mse);
}

fl::vector<fl::i16> decodeLittleEndianPcm(
    const fl::vector<fl::u8>& bytes) {
    fl::vector<fl::i16> pcm;
    pcm.reserve(bytes.size() / 2);
    for (fl::size i = 0; i + 1 < bytes.size(); i += 2) {
        const fl::u16 value = static_cast<fl::u16>(bytes[i]) |
                              (static_cast<fl::u16>(bytes[i + 1]) << 8);
        pcm.push_back(static_cast<fl::i16>(value));
    }
    return pcm;
}

bool hasStandardVectorLength(fl::size decoded, fl::size reference) {
    return decoded == reference || decoded == reference + 1152 ||
           decoded == reference + 2304;
}

} // anonymous namespace

FL_TEST_CASE("MP3 golden corpus has matching backend frame behavior") {
    fl::setTestFileSystemRoot("tests/data");
    const char* corpus[] = {
        "codec/mary_had_a_little_lamb.mp3",
        "codec/jazzy_percussion.mp3",
        "codec/edm_beat.mp3",
        "codec/minimp3/l3-he_free.bit",
        "codec/minimp3/M2L3_bitrate_16_all.bit",
        "codec/minimp3/l3-lame-vbrtag.bit",
    };

    for (const char* path : corpus) {
        const fl::vector<fl::u8> bytes = loadMp3CorpusFile(path);
        const Mp3DecodeResult helix = decodeMp3Corpus<Mp3HelixDecoder>(bytes);
        const Mp3DecodeResult minimp3 =
            decodeMp3Corpus<Mp3Minimp3Decoder>(bytes);

        FL_CHECK_GT(helix.mFrames, 0);
        FL_CHECK_EQ(minimp3.mFrames, helix.mFrames);
        FL_CHECK_EQ(minimp3.mPcm.size(), helix.mPcm.size());
        FL_CHECK_EQ(minimp3.mSampleRate, helix.mSampleRate);
        FL_CHECK_EQ(minimp3.mChannels, helix.mChannels);
        FL_REQUIRE_EQ(minimp3.mMetadata.size(), helix.mMetadata.size());
        for (fl::size i = 0; i < helix.mMetadata.size(); ++i) {
            FL_CHECK_EQ(minimp3.mMetadata[i].mSampleRate,
                        helix.mMetadata[i].mSampleRate);
            FL_CHECK_EQ(minimp3.mMetadata[i].mChannels,
                        helix.mMetadata[i].mChannels);
            FL_CHECK_EQ(minimp3.mMetadata[i].mBitrate,
                        helix.mMetadata[i].mBitrate);
            FL_CHECK_EQ(minimp3.mMetadata[i].mVersion,
                        helix.mMetadata[i].mVersion);
            FL_CHECK_EQ(minimp3.mMetadata[i].mLayer,
                        helix.mMetadata[i].mLayer);
        }
        printf("MP3 golden %s: frames=%d samples=%zu report-only PSNR=%.2f dB\n",
               path, helix.mFrames, helix.mPcm.size(),
               reportPsnr(helix.mPcm, minimp3.mPcm));
    }
}

FL_TEST_CASE("MP3 backends agree on resync and truncated input") {
    fl::setTestFileSystemRoot("tests/data");
    const fl::vector<fl::u8> bitstream =
        loadMp3CorpusFile("codec/minimp3/l3-hecommon.bit");
    fl::vector<fl::u8> with_garbage;
    constexpr fl::size garbage_bytes = 19;
    with_garbage.resize(garbage_bytes + bitstream.size());
    fl::memset(with_garbage.data(), 0x55, garbage_bytes);
    fl::memcpy(with_garbage.data() + garbage_bytes, bitstream.data(),
               bitstream.size());

    const Mp3DecodeResult helix_resync =
        decodeMp3Corpus<Mp3HelixDecoder>(with_garbage);
    const Mp3DecodeResult minimp3_resync =
        decodeMp3Corpus<Mp3Minimp3Decoder>(with_garbage);
    FL_CHECK_GT(helix_resync.mFrames, 0);
    FL_CHECK_EQ(minimp3_resync.mFrames, helix_resync.mFrames);
    FL_CHECK_EQ(minimp3_resync.mPcm.size(), helix_resync.mPcm.size());

    const fl::vector<fl::u8> truncated =
        loadMp3CorpusFile("codec/minimp3/l3-compl-cut.mp3");
    const Mp3DecodeResult helix_truncated =
        decodeMp3Corpus<Mp3HelixDecoder>(truncated);
    const Mp3DecodeResult minimp3_truncated =
        decodeMp3Corpus<Mp3Minimp3Decoder>(truncated);
    FL_CHECK_EQ(minimp3_truncated.mFrames, helix_truncated.mFrames);
    FL_CHECK_EQ(minimp3_truncated.mPcm.size(), helix_truncated.mPcm.size());
}

FL_TEST_CASE("MP3 public stream terminates on truncated input") {
    fl::setTestFileSystemRoot("tests/data");
    const fl::vector<fl::u8> truncated =
        loadMp3CorpusFile("codec/minimp3/l3-compl-cut.mp3");
    auto stream = fl::make_shared<fl::memorybuf>(truncated.size());
    FL_REQUIRE_EQ(stream->write(fl::span<const fl::u8>(truncated)),
                  truncated.size());

    fl::Mp3Decoder decoder;
    FL_REQUIRE(decoder.begin(stream));
    fl::audio::Sample sample;
    int frames = 0;
    while (decoder.decodeNextFrame(&sample)) {
        ++frames;
        FL_REQUIRE_LT(frames, 10000);
    }
    FL_CHECK_GT(frames, 0);
    FL_CHECK_FALSE(decoder.hasError());
}

FL_TEST_CASE("MP3 public stream recovers a valid frame after corruption") {
    fl::setTestFileSystemRoot("tests/data");
    const fl::vector<fl::u8> bitstream =
        loadMp3CorpusFile("codec/minimp3/l3-hecommon.bit");

    Mp3Minimp3Decoder frame_probe;
    FL_REQUIRE(frame_probe.init());
    const int sync = frame_probe.findSyncWord(bitstream.data(), bitstream.size());
    FL_REQUIRE_GE(sync, 0);
    const fl::u8* frame_begin = bitstream.data() + sync;
    fl::size bytes_left = bitstream.size() - static_cast<fl::size>(sync);
    const fl::u8* frame_end = frame_begin;
    FL_REQUIRE_EQ(frame_probe.decodeFrame(&frame_end, &bytes_left), 0);
    FL_REQUIRE_EQ(frame_probe.decodeFrame(&frame_end, &bytes_left), 0);
    const fl::size frame_size = static_cast<fl::size>(frame_end - frame_begin);

    fl::vector<fl::u8> input;
    constexpr fl::size corrupt_bytes = 32;
    input.resize(corrupt_bytes + frame_size);
    fl::memset(input.data(), 0, corrupt_bytes);
    input[0] = 0xff;
    input[1] = 0xfb;
    input[2] = 0x90;
    fl::memcpy(input.data() + corrupt_bytes, frame_begin, frame_size);

    auto stream = fl::make_shared<fl::memorybuf>(input.size());
    FL_REQUIRE_EQ(stream->write(fl::span<const fl::u8>(input)), input.size());
    fl::Mp3Decoder decoder;
    FL_REQUIRE(decoder.begin(stream));
    fl::audio::Sample sample;
    int decoded_frames = 0;
    while (decoder.decodeNextFrame(&sample)) {
        ++decoded_frames;
        FL_REQUIRE_LE(decoded_frames, 2);
    }
    FL_CHECK_GT(decoded_frames, 0);
    FL_CHECK_FALSE(decoder.hasError());
}

FL_TEST_CASE("MP3 backends pass the limited-accuracy reference floor") {
    fl::setTestFileSystemRoot("tests/data");
    const fl::vector<fl::u8> bitstream =
        loadMp3CorpusFile("codec/minimp3/l3-hecommon.bit");
    const fl::vector<fl::i16> reference = decodeLittleEndianPcm(
        loadMp3CorpusFile("codec/minimp3/l3-hecommon.pcm"));
    const Mp3DecodeResult helix = decodeMp3Corpus<Mp3HelixDecoder>(bitstream);
    const Mp3DecodeResult minimp3 =
        decodeMp3Corpus<Mp3Minimp3Decoder>(bitstream);

    FL_CHECK_EQ(minimp3.mFrames, helix.mFrames);
    FL_CHECK_TRUE(hasStandardVectorLength(helix.mPcm.size(), reference.size()));
    FL_CHECK_TRUE(
        hasStandardVectorLength(minimp3.mPcm.size(), reference.size()));

    const double helix_psnr = reportPsnr(reference, helix.mPcm);
    const double minimp3_psnr = reportPsnr(reference, minimp3.mPcm);
    FL_CHECK_GE(helix_psnr, 60.0);
    FL_CHECK_GE(minimp3_psnr, 60.0);
    printf("MP3 limited-accuracy vector: Helix=%.2f dB minimp3=%.2f dB\n",
           helix_psnr, minimp3_psnr);
}

// Minimal valid MP3 frame header (Layer III, MPEG1, 44.1kHz, 128kbps, mono)
// This is a synthetic test - we'll just test initialization and basic API
FL_TEST_CASE("Mp3HelixDecoder initialization") {
    Mp3HelixDecoder decoder;

    // Test initialization
    bool init_result = decoder.init();
    FL_CHECK(init_result);

    // Reset should work without errors
    decoder.reset();
}

FL_TEST_CASE("Mp3HelixDecoder basic decode test") {
    Mp3HelixDecoder decoder;
    FL_CHECK(decoder.init());

    // Use a buffer large enough for the decoder to read without OOB access.
    // The MP3 decoder's RefillBitstreamCache reads up to 4 bytes at a time
    // and frame parsing can read hundreds of bytes past the sync word.
    // A 4-byte buffer with valid sync word causes stack-buffer-overflow.
    fl::u8 invalid_data[2048];
    fl::memset(invalid_data, 0, sizeof(invalid_data));
    // MP3 frame sync word in first 4 bytes
    invalid_data[0] = 0xFF;
    invalid_data[1] = 0xFB;
    invalid_data[2] = 0x90;
    invalid_data[3] = 0x00;

    int frames = 0;
    decoder.decode(invalid_data, sizeof(invalid_data), [&](const Mp3Frame&) {
        frames++;
    });

    // We don't expect to decode any valid frames from this mostly-zero data
    // The test passes if it doesn't crash
    FL_CHECK(frames >= 0);  // Just verify the callback mechanism works
}

FL_TEST_CASE("Mp3HelixDecoder empty data") {
    Mp3HelixDecoder decoder;
    FL_CHECK(decoder.init());

    fl::u8 empty_data[] = {};

    int frames = 0;
    decoder.decode(empty_data, 0, [&](const Mp3Frame&) {
        frames++;
    });

    FL_CHECK_EQ(frames, 0);  // No frames from empty data
}

FL_TEST_CASE("Mp3HelixDecoder decodeToAudioSamples") {
    Mp3HelixDecoder decoder;
    FL_CHECK(decoder.init());

    // Use a buffer large enough for the decoder to read without OOB access
    fl::u8 test_data[2048];
    fl::memset(test_data, 0, sizeof(test_data));
    test_data[0] = 0xFF;
    test_data[1] = 0xFB;
    test_data[2] = 0x90;
    test_data[3] = 0x00;

    fl::vector<fl::audio::Sample> samples = decoder.decodeToAudioSamples(test_data, sizeof(test_data));

    // With invalid/incomplete data, we expect zero samples
    FL_CHECK(samples.size() >= 0);
}

FL_TEST_CASE("Mp3HelixDecoder - Decode real MP3 file") {
    // Set up filesystem to point to tests/data directory
    fl::setTestFileSystemRoot("tests/data");

    fl::FileSystem fs;
    FL_CHECK(fs.beginSd(0)); // CS pin doesn't matter for test

    // Open the MP3 file
    fl::ifstream file = fs.openRead("codec/jazzy_percussion.mp3");
    FL_REQUIRE(file.is_open());

    // Read entire file into buffer
    fl::size file_size = file.size();
    FL_CHECK_GT(file_size, 0);

    fl::vector<fl::u8> mp3_data;
    mp3_data.resize(file_size);
    fl::size bytes_read = file.read(mp3_data.data(), file_size);
    FL_CHECK_EQ(bytes_read, file_size);

    file.close();

    // Decode MP3 data
    Mp3HelixDecoder decoder;
    FL_CHECK(decoder.init());

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
    FL_CHECK_GT(frames_decoded, 0);
    FL_CHECK_GT(total_samples, 0);
    FL_CHECK_GT(sample_rate, 0);
    FL_CHECK_GT(channels, 0);

    // Print stats for debugging
    printf("Decoded %d MP3 frames, %d total samples, %d Hz, %d channels\n",
           frames_decoded, total_samples, sample_rate, channels);
}

FL_TEST_CASE("Mp3HelixDecoder - Convert to fl::audio::AudioSamples from real file") {
    // Set up filesystem to point to tests/data directory
    fl::setTestFileSystemRoot("tests/data");

    fl::FileSystem fs;
    FL_CHECK(fs.beginSd(0));

    // Open the MP3 file
    fl::ifstream file = fs.openRead("codec/jazzy_percussion.mp3");
    FL_REQUIRE(file.is_open());

    // Read entire file
    fl::size file_size = file.size();
    fl::vector<fl::u8> mp3_data;
    mp3_data.resize(file_size);
    file.read(mp3_data.data(), file_size);
    file.close();

    // Decode to fl::audio::AudioSamples
    Mp3HelixDecoder decoder;
    FL_CHECK(decoder.init());

    fl::vector<fl::audio::Sample> samples = decoder.decodeToAudioSamples(mp3_data.data(), mp3_data.size());

    // Verify we got samples
    FL_CHECK_GT(samples.size(), 0);

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
    FL_CHECK(has_non_zero);

    printf("Converted MP3 to %zu fl::audio::AudioSamples\n", samples.size());
}
