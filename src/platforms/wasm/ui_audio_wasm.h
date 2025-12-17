#pragma once

#ifdef __EMSCRIPTEN__

#include "fl/stl/string.h"
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
    WasmAudioImpl(const char *name);
    WasmAudioImpl(const char *name, const fl::AudioConfig& config);
    ~WasmAudioImpl();

    AudioSample next();
    bool hasNext();

    // Group setting (used by JSON UI system)
    void setGroup(const fl::string& groupName);

  private:
    fl::string mName;
    WasmAudioInput* mWasmInput;
    bool mOwnsInput;  // Track if we created the input
    fl::shared_ptr<JsonUiAudioInternal> mInternal;  // UI registration component
};

} // namespace fl

#endif // __EMSCRIPTEN__
