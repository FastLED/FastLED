#pragma once

#include <stdint.h>

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsAudioImpl {
  public:
    jsAudioImpl(const fl::string &name, int sampleRate, int channels);
    ~jsAudioImpl();
    jsAudioImpl &Group(const fl::string &name) {
        mInternal->setGroup(name);
        return *this;
    }

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    void onData(const float *samples, size_t nSamples, int sampleRate,
                int nChannels);
    const fl::string &groupName() const { return mInternal->groupName(); }
    
    // Method to allow parent UIBase class to set the group
    void setGroupInternal(const fl::string &groupName) { mInternal->setGroup(groupName); }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);

    jsUiInternalPtr mInternal;
    int mSampleRate;
    int mChannels;
};

} // namespace fl
