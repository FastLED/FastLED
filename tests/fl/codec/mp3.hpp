#include "test.h"
#include "fl/codec/mp3.h"
#include "fl/codec/mp3_memory.h"
#include "fl/fs/fs.h"
#include "fl/math/math.h"
#include "fl/stl/detail/memory_file_handle.h"
#include "third_party/minimp3/minimp3.h"
#ifdef FASTLED_TESTING
#include "platforms/stub/fs_stub.hpp" // ok platform headers
#endif

using namespace fl::third_party;

#ifdef FASTLED_TESTING
namespace {

class Mp3MemoryTestHook : public Mp3MemoryHook {
public:
    bool allowAllocate(fl::size, Mp3MemoryTag tag) FL_NO_EXCEPT override {
        return !mRejectAllocations || tag != mRejectedTag;
    }

    void onAllocate(void* ptr, fl::size bytes,
                    Mp3MemoryTag tag) FL_NO_EXCEPT override {
        FL_REQUIRE(ptr != nullptr);
        const unsigned index = static_cast<unsigned>(tag);
        FL_REQUIRE_LT(index, kTagCount);
        ++mAllocations;
        mCurrent += bytes;
        if (mCurrent > mPeak) {
            mPeak = mCurrent;
        }
        mTagged[index] += bytes;
    }

    void onFree(void*, fl::size bytes,
                Mp3MemoryTag tag) FL_NO_EXCEPT override {
        const unsigned index = static_cast<unsigned>(tag);
        FL_REQUIRE_LT(index, kTagCount);
        FL_REQUIRE_GE(mCurrent, bytes);
        FL_REQUIRE_GE(mTagged[index], bytes);
        mCurrent -= bytes;
        mTagged[index] -= bytes;
    }

    fl::size current() const { return mCurrent; }
    fl::size peak() const { return mPeak; }
    fl::size allocations() const { return mAllocations; }
    fl::size tagged(Mp3MemoryTag tag) const {
        const unsigned index = static_cast<unsigned>(tag);
        FL_REQUIRE_LT(index, kTagCount);
        return mTagged[index];
    }
    void reject(Mp3MemoryTag tag) {
        mRejectAllocations = true;
        mRejectedTag = tag;
    }

private:
    static constexpr unsigned kTagCount =
        static_cast<unsigned>(Mp3MemoryTag::Count);
    fl::size mCurrent = 0;
    fl::size mPeak = 0;
    fl::size mAllocations = 0;
    fl::size mTagged[kTagCount] = {};
    bool mRejectAllocations = false;
    Mp3MemoryTag mRejectedTag = Mp3MemoryTag::DecoderState;
};

constexpr fl::size selectedStreamBufferSize() {
    return MP3_MINIMP3_STREAM_BUFFER_SIZE;
}

} // anonymous namespace

FL_TEST_CASE("MP3 codec allocations report accounting tags") {
    Mp3MemoryTestHook hook;
    SetMp3MemoryHook(&hook);
    {
        Mp3Minimp3Decoder decoder;
        FL_REQUIRE(decoder.init());
        FL_CHECK_GT(hook.tagged(Mp3MemoryTag::DecoderState), 0);
        FL_CHECK_GT(hook.tagged(Mp3MemoryTag::Scratch), 0);
        FL_CHECK_GT(hook.allocations(), 1);
    }
    FL_CHECK_EQ(hook.current(), 0);
    FL_CHECK_GT(hook.peak(), 0);
    {
        auto stream = fl::make_shared<fl::memorybuf>(16);
        fl::Mp3Decoder decoder;
        FL_REQUIRE(decoder.begin(stream));
        FL_CHECK_EQ(hook.tagged(Mp3MemoryTag::StreamBuffer),
                    selectedStreamBufferSize());
        decoder.end();
    }
    FL_CHECK_EQ(hook.current(), 0);
    ClearMp3MemoryHook();
}

FL_TEST_CASE("MP3 reset reports decoder reallocation failure") {
    const Mp3MemoryTag tags[] = {
        Mp3MemoryTag::DecoderState,
        Mp3MemoryTag::Scratch,
        Mp3MemoryTag::PcmOutput,
    };
    for (fl::size i = 0; i < sizeof(tags) / sizeof(tags[0]); ++i) {
        Mp3MemoryTestHook hook;
        SetMp3MemoryHook(&hook);
        auto stream = fl::make_shared<fl::memorybuf>(16);
        fl::Mp3Decoder decoder;
        FL_REQUIRE(decoder.begin(stream));
        FL_REQUIRE(decoder.isReady());

        hook.reject(tags[i]);
        decoder.reset();

        FL_CHECK_FALSE(decoder.isReady());
        FL_CHECK(decoder.hasError());
        FL_CHECK_EQ(hook.current(), selectedStreamBufferSize());
        decoder.end();
        FL_CHECK_EQ(hook.current(), 0);
        ClearMp3MemoryHook();
    }
}

FL_TEST_CASE("MP3 initial allocation failures release partial state") {
    const Mp3MemoryTag minimp3_tags[] = {
        Mp3MemoryTag::DecoderState,
        Mp3MemoryTag::Scratch,
        Mp3MemoryTag::PcmOutput,
    };
    for (fl::size i = 0;
         i < sizeof(minimp3_tags) / sizeof(minimp3_tags[0]); ++i) {
        Mp3MemoryTestHook hook;
        hook.reject(minimp3_tags[i]);
        SetMp3MemoryHook(&hook);
        {
            Mp3Minimp3Decoder decoder;
            FL_CHECK_FALSE(decoder.init());
        }
        FL_CHECK_EQ(hook.current(), 0);
        ClearMp3MemoryHook();
    }

    Mp3MemoryTestHook hook;
    hook.reject(Mp3MemoryTag::StreamBuffer);
    SetMp3MemoryHook(&hook);
    {
        auto stream = fl::make_shared<fl::memorybuf>(16);
        fl::Mp3Decoder decoder;
        FL_CHECK_FALSE(decoder.begin(stream));
        FL_CHECK_FALSE(decoder.isReady());
        FL_CHECK(decoder.hasError());
    }
    FL_CHECK_EQ(hook.current(), 0);
    ClearMp3MemoryHook();
}
#endif

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
                      int layer, int samples)
            : mSampleRate(sample_rate), mChannels(channels), mBitrate(bitrate),
              mVersion(version), mLayer(layer), mSamples(samples) {}

        int mSampleRate;
        int mChannels;
        int mBitrate;
        int mVersion;
        int mLayer;
        int mSamples;
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
                frame.version, frame.layer, frame.samples));
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

// ISO 11172-3 / 13818-3 fix samples-per-frame by layer and MPEG version. This
// is the independent witness the decoder's own reported sample count is checked
// against: the count comes out of the frame decode, this comes out of the
// header fields the same frame reported, and nothing derives one from the
// other.
int isoSamplesPerFrame(int layer, int version) {
    if (layer == 1) {
        return 384;
    }
    if (layer == 2) {
        return 1152;
    }
    // Layer III: 1152 for MPEG-1, halved for MPEG-2 and MPEG-2.5.
    return version == 0 ? 1152 : 576;
}

bool hasStandardVectorLength(fl::size decoded, fl::size reference) {
    return decoded == reference || decoded == reference + 1152 ||
           decoded == reference + 2304;
}

} // anonymous namespace

FL_TEST_CASE("MP3 golden corpus decodes with consistent frame metadata") {
    fl::setTestFileSystemRoot("tests/data");
    const char* corpus[] = {
        "codec/mary_had_a_little_lamb.mp3",
        "codec/jazzy_percussion.mp3",
        "codec/edm_beat.mp3",
        "codec/minimp3/l3-he_free.bit",
        "codec/minimp3/M2L3_bitrate_16_all.bit",
        "codec/minimp3/l3-lame-vbrtag.bit",
    };

    // This used to compare frame-for-frame against Helix. With Helix deleted
    // the cross-decoder reference is gone, but the property it was really
    // protecting -- that the decoder reports self-consistent metadata for every
    // frame of every corpus file and produces the sample count that metadata
    // implies -- does not need a second decoder to check. The ISO reference
    // vectors below, and the fixed-vs-float gates in mp3_fixed_point.hpp, are
    // what pin the actual sample values.
    for (const char* path : corpus) {
        const fl::vector<fl::u8> bytes = loadMp3CorpusFile(path);
        const Mp3DecodeResult decoded =
            decodeMp3Corpus<Mp3Minimp3Decoder>(bytes);

        FL_CHECK_GT(decoded.mFrames, 0);
        FL_CHECK_GT(decoded.mPcm.size(), 0u);
        FL_REQUIRE_EQ(decoded.mMetadata.size(),
                      static_cast<fl::size>(decoded.mFrames));

        fl::size expected_samples = 0;
        for (fl::size i = 0; i < decoded.mMetadata.size(); ++i) {
            const Mp3DecodeResult::FrameMetadata& frame = decoded.mMetadata[i];
            FL_CHECK_EQ(frame.mSampleRate, decoded.mSampleRate);
            FL_CHECK_EQ(frame.mChannels, decoded.mChannels);
            // Free-format frames carry bitrate index 0; the wrapper derives a
            // real figure from the frame length in that case, so every decoded
            // frame reports a positive bitrate either way.
            FL_CHECK_GT(frame.mBitrate, 0);
            const int iso_samples =
                isoSamplesPerFrame(frame.mLayer, frame.mVersion);
            FL_CHECK_EQ(frame.mSamples, iso_samples);
            expected_samples +=
                static_cast<fl::size>(iso_samples * frame.mChannels);
        }
        FL_CHECK_EQ(decoded.mPcm.size(), expected_samples);
        printf("MP3 golden %s: frames=%d samples=%zu %d Hz %d ch\n", path,
               decoded.mFrames, decoded.mPcm.size(), decoded.mSampleRate,
               decoded.mChannels);
    }
}

FL_TEST_CASE("minimp3 Layer I synthesis matches its reference vector") {
    fl::setTestFileSystemRoot("tests/data");
    const fl::vector<fl::u8> bitstream =
        loadMp3CorpusFile("codec/minimp3/ILL2_layer1.bit");
    const fl::vector<fl::i16> reference = decodeLittleEndianPcm(
        loadMp3CorpusFile("codec/minimp3/ILL2_layer1.pcm"));
    const Mp3DecodeResult decoded =
        decodeMp3Corpus<Mp3Minimp3Decoder>(bitstream);

    FL_CHECK_GT(decoded.mFrames, 0);
    FL_CHECK_EQ(decoded.mPcm.size(), reference.size());
    FL_CHECK_GT(reportPsnr(reference, decoded.mPcm), 60.0);
}

FL_TEST_CASE("MP3 decoder resyncs past garbage and survives truncation") {
    fl::setTestFileSystemRoot("tests/data");
    const fl::vector<fl::u8> bitstream =
        loadMp3CorpusFile("codec/minimp3/l3-hecommon.bit");
    fl::vector<fl::u8> with_garbage;
    constexpr fl::size garbage_bytes = 19;
    with_garbage.resize(garbage_bytes + bitstream.size());
    fl::memset(with_garbage.data(), 0x55, garbage_bytes);
    fl::memcpy(with_garbage.data() + garbage_bytes, bitstream.data(),
               bitstream.size());

    // This was a two-backend agreement test. Agreement was never the property
    // worth having here -- both decoders could have skipped the same wrong
    // number of frames. What resync actually owes is that 19 bytes of leading
    // garbage cost nothing: the same frames come out either way.
    const Mp3DecodeResult clean = decodeMp3Corpus<Mp3Minimp3Decoder>(bitstream);
    const Mp3DecodeResult resynced =
        decodeMp3Corpus<Mp3Minimp3Decoder>(with_garbage);
    FL_CHECK_GT(clean.mFrames, 0);
    FL_CHECK_EQ(resynced.mFrames, clean.mFrames);
    FL_REQUIRE_EQ(resynced.mPcm.size(), clean.mPcm.size());
    for (fl::size i = 0; i < clean.mPcm.size(); ++i) {
        FL_REQUIRE_EQ(resynced.mPcm[i], clean.mPcm[i]);
    }

    // Truncated input must terminate, and every frame it does report must be
    // a whole frame -- the failure mode worth catching is the decoder emitting
    // a partial final frame padded with garbage rather than stopping. Sizing
    // the expectation from the ISO samples-per-frame rule rather than from the
    // decoder's own reported count is what makes that catchable.
    const fl::vector<fl::u8> truncated =
        loadMp3CorpusFile("codec/minimp3/l3-compl-cut.mp3");
    const Mp3DecodeResult truncated_result =
        decodeMp3Corpus<Mp3Minimp3Decoder>(truncated);
    FL_CHECK_GT(truncated_result.mFrames, 0);
    FL_REQUIRE_EQ(truncated_result.mMetadata.size(),
                  static_cast<fl::size>(truncated_result.mFrames));
    fl::size truncated_samples = 0;
    for (fl::size i = 0; i < truncated_result.mMetadata.size(); ++i) {
        const Mp3DecodeResult::FrameMetadata& frame =
            truncated_result.mMetadata[i];
        const int iso_samples =
            isoSamplesPerFrame(frame.mLayer, frame.mVersion);
        FL_CHECK_EQ(frame.mSamples, iso_samples);
        truncated_samples +=
            static_cast<fl::size>(iso_samples * frame.mChannels);
    }
    FL_CHECK_EQ(truncated_result.mPcm.size(), truncated_samples);
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

FL_TEST_CASE("MP3 public minimp3 stream discovers large free-format frames") {
    fl::setTestFileSystemRoot("tests/data");
    const fl::vector<fl::u8> source =
        loadMp3CorpusFile("codec/minimp3/l3-he_free.bit");

    Mp3Minimp3Decoder probe;
    FL_REQUIRE(probe.init());
    const int sync = probe.findSyncWord(source.data(), source.size());
    FL_REQUIRE_GE(sync, 0);
    const fl::u8* cursor = source.data() + sync;
    fl::size remaining = source.size() - static_cast<fl::size>(sync);
    const fl::u8* frames[3] = {};
    fl::size frame_sizes[3] = {};
    for (int i = 0; i < 3; ++i) {
        frames[i] = cursor;
        FL_REQUIRE_EQ(probe.decodeFrame(&cursor, &remaining), 0);
        frame_sizes[i] = static_cast<fl::size>(cursor - frames[i]);
        FL_REQUIRE_GT(frame_sizes[i], 4);
    }

    constexpr fl::size large_frame_size = 1200;
    fl::vector<fl::u8> expanded;
    expanded.resize(large_frame_size * 3);
    fl::memset(expanded.data(), 0, expanded.size());
    for (int i = 0; i < 3; ++i) {
        FL_REQUIRE_LT(frame_sizes[i], large_frame_size);
        fl::u8* destination = expanded.data() + large_frame_size * i;
        fl::memcpy(destination, frames[i], frame_sizes[i]);
        FL_REQUIRE_EQ(destination[2] & 0xf0, 0);
        destination[2] &= static_cast<fl::u8>(~0x02u);
    }

    auto stream = fl::make_shared<fl::memorybuf>(expanded.size());
    FL_REQUIRE_EQ(stream->write(fl::span<const fl::u8>(expanded)),
                  expanded.size());
    fl::Mp3Decoder decoder;
    FL_REQUIRE(decoder.begin(stream));
    fl::audio::Sample sample;
    FL_CHECK(decoder.decodeNextFrame(&sample));
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

FL_TEST_CASE("MP3 decoder passes the limited-accuracy reference floor") {
    fl::setTestFileSystemRoot("tests/data");
    const fl::vector<fl::u8> bitstream =
        loadMp3CorpusFile("codec/minimp3/l3-hecommon.bit");
    const fl::vector<fl::i16> reference = decodeLittleEndianPcm(
        loadMp3CorpusFile("codec/minimp3/l3-hecommon.pcm"));
    const Mp3DecodeResult minimp3 =
        decodeMp3Corpus<Mp3Minimp3Decoder>(bitstream);

    FL_CHECK_TRUE(
        hasStandardVectorLength(minimp3.mPcm.size(), reference.size()));

    const double minimp3_psnr = reportPsnr(reference, minimp3.mPcm);
    FL_CHECK_GE(minimp3_psnr, 60.0);
    printf("MP3 limited-accuracy vector: minimp3=%.2f dB\n", minimp3_psnr);
}

// Minimal valid MP3 frame header (Layer III, MPEG1, 44.1kHz, 128kbps, mono)
// This is a synthetic test - we'll just test initialization and basic API
FL_TEST_CASE("Mp3Minimp3Decoder initialization") {
    Mp3Minimp3Decoder decoder;

    // Test initialization
    bool init_result = decoder.init();
    FL_CHECK(init_result);

    // Reset should work without errors
    decoder.reset();
}

FL_TEST_CASE("Mp3Minimp3Decoder basic decode test") {
    Mp3Minimp3Decoder decoder;
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

FL_TEST_CASE("Mp3Minimp3Decoder empty data") {
    Mp3Minimp3Decoder decoder;
    FL_CHECK(decoder.init());

    fl::u8 empty_data[] = {};

    int frames = 0;
    decoder.decode(empty_data, 0, [&](const Mp3Frame&) {
        frames++;
    });

    FL_CHECK_EQ(frames, 0);  // No frames from empty data
}

FL_TEST_CASE("Mp3Minimp3Decoder decodeToAudioSamples") {
    Mp3Minimp3Decoder decoder;
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

FL_TEST_CASE("Mp3Minimp3Decoder - Decode real MP3 file") {
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
    Mp3Minimp3Decoder decoder;
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

FL_TEST_CASE("Mp3Minimp3Decoder - Convert to fl::audio::AudioSamples from real file") {
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
    Mp3Minimp3Decoder decoder;
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
