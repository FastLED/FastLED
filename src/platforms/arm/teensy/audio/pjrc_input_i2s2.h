// IWYU pragma: private

#pragma once

#include "fl/system/arduino.h"
#include "platforms/arm/teensy/audio/pjrc_audio_stream.h"

#if defined(__IMXRT1052__) || defined(__IMXRT1062__)

namespace fl {
namespace platforms {
namespace teensy {

// Private FastLED implementation of PJRC-compatible I2S2 input behavior.
class PjrcAudioInputI2S2 : public AudioStream {
public:
    PjrcAudioInputI2S2() : AudioStream(0, nullptr) { begin(); }
    void update() override;
    void begin();

protected:
    explicit PjrcAudioInputI2S2(int) : AudioStream(0, nullptr) {}
    static void isr();
};

class PjrcAudioInputI2S2Slave : public PjrcAudioInputI2S2 {
public:
    PjrcAudioInputI2S2Slave() : PjrcAudioInputI2S2(0) { begin(); }
    void begin();
};

} // namespace teensy
} // namespace platforms
} // namespace fl

#endif // defined(__IMXRT1052__) || defined(__IMXRT1062__)
