// IWYU pragma: private

/// @file _build.cpp.hpp
/// @brief Unity build header for mock/arm/teensy4/drivers/lpuart/ directory.
///
/// Provides the mock LPUART peripheral factory (`ILPUARTPeripheral::create()`)
/// for non-Teensy hosts. The real iMXRT register driver lives next to the
/// engine TU and is compiled via the Teensy platform's `_build.cpp.hpp`
/// hierarchy in a follow-up PR.

#include "platforms/shared/mock/arm/teensy4/drivers/lpuart/lpuart_peripheral_mock.cpp.hpp"
