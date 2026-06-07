// IWYU pragma: private

#include "platforms/wasm/is_wasm.h"

#ifdef FL_IS_WASM

#include "platforms/wasm/audio_input_wasm.hpp"
#include "fl/log/log.h"
#include "fl/log/log.h"
#include "fl/stl/stdio.h"
#include "fl/stl/string.h"
// IWYU pragma: begin_keep
#include <emscripten.h>
#include <emscripten/emscripten.h>
// IWYU pragma: end_keep

namespace fl {

// Global instance pointer for C callback
static WasmAudioInput* g_wasmAudioInput = nullptr;

WasmAudioInput::WasmAudioInput()
    : mHead(0)
    , mTail(0)
    , mRunning(false)
    , mHasError(false)
    , mDroppedBlocks(0)
    , mAccumPos(0)
    , mAccumTimestamp(0)
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

audio::Sample WasmAudioInput::read() {
    if (!mRunning || isEmpty()) {
        return audio::Sample();  // Return invalid sample
    }

    // Read from tail
    AudioBlock& block = mRingBuffer[mTail];

    if (!block.valid) {
        return audio::Sample();  // Shouldn't happen, but safety check
    }

    // Create audio::Sample from block data
    fl::span<const fl::i16> samples(block.samples, BLOCK_SIZE);
    audio::Sample result(samples, block.timestamp);

    // Mark block as consumed and advance tail
    block.valid = false;
    mTail = nextIndex(mTail);
    mReadBlocks++;

    if (mReadBlocks == 1) {
        fl::printf("WasmAudioInput: First audio block consumed by sketch "
               "(timestamp=%u ms)\n", (unsigned)block.timestamp);
    }

    return result;
}

void WasmAudioInput::pushSamples(const fl::i16* samples, int count, fl::u32 timestamp) {
    if (!mRunning) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            fl::printf("WasmAudioInput: pushSamples called but not running - "
                   "audio data is being dropped!\n");
        }
        return;
    }

    if (count <= 0 || count > BLOCK_SIZE) {
        FL_WARN("WasmAudioInput::pushSamples - invalid block size: " << count << " (max " << BLOCK_SIZE << ")");
        return;
    }

    // Accumulate samples until we have a full BLOCK_SIZE block.
    // AudioWorklet sends 128-sample frames; ScriptProcessor sends 512.
    int srcPos = 0;
    while (srcPos < count) {
        if (mAccumPos == 0) {
            mAccumTimestamp = timestamp;
        }
        int space = BLOCK_SIZE - mAccumPos;
        int toCopy = (count - srcPos) < space ? (count - srcPos) : space;
        fl::memcpy(mAccumBuf + mAccumPos, samples + srcPos, toCopy * sizeof(fl::i16));
        mAccumPos += toCopy;
        srcPos += toCopy;

        if (mAccumPos >= BLOCK_SIZE) {
            flushAccumBuffer();
        }
    }
}

void WasmAudioInput::flushAccumBuffer() {
    if (isFull()) {
        mDroppedBlocks++;
        if (mDroppedBlocks % 100 == 1) {
            FL_WARN("WasmAudioInput ring buffer overflow - dropped " << mDroppedBlocks << " blocks total");
        }
        mRingBuffer[mTail].valid = false;
        mTail = nextIndex(mTail);
    }

    AudioBlock& block = mRingBuffer[mHead];
    fl::memcpy(block.samples, mAccumBuf, BLOCK_SIZE * sizeof(fl::i16));
    block.timestamp = mAccumTimestamp;
    block.valid = true;

    mHead = nextIndex(mHead);
    mPushedBlocks++;
    mAccumPos = 0;

    if (mPushedBlocks == 1) {
        fl::printf("WasmAudioInput: First audio block received from JS "
               "(timestamp=%u ms)\n", (unsigned)mAccumTimestamp);
    } else if (mPushedBlocks % 172 == 0) {
        fl::printf("WasmAudioInput: %u blocks received, %u read, %u dropped\n",
               (unsigned)mPushedBlocks, (unsigned)mReadBlocks,
               (unsigned)mDroppedBlocks);
    }
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

fl::shared_ptr<audio::IInput> wasm_create_audio_input(const audio::Config& config, fl::string* error_message) {
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
void pushAudioSamples(const fl::i16* samples, int count, fl::u32 timestamp) {
    if (!fl::g_wasmAudioInput) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            fl::printf("pushAudioSamples: No WasmAudioInput instance - "
                   "UIAudio not created yet. Audio data dropped!\n");
        }
        return;
    }

    if (!samples) {
        FL_WARN("pushAudioSamples called with null samples pointer");
        return;
    }

    fl::g_wasmAudioInput->pushSamples(samples, count, timestamp);
}

} // extern "C"

#endif // FL_IS_WASM
