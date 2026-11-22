#include "fl/channels/encoder_availability.h"
#include "fl/channels/channel.h"

namespace fl {

namespace {
struct EncoderSelectBinder {
    using Result = EncoderAvailability::PixelEncoder;
    template<EncoderAvailability::Getter Get>
    static Result bind() FL_NO_EXCEPT { return Get(); }
    static Result none() FL_NO_EXCEPT { return nullptr; }
};
}  // namespace

FL_NO_INLINE
EncoderAvailability::PixelEncoder
EncoderAvailability::select(const ChipsetVariant& chipset) FL_NO_EXCEPT {
    return dispatch<EncoderSelectBinder>(chipset);
}

#if !defined(FASTLED_DISABLE_UCS7604) || !FASTLED_DISABLE_UCS7604
#define FL_ENCODER_UCS7604(encoder) encoder
#else
#define FL_ENCODER_UCS7604(encoder) nullptr
#endif
#if !defined(FASTLED_DISABLE_SPI_CHIPSETS) || !FASTLED_DISABLE_SPI_CHIPSETS
#define FL_ENCODER_SPI(encoder) encoder
#else
#define FL_ENCODER_SPI(encoder) nullptr
#endif

#define FL_ENCODER_GETTER(getter, value)                                          \
    FL_NO_INLINE EncoderAvailability::PixelEncoder EncoderAvailability::getter()  \
        FL_NO_EXCEPT {                                                            \
        return value;                                                             \
    }

FL_ENCODER_GETTER(ws2812, &Channel::encodeWS2812)
FL_ENCODER_GETTER(tm1812, &Channel::encodeTM1812RGBWW)
FL_ENCODER_GETTER(tm1908, &Channel::encodeTM1908)
FL_ENCODER_GETTER(ucs7604, FL_ENCODER_UCS7604(&Channel::encodeUCS7604))
FL_ENCODER_GETTER(apa102, FL_ENCODER_SPI(&Channel::encodeAPA102))
FL_ENCODER_GETTER(sk9822, FL_ENCODER_SPI(&Channel::encodeSK9822))
FL_ENCODER_GETTER(ws2801, FL_ENCODER_SPI(&Channel::encodeWS2801))
FL_ENCODER_GETTER(ws2803, FL_ENCODER_SPI(&Channel::encodeWS2803))
FL_ENCODER_GETTER(p9813, FL_ENCODER_SPI(&Channel::encodeP9813))
FL_ENCODER_GETTER(lpd8806, FL_ENCODER_SPI(&Channel::encodeLPD8806))
FL_ENCODER_GETTER(lpd6803, FL_ENCODER_SPI(&Channel::encodeLPD6803))
FL_ENCODER_GETTER(sm16716, FL_ENCODER_SPI(&Channel::encodeSM16716))
FL_ENCODER_GETTER(my9221, FL_ENCODER_SPI(&Channel::encodeMY9221))
FL_ENCODER_GETTER(hd108, FL_ENCODER_SPI(&Channel::encodeHD108))

#undef FL_ENCODER_GETTER
#undef FL_ENCODER_SPI
#undef FL_ENCODER_UCS7604

} // namespace fl
