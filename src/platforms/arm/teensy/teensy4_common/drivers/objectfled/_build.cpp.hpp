// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for drivers/objectfled/ directory

#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/channel_engine_objectfled.cpp.hpp"
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/objectfled_peripheral_real.cpp.hpp"

// BusTraits<Bus::OBJECT_FLED> specialization.
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/bus_traits.h"
