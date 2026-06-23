#pragma once

// IWYU pragma: private

#include "fl/stl/json.h"
#include "fl/stl/string.h"
#include "fl/stl/url.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "platforms/shared/ui/json/audio_buffer.h"
#include "fl/stl/vector.h"
#include "fl/audio/audio.h"
#include "fl/stl/noexcept.h"

namespace fl {

class JsonUiAudioInternal : public JsonUiInternal {
private:
    fl::json mAudioDataArray;  // Store actual JSON array instead of string
    fl::vector<audio::SampleImplPtr> mAudioSampleImpls;

public:
    JsonUiAudioInternal(const fl::string& name)
 FL_NOEXCEPT : JsonUiInternal(name) {}

    JsonUiAudioInternal(const fl::string& name, const fl::url& url)
 FL_NOEXCEPT : JsonUiInternal(name), mUrl(url) {}

    void toJson(fl::json& json) const FL_NOEXCEPT override;
    void updateInternal(const fl::json& json) FL_NOEXCEPT override;

    // Accessors for audio data
    fl::vector<audio::SampleImplPtr>& audioSamples() FL_NOEXCEPT { return mAudioSampleImpls; }
    const fl::vector<audio::SampleImplPtr>& audioSamples() const FL_NOEXCEPT { return mAudioSampleImpls; }

    const fl::url& url() const FL_NOEXCEPT { return mUrl; }

private:
    fl::url mUrl;
};

} // namespace fl
