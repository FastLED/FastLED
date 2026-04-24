#pragma once

#include "fl/asset.h"
#include "fl/audio/audio.h"
#include "fl/audio/audio_input.h"  // For audio::Config  // IWYU pragma: keep
#include "fl/stl/optional.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/url.h"
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"
#include "fl/ui/element.h"
#include "platforms/ui_defs.h"

#ifndef FASTLED_HAS_UI_AUDIO
#define FASTLED_HAS_UI_AUDIO 0
#endif

namespace fl {

#if !FASTLED_HAS_UI_AUDIO
class UIAudioImpl {
  public:
    UIAudioImpl(const fl::string& name) { FASTLED_UNUSED(name); }
    UIAudioImpl(const fl::string& name, const fl::url& url) {
        FASTLED_UNUSED(name);
        FASTLED_UNUSED(url);
    }
    UIAudioImpl(const fl::string& name, const fl::audio::Config& config) {
        FASTLED_UNUSED(name);
        FASTLED_UNUSED(config);
    }
    ~UIAudioImpl() FL_NOEXCEPT {}

    audio::Sample next() {
        FL_WARN("Audio sample not implemented");
        return audio::Sample();
    }

    bool hasNext() {
        FL_WARN("Audio sample not implemented");
        return false;
    }

    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }

    // Stub: no underlying audio input on non-WASM platforms
    fl::shared_ptr<audio::IInput> audioInput() { return nullptr; }
};
#endif

class UIAudio : public UIElement {
  public:
    FL_NO_COPY(UIAudio)
    UIAudio(const fl::string& name) FL_NOEXCEPT;
    UIAudio(const fl::string& name, const fl::url& url) FL_NOEXCEPT;
    /// Asset-handle overload (issue #2284). Resolves `asset` via
    /// `fl::resolve_asset()` at construction time: the manifest/registry
    /// is consulted first, then (on host/stub) the local filesystem and
    /// sibling `.lnk`. If resolution succeeds, forwards to the url()
    /// constructor; otherwise behaves like the name-only constructor.
    UIAudio(const fl::string& name, const fl::asset_ref& asset) FL_NOEXCEPT;
    UIAudio(const fl::string& name, const fl::audio::Config& config) FL_NOEXCEPT;
    ~UIAudio() FL_NOEXCEPT;
    audio::Sample next() FL_NOEXCEPT { return mImpl.next(); }
    bool hasNext() FL_NOEXCEPT { return mImpl.hasNext(); }

    // Expose underlying audio input for FastLED.add() auto-pump
    fl::shared_ptr<audio::IInput> audioInput() FL_NOEXCEPT { return mImpl.audioInput(); }

    // Returns the hardware microphone config, if one was provided.
    const fl::optional<audio::Config>& config() const FL_NOEXCEPT { return mConfig; }

    // Lazily registers with CFastLED::add() on first call and returns the
    // auto-pumped audio::Processor. Subsequent calls return the cached processor.
    fl::shared_ptr<audio::Processor> processor() FL_NOEXCEPT;

    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) FL_NOEXCEPT override {
        UIElement::setGroup(groupName);
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }

  protected:
    UIAudioImpl mImpl;
    fl::optional<audio::Config> mConfig;
    fl::shared_ptr<audio::Processor> mProcessor;
};

} // namespace fl
