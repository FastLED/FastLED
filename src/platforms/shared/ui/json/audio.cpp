#include "fl/thread_local.h"
#include "fl/warn.h"
#include "platforms/shared/ui/json/audio.h"
#include "fl/string.h"
#include "platforms/shared/ui/json/ui.h"
#include "fl/json.h"

#if FASTLED_ENABLE_JSON

using namespace fl;

namespace fl {

JsonAudioImpl::JsonAudioImpl(const fl::string &name) {
    auto updateFunc = JsonUiInternal::UpdateFunction(
        [this](const FLArduinoJson::JsonVariantConst &value) {
            static_cast<JsonAudioImpl *>(this)->updateInternal(value);
        });

    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<JsonAudioImpl *>(this)->toJson(json);
        });
    mInternal = JsonUiInternalPtr::New(name, fl::move(updateFunc),
                                     fl::move(toJsonFunc));
    mUpdater.init(this);
    addJsonUiComponent(mInternal);
}

JsonAudioImpl::~JsonAudioImpl() { removeJsonUiComponent(mInternal); }

JsonAudioImpl &JsonAudioImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonAudioImpl::name() const { return mInternal->name(); }

const fl::string &JsonAudioImpl::groupName() const { return mInternal->groupName(); }

void JsonAudioImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

void JsonAudioImpl::Updater::init(JsonAudioImpl *owner) {
    mOwner = owner;
    fl::EngineEvents::addListener(this);
}

JsonAudioImpl::Updater::~Updater() { fl::EngineEvents::removeListener(this); }

void JsonAudioImpl::Updater::onPlatformPreLoop2() {}

void JsonAudioImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = name();
    json["group"] = mInternal->groupName().c_str();
    json["type"] = "audio";
    json["id"] = mInternal->id();
    json["audioSamples"] = kJsAudioSamples;
    if (!mSerializeBuffer.empty()) {
        json["audioData"] = mSerializeBuffer.c_str();
    }
}

static bool isdigit(char c) { return c >= '0' && c <= '9'; }
static bool isspace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

static void parseJsonStringToInt16Vector(const fl::string &jsonStr,
                                         fl::vector<int16_t> *audioData) {
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
        while (i < n && isspace(jsonStr[i]))
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
        while (i < n && isdigit(static_cast<unsigned char>(jsonStr[i]))) {
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
        while (i < n && isspace(jsonStr[i]))
            ++i;
        // skip comma (if any)
        if (i < n && jsonStr[i] == ',')
            ++i;
    }
}

void JsonAudioImpl::updateInternal(const FLArduinoJson::JsonVariantConst &value) {
    // FASTLED_WARN("Unimplemented jsAudioImpl::updateInternal");
    mSerializeBuffer.clear();
    serializeJson(value, mSerializeBuffer);
    // fl::vector<int16_t> audio_data;
    mAudioDataBuffer.clear();
    parseJsonStringToInt16Vector(mSerializeBuffer, &mAudioDataBuffer);
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

AudioSample JsonAudioImpl::next() {
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

bool JsonAudioImpl::hasNext() { return !mAudioSampleImpls.empty(); }

} // namespace fl

#endif // FASTLED_ENABLE_JSON
