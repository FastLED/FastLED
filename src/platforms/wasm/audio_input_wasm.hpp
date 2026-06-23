#pragma once

// IWYU pragma: private

#include "platforms/wasm/is_wasm.h"
#ifdef FL_IS_WASM

#include "fl/audio/audio_input.h"
#include "fl/audio/audio.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/noexcept.h"

namespace fl {

// WASM Audio Input Implementation
// Receives 512-sample Int16 PCM blocks from JavaScript via pushAudioSamples()
// Stores up to 16 blocks in a ring buffer for consumption by FastLED engine
class WasmAudioInput : public audio::IInput {
public:
    static constexpr int BLOCK_SIZE = 512;
    static constexpr int RING_BUFFER_SLOTS = 16;

    WasmAudioInput() FL_NO_EXCEPT;
    ~WasmAudioInput() override;

    // audio::IInput interface
    void start() FL_NO_EXCEPT override;
    void stop() FL_NO_EXCEPT override;
    bool error(fl::string* msg = nullptr) FL_NO_EXCEPT override;
    audio::Sample read() FL_NO_EXCEPT override;

    // Called from JavaScript via EMSCRIPTEN_KEEPALIVE
    void pushSamples(const fl::i16* samples, int count, fl::u32 timestamp) FL_NO_EXCEPT;

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
    fl::u32 mPushedBlocks = 0;   // Total blocks received from JS
    fl::u32 mReadBlocks = 0;     // Total blocks consumed by sketch

    // Accumulation buffer for sub-BLOCK_SIZE pushes (e.g. AudioWorklet sends 128)
    fl::i16 mAccumBuf[BLOCK_SIZE];
    int mAccumPos = 0;
    fl::u32 mAccumTimestamp = 0;

    void flushAccumBuffer() FL_NO_EXCEPT;
    bool isFull() const FL_NO_EXCEPT;
    bool isEmpty() const FL_NO_EXCEPT;
    int nextIndex(int index) const FL_NO_EXCEPT;
};

// Factory function for creating WASM audio input
fl::shared_ptr<audio::IInput> wasm_create_audio_input(const audio::Config& config, fl::string* error_message = nullptr) FL_NO_EXCEPT;

// Get the global WASM audio input instance (for integration with UIAudio)
WasmAudioInput* wasm_get_audio_input() FL_NO_EXCEPT;

} // namespace fl

#endif // FL_IS_WASM
