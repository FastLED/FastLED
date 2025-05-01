#pragma once

#include <stdint.h>

#include "fl/engine_events.h"
#include "fl/str.h"
#include "fl/audio.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

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

    Ptr<const AudioSample> next() {
        Ptr<const AudioSample> out;
        if (mAudioSamples.empty()) {
            return out;
        }
        // auto sample = mAudioSamples.back();
        // mAudioSamples.pop_back();
        out = mAudioSamples.front();
        mAudioSamples.erase(mAudioSamples.begin());
        return out;
    }

  private:
    struct Updater : fl::EngineEvents::Listener {
        void init(jsAudioImpl *owner) {
            mOwner = owner;
            fl::EngineEvents::addListener(this);
        }
        ~Updater() { fl::EngineEvents::removeListener(this); }
        void onPlatformPreLoop2() override {

        }
        jsAudioImpl *mOwner = nullptr;
    };

    Updater mUpdater;

    void updateInternal(const FLArduinoJson::JsonVariantConst &value);

    jsUiInternalPtr mInternal;
    fl::Str mGroup;
    fl::vector<AudioSamplePtr> mAudioSamples;
};

} // namespace fl