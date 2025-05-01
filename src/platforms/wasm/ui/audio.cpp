
#ifdef __EMSCRIPTEN__

#include <string>
#include <vector>
#include <cctype>
#include <cstdint>

#include "fl/json.h"
#include "fl/namespace.h"

#include "platforms/wasm/ui/ui_manager.h"
#include "platforms/wasm/ui/audio.h"
#include "fl/warn.h"
#include "fl/json.h"

using namespace fl;

namespace fl {

jsAudioImpl::jsAudioImpl(const Str& name) {
    auto updateFunc = jsUiInternal::UpdateFunction([this](const FLArduinoJson::JsonVariantConst& value) {
        static_cast<jsAudioImpl*>(this)->updateInternal(value);
    });

    auto toJsonFunc = jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject& json) {
        static_cast<jsAudioImpl*>(this)->toJson(json);
    });
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc), std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
    mUpdater.init(this);
}

jsAudioImpl::~jsAudioImpl() {
    jsUiManager::removeComponent(mInternal);
}

const Str& jsAudioImpl::name() const {
    return mInternal->name();
}

void jsAudioImpl::toJson(FLArduinoJson::JsonObject& json) const {
    json["name"] = name();
    json["group"] = mGroup.c_str();
    json["type"] = "audio";
    json["id"] = mInternal->id();
}


static void parseJsonStringToInt16Vector(
    const std::string& jsonStr,
    std::vector<int16_t>* audioData
) {
    audioData->clear();

    size_t i = 0, n = jsonStr.size();
    // find the opening '['
    while (i < n && jsonStr[i] != '[') ++i;
    if (i == n) return;      // no array found
    ++i;                     // skip '['

    while (i < n) {
        // skip whitespace
        while (i < n && std::isspace(jsonStr[i])) ++i;
        // check for closing ']'
        if (i < n && jsonStr[i] == ']') break;

        // parse optional sign
        bool negative = false;
        if      (jsonStr[i] == '-') { negative = true; ++i; }
        else if (jsonStr[i] == '+') { ++i; }

        // accumulate digits
        int value = 0;
        bool hasDigits = false;
        while (i < n && std::isdigit(static_cast<unsigned char>(jsonStr[i]))) {
            hasDigits = true;
            value = value * 10 + (jsonStr[i] - '0');
            ++i;
        }
        if (!hasDigits) {
            // malformed? skip this char and continue
            ++i;
            continue;
        }
        if (negative) value = -value;
        audioData->push_back(static_cast<int16_t>(value));

        // skip whitespace
        while (i < n && std::isspace(jsonStr[i])) ++i;
        // skip comma (if any)
        if (i < n && jsonStr[i] == ',') ++i;
    }
}

void jsAudioImpl::updateInternal(const FLArduinoJson::JsonVariantConst& value) {
    FASTLED_WARN("Unimplemented jsAudioImpl::updateInternal");
    std::string s = value.as<std::string>();
    std::vector<int16_t> audio_data;
    parseJsonStringToInt16Vector(s, &audio_data);
    FASTLED_WARN("Found data of size " << audio_data.size() << " in jsAudioImpl::updateInternal");
    
    AudioSamplePtr sample = NewPtr<AudioSample>();
    sample->assign(audio_data.begin(), audio_data.end());
    FASTLED_WARN("assigning audio data of size " << audio_data.size());
    mAudioSamples.push_back(sample);
    while (mAudioSamples.size() > 10) {
        mAudioSamples.erase(mAudioSamples.begin());
    }
}




Ptr<const AudioSample> jsAudioImpl::next() {
    Ptr<const AudioSample> out;
    if (mAudioSamples.empty()) {
        FASTLED_WARN("No audio samples available");
        return out;
    }
    // auto sample = mAudioSamples.back();
    // mAudioSamples.pop_back();
    out = mAudioSamples.front();
    mAudioSamples.erase(mAudioSamples.begin());
    FASTLED_WARN("Returning audio sample of size " << out->pcm().size());
    return out;
}

}  // namespace fl

#endif  // __EMSCRIPTEN__
