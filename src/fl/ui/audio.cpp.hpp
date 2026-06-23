#include "fl/ui/audio.h"
#include "fl/audio/audio_manager.h"
#include "fl/stl/noexcept.h"

namespace fl {

UIAudio::UIAudio(const fl::string& name) FL_NOEXCEPT : mImpl(name) {}
UIAudio::UIAudio(const fl::string& name, const fl::url& url) FL_NOEXCEPT : mImpl(name, url) {}

// Asset-handle overload (issue #2284): resolves the asset via the
// v1 asset pipeline (registry + host/stub filesystem fallback) and
// forwards to the url-based ctor. If resolution fails, we forward
// the unresolved url so the consumer still sees a url field — the
// existing JsonAudioImpl code emits an empty string for an invalid
// url, matching the name-only behavior.
UIAudio::UIAudio(const fl::string& name, const fl::asset_ref& asset) FL_NOEXCEPT
    : mImpl(name, fl::resolve_asset(asset)) {}

UIAudio::UIAudio(const fl::string& name, const fl::audio::Config& config) FL_NOEXCEPT : mImpl(name, config), mConfig(config) {}
UIAudio::~UIAudio() FL_NOEXCEPT {}

fl::shared_ptr<audio::Processor> UIAudio::processor() FL_NOEXCEPT {
    if (!mProcessor) {
        mProcessor = audio::AudioManager::instance().add(*this);
    }
    return mProcessor;
}

} // namespace fl
