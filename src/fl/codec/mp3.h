#pragma once

#include "fl/audio.h"
#include "fl/span.h"
#include "fl/vector.h"
#include "fl/stdint.h"

namespace fl {
namespace third_party {

// Mp3Frame represents a decoded MP3 audio frame.
struct Mp3Frame {
    const fl::i16* pcm;         // Interleaved PCM data (L/R)
    int samples;                // Samples per channel
    int channels;               // 1 (mono) or 2 (stereo)
    int sample_rate;            // Sample rate in Hz
};

// Mp3HelixDecoder wraps the Helix MP3 fixed-point decoder.
// It provides a simple C++ interface for decoding MP3 data into PCM samples.
class Mp3HelixDecoder {
  public:
    Mp3HelixDecoder();
    ~Mp3HelixDecoder();

    // Initialize the decoder. Returns true on success.
    bool init();

    // Decode MP3 data from the input buffer. Calls the provided callback
    // for each decoded frame. Returns the number of frames decoded.
    template <typename Fn>
    int decode(const fl::u8* data, fl::size len, Fn on_frame) {
        if (!mDecoder) {
            return 0;
        }

        const fl::u8* inptr = data;
        fl::size bytes_left = len;
        int frames_decoded = 0;

        while (bytes_left > 0) {
            // Find sync word
            int offset = findSyncWord(inptr, bytes_left);
            if (offset < 0) {
                break;  // No sync word found
            }

            inptr += offset;
            bytes_left -= offset;

            // Decode one frame
            int result = decodeFrame(&inptr, &bytes_left);
            if (result == 0) {
                // Successfully decoded a frame
                Mp3Frame frame;
                frame.pcm = mPcmBuffer;
                frame.samples = mFrameInfo.outputSamps / mFrameInfo.nChans;
                frame.channels = mFrameInfo.nChans;
                frame.sample_rate = mFrameInfo.samprate;

                on_frame(frame);
                frames_decoded++;
            } else if (result < 0) {
                // Decode error - skip a bit and try again
                if (bytes_left > 0) {
                    inptr++;
                    bytes_left--;
                }
            }
        }

        return frames_decoded;
    }

    // Decode MP3 data and convert to AudioSample objects.
    // Returns a vector of decoded audio samples.
    fl::vector<AudioSample> decodeToAudioSamples(const fl::u8* data, fl::size len);

    // Reset the decoder state
    void reset();

  private:
    int findSyncWord(const fl::u8* buf, fl::size len);
    int decodeFrame(const fl::u8** inbuf, fl::size* bytes_left);

    void* mDecoder;  // HMP3Decoder handle
    fl::i16* mPcmBuffer;  // PCM output buffer
    struct FrameInfo {
        int bitrate;
        int nChans;
        int samprate;
        int bitsPerSample;
        int outputSamps;
        int layer;
        int version;
    };
    FrameInfo mFrameInfo;
};

}  // namespace third_party
}  // namespace fl
