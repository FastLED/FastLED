// IWYU pragma: private

/// @file _build.cpp.hpp
/// @brief Unity build header for drivers/lpuart/ directory

#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/channel_engine_lpuart.cpp.hpp"
#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/lpuart_driver.cpp.hpp"
#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/lpuart_peripheral_real.cpp.hpp"

// BusTraits<Bus::UART> specialization.
#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/bus_traits.h"
