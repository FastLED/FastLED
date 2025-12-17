#pragma once

#ifdef __EMSCRIPTEN__

#include "fl/audio_input.h"
#include "fl/audio.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"

namespace fl {

// WASM Audio Input Implementation
// Receives 512-sample Int16 PCM blocks from JavaScript via pushAudioSamples()
// Stores up to 16 blocks in a ring buffer for consumption by FastLED engine
class WasmAudioInput : public IAudioInput {
public:
    static constexpr int BLOCK_SIZE = 512;
    static constexpr int RING_BUFFER_SLOTS = 16;

    WasmAudioInput();
    ~WasmAudioInput() override;

    // IAudioInput interface
    void start() override;
    void stop() override;
    bool error(fl::string* msg = nullptr) override;
    AudioSample read() override;

    // Called from JavaScript via EMSCRIPTEN_KEEPALIVE
    void pushSamples(const fl::i16* samples, int count, fl::u32 timestamp);

private:
    struct AudioBlock {
        fl::i16 samples[BLOCK_SIZE];
        fl::u32 timestamp;
        bool valid;

        AudioBlock() : timestamp(0), valid(false) {}
    };

    AudioBlock mRingBuffer[RING_BUFFER_SLOTS];
    int mHead;  // Write position
    int mTail;  // Read position
    bool mRunning;
    bool mHasError;
    fl::string mErrorMessage;
    fl::u32 mDroppedBlocks;

    bool isFull() const;
    bool isEmpty() const;
    int nextIndex(int index) const;
};

// Factory function for creating WASM audio input
fl::shared_ptr<IAudioInput> wasm_create_audio_input(const AudioConfig& config, fl::string* error_message = nullptr);

// Get the global WASM audio input instance (for integration with UIAudio)
WasmAudioInput* wasm_get_audio_input();

} // namespace fl

#endif // __EMSCRIPTEN__
