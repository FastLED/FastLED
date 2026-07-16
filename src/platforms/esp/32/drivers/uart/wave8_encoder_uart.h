/// @file wave8_encoder_uart.h
/// @brief Legacy ESP-path forwarding header for the UART waveN encoder.
///
/// The encoder is platform-neutral (it models only an inverted UART frame
/// and `ChipsetTimingConfig`), so the implementation lives at the channel
/// layer. This header is retained for source compatibility with existing
/// ESP-path includes.

#pragma once

// IWYU pragma: private

// ok no namespace fl — pure forwarding header, no declarations of its own

#include "fl/channels/uart_wave_encoder.h"
