#pragma once

#include "fl/codec/common.h"
#include "fl/audio.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/bytestream.h"
#include "fl/stl/shared_ptr.h"
#include "fl/str.h"

namespace fl {

// MP3 metadata information structure
struct Mp3Info {
    fl::u32 sampleRate = 0;      // Sample rate in Hz
    fl::u8 channels = 0;          // Number of channels (1=mono, 2=stereo)
    fl::u32 bitrate = 0;          // Bitrate in kbps
    fl::u32 duration = 0;         // Duration in milliseconds (may be 0 if unknown)
    fl::u8 version = 0;           // MPEG version (1, 2, or 2.5)
    fl::u8 layer = 0;             // MPEG layer (1, 2, or 3)
    bool isValid = false;         // True if metadata was successfully parsed

    Mp3Info() = default;

    // Constructor for easy initialization
    Mp3Info(fl::u32 rate, fl::u8 ch, fl::u32 br)
        : sampleRate(rate), channels(ch), bitrate(br), isValid(true) {}
};

namespace third_party {
    // Forward declarations of internal implementation classes
    class Mp3StreamDecoderImpl;

    // Mp3Frame represents a decoded MP3 audio frame
    // This is exposed for testing purposes
    struct Mp3Frame {
        const fl::i16* pcm;         // Interleaved PCM data (L/R)
        int samples;                // Samples per channel
        int channels;               // 1 (mono) or 2 (stereo)
        int sample_rate;            // Sample rate in Hz
        int bitrate;                // Bitrate in kbps
        int version;                // MPEG version
        int layer;                  // MPEG layer
    };

    // Mp3HelixDecoder wraps the Helix MP3 fixed-point decoder
    // This is exposed for testing purposes
    class Mp3HelixDecoder {
    public:
        Mp3HelixDecoder();
        ~Mp3HelixDecoder();

        // Initialize the decoder. Returns true on success.
        bool init();

        // Reset decoder state
        void reset();

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
                    frame.bitrate = mFrameInfo.bitrate;
                    frame.version = mFrameInfo.version;
                    frame.layer = mFrameInfo.layer;

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

        // Decode MP3 data and convert to AudioSample objects
        fl::vector<AudioSample> decodeToAudioSamples(const fl::u8* data, fl::size len);

        // Public members/methods used by internal decoder implementation
        int findSyncWord(const fl::u8* buf, fl::size len);
        int decodeFrame(const fl::u8** inbuf, fl::size* bytes_left);

        fl::i16* mPcmBuffer;
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

    private:
        void* mDecoder;  // HMP3Decoder handle
    };
}

// MP3 decoder with streaming byte interface
// This decoder consumes MP3 data from a ByteStream and decodes audio frames on demand
class Mp3Decoder {
public:
    Mp3Decoder();
    ~Mp3Decoder();

    // Initialize the decoder with a byte stream
    // Returns true on success, false on failure
    bool begin(fl::ByteStreamPtr stream);

    // Clean up decoder resources
    void end();

    // Check if decoder is ready to use
    bool isReady() const;

    // Check for errors
    bool hasError(fl::string* msg = nullptr) const;

    // Decode the next audio frame from the stream
    // Returns true if a frame was decoded, false if end of stream or error
    bool decodeNextFrame(AudioSample* out_sample);

    // Get current stream position in bytes
    fl::size getPosition() const;

    // Reset decoder state (but keep stream)
    void reset();

    // Get MP3 stream information (only available after decoding first frame)
    Mp3Info getInfo() const;

private:
    fl::third_party::Mp3StreamDecoderImpl* mImpl;
};

FASTLED_SHARED_PTR(Mp3Decoder);

// MP3 factory for creating decoders and parsing metadata
class Mp3 {
public:
    // Create an MP3 decoder for streaming playback
    static Mp3DecoderPtr createDecoder(fl::string* error_message = nullptr);

    // Check if MP3 decoding is supported on this platform
    static bool isSupported();

    // Parse MP3 metadata from byte data without creating a decoder
    // This is a fast, lightweight operation that only reads the first MP3 frame header
    static Mp3Info parseMp3Info(fl::span<const fl::u8> data, fl::string* error_message = nullptr);
};

} // namespace fl
