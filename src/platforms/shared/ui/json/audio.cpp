#include "platforms/shared/ui/json/audio.h"
#include "fl/json.h"
#include "fl/string.h"
#include "fl/thread_local.h"
#include "fl/warn.h"
#include "platforms/shared/ui/json/ui.h"

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

const fl::string &JsonAudioImpl::groupName() const {
    return mInternal->groupName();
}

void JsonAudioImpl::setGroup(const fl::string &groupName) {
    mInternal->setGroup(groupName);
}

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
static bool isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

struct AudioBuffer {
    fl::vector<int16_t> samples;
    uint32_t timestamp;
};

static void parseJsonStringToAudioBuffers(const fl::string &jsonStr,
                                         fl::vector<AudioBuffer> *audioBuffers) {
    audioBuffers->clear();
    
    // Parse the JSON string manually to extract audio buffer objects
    // Expected format: [{"samples": [1,2,3...], "timestamp": 123456}, ...]
    
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
        
        // look for object start '{'
        if (i < n && jsonStr[i] == '{') {
            AudioBuffer buffer;
            ++i; // skip '{'
            
            // Parse object contents - look for "samples" and "timestamp"
            while (i < n && jsonStr[i] != '}') {
                // skip whitespace
                while (i < n && isspace(jsonStr[i]))
                    ++i;
                
                // look for "samples" or "timestamp"
                if (i + 8 < n && 
                    jsonStr[i] == '"' && jsonStr[i+1] == 's' && jsonStr[i+2] == 'a' && 
                    jsonStr[i+3] == 'm' && jsonStr[i+4] == 'p' && jsonStr[i+5] == 'l' && 
                    jsonStr[i+6] == 'e' && jsonStr[i+7] == 's' && jsonStr[i+8] == '"') {
                    i += 9;
                    // skip whitespace and ':'
                    while (i < n && (isspace(jsonStr[i]) || jsonStr[i] == ':'))
                        ++i;
                    // parse array of samples manually for performance
                    if (i < n && jsonStr[i] == '[') {
                        ++i; // skip '['
                        while (i < n && jsonStr[i] != ']') {
                            // skip whitespace
                            while (i < n && isspace(jsonStr[i]))
                                ++i;
                            if (i < n && jsonStr[i] == ']') break;
                            
                            // parse number
                            bool negative = false;
                            if (jsonStr[i] == '-') {
                                negative = true;
                                ++i;
                            } else if (jsonStr[i] == '+') {
                                ++i;
                            }
                            
                            int value = 0;
                            bool hasDigits = false;
                            while (i < n && isdigit(static_cast<unsigned char>(jsonStr[i]))) {
                                hasDigits = true;
                                value = value * 10 + (jsonStr[i] - '0');
                                ++i;
                            }
                            if (hasDigits) {
                                if (negative) value = -value;
                                buffer.samples.push_back(static_cast<int16_t>(value));
                            }
                            
                            // skip whitespace and comma
                            while (i < n && (isspace(jsonStr[i]) || jsonStr[i] == ','))
                                ++i;
                        }
                        if (i < n && jsonStr[i] == ']') ++i; // skip ']'
                    }
                } else if (i + 10 < n && 
                    jsonStr[i] == '"' && jsonStr[i+1] == 't' && jsonStr[i+2] == 'i' && 
                    jsonStr[i+3] == 'm' && jsonStr[i+4] == 'e' && jsonStr[i+5] == 's' && 
                    jsonStr[i+6] == 't' && jsonStr[i+7] == 'a' && jsonStr[i+8] == 'm' && 
                    jsonStr[i+9] == 'p' && jsonStr[i+10] == '"') {
                    i += 11;
                    // skip whitespace and ':'
                    while (i < n && (isspace(jsonStr[i]) || jsonStr[i] == ':'))
                        ++i;
                    // parse timestamp number
                    uint32_t timestamp = 0;
                    while (i < n && isdigit(static_cast<unsigned char>(jsonStr[i]))) {
                        timestamp = timestamp * 10 + (jsonStr[i] - '0');
                        ++i;
                    }
                    buffer.timestamp = timestamp;
                } else {
                    // skip unknown property
                    ++i;
                }
                
                // skip whitespace and comma
                while (i < n && (isspace(jsonStr[i]) || jsonStr[i] == ','))
                    ++i;
            }
            if (i < n && jsonStr[i] == '}') ++i; // skip '}'
            
            // Add buffer if it has samples
            if (!buffer.samples.empty()) {
                audioBuffers->push_back(fl::move(buffer));
            }
        } else {
            ++i; // skip unknown character
        }
        
        // skip whitespace and comma
        while (i < n && (isspace(jsonStr[i]) || jsonStr[i] == ','))
            ++i;
    }
}

void JsonAudioImpl::updateInternal(
    const FLArduinoJson::JsonVariantConst &value) {
    // FASTLED_WARN("Unimplemented jsAudioImpl::updateInternal");
    mSerializeBuffer.clear();
    serializeJson(value, mSerializeBuffer);
    
    // Parse audio buffers with timestamps
    fl::vector<AudioBuffer> audioBuffers;
    parseJsonStringToAudioBuffers(mSerializeBuffer, &audioBuffers);
    
    // Convert each audio buffer to AudioSample objects
    for (const auto& buffer : audioBuffers) {
        const fl::vector<int16_t>& samples = buffer.samples;
        uint32_t timestamp = buffer.timestamp;
        int size = samples.size();
        
        // Break up the data into chunks of kJsAudioSamples
        for (int i = 0; i < size; i += kJsAudioSamples) {
            AudioSampleImplPtr sample = NewPtr<AudioSampleImpl>();
            int endIdx = MIN(i + kJsAudioSamples, size);
            sample->assign(samples.begin() + i, samples.begin() + endIdx, timestamp);
            mAudioSampleImpls.push_back(sample);
            while (mAudioSampleImpls.size() > 10) {
                mAudioSampleImpls.erase(mAudioSampleImpls.begin());
            }
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
