/// @file rmt4_peripheral_esp.h
/// @brief Real-hardware `IRMT4Peripheral` for classic-ESP32 / IDF 4.x.
///
/// Pairs with `irmt4_peripheral.h`. Concrete implementation that wraps
/// the `driver/rmt.h` lifecycle API. Owns no state of its own — all
/// hardware state lives on the per-channel `ChannelState` in
/// `ChannelEngineRMT4Impl`.

#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_RMT &&                           \
    !FASTLED_ESP32_RMT5_ONLY_PLATFORM && !FASTLED_RMT5

#include "fl/stl/noexcept.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/irmt4_peripheral.h"

namespace fl {

/// @brief Real-hardware peripheral wrapper for the IDF-4.x RMT driver.
///
/// Stateless thin wrapper around `driver/rmt.h`. All ESP-IDF type
/// dependencies live in the `.cpp.hpp` so this header stays cheap to
/// include from `bus_traits.h`.
class Rmt4PeripheralESP : public detail::IRMT4Peripheral {
  public:
    Rmt4PeripheralESP() FL_NO_EXCEPT = default;
    ~Rmt4PeripheralESP() override = default;

    bool configureChannel(const detail::Rmt4ChannelConfig &cfg)
        FL_NO_EXCEPT override;
    bool installDriver(int channel) FL_NO_EXCEPT override;
    bool uninstallDriver(int channel) FL_NO_EXCEPT override;

    bool setTxThresholdIntrEnable(int channel, bool enable,
                                  u16 threshold) FL_NO_EXCEPT override;
    bool setTxIntrEnable(int channel, bool enable) FL_NO_EXCEPT override;

    bool setGpio(int channel, int gpio_pin, bool invert) FL_NO_EXCEPT override;

    bool installIsr(detail::Rmt4IsrHandler handler, void *arg,
                    void **out_handle) FL_NO_EXCEPT override;
    void freeIsr(void *handle) FL_NO_EXCEPT override;
};

} // namespace fl

#endif // FL_IS_ESP32 && FASTLED_ESP32_HAS_RMT && !RMT5-only && !FASTLED_RMT5
