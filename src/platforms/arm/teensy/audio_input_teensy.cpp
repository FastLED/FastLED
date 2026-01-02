#include "audio_input_teensy.h"

namespace fl {

#if TEENSY_AUDIO_LIBRARY_AVAILABLE

// TeensyAudioRecorder implementation

TeensyAudioRecorder::TeensyAudioRecorder() : AudioStream(2, inputQueueArray) {
    // 2 inputs: Left (0) and Right (1) channels
    reset();
}

void TeensyAudioRecorder::reset() {
    mBlockQueue.clear();
    mTotalBlocksReceived = 0;
    mTotalBlocksDropped = 0;
}

void TeensyAudioRecorder::update() {
    audio_block_t* leftBlock = receiveReadOnly(0);   // Left channel
    audio_block_t* rightBlock = receiveReadOnly(1);  // Right channel

    if (leftBlock) {
        if (queueBlock(leftBlock, 0)) {
            mTotalBlocksReceived++;
        }
        release(leftBlock);
    }
    if (rightBlock) {
        if (queueBlock(rightBlock, 1)) {
            mTotalBlocksReceived++;
        }
        release(rightBlock);
    }
}

bool TeensyAudioRecorder::queueBlock(const audio_block_t* block, u8 channel) {
    if (!block) return false;

    // Limit queue size to prevent unbounded memory growth
    const fl::size MAX_QUEUE_SIZE = 16;  // 16 blocks * 128 samples * 2.9ms = ~46ms buffer
    if (mBlockQueue.size() >= MAX_QUEUE_SIZE) {
        mTotalBlocksDropped++;
        return false;
    }

    // Copy block data to our queue
    QueuedBlock qb;
    qb.channel = channel;
    qb.timestamp = millis();
    // Copy samples from audio_block_t (128 samples of int16)
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        qb.samples[i] = block->data[i];
    }

    mBlockQueue.push_back(qb);
    return true;
}

bool TeensyAudioRecorder::dequeueBlock(fl::vector<fl::i16>& samples, u8& channel, u32& timestamp) {
    if (mBlockQueue.empty()) {
        return false;
    }

    const QueuedBlock& block = mBlockQueue.front();
    channel = block.channel;
    timestamp = block.timestamp;

    // Copy samples
    samples.clear();
    samples.reserve(AUDIO_BLOCK_SAMPLES);
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        samples.push_back(block.samples[i]);
    }

    mBlockQueue.erase(mBlockQueue.begin());
    return true;
}

// Teensy_I2S_Audio implementation

Teensy_I2S_Audio::Teensy_I2S_Audio(const AudioConfigI2S& config)
    : mConfig(config), mHasError(false), mTotalSamplesRead(0), mInitialized(false) {

    // Validate sample rate (Teensy Audio Library defaults to 44.1kHz)
    if (mConfig.mSampleRate != 44100) {
        mHasError = true;
        mErrorMessage = "Teensy Audio Library only supports 44100Hz sample rate";
        FL_WARN(mErrorMessage.c_str());
        return;
    }

    // Validate bit resolution (Teensy Audio Library uses 16-bit)
    if (mConfig.mBitResolution != 16) {
        mHasError = true;
        mErrorMessage = "Teensy Audio Library only supports 16-bit resolution";
        FL_WARN(mErrorMessage.c_str());
        return;
    }

#if defined(FL_IS_TEENSY_3X)
    // Teensy 3.x only has I2S1
    if (static_cast<TeensyI2S::I2SPort>(mConfig.mI2sNum) == TeensyI2S::I2S2) {
        mHasError = true;
        mErrorMessage = "I2S2 is not available on Teensy 3.x (only I2S1 supported)";
        FL_WARN(mErrorMessage.c_str());
        return;
    }
#endif

    // Create recorder to queue audio blocks
    mRecorder = fl::make_shared<TeensyAudioRecorder>();

    // Create the appropriate I2S input object and connections based on port selection
    if (static_cast<TeensyI2S::I2SPort>(mConfig.mI2sNum) == TeensyI2S::I2S1) {
        mI2sInput = fl::make_shared<AudioInputI2S>();
        // Create connections: I2S -> Recorder
        mConnectionLeft = fl::make_shared<AudioConnection>(*mI2sInput, 0, *mRecorder, 0);   // Left
        mConnectionRight = fl::make_shared<AudioConnection>(*mI2sInput, 1, *mRecorder, 1);  // Right
    }
#if defined(FL_IS_TEENSY_4X)
    else if (static_cast<TeensyI2S::I2SPort>(mConfig.mI2sNum) == TeensyI2S::I2S2) {
        mI2sInput2 = fl::make_shared<AudioInputI2S2>();
        // Create connections: I2S2 -> Recorder
        mConnectionLeft = fl::make_shared<AudioConnection>(*mI2sInput2, 0, *mRecorder, 0);   // Left
        mConnectionRight = fl::make_shared<AudioConnection>(*mI2sInput2, 1, *mRecorder, 1);  // Right
    }
#endif
    else {
        mHasError = true;
        mErrorMessage = "Invalid I2S port selection";
        FL_WARN(mErrorMessage.c_str());
        return;
    }
}

Teensy_I2S_Audio::~Teensy_I2S_Audio() {
    stop();
}

void Teensy_I2S_Audio::start() {
    if (mHasError) {
        FL_WARN("Cannot start Teensy I2S audio - initialization error occurred");
        return;
    }

    if (mInitialized) {
        FL_WARN("Teensy I2S audio is already initialized");
        return;
    }

    mInitialized = true;
    mTotalSamplesRead = 0;
    mBlocksAccumulated = 0;
    mAccumulatedSamples.clear();

    // Reset recorder queue
    if (mRecorder) {
        mRecorder->reset();
    }

    FL_WARN("Teensy I2S audio input started (512-sample mono buffering)");

#if defined(FL_IS_TEENSY_3X)
    FL_WARN("  Teensy 3.x I2S1 pins: BCLK=9, MCLK=11, RX=13, LRCLK=23");
#elif defined(FL_IS_TEENSY_4X)
    if (static_cast<TeensyI2S::I2SPort>(mConfig.mI2sNum) == TeensyI2S::I2S1) {
        FL_WARN("  Teensy 4.x I2S1 pins: BCLK=21, MCLK=23, RX=8, LRCLK=20");
    } else {
        FL_WARN("  Teensy 4.x I2S2 pins: BCLK=4, MCLK=33, RX=5, LRCLK=3");
    }
#endif

    // Log channel selection
    const char* channelName = (mConfig.mAudioChannel == AudioChannel::Left) ? "Left" :
                              (mConfig.mAudioChannel == AudioChannel::Right) ? "Right" : "Both (downmixed)";
    FL_WARN("  Channel: " << channelName);
}

void Teensy_I2S_Audio::stop() {
    if (!mInitialized) {
        return;
    }

    mInitialized = false;
    mTotalSamplesRead = 0;
    mBlocksAccumulated = 0;
    mAccumulatedSamples.clear();

    // Clear recorder queue
    if (mRecorder) {
        mRecorder->reset();
    }

    FL_WARN("Teensy I2S audio input stopped");
}

bool Teensy_I2S_Audio::error(fl::string* msg) {
    if (msg && mHasError) {
        *msg = mErrorMessage;
    }
    return mHasError;
}

AudioSample Teensy_I2S_Audio::read() {
    if (mHasError || !mInitialized || !mRecorder) {
        return AudioSample();  // Invalid sample
    }

    u32 firstBlockTimestamp = 0;

    if (mConfig.mAudioChannel == AudioChannel::Both) {
        // Stereo downmix mode: accumulate L/R blocks and downmix to mono
        // Each iteration: get L block + R block, average them -> 128 mono samples
        // Repeat 4 times -> 512 mono samples total
        while (mBlocksAccumulated < BLOCKS_TO_ACCUMULATE) {
            fl::vector<fl::i16> leftSamples, rightSamples;
            u8 leftChannel, rightChannel;
            u32 leftTimestamp, rightTimestamp;

            bool hasLeft = mRecorder->dequeueBlock(leftSamples, leftChannel, leftTimestamp);
            bool hasRight = mRecorder->dequeueBlock(rightSamples, rightChannel, rightTimestamp);

            if (!hasLeft || !hasRight || leftChannel != 0 || rightChannel != 1) {
                // Need both channels, but don't have them yet
                return AudioSample();
            }

            if (mBlocksAccumulated == 0) {
                firstBlockTimestamp = leftTimestamp;
            }

            // Downmix L+R to mono: (L + R) / 2
            fl::size minSize = (leftSamples.size() < rightSamples.size()) ?
                               leftSamples.size() : rightSamples.size();
            for (fl::size i = 0; i < minSize; i++) {
                fl::i16 monoSample = (static_cast<fl::i32>(leftSamples[i]) +
                                     static_cast<fl::i32>(rightSamples[i])) / 2;
                mAccumulatedSamples.push_back(monoSample);
            }

            mBlocksAccumulated++;
        }
    } else {
        // Mono mode: accumulate single channel blocks
        // Target: 512 samples from one channel (Left or Right)
        u8 expectedChannel = (mConfig.mAudioChannel == AudioChannel::Left) ? 0 : 1;

        while (mBlocksAccumulated < BLOCKS_TO_ACCUMULATE) {
            fl::vector<fl::i16> samples;
            u8 channel;
            u32 timestamp;

            if (!mRecorder->dequeueBlock(samples, channel, timestamp)) {
                // No data available yet
                return AudioSample();
            }

            if (channel != expectedChannel) {
                // Wrong channel, skip this block
                continue;
            }

            if (mBlocksAccumulated == 0) {
                firstBlockTimestamp = timestamp;
            }

            // Append samples to accumulator
            for (const auto& sample : samples) {
                mAccumulatedSamples.push_back(sample);
            }

            mBlocksAccumulated++;
        }
    }

    // We have enough samples - emit the buffer
    mTotalSamplesRead += mAccumulatedSamples.size();

    AudioSample result(fl::span<const fl::i16>(mAccumulatedSamples.data(),
                                               mAccumulatedSamples.size()),
                      firstBlockTimestamp);

    // Reset accumulator for next read
    mAccumulatedSamples.clear();
    mBlocksAccumulated = 0;

    return result;
}

// Platform-specific audio input creation function for Teensy
fl::shared_ptr<IAudioInput> teensy_create_audio_input(
    const AudioConfig& config,
    fl::string* error_message
) {
    if (config.is<AudioConfigI2S>()) {
        FL_WARN("Creating Teensy I2S audio source");
        AudioConfigI2S i2s_config = config.get<AudioConfigI2S>();
        auto audio = fl::make_shared<Teensy_I2S_Audio>(i2s_config);

        // Check if initialization failed
        fl::string init_error;
        if (audio->error(&init_error)) {
            if (error_message) {
                *error_message = init_error;
            }
            return fl::shared_ptr<IAudioInput>();  // Return null
        }

        return audio;
    } else if (config.is<AudioConfigPdm>()) {
        const char* ERROR_MESSAGE = "PDM audio not supported in Teensy Audio Library implementation";
        FL_WARN(ERROR_MESSAGE);
        if (error_message) {
            *error_message = ERROR_MESSAGE;
        }
        return fl::shared_ptr<IAudioInput>();  // Return null
    }

    const char* ERROR_MESSAGE = "Unsupported audio configuration for Teensy";
    FL_WARN(ERROR_MESSAGE);
    if (error_message) {
        *error_message = ERROR_MESSAGE;
    }
    return fl::shared_ptr<IAudioInput>();  // Return null
}

#else // !TEENSY_AUDIO_LIBRARY_AVAILABLE

// Null audio implementation fallback when Teensy Audio Library is not available
fl::shared_ptr<IAudioInput> teensy_create_audio_input(
    const AudioConfig& config,
    fl::string* error_message
) {
    FL_UNUSED(config);
    const char* ERROR_MESSAGE = "Teensy Audio Library not found. Install from Arduino Library Manager.";
    FL_WARN(ERROR_MESSAGE);
    if (error_message) {
        *error_message = ERROR_MESSAGE;
    }
    return fl::shared_ptr<IAudioInput>();  // Return null
}

#endif // TEENSY_AUDIO_LIBRARY_AVAILABLE

} // namespace fl
