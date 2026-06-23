// IWYU pragma: private

/// @file lpuart_peripheral_mock.cpp.hpp
/// @brief Provides ILPUARTPeripheral::create() factory for non-Teensy platforms.
///
/// On Teensy 4.x the real iMXRT register driver provides this factory (lands
/// in a follow-up PR that adds `lpuart_peripheral_real.cpp.hpp` next to the
/// engine). On every other host (unit-test build, WASM, native) the factory
/// returns a fresh `LPUARTPeripheralMock` so engine-level tests run without
/// touching real hardware.

#include "platforms/arm/teensy/is_teensy.h"

// Only compile on non-Teensy platforms (Teensy will use the real driver TU).
#if !defined(FL_IS_TEENSY_4X)

#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/ilpuart_peripheral.h"
#include "platforms/shared/mock/arm/teensy4/drivers/lpuart/lpuart_peripheral_mock.h"
#include "fl/stl/noexcept.h"

namespace fl {

fl::shared_ptr<ILPUARTPeripheral> ILPUARTPeripheral::create() FL_NOEXCEPT {
    return fl::make_shared<LPUARTPeripheralMock>();
}

} // namespace fl

#endif // !FL_IS_TEENSY_4X
