#pragma once

#include "fl/unused.h"
#include "fl/audio_input.h"

namespace fl {

class Null_Audio : public IAudioInput {
public:
    ~Null_Audio() = default;
    // Starts the audio source.
    void start() override {}
    // Stops the audio source, call this before light sleep.
    void stop() override {}

    bool error(fl::string* msg = nullptr) override {
        if (msg) {
            *msg = "No audio device available: this is a null device.";
        }
        return true;
    }
    // Read audio data and return as AudioSample with calculated timestamp.
    // Returns invalid AudioSample on error or when no data is available.
    AudioSample read() override {
        return AudioSample();  // Always return invalid sample
    }

};

}  // namespace fl