#include "fl/codec/mp3.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/cstring.h"

// Include Helix MP3 decoder internal API (in fl::third_party namespace)
#include "third_party/libhelix_mp3/pub/mp3dec.h"

namespace fl {
namespace third_party {

// Maximum PCM output: 1152 samples/channel * 2 channels = 2304 samples
constexpr fl::size MAX_PCM_SAMPLES = 2304;

// Mp3HelixDecoder implementation
Mp3HelixDecoder::Mp3HelixDecoder()
    : mPcmBuffer(nullptr), mDecoder(nullptr) {
    fl::memset(&mFrameInfo, 0, sizeof(mFrameInfo));
}

Mp3HelixDecoder::~Mp3HelixDecoder() {
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
    mPcmBuffer = new fl::i16[MAX_PCM_SAMPLES];
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

    if (mPcmBuffer) {
        delete[] mPcmBuffer;
        mPcmBuffer = nullptr;
    }

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
        fl::bit_cast<short*>(mPcmBuffer),
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

fl::vector<AudioSample> Mp3HelixDecoder::decodeToAudioSamples(const fl::u8* data, fl::size len) {
    fl::vector<AudioSample> samples;

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

            AudioSample sample(fl::span<const fl::i16>(mono_pcm.data(), mono_pcm.size()));
            samples.push_back(sample);
        } else {
            // Mono audio - use directly
            AudioSample sample(fl::span<const fl::i16>(frame.pcm, frame.samples));
            samples.push_back(sample);
        }
    });

    return samples;
}

// Mp3StreamDecoderImpl - internal implementation of streaming MP3 decoder
class Mp3StreamDecoderImpl {
  public:
    Mp3StreamDecoderImpl();
    ~Mp3StreamDecoderImpl();

    bool begin(fl::ByteStreamPtr stream);
    void end();
    bool isReady() const { return mStream != nullptr && mDecoder != nullptr; }
    bool hasError(fl::string* msg = nullptr) const;
    bool decodeNextFrame(AudioSample* out_sample);
    fl::size getPosition() const { return mBytesProcessed; }
    void reset();
    Mp3Info getInfo() const { return mInfo; }

  private:
    static constexpr fl::size BUFFER_SIZE = 4096;

    bool fillBuffer();
    bool findAndDecodeFrame(AudioSample* out_sample);

    fl::ByteStreamPtr mStream;
    Mp3HelixDecoder* mDecoder;
    fl::vector<fl::u8> mBuffer;
    fl::size mBufferPos;
    fl::size mBufferFilled;
    fl::size mBytesProcessed;
    fl::string mErrorMsg;
    bool mHasError;
    bool mEndOfStream;
    Mp3Info mInfo;
    bool mHasDecodedFirstFrame;
};

Mp3StreamDecoderImpl::Mp3StreamDecoderImpl()
    : mStream(nullptr), mDecoder(nullptr), mBufferPos(0), mBufferFilled(0),
      mBytesProcessed(0), mHasError(false), mEndOfStream(false),
      mHasDecodedFirstFrame(false) {
    mBuffer.reserve(BUFFER_SIZE);
}

Mp3StreamDecoderImpl::~Mp3StreamDecoderImpl() {
    end();
}

bool Mp3StreamDecoderImpl::begin(fl::ByteStreamPtr stream) {
    if (!stream) {
        mErrorMsg = "Invalid stream provided";
        mHasError = true;
        return false;
    }

    mStream = stream;
    mDecoder = new Mp3HelixDecoder();
    if (!mDecoder->init()) {
        mErrorMsg = "Failed to initialize MP3 decoder";
        mHasError = true;
        delete mDecoder;
        mDecoder = nullptr;
        return false;
    }

    mBuffer.resize(BUFFER_SIZE);
    mBufferPos = 0;
    mBufferFilled = 0;
    mBytesProcessed = 0;
    mHasError = false;
    mEndOfStream = false;
    mHasDecodedFirstFrame = false;

    return true;
}

void Mp3StreamDecoderImpl::end() {
    if (mDecoder) {
        delete mDecoder;
        mDecoder = nullptr;
    }
    if (mStream) {
        mStream->close();
        mStream = nullptr;
    }
    mBuffer.clear();
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
        mDecoder->init();
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
        fl::size bytesRead = mStream->read(mBuffer.data() + mBufferFilled, spaceAvailable);
        mBufferFilled += bytesRead;
        return bytesRead > 0;
    }

    return mBufferFilled > mBufferPos;
}

bool Mp3StreamDecoderImpl::findAndDecodeFrame(AudioSample* out_sample) {
    if (!mDecoder) {
        return false;
    }

    // Try to decode from current buffer
    const fl::u8* inptr = mBuffer.data() + mBufferPos;
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
    mBufferPos += consumed;
    mBytesProcessed += consumed;

    if (result == 0) {
        // Successfully decoded a frame
        Mp3Frame frame;
        frame.pcm = mDecoder->mPcmBuffer;
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

        // Convert to AudioSample (convert stereo to mono if needed)
        if (frame.channels == 2) {
            fl::vector<fl::i16> mono_pcm;
            mono_pcm.reserve(frame.samples);

            for (int i = 0; i < frame.samples; i++) {
                fl::i32 left = frame.pcm[i * 2];
                fl::i32 right = frame.pcm[i * 2 + 1];
                fl::i32 avg = (left + right) / 2;
                mono_pcm.push_back(static_cast<fl::i16>(avg));
            }

            *out_sample = AudioSample(fl::span<const fl::i16>(mono_pcm.data(), mono_pcm.size()));
        } else {
            // Mono audio - use directly
            *out_sample = AudioSample(fl::span<const fl::i16>(frame.pcm, frame.samples));
        }

        return true;
    }

    return false;
}

bool Mp3StreamDecoderImpl::decodeNextFrame(AudioSample* out_sample) {
    if (!isReady()) {
        mErrorMsg = "Decoder not ready";
        mHasError = true;
        return false;
    }

    if (mEndOfStream) {
        return false;
    }

    // Try to decode from existing buffer
    if (findAndDecodeFrame(out_sample)) {
        return true;
    }

    // Need more data - try to fill buffer and decode again
    while (fillBuffer()) {
        if (findAndDecodeFrame(out_sample)) {
            return true;
        }
    }

    // No more data available
    mEndOfStream = true;
    return false;
}

}  // namespace third_party

// Mp3Decoder implementation
Mp3Decoder::Mp3Decoder() : mImpl(new third_party::Mp3StreamDecoderImpl()) {}

Mp3Decoder::~Mp3Decoder() {
    delete mImpl;
}

bool Mp3Decoder::begin(fl::ByteStreamPtr stream) {
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

bool Mp3Decoder::decodeNextFrame(AudioSample* out_sample) {
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
    third_party::Mp3HelixDecoder decoder;
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
