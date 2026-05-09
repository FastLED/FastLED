// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\uart/ directory
/// Includes all implementation files in alphabetical order

#include "platforms/esp/32/drivers/uart/channel_driver_uart.cpp.hpp"
#include "platforms/esp/32/drivers/uart/uart_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/uart/wave8_encoder_uart.cpp.hpp"

// BusTraits<Bus::UART> specialization.
#include "platforms/esp/32/drivers/uart/bus_traits.h"
