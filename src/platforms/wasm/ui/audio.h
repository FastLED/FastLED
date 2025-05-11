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
    jsAudioImpl(const fl::Str &name);
    ~jsAudioImpl();
    jsAudioImpl &Group(const fl::Str &name) {
        mGroup = name;
        return *this;
    }

    const fl::Str &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    const fl::Str &groupName() const { return mGroup; }

    AudioSample next();
    bool hasNext();

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
    fl::Str mGroup;
    fl::vector<AudioSampleImplPtr> mAudioSampleImpls;
    std::string mSerializeBuffer;
    std::vector<int16_t> mAudioDataBuffer;
};

} // namespace fl