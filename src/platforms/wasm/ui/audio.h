#pragma once

#include <stdint.h>

#include "fl/audio.h"
#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"
#include <string>
#include <vector>

namespace fl {

enum {
    kJsAudioSamples = 512,
};

class jsAudioImpl {
  public:
    jsAudioImpl(const fl::string &name);
    ~jsAudioImpl();

    void setGroup(const fl::string& name);

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    AudioSample next();
    bool hasNext();
    const fl::string &groupName() const { return mInternal->group(); }

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
    fl::vector<AudioSampleImplPtr> mAudioSampleImpls;
    std::string mSerializeBuffer;
    std::vector<int16_t> mAudioDataBuffer;
};

} // namespace fl
