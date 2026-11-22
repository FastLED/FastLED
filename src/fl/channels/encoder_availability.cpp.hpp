#include "fl/channels/encoder_availability.h"
#include "fl/channels/channel.h"

namespace fl {

FL_NO_INLINE
EncoderAvailability::PixelEncoder
EncoderAvailability::select(const ChipsetVariant& chipset) FL_NO_EXCEPT {
        if (const ClocklessChipset* clockless = chipset.ptr<ClocklessChipset>()) {
            switch (clockless->encoder) {
                case ClocklessEncoder::CLOCKLESS_ENCODER_WS2812:
                    return &Channel::encodeWS2812;
                case ClocklessEncoder::CLOCKLESS_ENCODER_TM1812_RGBWW:
                    return &Channel::encodeTM1812RGBWW;
                case ClocklessEncoder::CLOCKLESS_ENCODER_TM1908:
                    return &Channel::encodeTM1908;
#if !defined(FASTLED_DISABLE_UCS7604) || !FASTLED_DISABLE_UCS7604
                case ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_8BIT:
                case ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT:
                case ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT_1600:
                    return &Channel::encodeUCS7604;
#endif
            }
        } else if (const SpiChipsetConfig* spi = chipset.ptr<SpiChipsetConfig>()) {
#if !defined(FASTLED_DISABLE_SPI_CHIPSETS) || !FASTLED_DISABLE_SPI_CHIPSETS
            switch (spi->timing.chipset) {
                case SpiChipset::APA102:
                case SpiChipset::DOTSTAR:
                case SpiChipset::APA102HD:
                case SpiChipset::DOTSTARHD:
                case SpiChipset::HD107:
                case SpiChipset::HD107HD:
                    return &Channel::encodeAPA102;
                case SpiChipset::SK9822:
                case SpiChipset::SK9822HD:
                    return &Channel::encodeSK9822;
                case SpiChipset::WS2801:
                    return &Channel::encodeWS2801;
                case SpiChipset::WS2803:
                    return &Channel::encodeWS2803;
                case SpiChipset::P9813:
                    return &Channel::encodeP9813;
                case SpiChipset::LPD8806:
                    return &Channel::encodeLPD8806;
                case SpiChipset::LPD6803:
                    return &Channel::encodeLPD6803;
                case SpiChipset::SM16716:
                    return &Channel::encodeSM16716;
                case SpiChipset::MY9221:
                    return &Channel::encodeMY9221;
                case SpiChipset::HD108:
                    return &Channel::encodeHD108;
            }
#endif  // !FASTLED_DISABLE_SPI_CHIPSETS
        }
        return nullptr;
    }

} // namespace fl
