#ifdef __EMSCRIPTEN__

#include "audio_input_wasm.hpp"
#include "fl/dbg.h"
#include "fl/stl/stdio.h"
#include "fl/str.h"
#include <emscripten.h>
#include <emscripten/emscripten.h>

namespace fl {

// Global instance pointer for C callback
static WasmAudioInput* g_wasmAudioInput = nullptr;

WasmAudioInput::WasmAudioInput()
    : mHead(0)
    , mTail(0)
    , mRunning(false)
    , mHasError(false)
    , mDroppedBlocks(0)
{
    // Initialize ring buffer
    for (int i = 0; i < RING_BUFFER_SLOTS; i++) {
        mRingBuffer[i].valid = false;
    }

    // Set global instance for C callback
    g_wasmAudioInput = this;

    FL_DBG("WasmAudioInput created - ring buffer: " << RING_BUFFER_SLOTS << " slots x " << BLOCK_SIZE << " samples");
}

WasmAudioInput::~WasmAudioInput() {
    stop();
    if (g_wasmAudioInput == this) {
        g_wasmAudioInput = nullptr;
    }
}

void WasmAudioInput::start() {
    if (mRunning) {
        FL_DBG("WasmAudioInput already running - skipping start");
        return;  // Already running, don't re-initialize
    }

    mRunning = true;
    mHasError = false;
    mErrorMessage.clear();
    FL_DBG("WasmAudioInput started");
}

void WasmAudioInput::stop() {
    mRunning = false;
    // Clear ring buffer
    mHead = 0;
    mTail = 0;
    for (int i = 0; i < RING_BUFFER_SLOTS; i++) {
        mRingBuffer[i].valid = false;
    }
    FL_DBG("WasmAudioInput stopped");
}

bool WasmAudioInput::error(fl::string* msg) {
    if (msg && mHasError) {
        *msg = mErrorMessage;
    }
    return mHasError;
}

AudioSample WasmAudioInput::read() {
    if (!mRunning || isEmpty()) {
        return AudioSample();  // Return invalid sample
    }

    // Read from tail
    AudioBlock& block = mRingBuffer[mTail];

    if (!block.valid) {
        return AudioSample();  // Shouldn't happen, but safety check
    }

    // Create AudioSample from block data
    fl::span<const fl::i16> samples(block.samples, BLOCK_SIZE);
    AudioSample result(samples, block.timestamp);

    // Mark block as consumed and advance tail
    block.valid = false;
    mTail = nextIndex(mTail);

    return result;
}

void WasmAudioInput::pushSamples(const fl::i16* samples, int count, fl::u32 timestamp) {
    if (!mRunning) {
        return;
    }

    if (count != BLOCK_SIZE) {
        FL_WARN("WasmAudioInput::pushSamples - unexpected block size: " << count << " (expected " << BLOCK_SIZE << ")");
        return;
    }

    if (isFull()) {
        // Ring buffer is full - drop oldest block
        mDroppedBlocks++;
        if (mDroppedBlocks % 100 == 1) {  // Log every 100 drops
            FL_WARN("WasmAudioInput ring buffer overflow - dropped " << mDroppedBlocks << " blocks total");
        }
        // Advance tail to drop oldest
        mTail = nextIndex(mTail);
    }

    // Write to head
    AudioBlock& block = mRingBuffer[mHead];
    fl::memcpy(block.samples, samples, BLOCK_SIZE * sizeof(fl::i16));
    block.timestamp = timestamp;
    block.valid = true;

    // Advance head
    mHead = nextIndex(mHead);
}

bool WasmAudioInput::isFull() const {
    return nextIndex(mHead) == mTail;
}

bool WasmAudioInput::isEmpty() const {
    return mHead == mTail && !mRingBuffer[mTail].valid;
}

int WasmAudioInput::nextIndex(int index) const {
    return (index + 1) % RING_BUFFER_SLOTS;
}

fl::shared_ptr<IAudioInput> wasm_create_audio_input(const AudioConfig& config, fl::string* error_message) {
    // Config is ignored for WASM - audio comes from JavaScript
    (void)config;

    auto input = fl::make_shared<WasmAudioInput>();

    if (error_message) {
        error_message->clear();
    }

    FL_DBG("Created WASM audio input");
    return input;
}

WasmAudioInput* wasm_get_audio_input() {
    return g_wasmAudioInput;
}

} // namespace fl

// C API for JavaScript
extern "C" {

/**
 * Push audio samples from JavaScript to C++ ring buffer
 * Called via Module.ccall('pushAudioSamples', ...)
 *
 * @param samples Pointer to Int16 PCM samples in WASM heap
 * @param count Number of samples (should be 512)
 * @param timestamp Timestamp in milliseconds
 */
EMSCRIPTEN_KEEPALIVE
void pushAudioSamples(const int16_t* samples, int count, uint32_t timestamp) {
    if (!fl::g_wasmAudioInput) {
        // No audio input instance created yet
        return;
    }

    if (!samples) {
        FL_WARN("pushAudioSamples called with null samples pointer");
        return;
    }

    fl::g_wasmAudioInput->pushSamples(samples, count, timestamp);
}

} // extern "C"

#endif // __EMSCRIPTEN__
