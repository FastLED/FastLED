/// @file rx_device.cpp
/// @brief Implementation of RxDevice factory

#include "rx_device.h"
#include "platforms/shared/rx_device_dummy.h"
#include "fl/chipsets/led_timing.h"
#include "fl/str.h"

#ifdef ESP32
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h"
#include "platforms/esp/32/drivers/gpio_isr_rx/gpio_isr_rx.h"
#endif

namespace fl {

// Private static helper - creates dummy device (singleton pattern)
fl::shared_ptr<RxDevice> RxDevice::createDummy() {
    static fl::shared_ptr<RxDevice> dummy = fl::make_shared<DummyRxDevice>(
        "RX devices not supported on this platform"
    );
    return dummy;
}

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

    ChipsetTiming4Phase result;

    // Bit 0 timing thresholds
    result.t0h_min_ns = (t0h > tolerance_ns) ? (t0h - tolerance_ns) : 0;
    result.t0h_max_ns = t0h + tolerance_ns;
    result.t0l_min_ns = (t0l > tolerance_ns) ? (t0l - tolerance_ns) : 0;
    result.t0l_max_ns = t0l + tolerance_ns;

    // Bit 1 timing thresholds
    result.t1h_min_ns = (t1h > tolerance_ns) ? (t1h - tolerance_ns) : 0;
    result.t1h_max_ns = t1h + tolerance_ns;
    result.t1l_min_ns = (t1l > tolerance_ns) ? (t1l - tolerance_ns) : 0;
    result.t1l_max_ns = t1l + tolerance_ns;

    // Reset pulse threshold
    result.reset_min_us = timing_3phase.RESET;

    // gap_tolerance_ns retains its default value of 0

    return result;
}

} // namespace fl



// ESP32-specific explicit template specializations
namespace fl {

#ifdef ESP32

// RMT device specialization for ESP32
template <>
fl::shared_ptr<RxDevice> RxDevice::create<RxDeviceType::RMT>(int pin) {
    auto device = RmtRxChannel::create(pin);
    if (!device) {
        return fl::make_shared<DummyRxDevice>("RMT RX channel creation failed");
    }
    return device;
}

// ISR device specialization for ESP32
template <>
fl::shared_ptr<RxDevice> RxDevice::create<RxDeviceType::ISR>(int pin) {
    auto device = GpioIsrRx::create(pin);
    if (!device) {
        return fl::make_shared<DummyRxDevice>("GPIO ISR RX creation failed");
    }
    return device;
}

#else

// RMT device specialization (dummy for non-ESP32)
template <>
fl::shared_ptr<RxDevice> RxDevice::create<RxDeviceType::RMT>(int pin) {
    (void)pin;  // Suppress unused parameter warning
    return RxDevice::createDummy();
}

// ISR device specialization (dummy for non-ESP32)
template <>
fl::shared_ptr<RxDevice> RxDevice::create<RxDeviceType::ISR>(int pin) {
    (void)pin;  // Suppress unused parameter warning
    return RxDevice::createDummy();
}

#endif // ESP32

} // namespace fl
