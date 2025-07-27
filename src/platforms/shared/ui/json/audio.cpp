#include "platforms/shared/ui/json/audio.h"
#include "fl/json.h"
#include "platforms/shared/ui/json/audio_internal.h"
#include "fl/string.h"
#include "fl/thread_local.h"
#include "fl/warn.h"
#include "platforms/shared/ui/json/ui.h"

#include "platforms/shared/ui/json/audio_buffer.h"

#if FASTLED_ENABLE_JSON


namespace fl {
// namespace {
//     fl::string& scratchBuffer() {
//         static fl::ThreadLocal<fl::string> buffer;
//         return buffer.access();
//     }
// }


JsonAudioImpl::JsonAudioImpl(const fl::string &name) {
    mInternal = fl::make_shared<JsonUiAudioInternal>(name);
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

AudioSample JsonAudioImpl::next() {
    AudioSampleImplPtr out;
    if (mInternal->audioSamples().empty()) {
        // FASTLED_WARN("No audio samples available");
        return out;
    }
    // auto sample = mInternal->audioSamples().back();
    // mInternal->audioSamples().pop_back();
    out = mInternal->audioSamples().front();
    mInternal->audioSamples().erase(mInternal->audioSamples().begin());
    // FASTLED_WARN("Returning audio sample of size " << out->pcm().size());
    return AudioSample(out);
}

bool JsonAudioImpl::hasNext() { return !mInternal->audioSamples().empty(); }

} // namespace fl

#endif // FASTLED_ENABLE_JSON
