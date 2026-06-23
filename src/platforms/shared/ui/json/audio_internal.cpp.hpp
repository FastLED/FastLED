// IWYU pragma: private

#include "platforms/shared/ui/json/audio_internal.h"
#include "fl/log/log.h"
#include "platforms/shared/ui/json/audio_buffer.h"
#include "fl/audio/audio.h"
#include "fl/stl/noexcept.h"

namespace fl {

void JsonUiAudioInternal::toJson(fl::json &json) const FL_NOEXCEPT {
    json.set("name", name());
    json.set("group", groupName());
    json.set("type", "audio");
    json.set("id", id());
    if (mUrl.isValid()) {
        json.set("url", mUrl.string());
    }
    // Audio data flows one-way (JS -> C++), never echo back to JS.
    // Echoing would create a feedback loop of growing JSON payloads.
}

void JsonUiAudioInternal::updateInternal(const fl::json &value) FL_NOEXCEPT {
    if (value.contains("audioData")) {
        fl::json audioDataArray = value["audioData"];

        if (audioDataArray.is_array()) {
            for (size_t i = 0; i < audioDataArray.size(); ++i) {
                fl::json bufferJson = audioDataArray[i];
                if (bufferJson.is_object()) {
                    fl::vector<i16> samples;
                    u32 timestamp = bufferJson["timestamp"] | 0;

                    if (bufferJson.contains("samples") &&
                        bufferJson["samples"].is_array()) {
                        fl::json samplesArray = bufferJson["samples"];
                        for (size_t j = 0; j < samplesArray.size(); ++j) {
                            samples.push_back(samplesArray[j] | 0);
                        }
                    }
                    // Create AudioBuffer equivalent and process
                    // This part of the loop is now inside the if
                    // (bufferJson.is_object()) and directly uses 'samples' and
                    // 'timestamp'
                    const fl::vector<i16> &current_samples = samples;
                    u32 current_timestamp = timestamp;

                    if (!current_samples.empty()) {
                        audio::SampleImplPtr sample =
                            fl::make_shared<audio::SampleImpl>();
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
