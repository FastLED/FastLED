#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"

#include "fl/audio/audio.h"
#include "fl/audio/audio_input.h"
#include "fl/system/engine_events.h"
#include "fl/stl/string.h"
#include "fl/stl/url.h"
#include "platforms/shared/ui/json/ui_internal.h"

#include "fl/stl/vector.h"
#include "fl/stl/json.h"

#include "platforms/shared/ui/json/audio_internal.h"
#include "fl/stl/noexcept.h"

namespace fl {

enum {
    kJsAudioSamples = 512,
};



class JsonAudioImpl {
  public:
    JsonAudioImpl(const fl::string &name) FL_NOEXCEPT;
    JsonAudioImpl(const fl::string &name, const fl::url& url) FL_NOEXCEPT;
    JsonAudioImpl(const fl::string &name, const fl::audio::Config& config) FL_NOEXCEPT;
    ~JsonAudioImpl();
    JsonAudioImpl &Group(const fl::string &name) FL_NOEXCEPT;

    const fl::string &name() const FL_NOEXCEPT;
    audio::Sample next() FL_NOEXCEPT;
    bool hasNext() FL_NOEXCEPT;
    fl::string groupName() const FL_NOEXCEPT;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName) FL_NOEXCEPT;

    // Stub: no underlying audio input for JSON UI
    fl::shared_ptr<audio::IInput> audioInput() FL_NOEXCEPT { return nullptr; }

    int id() const FL_NOEXCEPT {
      return mInternal->id();
    }

  private:
    fl::shared_ptr<JsonUiAudioInternal> mInternal;
    struct Updater : fl::EngineEvents::Listener {
        void init(JsonAudioImpl *owner) FL_NOEXCEPT;
        ~Updater();
        void onPlatformPreLoop2() FL_NOEXCEPT override;
        JsonAudioImpl *mOwner = nullptr;
    };
    Updater mUpdater;
};

    

} // namespace fl
