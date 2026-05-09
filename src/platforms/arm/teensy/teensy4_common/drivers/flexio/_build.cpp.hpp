// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for drivers/flexio/ directory

#include "platforms/arm/teensy/teensy4_common/drivers/flexio/channel_engine_flexio.cpp.hpp"
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/flexio_driver.cpp.hpp"

// BusTraits<Bus::FLEX_IO> specialization.
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/bus_traits.h"
