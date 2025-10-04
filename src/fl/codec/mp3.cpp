#include "fl/codec/mp3.h"

#include <string.h>  // for memset

// Include Helix MP3 decoder C API
extern "C" {
#include "third_party/libhelix_mp3/pub/mp3dec.h"
}

namespace fl {
namespace third_party {

// Maximum PCM output: 1152 samples/channel * 2 channels = 2304 samples
constexpr fl::size MAX_PCM_SAMPLES = 2304;

Mp3HelixDecoder::Mp3HelixDecoder()
    : mDecoder(nullptr), mPcmBuffer(nullptr) {
    memset(&mFrameInfo, 0, sizeof(mFrameInfo));
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

    memset(&mFrameInfo, 0, sizeof(mFrameInfo));
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
        mPcmBuffer,
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

}  // namespace third_party
}  // namespace fl
