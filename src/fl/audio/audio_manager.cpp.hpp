#include "fl/audio/audio_manager.h"
#include "fl/audio/input.h"
#include "fl/stl/singleton.h"
#include "fl/log/log.h"
#include "fl/ui/ui.h"

namespace fl {
namespace audio {

AudioManager &AudioManager::instance() {
    return Singleton<AudioManager>::instance();
}

shared_ptr<Processor> &AudioManager::processor() {
    return Singleton<shared_ptr<Processor>>::instance();
}

shared_ptr<Processor> AudioManager::add(const Config &config) {
#if FASTLED_HAS_AUDIO_INPUT
    fl::string errorMsg;
    auto input = IInput::create(config, &errorMsg);
    if (!input) {
        FL_WARN("Failed to create audio input: " << errorMsg);
        return nullptr;
    }
    input->start();
    auto proc = Processor::createWithAutoInput(fl::move(input));
    if (config.getMicProfile() != MicProfile::None) {
        proc->setMicProfile(config.getMicProfile());
    }
    if (processor()) {
        FL_WARN("Replacing existing audio processor");
    }
    processor() = proc;
    return proc;
#else
    (void)config;
    auto proc = fl::make_shared<Processor>();
    if (processor()) {
        FL_WARN("Replacing existing audio processor");
    }
    processor() = proc;
    return proc;
#endif
}

shared_ptr<Processor> AudioManager::add(shared_ptr<IInput> input) {
    if (!input) {
        FL_WARN("Cannot add null audio input");
        return nullptr;
    }
    input->start();
    auto proc = Processor::createWithAutoInput(fl::move(input));
    if (processor()) {
        FL_WARN("Replacing existing audio processor");
    }
    processor() = proc;
    return proc;
}

shared_ptr<Processor> AudioManager::add(UIAudio &uiAudio) {
    auto input = uiAudio.audioInput();
    if (input) {
        return add(fl::move(input));
    }
    // No platform audio input (e.g. hardware stub) — use the stored config
    // to create a real hardware audio input.
    const auto& cfg = uiAudio.config();
    if (cfg.has_value()) {
        return add(*cfg);
    }
    FL_WARN("UIAudio has no audio input and no hardware config");
    return nullptr;
}

void AudioManager::remove(shared_ptr<Processor> proc) {
    if (!proc) {
        return;
    }
    if (processor() == proc) {
        processor().reset();
    }
}

} // namespace audio
} // namespace fl
