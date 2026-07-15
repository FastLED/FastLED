#pragma once

// The encoder implementation is include-once so platform unity builds may
// share it without duplicate symbols.  The legacy implementation path is
// retained for source compatibility while this is the neutral build entry.
#include "platforms/esp/32/drivers/uart/wave8_encoder_uart.cpp.hpp"
