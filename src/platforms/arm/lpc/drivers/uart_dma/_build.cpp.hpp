// IWYU pragma: private

/// @file _build.cpp.hpp
/// @brief Unity build header for the LPC845 UART DMA channel engine.

#include <stdint.h>

#if defined(FL_LPC_UART_DMA_LOOPBACK_TEST)
extern "C" {
uint8_t* g_fastled_lpc_uart_loopback_rx_dst = nullptr;
uint32_t g_fastled_lpc_uart_loopback_rx_len = 0;
uint32_t g_fastled_lpc_uart_loopback_rx_arm_status = 0;
uint32_t g_fastled_lpc_uart_loopback_rx_arm_tx_len = 0;
uint32_t g_fastled_lpc_uart_loopback_rx_arm_xfer_readback = 0;
uint32_t g_fastled_lpc_uart_loopback_rx_arm_desc0_readback = 0;
}
#endif

#include "platforms/arm/lpc/drivers/uart_dma/channel_engine_lpc_uart_dma.cpp.hpp"
