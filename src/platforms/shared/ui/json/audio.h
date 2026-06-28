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
    JsonAudioImpl(const fl::string &name) FL_NO_EXCEPT;
    JsonAudioImpl(const fl::string &name, const fl::url& url) FL_NO_EXCEPT;
    JsonAudioImpl(const fl::string &name, const fl::audio::Config& config) FL_NO_EXCEPT;
    ~JsonAudioImpl();
    JsonAudioImpl &Group(const fl::string &name) FL_NO_EXCEPT;

    const fl::string &name() const FL_NO_EXCEPT;
    audio::Sample next() FL_NO_EXCEPT;
    bool hasNext() FL_NO_EXCEPT;
    fl::string groupName() const FL_NO_EXCEPT;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName) FL_NO_EXCEPT;

    // Stub: no underlying audio input for JSON UI
    fl::shared_ptr<audio::IInput> audioInput() FL_NO_EXCEPT { return nullptr; }

    int id() const FL_NO_EXCEPT {
      return mInternal->id();
    }

  private:
    fl::shared_ptr<JsonUiAudioInternal> mInternal;
    struct Updater : fl::EngineEvents::Listener {
        void init(JsonAudioImpl *owner) FL_NO_EXCEPT;
        ~Updater();
        void onPlatformPreLoop2() FL_NO_EXCEPT override;
        JsonAudioImpl *mOwner = nullptr;
    };
    Updater mUpdater;
};

    

} // namespace fl
