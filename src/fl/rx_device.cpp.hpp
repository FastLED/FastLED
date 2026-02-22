/// @file rx_device.cpp
/// @brief Implementation of RxDevice factory

#include "platforms/is_platform.h"
#include "rx_device.h"
// IWYU pragma: begin_keep
#include "platforms/shared/rx_device_dummy.h"  // ok platform headers
// IWYU pragma: end_keep // ok platform headers
#include "fl/chipsets/led_timing.h"
#include "fl/stl/string.h"

#ifdef FL_IS_ESP32
// IWYU pragma: begin_keep
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h" // ok platform headers
#include "platforms/esp/32/drivers/gpio_isr_rx/gpio_isr_rx.h" // ok platform headers
#endif
// IWYU pragma: end_keep

#ifdef FL_IS_TEENSY_4X
// IWYU pragma: begin_keep
#include "platforms/arm/teensy/teensy4_common/flexpwm_rx_channel.h" // ok platform headers
// IWYU pragma: end_keep
#endif

#ifdef FL_IS_STUB
// IWYU pragma: begin_keep
#include "platforms/shared/rx_device_native.h"  // ok platform headers
// IWYU pragma: end_keep
#endif

namespace fl {

// Private static helper - creates dummy device (singleton pattern)
fl::shared_ptr<RxDevice> RxDevice::createDummy() {
    static fl::shared_ptr<RxDevice> dummy = fl::make_shared<DummyRxDevice>( // okay static in header
        "RX devices not supported on this platform"
    );
    return dummy;
}

// Implementation of make4PhaseTiming function
ChipsetTiming4Phase make4PhaseTiming(const ChipsetTiming& timing_3phase,
                                      u32 tolerance_ns) {
    // Calculate derived values from 3-phase timing
    // The encoder uses:
    //   Bit 0: T1 high + (T2+T3) low
    //   Bit 1: (T1+T2) high + T3 low
    u32 t0h = timing_3phase.T1;                    // Bit 0 high time
    u32 t0l = timing_3phase.T2 + timing_3phase.T3; // Bit 0 low time
    u32 t1h = timing_3phase.T1 + timing_3phase.T2; // Bit 1 high time
    u32 t1l = timing_3phase.T3;                    // Bit 1 low time

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

#ifdef FL_IS_ESP32

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

// FLEXPWM not available on ESP32
template <>
fl::shared_ptr<RxDevice> RxDevice::create<RxDeviceType::FLEXPWM>(int pin) {
    (void)pin;
    return fl::make_shared<DummyRxDevice>("FLEXPWM RX not supported on ESP32");
}

#elif defined(FL_IS_TEENSY_4X)

// FLEXPWM device specialization for Teensy 4.x
template <>
fl::shared_ptr<RxDevice> RxDevice::create<RxDeviceType::FLEXPWM>(int pin) {
    auto device = FlexPwmRxChannel::create(pin);
    if (!device) {
        return fl::make_shared<DummyRxDevice>("FlexPWM RX channel creation failed");
    }
    return device;
}

// RMT not available on Teensy
template <>
fl::shared_ptr<RxDevice> RxDevice::create<RxDeviceType::RMT>(int pin) {
    (void)pin;
    return fl::make_shared<DummyRxDevice>("RMT RX not supported on Teensy");
}

// ISR not available on Teensy
template <>
fl::shared_ptr<RxDevice> RxDevice::create<RxDeviceType::ISR>(int pin) {
    (void)pin;
    return fl::make_shared<DummyRxDevice>("ISR RX not supported on Teensy");
}

#elif defined(FL_IS_STUB)

// RMT device specialization (native stub for host/desktop testing)
template <>
fl::shared_ptr<RxDevice> RxDevice::create<RxDeviceType::RMT>(int pin) {
    return NativeRxDevice::create(pin);
}

// ISR device specialization (native stub for host/desktop testing)
template <>
fl::shared_ptr<RxDevice> RxDevice::create<RxDeviceType::ISR>(int pin) {
    return NativeRxDevice::create(pin);
}

// FLEXPWM device specialization (native stub for host/desktop testing)
template <>
fl::shared_ptr<RxDevice> RxDevice::create<RxDeviceType::FLEXPWM>(int pin) {
    return NativeRxDevice::create(pin);
}

#else

// RMT device specialization (dummy for non-ESP32, non-stub)
template <>
fl::shared_ptr<RxDevice> RxDevice::create<RxDeviceType::RMT>(int pin) {
    (void)pin;  // Suppress unused parameter warning
    return RxDevice::createDummy();
}

// ISR device specialization (dummy for non-ESP32, non-stub)
template <>
fl::shared_ptr<RxDevice> RxDevice::create<RxDeviceType::ISR>(int pin) {
    (void)pin;  // Suppress unused parameter warning
    return RxDevice::createDummy();
}

// FLEXPWM device specialization (dummy for unsupported platforms)
template <>
fl::shared_ptr<RxDevice> RxDevice::create<RxDeviceType::FLEXPWM>(int pin) {
    (void)pin;  // Suppress unused parameter warning
    return RxDevice::createDummy();
}

#endif // FL_IS_ESP32 / FL_IS_TEENSY_4X / FL_IS_STUB

} // namespace fl
