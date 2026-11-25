#pragma once

#include "fl/channels/config.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/vector.h"

namespace fl {
class Channel;
class PixelIterator;

/// Library-owned optional encoder selection. Caller feature macros never
/// affect these declarations or the inline chipset-to-encoder mapping.
struct EncoderAvailability {
    // Return false if the output could not hold a complete frame.
    using PixelEncoder = bool (*)(Channel&, PixelIterator&, bool, vector_psram<u8>&);
    using Getter = PixelEncoder (*)();

    FL_NO_INLINE static PixelEncoder select(const ChipsetVariant&) FL_NO_EXCEPT;

    /// One out-of-line getter per encoder. Each references only its own
    /// encoder and returns nullptr when the library disables it, so a constant
    /// typed chipset links one encoder (#5162) while `FASTLED_DISABLE_*` stays
    /// a library-only decision (#5157).
    FL_NO_INLINE static PixelEncoder ws2812() FL_NO_EXCEPT;
    FL_NO_INLINE static PixelEncoder tm1812() FL_NO_EXCEPT;
    FL_NO_INLINE static PixelEncoder tm1908() FL_NO_EXCEPT;
    FL_NO_INLINE static PixelEncoder ucs7604() FL_NO_EXCEPT;
    FL_NO_INLINE static PixelEncoder apa102() FL_NO_EXCEPT;
    FL_NO_INLINE static PixelEncoder sk9822() FL_NO_EXCEPT;
    FL_NO_INLINE static PixelEncoder ws2801() FL_NO_EXCEPT;
    FL_NO_INLINE static PixelEncoder ws2803() FL_NO_EXCEPT;
    FL_NO_INLINE static PixelEncoder p9813() FL_NO_EXCEPT;
    FL_NO_INLINE static PixelEncoder lpd8806() FL_NO_EXCEPT;
    FL_NO_INLINE static PixelEncoder lpd6803() FL_NO_EXCEPT;
    FL_NO_INLINE static PixelEncoder sm16716() FL_NO_EXCEPT;
    FL_NO_INLINE static PixelEncoder my9221() FL_NO_EXCEPT;
    FL_NO_INLINE static PixelEncoder hd108() FL_NO_EXCEPT;

    /// The single chipset-to-encoder mapping. Returns
    /// `Binder::template bind<getter>()` for a known chipset, else
    /// `Binder::none()`. Inline, so a constant chipset folds to one getter.
    template<typename Binder>
    FL_ALWAYS_INLINE typename Binder::Result dispatch(const ClocklessChipset& chipset) FL_NO_EXCEPT {
        switch (chipset.encoder) {
            case ClocklessEncoder::CLOCKLESS_ENCODER_WS2812:
                return Binder::template bind<&EncoderAvailability::ws2812>();
            case ClocklessEncoder::CLOCKLESS_ENCODER_TM1812_RGBWW:
                return Binder::template bind<&EncoderAvailability::tm1812>();
            case ClocklessEncoder::CLOCKLESS_ENCODER_TM1908:
                return Binder::template bind<&EncoderAvailability::tm1908>();
            case ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_8BIT:
            case ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT:
            case ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT_1600:
                return Binder::template bind<&EncoderAvailability::ucs7604>();
        }
        return Binder::none();
    }

    template<typename Binder>
    FL_ALWAYS_INLINE typename Binder::Result dispatch(const SpiChipsetConfig& chipset) FL_NO_EXCEPT {
        switch (chipset.timing.chipset) {
            case SpiChipset::APA102:
            case SpiChipset::DOTSTAR:
            case SpiChipset::APA102HD:
            case SpiChipset::DOTSTARHD:
            case SpiChipset::HD107:
            case SpiChipset::HD107HD:
                return Binder::template bind<&EncoderAvailability::apa102>();
            case SpiChipset::SK9822:
            case SpiChipset::SK9822HD:
                return Binder::template bind<&EncoderAvailability::sk9822>();
            case SpiChipset::WS2801:
                return Binder::template bind<&EncoderAvailability::ws2801>();
            case SpiChipset::WS2803:
                return Binder::template bind<&EncoderAvailability::ws2803>();
            case SpiChipset::P9813:
                return Binder::template bind<&EncoderAvailability::p9813>();
            case SpiChipset::LPD8806:
                return Binder::template bind<&EncoderAvailability::lpd8806>();
            case SpiChipset::LPD6803:
                return Binder::template bind<&EncoderAvailability::lpd6803>();
            case SpiChipset::SM16716:
                return Binder::template bind<&EncoderAvailability::sm16716>();
            case SpiChipset::MY9221:
                return Binder::template bind<&EncoderAvailability::my9221>();
            case SpiChipset::HD108:
                return Binder::template bind<&EncoderAvailability::hd108>();
        }
        return Binder::none();
    }

    template<typename Binder>
    FL_ALWAYS_INLINE typename Binder::Result dispatch(const ChipsetVariant& chipset) FL_NO_EXCEPT {
        if (const auto* clockless = chipset.ptr<ClocklessChipset>()) {
            return dispatch<Binder>(*clockless);
        }
        if (const auto* spi = chipset.ptr<SpiChipsetConfig>()) {
            return dispatch<Binder>(*spi);
        }
        return Binder::none();
    }
};
} // namespace fl
