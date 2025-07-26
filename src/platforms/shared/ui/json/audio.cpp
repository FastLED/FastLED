#include "platforms/shared/ui/json/audio.h"
#include "fl/json.h"
#include "fl/json2.h"
#include "fl/string.h"
#include "fl/thread_local.h"
#include "fl/warn.h"
#include "platforms/shared/ui/json/ui.h"

#if FASTLED_ENABLE_JSON


namespace fl {
// namespace {
//     fl::string& scratchBuffer() {
//         static fl::ThreadLocal<fl::string> buffer;
//         return buffer.access();
//     }
// }


JsonAudioImpl::JsonAudioImpl(const fl::string &name) {
    auto updateFunc = JsonUiInternal::UpdateFunction(
        [this](const fl::json2::Json &value) {
            static_cast<JsonAudioImpl *>(this)->updateInternal(value);
        });

    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this](fl::json2::Json &json) {
            static_cast<JsonAudioImpl *>(this)->toJson(json);
        });
    mInternal = fl::make_shared<JsonUiInternal>(name, fl::move(updateFunc),
                                       fl::move(toJsonFunc));
    mUpdater.init(this);
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonAudioImpl::~JsonAudioImpl() { removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal)); }

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

void JsonAudioImpl::toJson(fl::json2::Json &json) const {
    json.set("name", name());
    json.set("group", mInternal->groupName());
    json.set("type", "audio");
    json.set("id", mInternal->id());
    if (!mSerializeBuffer.empty()) {
        json.set("audioData", mSerializeBuffer);
    }
}

static bool isdigit(char c) { return c >= '0' && c <= '9'; }
static bool isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

struct AudioBuffer {
    fl::vector<int16_t> samples;
    uint32_t timestamp = 0;
};

// Fast manual parsing of PCM data from a samples array string
// Input: samples string like "[1,2,3,-4,5]"
// Output: fills the samples vector
static void parsePcmSamplesString(const fl::string &samplesStr, fl::vector<int16_t> *samples) {
    samples->clear();
    
    size_t i = 0, n = samplesStr.size();
    
    // Find opening '['
    while (i < n && samplesStr[i] != '[')
        ++i;
    if (i == n) return; // no array found
    ++i; // skip '['
    
    while (i < n && samplesStr[i] != ']') {
        // Skip whitespace
        while (i < n && isspace(samplesStr[i]))
            ++i;
        if (i >= n || samplesStr[i] == ']') break;
        
        // Parse number
        bool negative = false;
        if (samplesStr[i] == '-') {
            negative = true;
            ++i;
        } else if (samplesStr[i] == '+') {
            ++i;
        }
        
        int value = 0;
        bool hasDigits = false;
        while (i < n && isdigit(static_cast<unsigned char>(samplesStr[i]))) {
            hasDigits = true;
            value = value * 10 + (samplesStr[i] - '0');
            ++i;
        }
        
        if (hasDigits) {
            if (negative) value = -value;
            samples->push_back(static_cast<int16_t>(value));
        }
        
        // Skip whitespace and comma
        while (i < n && (isspace(samplesStr[i]) || samplesStr[i] == ','))
            ++i;
    }
}

static void parseJsonToAudioBuffers(const fl::json2::Json &json2_obj,
                                    fl::vector<AudioBuffer> *audioBuffers) {
    audioBuffers->clear();

    if (!json2_obj.is_array()) {
        return;
    }
    
    // Create a non-const copy for indexing
    fl::json2::Json array_copy = json2_obj;
    
    for (size_t i = 0; i < array_copy.size(); ++i) {
        fl::json2::Json item = array_copy[i];
        if (!item.is_object()) {
            continue;
        }
        
        AudioBuffer buffer;
        // Create a non-const copy for indexing
        fl::json2::Json item_copy = item;
        buffer.timestamp = item_copy["timestamp"] | 0u;
        
        fl::json2::Json samples_json2 = item_copy["samples"];
        if (samples_json2.is_array()) {
            fl::string samples_str = samples_json2.to_string();
            parsePcmSamplesString(samples_str, &buffer.samples);
        }
        
        if (!buffer.samples.empty()) {
            audioBuffers->push_back(fl::move(buffer));
        }
    }
}

void JsonAudioImpl::updateInternal(
    const fl::json2::Json &value) {
    // FASTLED_WARN("Unimplemented jsAudioImpl::updateInternal");
    mSerializeBuffer.clear();
    mSerializeBuffer = value.to_string();
    
    // Parse audio buffers with timestamps using hybrid JSON/manual parsing
    fl::vector<AudioBuffer> audioBuffers;
    // Create a non-const copy for parsing
    fl::json2::Json value_copy = value;
    parseJsonToAudioBuffers(value_copy, &audioBuffers);
    
    // Convert each audio buffer to a single AudioSample object (no chunking)
    for (const auto& buffer : audioBuffers) {
        const fl::vector<int16_t>& samples = buffer.samples;
        uint32_t timestamp = buffer.timestamp;
        
        // Create one AudioSample per buffer object (preserve separation)
        if (!samples.empty()) {
            AudioSampleImplPtr sample = fl::make_shared<AudioSampleImpl>();
            sample->assign(samples.begin(), samples.end(), timestamp);
            mAudioSampleImpls.push_back(sample);
            
            // Maintain buffer limit to prevent excessive accumulation
            while (mAudioSampleImpls.size() > 10) {
                mAudioSampleImpls.erase(mAudioSampleImpls.begin());
            }
        }
    }
}


AudioSample JsonAudioImpl::next() {
    AudioSampleImplPtr out;
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
