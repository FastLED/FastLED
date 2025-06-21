#pragma once

#include <stdint.h>

#include "fl/audio.h"
#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"
#include "fl/vector.h"

namespace fl {

enum {
    kJsAudioSamples = 512,
};

class jsAudioImpl {
  public:
    jsAudioImpl(const fl::string &name);
    ~jsAudioImpl();
    jsAudioImpl &Group(const fl::string &name) {
        mGroup = name;
        return *this;
    }

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    AudioSample next();
    bool hasNext();
    const fl::string &groupName() const { return mGroup; }
    
    // Method to allow parent UIBase class to set the group
    void setGroupInternal(const fl::string &groupName) { mGroup = groupName; }

  private:
    struct Updater : fl::EngineEvents::Listener {
        void init(jsAudioImpl *owner) {
            mOwner = owner;
            fl::EngineEvents::addListener(this);
        }
        ~Updater() { fl::EngineEvents::removeListener(this); }
        void onPlatformPreLoop2() override {}
        jsAudioImpl *mOwner = nullptr;
    };

    Updater mUpdater;

    void updateInternal(const FLArduinoJson::JsonVariantConst &value);

    jsUiInternalPtr mInternal;
    fl::string mGroup;
    fl::vector<AudioSampleImplPtr> mAudioSampleImpls;
    fl::string mSerializeBuffer;
    fl::vector<int16_t> mAudioDataBuffer;
};

} // namespace fl
