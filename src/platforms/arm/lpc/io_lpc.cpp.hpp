#pragma once

// IWYU pragma: private
#include "platforms/arm/lpc/is_lpc.h"

#ifdef FL_IS_ARM_LPC

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

// Serial initialization stub - TODO: implement UART for LPC8xx
void begin(u32 baudRate) FL_NOEXCEPT {
    (void)baudRate;
    // TODO: Configure USART0 for serial output
}

// Print functions - TODO: implement UART output
void print(const char* str) FL_NOEXCEPT {
    (void)str;
    // TODO: Send string via USART0
    // For now, no-op to allow compilation
}

void println(const char* str) FL_NOEXCEPT {
    print(str);
    print("\n");
}

// Input functions stub
int available() FL_NOEXCEPT {
    return 0;  // No input available
}

int read() FL_NOEXCEPT {
    return -1;  // No data
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_ARM_LPC
