#pragma once

#include "fl/audio.h"
#include "fl/span.h"
#include "fl/vector.h"
#include "fl/stdint.h"
#include "fl/bytestream.h"
#include "fl/shared_ptr.h"
#include "fl/str.h"

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

    // Public access for Mp3StreamDecoder
    int findSyncWord(const fl::u8* buf, fl::size len);
    int decodeFrame(const fl::u8** inbuf, fl::size* bytes_left);

  private:
    void* mDecoder;  // HMP3Decoder handle

  public:
    fl::i16* mPcmBuffer;  // PCM output buffer (public for Mp3StreamDecoder)
    struct FrameInfo {
        int bitrate;
        int nChans;
        int samprate;
        int bitsPerSample;
        int outputSamps;
        int layer;
        int version;
    };
    FrameInfo mFrameInfo;  // Public for Mp3StreamDecoder
};

// Mp3StreamDecoder provides streaming MP3 decoding from a ByteStream.
// This allows decoding large MP3 files without loading them entirely into memory.
class Mp3StreamDecoder {
  public:
    Mp3StreamDecoder();
    ~Mp3StreamDecoder();

    // Initialize with a byte stream
    bool begin(fl::ByteStreamPtr stream);
    void end();

    // Check if decoder is ready to use
    bool isReady() const { return mStream != nullptr && mDecoder != nullptr; }

    // Check for errors
    bool hasError(fl::string* msg = nullptr) const;

    // Decode the next audio frame from the stream
    // Returns true if a frame was decoded, false if end of stream or error
    bool decodeNextFrame(AudioSample* out_sample);

    // Get current stream position
    fl::size getPosition() const { return mBytesProcessed; }

    // Reset decoder state (but keep stream)
    void reset();

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
};

FASTLED_SMART_PTR(Mp3StreamDecoder);

}  // namespace third_party
}  // namespace fl
