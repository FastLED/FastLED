#pragma once

// IWYU pragma: private
// Composed once into the native SPI implementation, before SDK acquisition.

#include "platforms/esp/32/drivers/spi/spi_provider.h"

namespace fl {
namespace platforms {
namespace detail {
fl::shared_ptr<SpiHw1> makeNativeSpiHw1(u8 bus) FL_NO_EXCEPT;
} // namespace detail

FL_LINK_WEAK fl::shared_ptr<SpiHw1> fastled_spi_provider(u8 bus) FL_NO_EXCEPT {
    return detail::makeNativeSpiHw1(bus);
}
} // namespace platforms

fl::shared_ptr<SpiHw1>& getController2() FL_NO_EXCEPT {
    static fl::shared_ptr<SpiHw1> instance = platforms::fastled_spi_provider(2);
    return instance;
}

#if FASTLED_ESP32_HAS_SPI3
fl::shared_ptr<SpiHw1>& getController3() FL_NO_EXCEPT {
    static fl::shared_ptr<SpiHw1> instance = platforms::fastled_spi_provider(3);
    return instance;
}
#endif

} // namespace fl
