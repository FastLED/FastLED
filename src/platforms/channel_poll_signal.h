// ok no namespace fl
#pragma once

/// @file platforms/channel_poll_signal.h
/// @brief Platform dispatch for channel-manager poll wake signals.

#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
#include "platforms/esp/32/channel_poll_signal_esp32.h"
#else
#include "platforms/shared/channel_poll_signal.h"
#endif
