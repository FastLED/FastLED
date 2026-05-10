// IWYU pragma: private

#include "platforms/shared/ui/json/audio.h"
#include "fl/stl/json.h"
#include "platforms/shared/ui/json/audio_internal.h"
#include "fl/stl/string.h"
#include "fl/stl/thread_local.h"
#include "fl/log/log.h"
#include "fl/stl/compiler_control.h"
#include "platforms/shared/ui/json/ui.h"

#include "platforms/shared/ui/json/audio_buffer.h"
#include "fl/stl/noexcept.h"
namespace fl {
// namespace {
//     fl::string& scratchBuffer() {
//         static fl::ThreadLocal<fl::string> buffer;
//         return buffer.access();
//     }
// }


JsonAudioImpl::JsonAudioImpl(const fl::string &name) FL_NOEXCEPT {
    mInternal = fl::make_shared<JsonUiAudioInternal>(name);
    mUpdater.init(this);
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonAudioImpl::JsonAudioImpl(const fl::string &name, const fl::url& url) FL_NOEXCEPT {
    mInternal = fl::make_shared<JsonUiAudioInternal>(name, url);
    mUpdater.init(this);
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonAudioImpl::JsonAudioImpl(const fl::string &name, const fl::audio::Config& config) FL_NOEXCEPT {
    // JSON UI gets audio from browser, so config is ignored
    FL_UNUSED(config);
    mInternal = fl::make_shared<JsonUiAudioInternal>(name);
    mUpdater.init(this);
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonAudioImpl::~JsonAudioImpl() { removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal)); }

JsonAudioImpl &JsonAudioImpl::Group(const fl::string &name) FL_NOEXCEPT {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonAudioImpl::name() const FL_NOEXCEPT { return mInternal->name(); }

fl::string JsonAudioImpl::groupName() const FL_NOEXCEPT {
    return mInternal->groupName();
}

void JsonAudioImpl::setGroup(const fl::string &groupName) FL_NOEXCEPT {
    mInternal->setGroup(groupName);
}

void JsonAudioImpl::Updater::init(JsonAudioImpl *owner) FL_NOEXCEPT {
    mOwner = owner;
    fl::EngineEvents::addListener(this);
}

JsonAudioImpl::Updater::~Updater() { fl::EngineEvents::removeListener(this); }

void JsonAudioImpl::Updater::onPlatformPreLoop2() FL_NOEXCEPT {}

audio::Sample JsonAudioImpl::next() FL_NOEXCEPT {
    audio::SampleImplPtr out;
    if (mInternal->audioSamples().empty()) {
        // FL_WARN("No audio samples available");
        return out;
    }
    // auto sample = mInternal->audioSamples().back();
    // mInternal->audioSamples().pop_back();
    out = mInternal->audioSamples().front();
    mInternal->audioSamples().erase(mInternal->audioSamples().begin());
    // FL_WARN("Returning audio sample of size " << out->pcm().size());
    return audio::Sample(out);
}

bool JsonAudioImpl::hasNext() FL_NOEXCEPT { return !mInternal->audioSamples().empty(); }

} // namespace fl
