#pragma once

#include "fl/channels/config.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/vector.h"

namespace fl {
class Channel;
class PixelIterator;

/// Library-owned optional encoder selection. Caller feature macros never
/// affect these declarations or the unconditional WS2812 fast path.
struct EncoderAvailability {
    using PixelEncoder = void (*)(Channel&, PixelIterator&, bool, vector_psram<u8>&);

    FL_NO_INLINE static PixelEncoder select(const ChipsetVariant&) FL_NO_EXCEPT;

    FL_ALWAYS_INLINE PixelEncoder known(const ClocklessChipset& chipset,
                                       PixelEncoder ws2812) FL_NO_EXCEPT {
        return chipset.encoder == ClocklessEncoder::CLOCKLESS_ENCODER_WS2812
            ? ws2812 : nullptr;
    }
    FL_ALWAYS_INLINE PixelEncoder known(const SpiChipsetConfig&,
                                       PixelEncoder) FL_NO_EXCEPT { return nullptr; }
    FL_ALWAYS_INLINE PixelEncoder known(const ChipsetVariant& chipset,
                                       PixelEncoder ws2812) FL_NO_EXCEPT {
        const auto* clockless = chipset.ptr<ClocklessChipset>();
        return clockless ? known(*clockless, ws2812) : nullptr;
    }
};
} // namespace fl
