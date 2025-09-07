
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
    // Transfer internal buffer to the caller.
    // -1 on error, 0 on no data, >0 on number of bytes read
    int read(fl::vector_inlined<i16, I2S_AUDIO_BUFFER_LEN>* buffer) override {
        FL_UNUSED(buffer);
        return -1;
    }

};

}  // namespace fl
