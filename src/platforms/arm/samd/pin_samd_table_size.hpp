#pragma once

// IWYU pragma: private

/// @file platforms/arm/samd/pin_samd_table_size.hpp
/// Number of entries FastLED may read from a SAMD variant's g_APinDescription.
///
/// g_APinDescription is declared `extern const PinDescription
/// g_APinDescription[]`, so its size is unknown to sizeof. PINS_COUNT is not a
/// reliable bound either: variants index pins past it (#5185). The SparkFun
/// SAMD21 Dev board defines PINS_COUNT 26 but PIN_LED_TXL 26, and
/// g_APinDescription[26] (PA27) is a real entry. The count is therefore the
/// largest of PINS_COUNT and (pin + 1) for each named variant pin that indexes
/// the table. NUM_DIGITAL_PINS is deliberately not used: variants set it
/// inconsistently. PIN_ATN is not used either: the Adafruit Crickit M0 defines
/// it as 38 but has a 37-entry table.
///
/// Not constexpr: Arduino SAMD 1.8.x variants define PINS_COUNT as
/// PINCOUNT_fn(), a runtime function returning the exact table size.
///
/// Unnamed entries past the largest named pin (the SparkFun SAMD21 Dev has
/// some) are not covered; such variants set FL_SAMD_PIN_DESCRIPTION_COUNT.
///
/// Define FL_SAMD_PIN_DESCRIPTION_COUNT to override the count for a
/// variant whose table is larger still.
///
/// This header includes no vendor headers; the caller includes <variant.h>
/// first. Without PINS_COUNT or the override it defines nothing.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

#if defined(FL_SAMD_PIN_DESCRIPTION_COUNT) || defined(PINS_COUNT)

namespace fl {
namespace platforms {
namespace samd_detail {

inline fl::u32 pin_count_max(fl::u32 a, fl::u32 b) FL_NO_EXCEPT {
    return a > b ? a : b;
}

}  // namespace samd_detail

/// Entries of g_APinDescription that getPinMapping() may index.
inline fl::u32 samd_pin_description_count() FL_NO_EXCEPT {
#if defined(FL_SAMD_PIN_DESCRIPTION_COUNT)
    return static_cast<fl::u32>(FL_SAMD_PIN_DESCRIPTION_COUNT);
#else
    using samd_detail::pin_count_max;
    return pin_count_max(static_cast<fl::u32>(PINS_COUNT),
#ifdef PIN_LED_TXL
           pin_count_max(static_cast<fl::u32>(PIN_LED_TXL) + 1u,
#endif
#ifdef PIN_LED_RXL
           pin_count_max(static_cast<fl::u32>(PIN_LED_RXL) + 1u,
#endif
#ifdef PIN_LED
           pin_count_max(static_cast<fl::u32>(PIN_LED) + 1u,
#endif
#ifdef PIN_LED2
           pin_count_max(static_cast<fl::u32>(PIN_LED2) + 1u,
#endif
#ifdef PIN_LED3
           pin_count_max(static_cast<fl::u32>(PIN_LED3) + 1u,
#endif
#ifdef PIN_USB_HOST_ENABLE
           pin_count_max(static_cast<fl::u32>(PIN_USB_HOST_ENABLE) + 1u,
#endif
#ifdef PIN_SERIAL_RX
           pin_count_max(static_cast<fl::u32>(PIN_SERIAL_RX) + 1u,
#endif
#ifdef PIN_SERIAL_TX
           pin_count_max(static_cast<fl::u32>(PIN_SERIAL_TX) + 1u,
#endif
#ifdef PIN_SPI_MISO
           pin_count_max(static_cast<fl::u32>(PIN_SPI_MISO) + 1u,
#endif
#ifdef PIN_SPI_MOSI
           pin_count_max(static_cast<fl::u32>(PIN_SPI_MOSI) + 1u,
#endif
#ifdef PIN_SPI_SCK
           pin_count_max(static_cast<fl::u32>(PIN_SPI_SCK) + 1u,
#endif
#ifdef PIN_WIRE_SDA
           pin_count_max(static_cast<fl::u32>(PIN_WIRE_SDA) + 1u,
#endif
#ifdef PIN_WIRE_SCL
           pin_count_max(static_cast<fl::u32>(PIN_WIRE_SCL) + 1u,
#endif
           0u
#ifdef PIN_LED_TXL
           )
#endif
#ifdef PIN_LED_RXL
           )
#endif
#ifdef PIN_LED
           )
#endif
#ifdef PIN_LED2
           )
#endif
#ifdef PIN_LED3
           )
#endif
#ifdef PIN_USB_HOST_ENABLE
           )
#endif
#ifdef PIN_SERIAL_RX
           )
#endif
#ifdef PIN_SERIAL_TX
           )
#endif
#ifdef PIN_SPI_MISO
           )
#endif
#ifdef PIN_SPI_MOSI
           )
#endif
#ifdef PIN_SPI_SCK
           )
#endif
#ifdef PIN_WIRE_SDA
           )
#endif
#ifdef PIN_WIRE_SCL
           )
#endif
           );
#endif
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_SAMD_PIN_DESCRIPTION_COUNT || PINS_COUNT
