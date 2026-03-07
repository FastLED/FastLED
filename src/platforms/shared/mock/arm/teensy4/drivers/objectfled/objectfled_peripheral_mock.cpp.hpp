// IWYU pragma: private

/// @file objectfled_peripheral_mock.cpp.hpp
/// @brief Provides IObjectFLEDPeripheral::create() factory for non-Teensy platforms

#include "platforms/arm/teensy/is_teensy.h"

// Only compile on non-Teensy platforms (Teensy uses real ObjectFLED hardware)
#if !defined(FL_IS_TEENSY_4X)

#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/iobjectfled_peripheral.h"
#include "platforms/shared/mock/arm/teensy4/drivers/objectfled/objectfled_peripheral_mock.h"

namespace fl {

// Factory: create mock peripheral on non-Teensy platforms
fl::shared_ptr<IObjectFLEDPeripheral> IObjectFLEDPeripheral::create() {
    return fl::make_shared<ObjectFLEDPeripheralMock>();
}

} // namespace fl

#endif // !FL_IS_TEENSY_4X
