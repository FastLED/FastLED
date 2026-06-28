#pragma once

#include "fl/audio/audio_processor.h"
#include "fl/audio/input.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/noexcept.h"

namespace fl {

class UIAudio;  // forward declaration

namespace audio {

class AudioManager {
  public:
    static AudioManager &instance() FL_NO_EXCEPT;

    shared_ptr<Processor> add(const Config &config) FL_NO_EXCEPT;
    shared_ptr<Processor> add(shared_ptr<IInput> input) FL_NO_EXCEPT;
    shared_ptr<Processor> add(UIAudio &uiAudio) FL_NO_EXCEPT;
    void remove(shared_ptr<Processor> processor) FL_NO_EXCEPT;

    shared_ptr<Processor> &processor() FL_NO_EXCEPT;

    AudioManager() FL_NO_EXCEPT = default;
    ~AudioManager() FL_NO_EXCEPT = default;

  private:
    AudioManager(const AudioManager &) FL_NO_EXCEPT = delete;
    AudioManager &operator=(const AudioManager &) FL_NO_EXCEPT = delete;
};

} // namespace audio
} // namespace fl
