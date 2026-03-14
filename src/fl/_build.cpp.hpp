/// @file _build.hpp
/// @brief Unity build header for fl/ directory
/// Includes all implementation files in alphabetical order

// Note: Teensy 3.x <new.h> compatibility is handled in _build.cpp
// which includes is_teensy.h before this header.

#include "fl/engine_events.cpp.hpp"
#include "fl/fastled_internal.cpp.hpp"
#include "fl/fltest.cpp.hpp"
#include "fl/id_tracker.cpp.hpp"
#include "fl/leds.cpp.hpp"
#include "fl/memfill.cpp.hpp"
#include "fl/ota.cpp.hpp"
#include "fl/pin.cpp.hpp"
#include "fl/pins.cpp.hpp"
#include "fl/remote/_build.cpp.hpp"
#include "fl/rx_device.cpp.hpp"
#include "fl/serial.cpp.hpp"
#include "fl/system/_build.cpp.hpp"
#include "fl/sin32.cpp.hpp"
#include "fl/spi.cpp.hpp"
#include "fl/static_constexpr_defs.cpp.hpp"
#include "fl/str.cpp.hpp"
#include "fl/str_ui.cpp.hpp"
#include "fl/time_alpha.cpp.hpp"
#include "fl/trace.cpp.hpp"
#include "fl/transposition.cpp.hpp"
#include "fl/ui.cpp.hpp"
