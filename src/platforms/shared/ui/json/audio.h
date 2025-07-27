#pragma once

#include "fl/stdint.h"

#include "fl/audio.h"
#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"

#include "fl/vector.h"
#include "fl/json.h"

#include "platforms/shared/ui/json/audio_internal.h"

namespace fl {

enum {
    kJsAudioSamples = 512,
};



class JsonAudioImpl {
  public:
    JsonAudioImpl(const fl::string &name);
    ~JsonAudioImpl();
    JsonAudioImpl &Group(const fl::string &name);

    const fl::string &name() const;
    AudioSample next();
    bool hasNext();
    const fl::string &groupName() const;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

    int id() const {
      return mInternal->id();
    }

  private:
    fl::shared_ptr<JsonUiAudioInternal> mInternal;
    struct Updater : fl::EngineEvents::Listener {
        void init(JsonAudioImpl *owner);
        ~Updater();
        void onPlatformPreLoop2() override;
        JsonAudioImpl *mOwner = nullptr;
    };
    Updater mUpdater;
};

    

} // namespace fl
