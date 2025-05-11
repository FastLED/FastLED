
#ifdef __EMSCRIPTEN__

#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include "fl/json.h"
#include "fl/namespace.h"

#include "fl/json.h"
#include "fl/thread_local.h"
#include "fl/warn.h"
#include "platforms/wasm/ui/audio.h"
#include "platforms/wasm/ui/ui_manager.h"

using namespace fl;

namespace fl {

jsAudioImpl::jsAudioImpl(const Str &name) {
    auto updateFunc = jsUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &value) {
            static_cast<jsAudioImpl *>(this)->updateInternal(value);
        });

    auto toJsonFunc =
        jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<jsAudioImpl *>(this)->toJson(json);
        });
    mInternal = jsUiInternalPtr::New(name, std::move(updateFunc),
                                     std::move(toJsonFunc));
    jsUiManager::addComponent(mInternal);
    mUpdater.init(this);
}

jsAudioImpl::~jsAudioImpl() { jsUiManager::removeComponent(mInternal); }

const Str &jsAudioImpl::name() const { return mInternal->name(); }

void jsAudioImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["group"] = mGroup.c_str();
    json["type"] = "audio";
    json["id"] = mInternal->id();
}

static void parseJsonStringToInt16Vector(const std::string &jsonStr,
                                         std::vector<int16_t> *audioData) {
    audioData->clear();

    size_t i = 0, n = jsonStr.size();
    // find the opening '['
    while (i < n && jsonStr[i] != '[')
        ++i;
    if (i == n)
        return; // no array found
    ++i;        // skip '['

    while (i < n) {
        // skip whitespace
        while (i < n && std::isspace(jsonStr[i]))
            ++i;
        // check for closing ']'
        if (i < n && jsonStr[i] == ']')
            break;

        // parse optional sign
        bool negative = false;
        if (jsonStr[i] == '-') {
            negative = true;
            ++i;
        } else if (jsonStr[i] == '+') {
            ++i;
        }

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
        if (negative)
            value = -value;
        audioData->push_back(static_cast<int16_t>(value));

        // skip whitespace
        while (i < n && std::isspace(jsonStr[i]))
            ++i;
        // skip comma (if any)
        if (i < n && jsonStr[i] == ',')
            ++i;
    }
}

void jsAudioImpl::updateInternal(const FLArduinoJson::JsonVariantConst &value) {
    // FASTLED_WARN("Unimplemented jsAudioImpl::updateInternal");
    // std::string s = value.as<std::string>();
    mSerializeBuffer.clear();
    serializeJson(value, mSerializeBuffer);
    // std::vector<int16_t> audio_data;
    mAudioDataBuffer.clear();
    parseJsonStringToInt16Vector(mSerializeBuffer, &mAudioDataBuffer);
    // kJsAudioSamples;
    int offset = 0;
    int size = mAudioDataBuffer.size();
    // take in the data and break it up into chunks of kJsAudioSamples
    for (int i = 0; i < size; i += kJsAudioSamples) {
        AudioSampleImplPtr sample = NewPtr<AudioSampleImpl>();
        sample->assign(mAudioDataBuffer.begin() + i,
                       mAudioDataBuffer.begin() +
                           MIN(i + kJsAudioSamples, size));
        mAudioSampleImpls.push_back(sample);
        while (mAudioSampleImpls.size() > 10) {
            mAudioSampleImpls.erase(mAudioSampleImpls.begin());
        }
    }
}

AudioSample jsAudioImpl::next() {
    Ptr<AudioSampleImpl> out;
    if (mAudioSampleImpls.empty()) {
        // FASTLED_WARN("No audio samples available");
        return out;
    }
    // auto sample = mAudioSampleImpls.back();
    // mAudioSampleImpls.pop_back();
    out = mAudioSampleImpls.front();
    mAudioSampleImpls.erase(mAudioSampleImpls.begin());
    // FASTLED_WARN("Returning audio sample of size " << out->pcm().size());
    return AudioSample(out);
}

bool jsAudioImpl::hasNext() { return !mAudioSampleImpls.empty(); }

} // namespace fl

#endif // __EMSCRIPTEN__
