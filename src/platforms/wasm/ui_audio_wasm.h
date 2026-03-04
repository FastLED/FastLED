#pragma once

// IWYU pragma: private

#include "is_wasm.h"
#ifdef FL_IS_WASM

#include "fl/stl/string.h"
#include "fl/stl/url.h"
#include "fl/audio.h"
#include "fl/audio_input.h"  // For AudioConfig
#include "fl/stl/shared_ptr.h"

namespace fl {

class WasmAudioInput;
class JsonUiAudioInternal;

/**
 * WASM-specific UIAudio implementation
 * Directly wraps WasmAudioInput to read from JavaScript-provided audio samples
 */
class WasmAudioImpl {
  public:
    WasmAudioImpl(const fl::string& name);
    WasmAudioImpl(const fl::string& name, const fl::url& url);
    WasmAudioImpl(const fl::string& name, const fl::AudioConfig& config);
    ~WasmAudioImpl();

    AudioSample next();
    bool hasNext();

    // Group setting (used by JSON UI system)
    void setGroup(const fl::string& groupName);

    // Expose underlying audio input for FastLED.add() auto-pump
    fl::shared_ptr<IAudioInput> audioInput() { return mWasmInputOwner; }

  private:
    fl::string mName;
    WasmAudioInput* mWasmInput;
    fl::shared_ptr<IAudioInput> mWasmInputOwner;  // Prevent premature destruction
    bool mOwnsInput;  // Track if we created the input
    fl::shared_ptr<JsonUiAudioInternal> mInternal;  // UI registration component
};

} // namespace fl

#endif // FL_IS_WASM
