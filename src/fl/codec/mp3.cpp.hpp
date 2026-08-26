#include "fl/codec/mp3.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/cstring.h"
#include "fl/stl/singleton.h"

// Include Helix MP3 decoder internal API (in fl::third_party namespace)
// IWYU pragma: begin_keep
#include "third_party/libhelix_mp3/pub/mp3dec.h"
#include "third_party/minimp3/minimp3.h"
#include "fl/stl/noexcept.h"
// IWYU pragma: end_keep

namespace fl {
namespace third_party {

template <typename T>
T* Mp3MemoryAllocateArray(fl::size count,
                          Mp3MemoryTag tag) FL_NO_EXCEPT {
    return static_cast<T*>(Mp3MemoryAllocate(sizeof(T) * count, tag));
}

template <typename T>
Mp3MemoryDeleter<T>::Mp3MemoryDeleter() FL_NO_EXCEPT
    : mBytes(0), mTag(Mp3MemoryTag::DecoderState) {}

template <typename T>
Mp3MemoryDeleter<T>::Mp3MemoryDeleter(
    fl::size bytes, Mp3MemoryTag tag) FL_NO_EXCEPT
    : mBytes(bytes), mTag(tag) {}

template <typename T>
void Mp3MemoryDeleter<T>::operator()(T* ptr) const FL_NO_EXCEPT {
    Mp3MemoryFree(ptr, mBytes, mTag);
}

namespace {
#if defined(FASTLED_TESTING)
struct Mp3MemoryHookState {
    Mp3MemoryHook* hook = nullptr;
};

Mp3MemoryHook*& mp3MemoryHook() FL_NO_EXCEPT {
    return fl::Singleton<Mp3MemoryHookState>::instance().hook;
}
#endif
} // anonymous namespace

const char* Mp3MemoryTagName(Mp3MemoryTag tag) FL_NO_EXCEPT {
    switch (tag) {
    case Mp3MemoryTag::DecoderState:
        return "decoder-state";
    case Mp3MemoryTag::Scratch:
        return "scratch";
    case Mp3MemoryTag::StreamBuffer:
        return "stream-buffer";
    case Mp3MemoryTag::PcmOutput:
        return "pcm-output";
    case Mp3MemoryTag::Count:
        break;
    }
    return "unknown";
}

#if defined(FASTLED_TESTING)
Mp3MemoryHook::~Mp3MemoryHook() FL_NO_EXCEPT = default;

bool Mp3MemoryHook::allowAllocate(fl::size,
                                  Mp3MemoryTag) FL_NO_EXCEPT {
    return true;
}

void SetMp3MemoryHook(Mp3MemoryHook* hook) FL_NO_EXCEPT {
    mp3MemoryHook() = hook;
}

void ClearMp3MemoryHook() FL_NO_EXCEPT {
    mp3MemoryHook() = nullptr;
}
#endif

FL_NO_INLINE void* Mp3MemoryAllocate(
    fl::size bytes, Mp3MemoryTag tag) FL_NO_EXCEPT {
#if defined(FASTLED_TESTING)
    if (mp3MemoryHook() && !mp3MemoryHook()->allowAllocate(bytes, tag)) {
        return nullptr;
    }
#endif
    void* ptr = fl::Malloc(bytes);
#if defined(FASTLED_TESTING)
    if (ptr && mp3MemoryHook()) {
        mp3MemoryHook()->onAllocate(ptr, bytes, tag);
    }
#else
    FL_UNUSED(tag);
#endif
    return ptr;
}

void Mp3MemoryFree(void* ptr, fl::size bytes,
                   Mp3MemoryTag tag) FL_NO_EXCEPT {
    if (!ptr) {
        return;
    }
#if defined(FASTLED_TESTING)
    if (mp3MemoryHook()) {
        mp3MemoryHook()->onFree(ptr, bytes, tag);
    }
#else
    FL_UNUSED(bytes);
    FL_UNUSED(tag);
#endif
    fl::Free(ptr);
}

// Maximum PCM output: 1152 samples/channel * 2 channels = 2304 samples
constexpr fl::size MAX_PCM_SAMPLES = 2304;

// Mp3HelixDecoder implementation
Mp3HelixDecoder::Mp3HelixDecoder() FL_NO_EXCEPT
    : mPcmBuffer(nullptr, Mp3MemoryDeleter<fl::i16>(
                              sizeof(fl::i16) * MAX_PCM_SAMPLES,
                              Mp3MemoryTag::PcmOutput)),
      mDecoder(nullptr) {
    fl::memset(&mFrameInfo, 0, sizeof(mFrameInfo));
}

Mp3HelixDecoder::~Mp3HelixDecoder() FL_NO_EXCEPT {
    reset();
}

bool Mp3HelixDecoder::init() {
    if (mDecoder) {
        return true;  // Already initialized
    }

    // Initialize Helix decoder
    mDecoder = MP3InitDecoder();
    if (!mDecoder) {
        return false;
    }

    // Allocate PCM buffer
    mPcmBuffer.reset(Mp3MemoryAllocateArray<fl::i16>(
        MAX_PCM_SAMPLES, Mp3MemoryTag::PcmOutput));
    if (!mPcmBuffer) {
        MP3FreeDecoder(static_cast<HMP3Decoder>(mDecoder));
        mDecoder = nullptr;
        return false;
    }

    return true;
}

void Mp3HelixDecoder::reset() {
    if (mDecoder) {
        MP3FreeDecoder(static_cast<HMP3Decoder>(mDecoder));
        mDecoder = nullptr;
    }

    mPcmBuffer.reset();

    fl::memset(&mFrameInfo, 0, sizeof(mFrameInfo));
}

int Mp3HelixDecoder::findSyncWord(const fl::u8* buf, fl::size len) {
    int offset = MP3FindSyncWord(buf, static_cast<int>(len));
    return offset;
}

int Mp3HelixDecoder::decodeFrame(const fl::u8** inbuf, fl::size* bytes_left) {
    if (!mDecoder || !mPcmBuffer) {
        return ERR_MP3_NULL_POINTER;
    }

    // Decode one frame
    int result = MP3Decode(
        static_cast<HMP3Decoder>(mDecoder),
        inbuf,
        bytes_left,
        fl::bit_cast<short*>(mPcmBuffer.get()),
        0  // useSize = 0 (use default)
    );

    if (result == ERR_MP3_NONE) {
        // Get frame info
        MP3FrameInfo helix_info;
        MP3GetLastFrameInfo(static_cast<HMP3Decoder>(mDecoder), &helix_info);

        mFrameInfo.bitrate = helix_info.bitrate;
        mFrameInfo.nChans = helix_info.nChans;
        mFrameInfo.samprate = helix_info.samprate;
        mFrameInfo.bitsPerSample = helix_info.bitsPerSample;
        mFrameInfo.outputSamps = helix_info.outputSamps;
        mFrameInfo.layer = helix_info.layer;
        mFrameInfo.version = helix_info.version;
    }

    return result;
}

fl::vector<audio::Sample> Mp3HelixDecoder::decodeToAudioSamples(const fl::u8* data, fl::size len) {
    fl::vector<audio::Sample> samples;

    decode(data, len, [&](const Mp3Frame& frame) {
        // Convert stereo to mono by averaging channels
        if (frame.channels == 2) {
            fl::vector<fl::i16> mono_pcm;
            mono_pcm.reserve(frame.samples);

            for (int i = 0; i < frame.samples; i++) {
                fl::i32 left = frame.pcm[i * 2];
                fl::i32 right = frame.pcm[i * 2 + 1];
                fl::i32 avg = (left + right) / 2;
                mono_pcm.push_back(static_cast<fl::i16>(avg));
            }

            audio::Sample sample(mono_pcm);
            samples.push_back(sample);
        } else {
            // Mono audio - use directly
            audio::Sample sample(fl::span<const fl::i16>(frame.pcm, frame.samples));
            samples.push_back(sample);
        }
    });

    return samples;
}

namespace {

int minimp3Version(const fl::u8* header) FL_NO_EXCEPT {
    const int version_bits = (header[1] >> 3) & 0x03;
    if (version_bits == 0x03) {
        return 0; // MPEG-1, matching Helix's MPEGVersion encoding.
    }
    if (version_bits == 0x02) {
        return 1; // MPEG-2.
    }
    return 2; // MPEG-2.5.
}

} // anonymous namespace

Mp3Minimp3Decoder::Mp3Minimp3Decoder() FL_NO_EXCEPT
    : mPcmBuffer(nullptr, Mp3MemoryDeleter<fl::i16>(
                              sizeof(fl::i16) * MAX_PCM_SAMPLES,
                              Mp3MemoryTag::PcmOutput)),
      mDecoder(nullptr, Mp3MemoryDeleter<mp3dec_t>(
                            sizeof(mp3dec_t), Mp3MemoryTag::DecoderState)),
      mScratch(nullptr, Mp3MemoryDeleter<mp3dec_scratch_t>(
                            sizeof(mp3dec_scratch_t),
                            Mp3MemoryTag::Scratch)) {
    fl::memset(&mFrameInfo, 0, sizeof(mFrameInfo));
}

Mp3Minimp3Decoder::~Mp3Minimp3Decoder() FL_NO_EXCEPT {
    reset();
}

bool Mp3Minimp3Decoder::init() FL_NO_EXCEPT {
    if (mDecoder) {
        return true;
    }

    mDecoder.reset(Mp3MemoryAllocateArray<mp3dec_t>(
        1, Mp3MemoryTag::DecoderState));
    mScratch.reset(Mp3MemoryAllocateArray<mp3dec_scratch_t>(
        1, Mp3MemoryTag::Scratch));
    mPcmBuffer.reset(Mp3MemoryAllocateArray<fl::i16>(
        MAX_PCM_SAMPLES, Mp3MemoryTag::PcmOutput));
    if (!mDecoder || !mScratch || !mPcmBuffer) {
        mDecoder.reset();
        mScratch.reset();
        mPcmBuffer.reset();
        return false;
    }

    mp3dec_init(mDecoder.get());
    return true;
}

void Mp3Minimp3Decoder::reset() FL_NO_EXCEPT {
    mDecoder.reset();
    mScratch.reset();
    mPcmBuffer.reset();
    fl::memset(&mFrameInfo, 0, sizeof(mFrameInfo));
}

int Mp3Minimp3Decoder::findSyncWord(const fl::u8* buf,
                                    fl::size len) FL_NO_EXCEPT {
    if (!buf || len < 4) {
        return -1;
    }

    for (fl::size i = 0; i <= len - 4; ++i) {
        const fl::u8 version = (buf[i + 1] >> 3) & 0x03;
        const fl::u8 layer = (buf[i + 1] >> 1) & 0x03;
        const fl::u8 bitrate = (buf[i + 2] >> 4) & 0x0f;
        const fl::u8 sample_rate = (buf[i + 2] >> 2) & 0x03;
        if (buf[i] == 0xff && (buf[i + 1] & 0xe0) == 0xe0 &&
            version != 0x01 && layer != 0x00 && bitrate != 0x0f &&
            sample_rate != 0x03) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Mp3Minimp3Decoder::decodeFrame(const fl::u8** inbuf,
                                   fl::size* bytes_left) FL_NO_EXCEPT {
    if (!mDecoder || !mScratch || !mPcmBuffer || !inbuf || !bytes_left ||
        !*inbuf || *bytes_left == 0) {
        return -1;
    }

    const fl::u8* input = *inbuf;
    const fl::size max_input = 0x7fffffff;
    const int input_bytes = static_cast<int>(
        *bytes_left > max_input ? max_input : *bytes_left);
    mp3dec_frame_info_t info = {};
    const int samples = mp3dec_decode_frame_r(
        mDecoder.get(), mScratch.get(), input, input_bytes, mPcmBuffer.get(),
        &info);
    if (samples <= 0 || info.frame_bytes <= 0 ||
        static_cast<fl::size>(info.frame_bytes) > *bytes_left) {
        return -1;
    }

    const fl::u8* header = input + info.frame_offset;
    *inbuf += info.frame_bytes;
    *bytes_left -= static_cast<fl::size>(info.frame_bytes);

    int frame_bytes = info.frame_bytes - info.frame_offset;
    if (frame_bytes > 0 && (header[2] & 0x02) != 0) {
        --frame_bytes;
    }
    mFrameInfo.bitrate = info.bitrate_kbps * 1000;
    if (mFrameInfo.bitrate == 0 && samples > 0 && frame_bytes > 0) {
        mFrameInfo.bitrate =
            (frame_bytes * info.hz * 8) / samples;
    }
    mFrameInfo.nChans = info.channels;
    mFrameInfo.samprate = info.hz;
    mFrameInfo.bitsPerSample = 16;
    mFrameInfo.outputSamps = samples * info.channels;
    mFrameInfo.layer = info.layer;
    mFrameInfo.version = minimp3Version(header);
    return 0;
}

fl::vector<audio::Sample>
Mp3Minimp3Decoder::decodeToAudioSamples(const fl::u8* data,
                                        fl::size len) FL_NO_EXCEPT {
    fl::vector<audio::Sample> samples;

    decode(data, len, [&](const Mp3Frame& frame) {
        if (frame.channels == 2) {
            fl::vector<fl::i16> mono_pcm;
            mono_pcm.reserve(frame.samples);
            for (int i = 0; i < frame.samples; ++i) {
                const fl::i32 left = frame.pcm[i * 2];
                const fl::i32 right = frame.pcm[i * 2 + 1];
                mono_pcm.push_back(static_cast<fl::i16>((left + right) / 2));
            }
            samples.push_back(audio::Sample(mono_pcm));
        } else {
            samples.push_back(
                audio::Sample(fl::span<const fl::i16>(frame.pcm,
                                                       frame.samples)));
        }
    });

    return samples;
}

#if defined(FASTLED_MP3_BACKEND_MINIMP3)
using Mp3SelectedDecoder = Mp3Minimp3Decoder;
#else
using Mp3SelectedDecoder = Mp3HelixDecoder;
#endif

// Mp3StreamDecoderImpl - internal implementation of streaming MP3 decoder
class Mp3StreamDecoderImpl {
  public:
    Mp3StreamDecoderImpl() FL_NO_EXCEPT;
    ~Mp3StreamDecoderImpl() FL_NO_EXCEPT;

    bool begin(fl::filebuf_ptr stream);
    void end();
    bool isReady() const { return mStream != nullptr && mDecoder != nullptr; }
    bool hasError(fl::string* msg = nullptr) const;
    bool decodeNextFrame(audio::Sample* out_sample);
    fl::size getPosition() const { return mBytesProcessed; }
    void reset();
    Mp3Info getInfo() const { return mInfo; }

  private:
#if defined(FASTLED_MP3_BACKEND_MINIMP3)
    static constexpr fl::size BUFFER_SIZE = MP3_MINIMP3_STREAM_BUFFER_SIZE;
#else
    static constexpr fl::size BUFFER_SIZE = MP3_HELIX_STREAM_BUFFER_SIZE;
#endif

    bool fillBuffer();
    bool findAndDecodeFrame(audio::Sample* out_sample);

    fl::filebuf_ptr mStream;
    fl::unique_ptr<Mp3SelectedDecoder> mDecoder;
    fl::unique_ptr<fl::u8[], Mp3MemoryDeleter<fl::u8>> mBuffer;
    fl::size mBufferPos;
    fl::size mBufferFilled;
    fl::size mBytesProcessed;
    fl::string mErrorMsg;
    bool mHasError;
    bool mEndOfStream;
    Mp3Info mInfo;
    bool mHasDecodedFirstFrame;
};

Mp3StreamDecoderImpl::Mp3StreamDecoderImpl() FL_NO_EXCEPT
    : mStream(nullptr),
      mBuffer(nullptr, Mp3MemoryDeleter<fl::u8>(
                           BUFFER_SIZE, Mp3MemoryTag::StreamBuffer)),
      mBufferPos(0), mBufferFilled(0),
      mBytesProcessed(0), mHasError(false), mEndOfStream(false),
      mHasDecodedFirstFrame(false) {}

Mp3StreamDecoderImpl::~Mp3StreamDecoderImpl() FL_NO_EXCEPT {
    end();
}

bool Mp3StreamDecoderImpl::begin(fl::filebuf_ptr stream) {
    if (!stream) {
        mErrorMsg = "Invalid stream provided";
        mHasError = true;
        return false;
    }

    mStream = stream;
    mDecoder = fl::make_unique<Mp3SelectedDecoder>();
    if (!mDecoder->init()) {
        mErrorMsg = "Failed to initialize MP3 decoder";
        mHasError = true;
        mDecoder.reset();
        return false;
    }

    mBuffer.reset(Mp3MemoryAllocateArray<fl::u8>(
        BUFFER_SIZE, Mp3MemoryTag::StreamBuffer));
    if (!mBuffer) {
        mErrorMsg = "Failed to allocate MP3 stream buffer";
        mHasError = true;
        mDecoder.reset();
        return false;
    }
    mBufferPos = 0;
    mBufferFilled = 0;
    mBytesProcessed = 0;
    mHasError = false;
    mEndOfStream = false;
    mHasDecodedFirstFrame = false;

    return true;
}

void Mp3StreamDecoderImpl::end() {
    mDecoder.reset();
    if (mStream) {
        mStream->close();
        mStream = nullptr;
    }
    mBuffer.reset();
}

bool Mp3StreamDecoderImpl::hasError(fl::string* msg) const {
    if (msg && mHasError) {
        *msg = mErrorMsg;
    }
    return mHasError;
}

void Mp3StreamDecoderImpl::reset() {
    if (mDecoder) {
        mDecoder->reset();
        if (!mDecoder->init()) {
            mErrorMsg = "Failed to reinitialize MP3 decoder";
            mHasError = true;
            mDecoder.reset();
            return;
        }
    }
    mBufferPos = 0;
    mBufferFilled = 0;
    mBytesProcessed = 0;
    mHasError = false;
    mEndOfStream = false;
    mHasDecodedFirstFrame = false;
}

bool Mp3StreamDecoderImpl::fillBuffer() {
    // Shift remaining data to beginning of buffer
    if (mBufferPos > 0 && mBufferFilled > mBufferPos) {
        fl::size remaining = mBufferFilled - mBufferPos;
        for (fl::size i = 0; i < remaining; i++) {
            mBuffer[i] = mBuffer[mBufferPos + i];
        }
        mBufferFilled = remaining;
        mBufferPos = 0;
    } else if (mBufferPos >= mBufferFilled) {
        mBufferPos = 0;
        mBufferFilled = 0;
    }

    // Fill the rest of the buffer from stream
    fl::size spaceAvailable = BUFFER_SIZE - mBufferFilled;
    if (spaceAvailable > 0 && mStream && mStream->available(1)) {
        fl::size bytesRead =
            mStream->read(mBuffer.get() + mBufferFilled, spaceAvailable);
        mBufferFilled += bytesRead;
        return bytesRead > 0;
    }

    return false;
}

bool Mp3StreamDecoderImpl::findAndDecodeFrame(audio::Sample* out_sample) {
    if (!mDecoder) {
        return false;
    }

    // Try to decode from current buffer
    const fl::u8* inptr = mBuffer.get() + mBufferPos;
    fl::size bytes_left = mBufferFilled - mBufferPos;

    if (bytes_left == 0) {
        return false;
    }

    // Find sync word
    int offset = mDecoder->findSyncWord(inptr, bytes_left);
    if (offset < 0) {
        // No sync word found, consume buffer and try again
        mBufferPos = mBufferFilled;
        return false;
    }

    inptr += offset;
    bytes_left -= offset;
    mBufferPos += offset;

    // Try to decode one frame
    const fl::u8* decode_ptr = inptr;
    fl::size decode_bytes = bytes_left;

    int result = mDecoder->decodeFrame(&decode_ptr, &decode_bytes);

    // Update buffer position based on how many bytes were consumed
    fl::size consumed = (decode_ptr - inptr);
    if (result != 0 && consumed == 0 && bytes_left == BUFFER_SIZE) {
        // A full buffer cannot grow to complete this frame. Drop one byte so
        // corrupt input cannot pin the streaming decoder on the same sync.
        consumed = 1;
    }
    mBufferPos += consumed;
    mBytesProcessed += consumed;

    if (result == 0) {
        // Successfully decoded a frame
        Mp3Frame frame;
        frame.pcm = mDecoder->mPcmBuffer.get();
        frame.samples = mDecoder->mFrameInfo.outputSamps / mDecoder->mFrameInfo.nChans;
        frame.channels = mDecoder->mFrameInfo.nChans;
        frame.sample_rate = mDecoder->mFrameInfo.samprate;
        frame.bitrate = mDecoder->mFrameInfo.bitrate;
        frame.version = mDecoder->mFrameInfo.version;
        frame.layer = mDecoder->mFrameInfo.layer;

        // Update stream info on first successful decode
        if (!mHasDecodedFirstFrame) {
            mInfo.sampleRate = frame.sample_rate;
            mInfo.channels = static_cast<fl::u8>(frame.channels);
            mInfo.bitrate = frame.bitrate;
            mInfo.version = static_cast<fl::u8>(frame.version);
            mInfo.layer = static_cast<fl::u8>(frame.layer);
            mInfo.isValid = true;
            mHasDecodedFirstFrame = true;
        }

        // Convert to audio::Sample (convert stereo to mono if needed)
        if (frame.channels == 2) {
            fl::vector<fl::i16> mono_pcm;
            mono_pcm.reserve(frame.samples);

            for (int i = 0; i < frame.samples; i++) {
                fl::i32 left = frame.pcm[i * 2];
                fl::i32 right = frame.pcm[i * 2 + 1];
                fl::i32 avg = (left + right) / 2;
                mono_pcm.push_back(static_cast<fl::i16>(avg));
            }

            *out_sample = audio::Sample(mono_pcm);
        } else {
            // Mono audio - use directly
            *out_sample = audio::Sample(fl::span<const fl::i16>(frame.pcm, frame.samples));
        }

        return true;
    }

    return false;
}

bool Mp3StreamDecoderImpl::decodeNextFrame(audio::Sample* out_sample) {
    if (!isReady()) {
        mErrorMsg = "Decoder not ready";
        mHasError = true;
        return false;
    }

    if (mEndOfStream) {
        return false;
    }

    while (true) {
        const fl::size previous_position = mBufferPos;
        if (findAndDecodeFrame(out_sample)) {
            return true;
        }

        if (mBufferPos > previous_position) {
            // The decoder rejected or skipped bytes. Retry the remainder before
            // refilling so a valid frame after corruption is not discarded.
            continue;
        }

        if (fillBuffer()) {
            continue;
        }

        if (mBufferPos < mBufferFilled) {
            // No more bytes are available to complete the current candidate.
            // Advance through the tail so a later sync can still be recovered.
            ++mBufferPos;
            ++mBytesProcessed;
            continue;
        }

        mEndOfStream = true;
        return false;
    }
}

}  // namespace third_party

// Mp3Decoder implementation
Mp3Decoder::Mp3Decoder() : mImpl(fl::make_unique<third_party::Mp3StreamDecoderImpl>()) {}

Mp3Decoder::~Mp3Decoder() FL_NO_EXCEPT = default;

bool Mp3Decoder::begin(fl::filebuf_ptr stream) {
    return mImpl->begin(stream);
}

void Mp3Decoder::end() {
    mImpl->end();
}

bool Mp3Decoder::isReady() const {
    return mImpl->isReady();
}

bool Mp3Decoder::hasError(fl::string* msg) const {
    return mImpl->hasError(msg);
}

bool Mp3Decoder::decodeNextFrame(audio::Sample* out_sample) {
    return mImpl->decodeNextFrame(out_sample);
}

fl::size Mp3Decoder::getPosition() const {
    return mImpl->getPosition();
}

void Mp3Decoder::reset() {
    mImpl->reset();
}

Mp3Info Mp3Decoder::getInfo() const {
    return mImpl->getInfo();
}

// Mp3 factory implementation
Mp3DecoderPtr Mp3::createDecoder(fl::string* error_message) {
    FL_UNUSED(error_message);
    return fl::make_shared<Mp3Decoder>();
}

bool Mp3::isSupported() {
    // MP3 decoder is available on all platforms
    return true;
}

Mp3Info Mp3::parseMp3Info(fl::span<const fl::u8> data, fl::string* error_message) {
    Mp3Info info;

    // Validate input data
    if (data.empty()) {
        if (error_message) {
            *error_message = "Empty MP3 data";
        }
        return info; // returns invalid info
    }

    // Minimum size check - need at least an MP3 frame header (4 bytes) + some data
    if (data.size() < 128) {
        if (error_message) {
            *error_message = "MP3 data too small";
        }
        return info;
    }

    // Look for MP3 sync word (11 bits set: 0xFFE or 0xFFF)
    bool foundSync = false;
    fl::size syncOffset = 0;

    for (fl::size i = 0; i <= data.size() - 4; i++) {
        if (data[i] == 0xFF && (data[i + 1] & 0xE0) == 0xE0) {
            foundSync = true;
            syncOffset = i;
            break;
        }
    }

    if (!foundSync) {
        if (error_message) {
            *error_message = "Invalid MP3 stream - no sync word found";
        }
        return info;
    }

    // Use the decoder to parse the first frame
    third_party::Mp3SelectedDecoder decoder;
    if (!decoder.init()) {
        if (error_message) {
            *error_message = "Failed to initialize MP3 decoder";
        }
        return info;
    }

    const fl::u8* inptr = data.data() + syncOffset;
    fl::size bytes_left = data.size() - syncOffset;

    // Try to decode first frame to get metadata
    int offset = decoder.findSyncWord(inptr, bytes_left);
    if (offset >= 0) {
        inptr += offset;
        bytes_left -= offset;

        int result = decoder.decodeFrame(&inptr, &bytes_left);
        if (result == 0) {
            // Successfully decoded - extract metadata
            info.sampleRate = decoder.mFrameInfo.samprate;
            info.channels = static_cast<fl::u8>(decoder.mFrameInfo.nChans);
            info.bitrate = decoder.mFrameInfo.bitrate;
            info.version = static_cast<fl::u8>(decoder.mFrameInfo.version);
            info.layer = static_cast<fl::u8>(decoder.mFrameInfo.layer);
            info.isValid = true;
        } else if (error_message) {
            *error_message = "Failed to decode MP3 frame header";
        }
    } else if (error_message) {
        *error_message = "Failed to find MP3 sync word";
    }

    return info;
}

} // namespace fl
