// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\uart/ directory
/// Includes all implementation files in alphabetical order
///
/// Phase 5c of #2428: UART is never a platform default. When the user opts
/// in to `FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY=1`, this TU becomes empty
/// and the UART driver is dropped from the binary.

#include "fl/channels/bus.h"  // brings in FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY

#if !FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY

#include "platforms/esp/32/drivers/uart/channel_driver_uart.cpp.hpp"
#include "platforms/esp/32/drivers/uart/uart_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/uart/wave8_encoder_uart.cpp.hpp"

// BusTraits<Bus::UART> specialization.
#include "platforms/esp/32/drivers/uart/bus_traits.h"

#endif  // !FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY
