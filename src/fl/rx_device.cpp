/// @file rx_device.cpp
/// @brief Implementation of RxDevice factory

#include "rx_device.h"
#include "platforms/shared/rx_device_dummy.h"
#include "fl/chipsets/led_timing.h"
#include <cstring>

#ifdef ESP32
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h"
#include "platforms/esp/32/drivers/gpio_isr_rx/gpio_isr_rx.h"
#endif

namespace fl {

// Implementation of make4PhaseTiming function
ChipsetTiming4Phase make4PhaseTiming(const ChipsetTiming& timing_3phase,
                                      uint32_t tolerance_ns) {
    // Calculate derived values from 3-phase timing
    // The encoder uses:
    //   Bit 0: T1 high + (T2+T3) low
    //   Bit 1: (T1+T2) high + T3 low
    uint32_t t0h = timing_3phase.T1;                    // Bit 0 high time
    uint32_t t0l = timing_3phase.T2 + timing_3phase.T3; // Bit 0 low time
    uint32_t t1h = timing_3phase.T1 + timing_3phase.T2; // Bit 1 high time
    uint32_t t1l = timing_3phase.T3;                    // Bit 1 low time

    return ChipsetTiming4Phase{
        // Bit 0 timing thresholds
        .t0h_min_ns = (t0h > tolerance_ns) ? (t0h - tolerance_ns) : 0,
        .t0h_max_ns = t0h + tolerance_ns,
        .t0l_min_ns = (t0l > tolerance_ns) ? (t0l - tolerance_ns) : 0,
        .t0l_max_ns = t0l + tolerance_ns,

        // Bit 1 timing thresholds
        .t1h_min_ns = (t1h > tolerance_ns) ? (t1h - tolerance_ns) : 0,
        .t1h_max_ns = t1h + tolerance_ns,
        .t1l_min_ns = (t1l > tolerance_ns) ? (t1l - tolerance_ns) : 0,
        .t1l_max_ns = t1l + tolerance_ns,

        // Reset pulse threshold
        .reset_min_us = timing_3phase.RESET
    };
}

} // namespace fl

#ifdef ESP32

// ESP32-specific implementation
namespace fl {

// Factory implementation for ESP32
// Both RmtRxChannel and GpioIsrRx inherit from RxDevice, so no adapters needed
fl::shared_ptr<RxDevice> RxDevice::create(const char* type,
                                           int pin,
                                           size_t buffer_size,
                                           fl::optional<uint32_t> hz) {
    if (strcmp(type, "RMT") == 0) {
        // Use platform default (40MHz) if not specified
        uint32_t resolution_hz = hz.has_value() ? hz.value() : 40000000;
        auto device = RmtRxChannel::create(pin, resolution_hz, buffer_size);
        if (!device) {
            return fl::make_shared<DummyRxDevice>("RMT RX channel creation failed");
        }
        return device;
    }
    else if (strcmp(type, "ISR") == 0) {
        auto device = GpioIsrRx::create(pin, buffer_size);
        if (!device) {
            return fl::make_shared<DummyRxDevice>("GPIO ISR RX creation failed");
        }
        return device;
    }
    else {
        return fl::make_shared<DummyRxDevice>("Unknown RX device type (valid: \"RMT\", \"ISR\")");
    }
}

} // namespace fl

#else // !ESP32

// Stub implementation for non-ESP32 platforms
namespace fl {

fl::shared_ptr<RxDevice> RxDevice::create(const char* type,
                                           int pin,
                                           size_t buffer_size,
                                           fl::optional<uint32_t> hz) {
    (void)type;
    (void)pin;
    (void)buffer_size;
    (void)hz;
    return fl::make_shared<DummyRxDevice>("RX devices not supported on this platform");
}

} // namespace fl

#endif // ESP32
