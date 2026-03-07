// IWYU pragma: private

/// @file flexio_peripheral_mock.cpp.hpp
/// @brief Provides IFlexIOPeripheral::create() factory for non-Teensy platforms

#include "platforms/arm/teensy/is_teensy.h"

// Only compile on non-Teensy platforms (Teensy uses real FlexIO hardware)
#if !defined(FL_IS_TEENSY_4X)

#include "platforms/arm/teensy/teensy4_common/drivers/flexio/iflexio_peripheral.h"
#include "platforms/shared/mock/arm/teensy4/drivers/flexio/flexio_peripheral_mock.h"

namespace fl {

// Factory: create mock peripheral on non-Teensy platforms
fl::shared_ptr<IFlexIOPeripheral> IFlexIOPeripheral::create() {
    return fl::make_shared<FlexIOPeripheralMock>();
}

} // namespace fl

#endif // !FL_IS_TEENSY_4X
