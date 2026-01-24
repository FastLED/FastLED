/// @file wave8_encoder_uart.cpp
/// @brief Implementation of wave8 UART encoder
///
/// This file contains the implementation of the LED-to-UART encoding function.
/// The encoding logic is kept simple with inline helper functions for performance.

#include "wave8_encoder_uart.h"

namespace fl {

FL_IRAM FL_OPTIMIZE_FUNCTION
size_t encodeLedsToUart(const uint8_t* input,
                        size_t input_size,
                        uint8_t* output,
                        size_t output_capacity) {
    // Calculate required output buffer size
    size_t required_size = input_size * 4;  // 4 UART bytes per LED byte

    // Validate buffer capacity
    if (required_size > output_capacity) {
        return 0;  // Insufficient capacity
    }

    // Encode each input byte to 4 output bytes
    // Loop is simple and compiler-friendly for auto-vectorization
    for (size_t i = 0; i < input_size; ++i) {
        detail::encodeUartByte(input[i], &output[i * 4]);
    }

    return required_size;
}

} // namespace fl
