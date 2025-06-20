#pragma once

#include "fl/audio.h"

namespace fl {

class AudioReactive {

  public:
    AudioReactive();
    ~AudioReactive();

    void update(const AudioSample &audio);

  private:
};

} // namespace fl
