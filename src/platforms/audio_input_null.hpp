#pragma once

#include "fl/stl/compiler_control.h"
#include "fl/audio/audio_input.h"
#include "fl/stl/noexcept.h"

namespace fl {

class Null_Audio : public audio::IInput {
public:
    ~Null_Audio() = default;
    // Starts the audio source.
    void start() override {}
    // Stops the audio source, call this before light sleep.
    void stop() override {}

    bool error(fl::string* msg = nullptr) FL_NOEXCEPT override {
        if (msg) {
            *msg = "No audio device available: this is a null device.";
        }
        return true;
    }
    // Read audio data and return as audio::Sample with calculated timestamp.
    // Returns invalid audio::Sample on error or when no data is available.
    audio::Sample read() FL_NOEXCEPT override {
        return audio::Sample();  // Always return invalid sample
    }

};

}  // namespace fl