// ============================================================================
// stb_vorbis configuration for FastLED embedded targets
// Use FL_STB_* macros (translated to STB_* in third_party header)
// These MUST be defined BEFORE including the third_party header
// ============================================================================




// ============================================================================

#include "fl/codec/vorbis.h"
#include "fl/stl/cstring.h"

// Include stb_vorbis raw API (FL_STB_* macros translated to STB_* internally)
#include "third_party/stb/stb_vorbis.h"

namespace fl {

// Import stb_vorbis types and functions from nested namespace
using fl::third_party::vorbis::stb_vorbis;
using fl::third_party::vorbis::stb_vorbis_info;
using fl::third_party::vorbis::stb_vorbis_open_memory;
using fl::third_party::vorbis::stb_vorbis_close;
using fl::third_party::vorbis::stb_vorbis_get_info;
using fl::third_party::vorbis::stb_vorbis_stream_length_in_samples;
using fl::third_party::vorbis::stb_vorbis_get_samples_short_interleaved;
using fl::third_party::vorbis::stb_vorbis_get_samples_float;
using fl::third_party::vorbis::stb_vorbis_seek;
using fl::third_party::vorbis::stb_vorbis_get_sample_offset;

// NOTE: All 'int' types from stb_vorbis API must be cast to/from fl::i32
// for portability across platforms where int size may vary.

// StbVorbisDecoder implementation
StbVorbisDecoder::StbVorbisDecoder() : mVorbis(nullptr) {}

StbVorbisDecoder::~StbVorbisDecoder() {
    close();
}

bool StbVorbisDecoder::openMemory(fl::span<const fl::u8> data) {
    close();  // Close any existing stream

    fl::i32 error = 0;
    mVorbis = stb_vorbis_open_memory(data.data(), static_cast<fl::i32>(data.size()), &error, nullptr);
    return mVorbis != nullptr;
}

void StbVorbisDecoder::close() {
    if (mVorbis) {
        stb_vorbis_close(static_cast<stb_vorbis*>(mVorbis));
        mVorbis = nullptr;
    }
}

bool StbVorbisDecoder::isOpen() const {
    return mVorbis != nullptr;
}

VorbisInfo StbVorbisDecoder::getInfo() const {
    VorbisInfo info;
    if (mVorbis) {
        stb_vorbis_info vi = stb_vorbis_get_info(static_cast<stb_vorbis*>(mVorbis));
        info.sampleRate = vi.sample_rate;
        info.channels = static_cast<fl::u8>(vi.channels);
        info.maxFrameSize = static_cast<fl::u32>(vi.max_frame_size);
        info.totalSamples = stb_vorbis_stream_length_in_samples(static_cast<stb_vorbis*>(mVorbis));
        info.isValid = true;
    }
    return info;
}

fl::i32 StbVorbisDecoder::getSamplesShortInterleaved(fl::i32 channels, fl::i16* buffer, fl::i32 numShorts) {
    if (!mVorbis) return 0;
    // Cast to short* for AVR compatibility where fl::i16 is int but stb_vorbis expects short
    return static_cast<fl::i32>(stb_vorbis_get_samples_short_interleaved(
        static_cast<stb_vorbis*>(mVorbis),
        static_cast<fl::i32>(channels),
        buffer,
        static_cast<fl::i32>(numShorts)
    ));
}

fl::i32 StbVorbisDecoder::getSamplesFloat(fl::i32 channels, float** buffer, fl::i32 numSamples) {
    if (!mVorbis) return 0;
    return static_cast<fl::i32>(stb_vorbis_get_samples_float(
        static_cast<stb_vorbis*>(mVorbis),
        static_cast<fl::i32>(channels),
        buffer,
        static_cast<fl::i32>(numSamples)
    ));
}

bool StbVorbisDecoder::seek(fl::u32 sampleNumber) {
    if (!mVorbis) return false;
    return stb_vorbis_seek(static_cast<stb_vorbis*>(mVorbis), sampleNumber) != 0;
}

fl::u32 StbVorbisDecoder::getSampleOffset() const {
    if (!mVorbis) return 0;
    return static_cast<fl::u32>(stb_vorbis_get_sample_offset(static_cast<stb_vorbis*>(mVorbis)));
}

fl::u32 StbVorbisDecoder::getTotalSamples() const {
    if (!mVorbis) return 0;
    return stb_vorbis_stream_length_in_samples(static_cast<stb_vorbis*>(mVorbis));
}

// VorbisDecoderImpl - internal implementation
class VorbisDecoderImpl {
public:
    VorbisDecoderImpl();
    ~VorbisDecoderImpl();

    bool begin(fl::ByteStreamPtr stream);
    void end();
    bool isReady() const { return mDecoder.isOpen(); }
    bool hasError(fl::string* msg = nullptr) const;
    bool decodeNextFrame(AudioSample* outSample);
    fl::size getPosition() const { return mPosition; }
    void reset();
    VorbisInfo getInfo() const { return mDecoder.getInfo(); }

private:
    static constexpr fl::size FRAME_SIZE = 1024;  // Samples per frame

    StbVorbisDecoder mDecoder;
    fl::vector<fl::u8> mFileData;    // Entire file in memory (stb_vorbis requirement)
    fl::vector<fl::i16> mPcmBuffer;  // Decode buffer
    fl::string mError;
    fl::size mPosition;              // Current stream position in bytes
    bool mEndOfStream;
};

VorbisDecoderImpl::VorbisDecoderImpl() : mPosition(0), mEndOfStream(false) {
    mPcmBuffer.resize(FRAME_SIZE * 2);  // Stereo
}

VorbisDecoderImpl::~VorbisDecoderImpl() {
    end();
}

bool VorbisDecoderImpl::begin(fl::ByteStreamPtr stream) {
    end();  // Clean up any previous state

    if (!stream) {
        mError = "Null stream";
        return false;
    }

    // stb_vorbis pulldata API requires entire file in memory
    // Read entire stream into buffer
    mFileData.clear();
    fl::u8 buffer[1024];
    while (stream->available(1)) {
        fl::size bytesRead = stream->read(buffer, sizeof(buffer));
        if (bytesRead == 0) break;
        for (fl::size i = 0; i < bytesRead; ++i) {
            mFileData.push_back(buffer[i]);
        }
    }

    if (mFileData.empty()) {
        mError = "Empty stream";
        return false;
    }

    // Open decoder
    if (!mDecoder.openMemory(mFileData)) {
        mError = "Failed to decode Vorbis stream";
        return false;
    }

    mEndOfStream = false;
    mError.clear();
    return true;
}

void VorbisDecoderImpl::end() {
    mDecoder.close();
    mFileData.clear();
    mPosition = 0;
    mEndOfStream = false;
    mError.clear();
}

bool VorbisDecoderImpl::hasError(fl::string* msg) const {
    if (!mError.empty()) {
        if (msg) *msg = mError;
        return true;
    }
    return false;
}

bool VorbisDecoderImpl::decodeNextFrame(AudioSample* outSample) {
    if (!mDecoder.isOpen() || mEndOfStream) {
        return false;
    }

    VorbisInfo info = mDecoder.getInfo();
    fl::i32 channels = info.channels > 0 ? info.channels : 1;

    // Decode to i16 interleaved
    fl::i32 samplesDecoded = mDecoder.getSamplesShortInterleaved(
        channels,
        mPcmBuffer.data(),
        static_cast<fl::i32>(mPcmBuffer.size())
    );

    if (samplesDecoded == 0) {
        mEndOfStream = true;
        mPosition = mFileData.size();  // At end of stream
        return false;
    }

    // Update position estimate based on sample offset ratio
    fl::u32 totalSamples = mDecoder.getTotalSamples();
    if (totalSamples > 0) {
        fl::u32 currentSample = mDecoder.getSampleOffset();
        mPosition = (mFileData.size() * currentSample) / totalSamples;
    }

    // Convert stereo to mono by averaging if needed
    if (channels == 2) {
        fl::vector<fl::i16> mono;
        mono.reserve(samplesDecoded);
        for (fl::i32 i = 0; i < samplesDecoded; ++i) {
            fl::i32 left = mPcmBuffer[i * 2];
            fl::i32 right = mPcmBuffer[i * 2 + 1];
            mono.push_back(static_cast<fl::i16>((left + right) / 2));
        }
        *outSample = AudioSample(fl::span<const fl::i16>(mono.data(), mono.size()));
    } else {
        *outSample = AudioSample(fl::span<const fl::i16>(mPcmBuffer.data(), samplesDecoded));
    }

    return true;
}

void VorbisDecoderImpl::reset() {
    if (mDecoder.isOpen()) {
        mDecoder.seek(0);
        mPosition = 0;
        mEndOfStream = false;
    }
}

// VorbisDecoder public implementation
VorbisDecoder::VorbisDecoder() : mImpl(new VorbisDecoderImpl()) {}
VorbisDecoder::~VorbisDecoder() { delete mImpl; }

bool VorbisDecoder::begin(fl::ByteStreamPtr stream) { return mImpl->begin(stream); }
void VorbisDecoder::end() { mImpl->end(); }
bool VorbisDecoder::isReady() const { return mImpl->isReady(); }
bool VorbisDecoder::hasError(fl::string* msg) const { return mImpl->hasError(msg); }
bool VorbisDecoder::decodeNextFrame(AudioSample* outSample) { return mImpl->decodeNextFrame(outSample); }
fl::size VorbisDecoder::getPosition() const { return mImpl->getPosition(); }
void VorbisDecoder::reset() { mImpl->reset(); }
VorbisInfo VorbisDecoder::getInfo() const { return mImpl->getInfo(); }

// Vorbis factory implementation
VorbisDecoderPtr Vorbis::createDecoder(fl::string* errorMessage) {
    FL_UNUSED(errorMessage);
    return fl::make_shared<VorbisDecoder>();
}

bool Vorbis::isSupported() {
    return true;  // stb_vorbis is always available
}

VorbisInfo Vorbis::parseVorbisInfo(fl::span<const fl::u8> data, fl::string* errorMessage) {
    StbVorbisDecoder decoder;
    if (!decoder.openMemory(data)) {
        if (errorMessage) *errorMessage = "Failed to parse Vorbis header";
        return VorbisInfo();
    }
    return decoder.getInfo();
}

fl::vector<AudioSample> Vorbis::decodeAll(fl::span<const fl::u8> data, fl::string* errorMessage) {
    fl::vector<AudioSample> samples;

    StbVorbisDecoder decoder;
    if (!decoder.openMemory(data)) {
        if (errorMessage) *errorMessage = "Failed to open Vorbis stream";
        return samples;
    }

    VorbisInfo info = decoder.getInfo();
    const fl::i32 channels = info.channels > 0 ? info.channels : 1;
    constexpr fl::i32 FRAME_SIZE = 1024;
    fl::vector<fl::i16> buffer(FRAME_SIZE * 2);

    while (true) {
        fl::i32 samplesDecoded = decoder.getSamplesShortInterleaved(
            channels, buffer.data(), static_cast<fl::i32>(buffer.size())
        );

        if (samplesDecoded == 0) break;

        // Convert stereo to mono
        if (channels == 2) {
            fl::vector<fl::i16> mono;
            mono.reserve(samplesDecoded);
            for (fl::i32 i = 0; i < samplesDecoded; ++i) {
                fl::i32 left = buffer[i * 2];
                fl::i32 right = buffer[i * 2 + 1];
                mono.push_back(static_cast<fl::i16>((left + right) / 2));
            }
            samples.push_back(AudioSample(fl::span<const fl::i16>(mono.data(), mono.size())));
        } else {
            samples.push_back(AudioSample(fl::span<const fl::i16>(buffer.data(), samplesDecoded)));
        }
    }

    return samples;
}

} // namespace fl
