#ifdef __EMSCRIPTEN__

#include "fl/json.h"
#include "fl/namespace.h"

#include "platforms/wasm/ui/audio.h"
#include "platforms/wasm/ui/ui_manager.h"

using namespace fl;

namespace fl {

jsAudioImpl::jsAudioImpl(const Str &name, int sampleRate, int channels)
    : mSampleRate(sampleRate), mChannels(channels) {
    auto updateFunc = jsUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &value) {
            static_cast<jsAudioImpl *>(this)->updateInternal(value);
        });

    auto toJsonFunc =
        jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<jsAudioImpl *>(this)->toJson(json);
        });
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc), std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
}

jsAudioImpl::~jsAudioImpl() { jsUiManager::removeComponent(mInternal); }

const Str &jsAudioImpl::name() const { return mInternal->name(); }

void jsAudioImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["type"] = "audio";
    json["id"] = mInternal->id();
    json["sampleRate"] = mSampleRate;
    json["channels"] = mChannels;
}

void jsAudioImpl::onData(const float *samples, size_t nSamples, int sampleRate,
                         int nChannels) {
    // Implementation for handling audio data
    // This would typically process the audio samples and update the UI
}

void jsAudioImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    // Handle updates from the UI if needed
}

} // namespace fl

#endif // __EMSCRIPTEN__
