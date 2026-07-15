#pragma once

/// @file uart_wave_encoder.h
/// @brief Platform-neutral UART wave4/wave5 encoder API.
///
/// The implementation originated with the ESP UART backend, but it models
/// only an inverted UART frame and `ChipsetTimingConfig`; RP UART DMA and
/// future backends consume this stable channel-layer API.  Keep the legacy
/// ESP-path header as a forwarding-compatible include while callers use this
/// neutral path.

#include "platforms/esp/32/drivers/uart/wave8_encoder_uart.h"
