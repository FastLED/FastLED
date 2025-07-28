#include "platforms/shared/ui/json/audio_internal.h"
#include "fl/warn.h"
#include "platforms/shared/ui/json/audio_buffer.h"
#include "fl/audio.h"

namespace fl {

void JsonUiAudioInternal::toJson(fl::Json &json) const {
    json.set("name", name());
    json.set("group", groupName());
    json.set("type", "audio");
    json.set("id", id());
    if (!mAudioDataArray.is_null() && mAudioDataArray.is_array()) {
        json.set("audioData", mAudioDataArray);
    }
}

void JsonUiAudioInternal::updateInternal(const fl::Json &value) {
    mAudioDataArray = fl::Json();  // Clear the stored audio data
    
    if (value.contains("audioData")) {
        fl::Json audioDataArray = value["audioData"];
        // Store the actual JSON array, not a string representation
        mAudioDataArray = audioDataArray;
        
        if (audioDataArray.is_array()) {
            for (size_t i = 0; i < audioDataArray.size(); ++i) {
                fl::Json bufferJson = audioDataArray[i];
                if (bufferJson.is_object()) {
                    fl::vector<int16_t> samples;
                    uint32_t timestamp = bufferJson["timestamp"] | 0;

                    if (bufferJson.contains("samples") &&
                        bufferJson["samples"].is_array()) {
                        fl::Json samplesArray = bufferJson["samples"];
                        for (size_t j = 0; j < samplesArray.size(); ++j) {
                            samples.push_back(samplesArray[j] | 0);
                        }
                    }
                    // Create AudioBuffer equivalent and process
                    // This part of the loop is now inside the if
                    // (bufferJson.is_object()) and directly uses 'samples' and
                    // 'timestamp'
                    const fl::vector<int16_t> &current_samples = samples;
                    uint32_t current_timestamp = timestamp;

                    if (!current_samples.empty()) {
                        AudioSampleImplPtr sample =
                            fl::make_shared<AudioSampleImpl>();
                        sample->assign(current_samples.begin(),
                                       current_samples.end(),
                                       current_timestamp);
                        mAudioSampleImpls.push_back(sample);

                        while (mAudioSampleImpls.size() > 10) {
                            mAudioSampleImpls.erase(mAudioSampleImpls.begin());
                        }
                    }
                }
            }
        }
    }
}
} // namespace fl
