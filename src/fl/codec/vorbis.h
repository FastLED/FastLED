#pragma once

#include "fl/codec/common.h"
#include "fl/audio.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/bytestream.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"

namespace fl {

// Vorbis metadata information structure
struct VorbisInfo {
    fl::u32 sampleRate = 0;      // Sample rate in Hz
    fl::u8 channels = 0;          // Number of channels (1=mono, 2=stereo)
    fl::u32 totalSamples = 0;     // Total samples (0 if unknown/streaming)
    fl::u32 maxFrameSize = 0;     // Maximum frame size in samples
    bool isValid = false;         // True if metadata was successfully parsed

    VorbisInfo() = default;
    VorbisInfo(fl::u32 rate, fl::u8 ch)
        : sampleRate(rate), channels(ch), isValid(true) {}
};

// Forward declaration of internal implementation
class VorbisDecoderImpl;

// VorbisFrame represents a decoded Vorbis audio frame
struct VorbisFrame {
    const float* pcm;           // Interleaved PCM data (float, -1.0 to 1.0)
    fl::i32 samples;            // Samples per channel
    fl::i32 channels;           // 1 (mono) or 2 (stereo)
    fl::i32 sampleRate;         // Sample rate in Hz
};

// Low-level stb_vorbis wrapper for testing
class StbVorbisDecoder {
public:
    StbVorbisDecoder();
    ~StbVorbisDecoder();

    // Open from memory buffer (entire file must be in memory)
    bool openMemory(fl::span<const fl::u8> data);

    // Close and release resources
    void close();

    // Check if decoder is open
    bool isOpen() const;

    // Get stream info
    VorbisInfo getInfo() const;

    // Decode samples into buffer
    // Returns number of samples decoded per channel (0 = end of stream)
    fl::i32 getSamplesShortInterleaved(fl::i32 channels, fl::i16* buffer, fl::i32 numShorts);

    // Decode float samples
    fl::i32 getSamplesFloat(fl::i32 channels, float** buffer, fl::i32 numSamples);

    // Seek to sample position
    bool seek(fl::u32 sampleNumber);

    // Get current sample offset
    fl::u32 getSampleOffset() const;

    // Get total samples in stream
    fl::u32 getTotalSamples() const;

private:
    void* mVorbis;  // stb_vorbis* handle
};

// Vorbis decoder with streaming byte interface
// This decoder consumes Vorbis data from a ByteStream and decodes audio frames on demand
class VorbisDecoder {
public:
    VorbisDecoder();
    ~VorbisDecoder();

    // Initialize the decoder with a byte stream
    // Note: stb_vorbis requires the entire stream in memory for pulldata API
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
    bool decodeNextFrame(AudioSample* outSample);

    // Get current stream position in bytes
    fl::size getPosition() const;

    // Reset decoder state (but keep stream)
    void reset();

    // Get Vorbis stream information
    VorbisInfo getInfo() const;

private:
    VorbisDecoderImpl* mImpl;
};

FASTLED_SHARED_PTR(VorbisDecoder);

// Vorbis factory for creating decoders and parsing metadata
//
// PERFORMANCE NOTE:
// Benchmark testing (see REPORT.md) compared MP3 vs OGG Vorbis decoding performance
// using FFmpeg on a host machine (not embedded platform). Results showed:
//   - OGG decode time: 327ms (10 second audio sample)
//   - MP3 decode time: 420ms (OGG 28.6% faster than MP3)
//   - OGG file size: 108KB at 128kbps
//   - MP3 file size: 157KB at 128kbps (OGG 31% smaller than MP3)
//   - Audio quality difference: 0.67% RMS error (negligible)
//
// IMPORTANT: These benchmarks used native FFmpeg decoders on a desktop host.
// Performance on embedded platforms (ESP32, ARM, etc.) using the stb_vorbis
// decoder may differ significantly due to:
//   - Different decoder implementations (stb_vorbis vs FFmpeg)
//   - CPU architecture differences (ARM Cortex-M vs x86/x64)
//   - Clock speeds and available RAM
//   - Compiler optimizations and platform-specific code paths
//
// RECOMMENDATION: Always profile on your target platform before choosing a codec.
// What works best on desktop FFmpeg may not match embedded performance characteristics.
// Run your own benchmarks using your specific hardware, sample data, and use case.
//
class Vorbis {
public:
    // Create a Vorbis decoder for streaming playback
    static VorbisDecoderPtr createDecoder(fl::string* errorMessage = nullptr);

    // Check if Vorbis decoding is supported on this platform
    static bool isSupported();

    // Parse Vorbis metadata from byte data without full decoding
    static VorbisInfo parseVorbisInfo(fl::span<const fl::u8> data,
                                       fl::string* errorMessage = nullptr);

    // Decode entire file to AudioSample vector (convenience function)
    static fl::vector<AudioSample> decodeAll(fl::span<const fl::u8> data,
                                              fl::string* errorMessage = nullptr);
};

} // namespace fl
