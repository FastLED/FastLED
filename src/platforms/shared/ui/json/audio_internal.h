#pragma once

#include "fl/json.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "platforms/shared/ui/json/audio_buffer.h"
#include "fl/vector.h"
#include "fl/audio.h"

namespace fl {

class JsonUiAudioInternal : public JsonUiInternal {
private:
    fl::Json mAudioDataArray;  // Store actual JSON array instead of string
    fl::vector<AudioSampleImplPtr> mAudioSampleImpls;

public:
    JsonUiAudioInternal(const fl::string& name)
        : JsonUiInternal(name) {}

    void toJson(fl::Json& json) const override;
    void updateInternal(const fl::Json& json) override;

    // Accessors for audio data
    fl::vector<AudioSampleImplPtr>& audioSamples() { return mAudioSampleImpls; }
    const fl::vector<AudioSampleImplPtr>& audioSamples() const { return mAudioSampleImpls; }
};

} // namespace fl
