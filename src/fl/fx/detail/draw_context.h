#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"

namespace fl {

class AudioBatch; // forward declaration — pointer only, no include needed

struct DrawContext {
    fl::u32 now;
    fl::span<CRGB> leds;
    u16 frame_time = 0;
    float speed = 1.0f;
    const AudioBatch *audio = nullptr; ///< Non-owning. Null when no audio.
    DrawContext(fl::u32 now, fl::span<CRGB> leds, u16 frame_time = 0,
                float speed = 1.0f, const AudioBatch *audio = nullptr)
        FL_NO_EXCEPT : now(now), leds(leds), frame_time(frame_time), speed(speed),
          audio(audio) {}
};

} // namespace fl
