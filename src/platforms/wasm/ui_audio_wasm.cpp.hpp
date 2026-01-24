#ifdef __EMSCRIPTEN__

#include "platforms/wasm/ui_audio_wasm.h"
#include "platforms/wasm/audio_input_wasm.hpp"
#include "fl/warn.h"
#include "platforms/shared/ui/json/audio_internal.h"
#include "platforms/shared/ui/json/ui.h"

namespace fl {

// Private helper to initialize WasmAudioInput
static void initWasmAudio(const char* name, WasmAudioInput*& wasmInput, bool& ownsInput) {
    // Get or create the global WASM audio input
    wasmInput = wasm_get_audio_input();

    if (!wasmInput) {
        // Create the WASM audio input if it doesn't exist
        fl::string error;
        // Use dummy I2S config (config is ignored for WASM anyway)
        AudioConfigI2S dummyConfig(0, 0, 0, 0, fl::AudioChannel::Left, 44100, 16);
        auto audioInput = wasm_create_audio_input(fl::AudioConfig(dummyConfig), &error);
        if (audioInput) {
            audioInput->start();
            wasmInput = wasm_get_audio_input();  // Get the created instance
            ownsInput = true;
            FL_WARN("WasmAudioImpl: Created and started WasmAudioInput for '" << name << "'");
        } else {
            FL_WARN("WasmAudioImpl: Failed to create WasmAudioInput: " << error.c_str());
        }
    } else {
        // Ensure the existing input is started
        wasmInput->start();
        FL_WARN("WasmAudioImpl: Using existing WasmAudioInput for '" << name << "'");
    }
}

WasmAudioImpl::WasmAudioImpl(const char *name)
    : mName(name)
    , mWasmInput(nullptr)
    , mOwnsInput(false)
{
    // Create UI component for JSON UI system
    mInternal = fl::make_shared<JsonUiAudioInternal>(fl::string(name));
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));

    // Initialize WASM audio input
    initWasmAudio(name, mWasmInput, mOwnsInput);
}

WasmAudioImpl::WasmAudioImpl(const char *name, const fl::AudioConfig& config)
    : mName(name)
    , mWasmInput(nullptr)
    , mOwnsInput(false)
{
    // Create UI component for JSON UI system
    mInternal = fl::make_shared<JsonUiAudioInternal>(fl::string(name));
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));

    // Config is ignored for WASM - audio comes from JavaScript
    (void)config;

    // Initialize WASM audio input
    initWasmAudio(name, mWasmInput, mOwnsInput);
}

WasmAudioImpl::~WasmAudioImpl() {
    // Remove UI component from JSON UI system
    removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));

    // Note: We don't stop or destroy the WasmAudioInput here because:
    // 1. It's a global singleton that may be used by other components
    // 2. JavaScript continues to push samples regardless of C++ lifecycle
    // 3. The WasmAudioInput is cleaned up when the module unloads
}

AudioSample WasmAudioImpl::next() {
    if (!mWasmInput) {
        return AudioSample();  // Return invalid sample
    }

    return mWasmInput->read();
}

bool WasmAudioImpl::hasNext() {
    // On WASM, audio samples are pushed from JavaScript asynchronously
    // We always return true since samples may arrive at any time
    // The actual read() will return an invalid sample if the buffer is empty
    return mWasmInput != nullptr;
}

void WasmAudioImpl::setGroup(const fl::string& groupName) {
    if (mInternal) {
        mInternal->setGroup(groupName);
    }
}

} // namespace fl

#endif // __EMSCRIPTEN__
