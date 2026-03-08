#pragma once

// IWYU pragma: private

#include "fl/stl/json.h"
#include "fl/stl/string.h"
#include "fl/stl/url.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "platforms/shared/ui/json/audio_buffer.h"
#include "fl/stl/vector.h"
#include "fl/audio/audio.h"

namespace fl {

class JsonUiAudioInternal : public JsonUiInternal {
private:
    fl::json mAudioDataArray;  // Store actual JSON array instead of string
    fl::vector<AudioSampleImplPtr> mAudioSampleImpls;

public:
    JsonUiAudioInternal(const fl::string& name)
        : JsonUiInternal(name) {}

    JsonUiAudioInternal(const fl::string& name, const fl::url& url)
        : JsonUiInternal(name), mUrl(url) {}

    void toJson(fl::json& json) const override;
    void updateInternal(const fl::json& json) override;

    // Accessors for audio data
    fl::vector<AudioSampleImplPtr>& audioSamples() { return mAudioSampleImpls; }
    const fl::vector<AudioSampleImplPtr>& audioSamples() const { return mAudioSampleImpls; }

    const fl::url& url() const { return mUrl; }

private:
    fl::url mUrl;
};

} // namespace fl
