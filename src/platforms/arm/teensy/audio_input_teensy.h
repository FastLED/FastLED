#pragma once

// IWYU pragma: private

#include "fl/audio/audio_input.h"
#include "fl/log/log.h"
#include "fl/stl/has_include.h"
#include "fl/stl/assert.h"
#include "fl/stl/vector.h"
#include "fl/stl/span.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/arm/teensy/audio_input_teensy_config.h"
#include "platforms/arm/teensy/is_teensy.h"

// Detect Teensy Audio Library availability
#if TEENSY_AUDIO_LIBRARY_AVAILABLE
// IWYU pragma: begin_keep
#define FASTLED_TEENSY_AUDIO_LIBRARY_HEADER <Audio.h>
#include FASTLED_TEENSY_AUDIO_LIBRARY_HEADER
#undef FASTLED_TEENSY_AUDIO_LIBRARY_HEADER
#include "fl/stl/noexcept.h"
// IWYU pragma: end_keep
#endif

namespace fl {

#if TEENSY_AUDIO_LIBRARY_AVAILABLE

// Type aliases for Teensy Audio Library classes to avoid naming collisions
using TeensyAudioInputI2S = ::AudioInputI2S;
#if defined(FL_IS_TEENSY_4X)
using TeensyAudioInputI2S2 = ::AudioInputI2S2;
#endif
using TeensyAudioConnection = ::AudioConnection;

// Wrapper classes that add virtual destructors to Teensy Audio Library classes
// The library's classes lack virtual destructors, so we subclass them to make them safe for use with shared_ptr
class AudioInputI2S : public TeensyAudioInputI2S {
public:
    virtual ~AudioInputI2S() = default;
};

#if defined(FL_IS_TEENSY_4X)
class AudioInputI2S2 : public TeensyAudioInputI2S2 {
public:
    virtual ~AudioInputI2S2() = default;
};
#endif

class AudioConnection : public TeensyAudioConnection {
public:
    AudioConnection(AudioStream& source, unsigned char sourceOutput,
                    AudioStream& destination, unsigned char destinationInput)
        : TeensyAudioConnection(source, sourceOutput, destination, destinationInput) {}
    virtual ~AudioConnection() = default;
};

// TeensyAudioRecorder: AudioStream subclass that queues audio blocks for FastLED
// This class receives audio blocks from the Teensy Audio Library via the update()
// callback and queues them for consumption by Teensy_I2S_Audio::read()
class TeensyAudioRecorder : public AudioStream {
public:
    TeensyAudioRecorder() FL_NOEXCEPT;
    virtual ~TeensyAudioRecorder() = default;

    void reset() FL_NOEXCEPT;

    // Called by Audio Library when new audio block is available
    // This runs in interrupt context - keep it fast!
    void update() FL_NOEXCEPT override;

    // Queue a block for later consumption
    // Returns true if queued, false if queue is full
    bool queueBlock(const audio_block_t* block, u8 channel) FL_NOEXCEPT;

    // Dequeue oldest block
    bool dequeueBlock(fl::vector<fl::i16>& samples, u8& channel, u32& timestamp) FL_NOEXCEPT;

    fl::size getQueueSize() const { return mBlockQueue.size(); }
    u64 getTotalBlocksReceived() const { return mTotalBlocksReceived; }
    u64 getTotalBlocksDropped() const { return mTotalBlocksDropped; }

private:
    struct QueuedBlock {
        fl::i16 samples[AUDIO_BLOCK_SAMPLES];  // 128 samples
        u8 channel;   // 0=Left, 1=Right
        u32 timestamp;
    };

    audio_block_t* inputQueueArray[2];  // Required by AudioStream constructor
    fl::vector<QueuedBlock> mBlockQueue;
    u64 mTotalBlocksReceived;
    u64 mTotalBlocksDropped;
};

// Teensy I2S Audio Input Implementation
// Uses the Teensy Audio Library's AudioInputI2S or AudioInputI2S2 classes
// to capture audio from I2S microphones (INMP441, ICS43432, SPH0645LM4H, etc.)
//
// Key characteristics:
// - Fixed hardware pins (cannot be changed in software)
// - 128-sample blocks @ 44.1kHz (2.9ms per block)
// - Accumulates 4 blocks to emit 512 samples (matching ESP32 buffer size)
// - Mono output: single channel (Left/Right) or stereo downmix (Both)
// - DMA-based buffering (managed by Audio Library)
//
// Architecture:
// - AudioInputI2S/I2S2 captures audio from hardware
// - AudioConnection routes audio to TeensyAudioRecorder
// - TeensyAudioRecorder (AudioStream subclass) queues 128-sample blocks
// - Teensy_I2S_Audio::read() accumulates 4 blocks → 512 mono samples
//   - Left/Right mode: 4 blocks from selected channel
//   - Both mode: 4 pairs of L/R blocks, downmixed to mono (L+R)/2
class Teensy_I2S_Audio : public audio::IInput {
public:
    static constexpr int BLOCKS_TO_ACCUMULATE = 4;  // 4 * 128 = 512 mono samples
    static constexpr int TARGET_BUFFER_SIZE = AUDIO_BLOCK_SAMPLES * BLOCKS_TO_ACCUMULATE;

    Teensy_I2S_Audio(const audio::ConfigI2S& config) FL_NOEXCEPT;
    ~Teensy_I2S_Audio() override;

    void start() FL_NOEXCEPT override;
    void stop() FL_NOEXCEPT override;
    bool error(fl::string* msg = nullptr) FL_NOEXCEPT override;
    audio::Sample read() FL_NOEXCEPT override;

private:
    audio::ConfigI2S mConfig;
    bool mHasError;
    fl::string mErrorMessage;
    u64 mTotalSamplesRead;
    bool mInitialized;

    // I2S input objects (created based on port selection)
    fl::shared_ptr<AudioInputI2S> mI2sInput;
#if defined(FL_IS_TEENSY_4X)
    fl::shared_ptr<AudioInputI2S2> mI2sInput2;
#endif

    // Audio recorder and connections
    fl::shared_ptr<TeensyAudioRecorder> mRecorder;
    fl::shared_ptr<AudioConnection> mConnectionLeft;
    fl::shared_ptr<AudioConnection> mConnectionRight;

    // Sample accumulation for 512-sample buffer
    fl::vector<fl::i16> mAccumulatedSamples;
    int mBlocksAccumulated;
};

#endif // TEENSY_AUDIO_LIBRARY_AVAILABLE

// Platform-specific audio input creation function for Teensy
fl::shared_ptr<audio::IInput> teensy_create_audio_input(
    const audio::Config& config,
    fl::string* error_message
) FL_NOEXCEPT;

} // namespace fl
